// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_GeneratorState.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

#include <Mib/Core/RuntimeType>
#include <Mib/Cryptography/UUID>
#include <Mib/Encoding/EJSON>

namespace NMib::NBuildSystem
{
	CUniversallyUniqueIdentifier g_GeneratorStateUUIDNamespace("{8220886E-63BD-4F4E-B32B-71D68D7346D4}");

	namespace
	{
		struct CLocalGeneratorInteface : public CGeneratorInterface
		{
			CLocalGeneratorInteface(CStr const &_OutputDir)
				: m_OutputDir(_OutputDir)
			{
			}

			bool f_GetBuiltin(CStr const &_Value, CStr &_Result) const override
			{
				if (_Value == "GeneratedBuildSystemDir")
				{
					_Result = m_OutputDir;
					return true;
				}
				return false;
			}

			CStr f_GetExpandedPath(CStr const &_Path, CStr const& _Base) const override
			{
				return CFile::fs_GetExpandedPath(_Path, _Base);
			}

			CSystemEnvironment f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const override
			{
				return fg_GetSys()->f_Environment();
			}

			CStr m_OutputDir;
		};
	}

	void CBuildSystem::fp_SaveEnvironment()
	{
		try
		{
			CEJSON JSON;
			for (auto &EnvVar : mp_SaveEnvironment)
				JSON[mp_SaveEnvironment.fs_GetKey(EnvVar)] = EnvVar;

			TCVector<uint8> FileData;
			CFile::fs_WriteStringToVector(FileData, JSON.f_ToString());

			CStr EnvironmentStateFile = CFile::fs_AppendPath(mp_OutputDir, "Environment.json");

			CFile::fs_CreateDirectory(CFile::fs_GetPath(EnvironmentStateFile));
			CFile::fs_CopyFileDiff(FileData, EnvironmentStateFile, CTime::fs_NowUTC());
		}
		catch (NException::CException const &_Exception)
		{
			DMibError("Failed to save Environment.json: {}"_f << _Exception);
		}

	}
	
	bool CBuildSystem::f_Generate(CGenerateSettings const &_GenerateSettings, ERetry &o_Retry)
	{
		o_Retry = ERetry_None;
		CClock Clock;
		Clock.f_Start();
		
		mp_GenerateSettings = _GenerateSettings;
		
		if (_GenerateSettings.m_GenerationFlags & EGenerationFlag_SingleThreaded)
			g_ThreadPool.f_Construct(0);
		else
			g_ThreadPool.f_Construct();

		// DMibScopeConOutTimer("Generate");
		if (_GenerateSettings.m_Action == "Clean")
			return false; // For now we don't support clean

		bool bBuildAction = _GenerateSettings.m_Action == "Build" || _GenerateSettings.m_Action == "ReBuild";

		TCUniquePointer<CBuildSystemGenerator> pGenerator 
			= fg_Explicit(fg_CreateRuntimeType<CBuildSystemGenerator>(NMib::NStr::CStr("CBuildSystemGenerator_") + _GenerateSettings.m_Generator));

		if (!pGenerator)
			DError(CStr::CFormat("No such generator: {}") << _GenerateSettings.m_Generator);

		CStr FileLocation = CFile::fs_GetExpandedPath(_GenerateSettings.m_SourceFile);

		CStr OutputDir;

		if (_GenerateSettings.m_OutputDir.f_IsEmpty())
			OutputDir = "{}/BuildSystem/Default"_f << CFile::fs_GetPath(FileLocation);
		else
			OutputDir = CFile::fs_GetExpandedPath(_GenerateSettings.m_OutputDir);

		CStr RelativeFileLocation = CFile::fs_MakePathRelative(FileLocation, OutputDir);

		mp_OutputDir = OutputDir;
		mp_GenerateWorkspace = _GenerateSettings.m_Workspace;

		CStr GlobalGeneratorStateFileName;
		{
			CUniversallyUniqueIdentifier UniqueIdentifier
				(
					EUniversallyUniqueIdentifierGenerate_StringHash
					, g_GeneratorStateUUIDNamespace
					, "Global!" + RelativeFileLocation + _GenerateSettings.m_Generator
				)
			;

			GlobalGeneratorStateFileName = CFile::fs_AppendPath(OutputDir, "GeneratorStates/" + UniqueIdentifier.f_GetAsString() + ".MGeneratorState");
		}
		CStr WorkspaceGeneratorStateFileName;
		{
			CUniversallyUniqueIdentifier UniqueIdentifier
				(
					EUniversallyUniqueIdentifierGenerate_StringHash
					, g_GeneratorStateUUIDNamespace
					, RelativeFileLocation + _GenerateSettings.m_Generator + mp_GenerateWorkspace
				)
			;

			WorkspaceGeneratorStateFileName = CFile::fs_AppendPath(OutputDir, "GeneratorStates/" + UniqueIdentifier.f_GetAsString() + ".MGeneratorState");
		}

		CStr EnvironmentStateFile = CFile::fs_AppendPath(OutputDir, "Environment.json");

		mp_GeneratorStateFileName = GlobalGeneratorStateFileName;
		bool bUseCachedEnvironment = (_GenerateSettings.m_GenerationFlags & EGenerationFlag_UseCachedEnvironment) != EGenerationFlag_None;

		if (bUseCachedEnvironment)
		{
			if (!CFile::fs_FileExists(EnvironmentStateFile, EFileAttrib_File))
			{
				DMibConErrOut2("Cached environment was not found at: {}. Saving current environment.\n", EnvironmentStateFile);
				mp_Environment = mp_SaveEnvironment = fg_GetSys()->f_Environment();
				fp_SaveEnvironment();
			}

			mp_SaveEnvironment.f_Clear();
			mp_Environment.f_Clear();
			try
			{
				auto EnvironmentJSON = CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(EnvironmentStateFile, true), EnvironmentStateFile);
				for (auto &EnvVar : EnvironmentJSON.f_Object())
					mp_Environment[EnvVar.f_Name()] = EnvVar.f_Value().f_String();
			}
			catch (NException::CException const &_Exception)
			{
				DMibError("Failed to parse Environment.json: {}"_f << _Exception);
			}
			mp_SaveEnvironment = mp_Environment;
		}
		else
			mp_SaveEnvironment = mp_Environment = fg_GetSys()->f_Environment();

		CStr OverrideEnvironmentFile = CFile::fs_AppendPath(OutputDir, "OverrideEnvironment.json");
		bool bFoundOverrideEnvironmentFile = false;
		{
			if (CFile::fs_FileExists(OverrideEnvironmentFile, EFileAttrib_File))
			{
				bFoundOverrideEnvironmentFile = true;
				try
				{
					auto EnvironmentJSON = CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(OverrideEnvironmentFile, true), OverrideEnvironmentFile);
					for (auto &EnvVar : EnvironmentJSON.f_Object())
						mp_Environment[EnvVar.f_Name()] = EnvVar.f_Value().f_String();
				}
				catch (NException::CException const &_Exception)
				{
					DMibError("Failed to parse Environment.json: {}"_f << _Exception);
				}
			}
		}

		if (bBuildAction)
		{
			if (CFile::fs_FileExists(WorkspaceGeneratorStateFileName, EFileAttrib_File))
			{
				CGeneratorState State;
				try
				{
					TCVector<uint8> FileData = CFile::fs_ReadFileTry(WorkspaceGeneratorStateFileName);
					CBinaryStreamMemory<> Stream;
					Stream.f_Open(FileData);
					Stream >> State;
				}
				catch (...)
				{
					State = CGeneratorState();
				}

				if (!State.m_SourceFiles.f_IsEmpty() && _GenerateSettings.m_Action == "Build")
				{
					TCAtomic<bool> bChanged;

					// DMibScopeConOutTimer("Check source files");

					auto fFileChanged
						= [&bChanged, &OutputDir](CGeneratorState::CProcessedFile const &_File) -> bool
						{
							CStr FileName = CFile::fs_GetExpandedPath(_File.f_GetFileName(), OutputDir);

							if (_File.m_Flags & EGeneratedFileFlag_NoDateCheck)
							{
								try
								{
									if (!CFile::fs_FileExists(FileName))
									{
										if (!bChanged.f_Exchange(true))
											DConOut("Dependency check: Regenerating build system because file is missing: {}{\n}", FileName);
										return true;
									}
								}
								catch (...)
								{
									if (!bChanged.f_Exchange(true))
										DConOut("Dependency check: Regenerating build system because file is missing: {}{\n}", FileName);
									return true;
								}
							}
							else
							{
#ifdef DPlatformFamily_Windows
								try
								{
									if (!_File.m_WriteTime.f_IsValid())
									{
										if (CFile::fs_FileExists(FileName))
										{
											if (!bChanged.f_Exchange(true))
												DConOut2("Dependency check: Regenerating build system because file now exists: {}{\n}", FileName);
											return true;
										}
									}
									else
									{
										CFile File;
										File.f_Open(FileName, EFileOpen_ReadAttribs | EFileOpen_ShareAll);
										NTime::CTime DiskTime = File.f_GetWriteTime();
										if (DiskTime != _File.m_WriteTime)
										{
											if (!bChanged.f_Exchange(true))
											{
												DConOut
													(
														"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"
														, NTime::fg_GetFullTimeStr(DiskTime) << NTime::fg_GetFullTimeStr(_File.m_WriteTime) << FileName
													)
												;
											}
											return true;
										}
									}
								}
								catch (...)
								{
									try
									{
										CFile File;
										File.f_Open(FileName, EFileOpen_Directory | EFileOpen_ReadAttribs | EFileOpen_ShareAll);
										NTime::CTime DiskTime = File.f_GetWriteTime();
										if (DiskTime != _File.m_WriteTime)
										{
											if (!bChanged.f_Exchange(true))
											{
												DConOut
													(
														"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"
														, NTime::fg_GetFullTimeStr(DiskTime) << NTime::fg_GetFullTimeStr(_File.m_WriteTime) << FileName
													)
												;
											}
											return true;
										}
									}
									catch (CException const &_Exception)
									{
										if (!bChanged.f_Exchange(true))
											DConOut2("Dependency check: Regenerating build system because file date check failed: {}{\n}{}{\n}", FileName, _Exception);
										return true;
									}
									catch (...)
									{
										if (!bChanged.f_Exchange(true))
											DConOut("Dependency check: Regenerating build system because file is missing: {}{\n}", FileName);
										return true;
									}
								}
#else
								try
								{
									if (!_File.m_WriteTime.f_IsValid())
									{
										if (CFile::fs_FileExists(FileName))
										{
											if (!bChanged.f_Exchange(true))
												DConOut2("Dependency check: Regenerating build system because file now exists: {}{\n}", FileName);
											return true;
										}
									}
									else
									{
										NTime::CTime DiskTime = CFile::fs_GetWriteTime(FileName);
										if (DiskTime != _File.m_WriteTime)
										{
											if (!bChanged.f_Exchange(true))
											{
												DConOut
													(
														"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"
														, NTime::fg_GetFullTimeStr(DiskTime) << NTime::fg_GetFullTimeStr(_File.m_WriteTime) << FileName
													)
												;
											}
											return true;
										}
									}
								}
								catch (CException const &_Exception)
								{
									if (!bChanged.f_Exchange(true))
										DConOut2("Dependency check: Regenerating build system because file date check failed: {}{\n}{}{\n}", FileName, _Exception);
									return true;
								}
								catch (...)
								{
									if (!bChanged.f_Exchange(true))
										DConOut("Dependency check: Regenerating build system because file is missing: {}{\n}", FileName);
									return true;
								}
#endif
							}
							return false;
						}
					;

					auto OldExeFile = *State.m_ExeFile.f_FindSmallest();

					if (!bChanged)
					{
						State.m_ExeFile.f_Clear();
						auto &ExeFile = State.m_ExeFile[CFile::fs_MakePathRelative(CStr(CFile::fs_GetProgramPath()), OutputDir)];
						ExeFile = OldExeFile;
						if (fFileChanged(ExeFile))
							bChanged = true;
					}

					if (!bChanged)
					{
						TCVector<TCFunctionMutable<void ()>> ToProcess;

						TCVector<CGeneratorState::CProcessedFile *> CurrentInsert;

						auto fAddFiles = [&](auto &&_Files)
							{
								for (auto iFile = _Files.f_GetIterator(); iFile; ++iFile)
								{
									CurrentInsert.f_Insert(&*iFile);
									if (CurrentInsert.f_GetLen() >= 128)
									{
										ToProcess.f_Insert
											(
											 	[Files = fg_Move(CurrentInsert), &fFileChanged]() mutable
											 	{
													for (auto pFile : Files)
														fFileChanged(*pFile);
												}
											)
										;
									}
								}
							}
						;

						fAddFiles(State.m_SourceFiles);
						fAddFiles(State.m_ReferencedFiles);
						fAddFiles(State.m_GeneratedFiles);
						ToProcess.f_Insert
							(
								[Files = fg_Move(CurrentInsert), &fFileChanged]() mutable
								{
									for (auto pFile : Files)
										fFileChanged(*pFile);
								}
							)
						;

						auto fSearchChanged = [&](TCVector<CFile::CFoundFile> const &_Search)
							{
								auto &Search = _Search;
								auto &FindOptions = TCMap<CFindOptions, TCVector<CFile::CFoundFile>>::fs_GetKey(_Search);
								auto Files = mp_FindCache.f_FindFiles(FindOptions, false);
								bool bChangedLocal = false;
								if (Files.f_GetLen() != Search.f_GetLen())
									bChangedLocal = true;
								else
								{
									auto iLeft = Files.f_GetIterator();
									auto iRight = Search.f_GetIterator();
									for (;iLeft; ++iLeft, ++iRight)
									{
										if (iLeft->m_Path != iRight->m_Path)
										{
											bChangedLocal = true;
											break;
										}
										if ((iLeft->m_Attribs & (EFileAttrib_File | EFileAttrib_Directory)) != (iRight->m_Attribs & (EFileAttrib_File | EFileAttrib_Directory)))
										{
											bChangedLocal = true;
											break;
										}
									}
								}
								if (bChangedLocal)
								{
									if (!bChanged.f_Exchange(true))
										DConOut("Dependency check: Regenerating build system because search changed: {}" DNewLine, FindOptions.m_Path);
								}
							}
						;

						for (auto iSearch = State.m_SourceSearches.f_GetIterator(); iSearch; ++iSearch)
						{
							ToProcess.f_Insert
								(
									[pSearch = &*iSearch, &fSearchChanged]() mutable
									{
										fSearchChanged(*pSearch);
									}
								)
							;
						}

						fg_ParallellForEach
							(
								ToProcess
								, [&](auto &&_fToProcess)
								{
									_fToProcess();
								}
							)
						;
					}

					if (!bChanged)
					{
						for (auto iEnv = State.m_Environment.f_GetIterator(); iEnv; ++iEnv)
						{
							//DConOut("{} = {}" DNewLine, iEnv.f_GetKey() << *iEnv);
							CStr Value = f_GetEnvironmentVariable(iEnv.f_GetKey());

							if (Value != *iEnv)
							{
								DConOut("Dependency check: Regenerating build system because env var '{}' changed: '{}' != '{}'" DNewLine, iEnv.f_GetKey() << Value << *iEnv);
								bChanged = true;
								break;
							}

						}
					}

					EGenerationFlag InterestingGenerationFlags = EGenerationFlag_AbsoluteFilePaths | EGenerationFlag_DisableUserSettings;

					if ((mp_GenerateSettings.m_GenerationFlags & InterestingGenerationFlags) != (State.m_GenerationFlags & InterestingGenerationFlags))
					{
						DConOut
							(
							 	"Dependency check: Regenerating build system because generation flags changed {} != {}" DNewLine
							 	, mp_GenerateSettings.m_GenerationFlags << State.m_GenerationFlags
							)
						;
						bChanged = true;
					}

					if (!bChanged)
					{
						fp64 Time1 = Clock.f_GetTime();
						DConOut("Dependency check: Checked for changes {fe2} s{\n}", Time1);

						return false;
					}
				}
				else
					DConOut("Dependency check: Regenerating build system because there are no source files in state" DNewLine, 0);
			}
			else
				DConOut("Dependency check: Regenerating build system because there is no state file" DNewLine, 0);

		}

		fp64 Time1 = Clock.f_GetTime();

		if (bBuildAction)
			DConOut("Checked for changes {fe2} s{\n}", Time1);

		CGeneratorState GlobalState;

		if (CFile::fs_FileExists(GlobalGeneratorStateFileName, EFileAttrib_File))
		{
			try
			{
				TCVector<uint8> FileData = CFile::fs_ReadFileTry(GlobalGeneratorStateFileName);
				CBinaryStreamMemory<> Stream;
				Stream.f_Open(FileData);
				Stream >> GlobalState;

				if (GlobalState.m_GeneratedFiles.f_IsEmpty())
					mp_GenerateWorkspace.f_Clear();
			}
			catch (...)
			{
				GlobalState = CGeneratorState();
				mp_GenerateWorkspace.f_Clear();
			}
		}
		else
			mp_GenerateWorkspace.f_Clear(); // If we don't have global state we need to create the whole thing to get the correct generated files

		CGeneratorState BeforeGlobalState = GlobalState;
		
		if (mp_GenerateWorkspace.f_IsEmpty())
		{
			GlobalState.m_GeneratedFiles.f_Clear();
		}
		else
		{
			for (auto iFile = GlobalState.m_GeneratedFiles.f_GetIterator(); iFile; )
			{
				iFile->m_Workspaces.f_Remove(mp_GenerateWorkspace);
				if (iFile->m_Workspaces.f_IsEmpty())
				{
					iFile.f_Remove();
					continue;
				}

				++iFile;
			}
		}

		bool bDisableUserSettings = (_GenerateSettings.m_GenerationFlags & EGenerationFlag_DisableUserSettings) != EGenerationFlag_None;

		CStr UserSettingsFileNameLocal = OutputDir / "UserSettings.MSettings";
		CStr UserSettingsFileNameGlobal = CFile::fs_GetUserHomeDirectory() / "UserSettingGlobal.MSettings";
		mp_UserSettingsFileLocal = UserSettingsFileNameLocal;
		mp_UserSettingsFileGlobal = UserSettingsFileNameGlobal;

		auto GeneratorValues = pGenerator->f_GetValues(*this, OutputDir);

		{
			CLocalGeneratorInteface LocalInterface{OutputDir};
			auto pOldInterface = fg_Move(mp_GeneratorInterface);
			auto Cleanup = g_OnScopeExit > [&]
				{
					mp_GeneratorInterface = fg_Move(pOldInterface);
				}
			;
			mp_GeneratorInterface = &LocalInterface;

			TCMap<CStr, EHandleRepositoryAction> RepositoryActions;

			auto ActionParams = _GenerateSettings.m_ActionParams;
			bool bSkipRepoUpdate = false;
			{
				CDisableExceptionTraceScope DisableTrace;
				auto OldActionParams = fg_Move(ActionParams);
				for (auto &Param : OldActionParams)
				{
					if (Param == "--skip-update")
					{
						bSkipRepoUpdate = true;
						continue;
					}

					if (Param.f_StartsWith("--reconcile="))
					{
						for (auto &RepoOptions : Param.f_Extract(12).f_Split(","))
						{
							CStr WildCard;
							CStr ActionStr;
							aint nParsed = 0;
							(CStr::CParse("{}:{}") >> WildCard >> ActionStr).f_Parse(RepoOptions, nParsed);
							if (nParsed != 2)
								DError("Invalid format for --reconcile. Expected --reconcile=Wildcard:Action[,Wildcard:Action]...");
							EHandleRepositoryAction Action;
							if (ActionStr == "auto")
								Action = EHandleRepositoryAction_Auto;
							else if (ActionStr == "reset")
								Action = EHandleRepositoryAction_Reset;
							else if (ActionStr == "rebase")
								Action = EHandleRepositoryAction_Rebase;
							else if (ActionStr == "reset_delete")
								Action = EHandleRepositoryAction_ResetDelete;
							else if (ActionStr == "leave_removed")
								Action = EHandleRepositoryAction_LeaveRemoved;
							else if (ActionStr == "delete_removed")
								Action = EHandleRepositoryAction_DeleteRemoved;
							else
								DError("Invalid format for --reconcile. Expected action to be one of: [auto, reset, rebase]");
							RepositoryActions[WildCard] = Action;
						}
						continue;
					}

					ActionParams.f_Insert(Param);
				}
			}

			bool bTryParsed = false;
			try
			{
				mp_FileLocation = CFile::fs_GetExpandedPath(_GenerateSettings.m_SourceFile);
				mp_BaseDir = CFile::fs_GetPath(mp_FileLocation);
				mp_FileLocationFile = CFile::fs_GetFile(mp_FileLocation);

				if (!bDisableUserSettings)
				{
					if (CFile::fs_FileExists(UserSettingsFileNameGlobal))
					{
						CBuildSystemPreprocessor Preprocessor(mp_UserSettingsGlobal.m_Registry, mp_SourceFiles, mp_FindCache, mp_Environment);
						Preprocessor.f_ReadFile(UserSettingsFileNameGlobal);
						mp_Registry = mp_UserSettingsGlobal.m_Registry;
					}
					if (CFile::fs_FileExists(UserSettingsFileNameLocal))
					{
						// Once to get only local registry
						{
							CBuildSystemPreprocessor Preprocessor(mp_UserSettingsLocal.m_Registry, mp_SourceFiles, mp_FindCache, mp_Environment);
							Preprocessor.f_ReadFile(UserSettingsFileNameLocal);
						}
						// Once to merge with global settings
						{
							CBuildSystemPreprocessor Preprocessor(mp_Registry, mp_SourceFiles, mp_FindCache, mp_Environment);
							Preprocessor.f_ReadFile(UserSettingsFileNameLocal);
						}
					}
				}
				{
					CBuildSystemPreprocessor Preprocessor(mp_Registry, mp_SourceFiles, mp_FindCache, mp_Environment);
					Preprocessor.f_ReadFile(_GenerateSettings.m_SourceFile);
				}
				if (!bDisableUserSettings)
				{
					mp_SourceFiles[UserSettingsFileNameLocal];
					mp_SourceFiles[UserSettingsFileNameGlobal];
				}

				mp_SourceFiles[EnvironmentStateFile];
				mp_SourceFiles[OverrideEnvironmentFile];

				bTryParsed = true;
				fp_ParseData(mp_Data.m_RootEntity, mp_Registry, &mp_Data.m_ConfigurationTypes);
			}
			catch (CException const &)
			{
				if (!bTryParsed)
					fp_ParseData(mp_Data.m_RootEntity, mp_Registry, &mp_Data.m_ConfigurationTypes);
				if (auto Retry = fp_HandleRepositories(GeneratorValues, bSkipRepoUpdate, RepositoryActions))
				{
					o_Retry = Retry;
					return false;
				}
				else
					throw;
			}

			if (auto Retry = fp_HandleRepositories(GeneratorValues, bSkipRepoUpdate, RepositoryActions))
			{
				o_Retry = Retry;
				return false;
			}

			if (!bBuildAction)
			{
				fp_HandleAction(_GenerateSettings.m_Action, ActionParams);
				return false;
			}
		}

		// Clear out evaluated properties from repositories
		mp_Data.m_RootEntity.m_EvaluatedProperties.f_Clear();

		fp64 Time2 = Clock.f_GetTime();
		DConOut("Parsed data {fe2} s{\n}", Time2 - Time1);

		pGenerator->f_Generate(*this, mp_Data, OutputDir);

		fp64 Time3 = Clock.f_GetTime();

		if (!bDisableUserSettings)
		{
			auto fSaveFile = [&](auto &_Registry, auto &_FileName)
				{
					auto StringData = _Registry.f_GenerateStr();

					TCVector<uint8> FileData;
					CFile::fs_WriteStringToVector(FileData, StringData);

					CFile::fs_CreateDirectory(CFile::fs_GetPath(_FileName));
					CFile::fs_CopyFileDiff(FileData, _FileName, CTime::fs_NowUTC());
				}
			;
			fSaveFile(mp_UserSettingsLocal.m_Registry, UserSettingsFileNameLocal);
			fSaveFile(mp_UserSettingsGlobal.m_Registry, UserSettingsFileNameGlobal);
		}

		if (!bUseCachedEnvironment)
			fp_SaveEnvironment();

		{
			CGeneratorState State;
			auto fl_UpdateFileInfo
				= [&](TCMap<CStr, CGeneratorState::CProcessedFile> &_Files, CStr const &_FileName) -> CGeneratorState::CProcessedFile &
				{
					CStr FileName = CFile::fs_MakePathRelative(_FileName, OutputDir);
					auto &ProcessedFile = _Files[FileName];
					try
					{
						ProcessedFile.m_WriteTime = CFile::fs_GetWriteTime(_FileName);
					}
					catch (CExceptionFile const &)
					{
					}
					return ProcessedFile;
				}
			;

			fl_UpdateFileInfo(State.m_ExeFile, CFile::fs_GetProgramPath());

			mint nSourceFiles = 0;
			for (auto iFile = mp_SourceFiles.f_GetIterator(); iFile; ++iFile)
			{
				++nSourceFiles;
				fl_UpdateFileInfo(State.m_SourceFiles, *iFile);
			}

			mint nReferencedFiles = 0;
			{
				auto SourceFiles = mp_FindCache.f_GetSourceFiles();
				for (auto iFile = SourceFiles.f_GetIterator(); iFile; ++iFile)
				{
					++nReferencedFiles;
					fl_UpdateFileInfo(State.m_ReferencedFiles, *iFile).m_Flags |= EGeneratedFileFlag_NoDateCheck;
				}
			}

			mint nGeneratedFiles = 0;
			
			
			TCMap<CStr, NTime::CTime> GeneratedWriteTimes;
			{
				struct CToEdit
				{
					CStr const *m_pPath;
					NTime::CTime *m_pWriteTime;
				};
				TCVector<TCVector<CToEdit>> ToProcess;
				auto *pCurrentInsert = &ToProcess.f_Insert();
				for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
				{
					++nGeneratedFiles;
					auto &ToEdit = pCurrentInsert->f_Insert();
					ToEdit.m_pPath = &iFile.f_GetKey();
					ToEdit.m_pWriteTime = &GeneratedWriteTimes[iFile.f_GetKey()];
					
					if (pCurrentInsert->f_GetLen() >= 1024)
						pCurrentInsert = &ToProcess.f_Insert();
				}
				fg_ParallellForEach
					(
						ToProcess
						, [&](TCVector<CToEdit> const &_Files)
						{
							for (auto &ToEdit : _Files)
							{
								try
								{
									*ToEdit.m_pWriteTime = CFile::fs_GetWriteTime(*ToEdit.m_pPath);
								}
								catch (CExceptionFile const &)
								{
								}
							}
						}
					)
				;
			}
			
			for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
			{
				CStr FileName = CFile::fs_MakePathRelative(iFile.f_GetKey(), OutputDir);
				auto &File = State.m_GeneratedFiles[FileName];
				File.m_WriteTime = GeneratedWriteTimes[iFile.f_GetKey()];
				File.m_Flags = iFile->m_Flags;
			}

			for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
			{
				CStr FileName = CFile::fs_MakePathRelative(iFile.f_GetKey(), OutputDir);
				auto &File = GlobalState.m_GeneratedFiles[FileName];
				File.m_WriteTime = GeneratedWriteTimes[iFile.f_GetKey()];
				File.m_Workspaces += iFile->m_Workspaces;
				File.m_Flags = iFile->m_Flags;
			}

			State.m_SourceSearches = mp_FindCache.f_GetAllTagged();

			mint nFileSearches = 0;
			for (auto iSearch = State.m_SourceSearches.f_GetIterator(); iSearch; ++iSearch)
			{
				++nFileSearches;
				nReferencedFiles += iSearch->f_GetLen();
			}
			DConOut("{} source files{\n}", nSourceFiles);
			DConOut("{} file searches{\n}", nFileSearches);
			DConOut("{} referenced files{\n}", nReferencedFiles);
			DConOut("{} generated files{\n}", nGeneratedFiles);

			for (auto iEnv = mp_UsedExternals.f_GetIterator(); iEnv; ++iEnv)
			{
				CStr Value = f_GetEnvironmentVariable(*iEnv);
				State.m_Environment[*iEnv] = Value;
			}

			GlobalState.m_GenerationFlags = mp_GenerateSettings.m_GenerationFlags;
			State.m_GenerationFlags = mp_GenerateSettings.m_GenerationFlags;

			{
				CBinaryStreamMemory<> Stream;
				Stream << State;

				CFile::fs_CreateDirectory(CFile::fs_GetPath(WorkspaceGeneratorStateFileName));
				CFile::fs_CopyFileDiff(Stream.f_MoveVector(), WorkspaceGeneratorStateFileName, CTime::fs_NowUTC());
			}
			{
				CBinaryStreamMemory<> Stream;
				Stream << GlobalState;

				CFile::fs_CreateDirectory(CFile::fs_GetPath(GlobalGeneratorStateFileName));
				CFile::fs_CopyFileDiff(Stream.f_MoveVector(), GlobalGeneratorStateFileName, CTime::fs_NowUTC());
			}
		}

		TCVector<CStr> DeletedFiles;
		TCSet<CStr> PotentiallyOrphanedDirectories;

		for (auto iFile = BeforeGlobalState.m_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
		{
			if (iFile->m_Flags & EGeneratedFileFlag_KeepGeneratedFile)
				continue;

			if (!GlobalState.m_GeneratedFiles.f_FindEqual(iFile.f_GetKey()))
			{
				CStr FileName = CFile::fs_GetExpandedPath(iFile.f_GetKey(), OutputDir);

				try
				{
					EFileAttrib ExistsMask = EFileAttrib_File;
					if (iFile->m_Flags & EGeneratedFileFlag_Symlink)
						ExistsMask = EFileAttrib_Link;

					if (CFile::fs_FileExists(FileName, ExistsMask))
					{
						CFile::fs_DeleteFile(FileName);
						DeletedFiles.f_Insert(FileName);
						PotentiallyOrphanedDirectories[CFile::fs_GetPath(FileName)];
					}
				}
				catch (CException const &_Exception)
				{
					DConOut("Error deleting file: {}{\n}", _Exception.f_GetErrorStr());
				}
			}
		}
		
		while (!PotentiallyOrphanedDirectories.f_IsEmpty())
		{
			TCSet<CStr> NewPotentiallyOrphanedDirectories;
			for (auto &Directory : PotentiallyOrphanedDirectories)
			{
				try
				{
					if (CFile::fs_FindFiles(Directory + "/*").f_IsEmpty())
					{
						CFile::fs_DeleteDirectory(Directory);
						NewPotentiallyOrphanedDirectories[CFile::fs_GetPath(Directory)];
					}
				}
				catch (CException const &_Exception)
				{
					DConOut("Error deleting directory: {}{\n}", _Exception.f_GetErrorStr());
				}
			}
			PotentiallyOrphanedDirectories = fg_Move(NewPotentiallyOrphanedDirectories);
		}

		if (DeletedFiles.f_GetLen() <= 32)
		{
			for (auto &File : DeletedFiles)
				DConOut("Deleted unused file: {}{\n}", File);
		}
		else
			DConOut("Deleted {} unused files{\n}", DeletedFiles.f_GetLen());
		
		fp64 Time4 = Clock.f_GetTime();
		DConOut("Saved state {fe2} s{\n}", Time4 - Time3);
		DConOut("Total time {fe2} s{\n}", Time4);
		
		return mp_FileChanged;
	}
}
