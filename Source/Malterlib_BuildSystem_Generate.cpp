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

	void CBuildSystem::f_NoReconcileOptions()
	{
		mp_bNoReconcileOptions = true;
	}

	CBuildSystem::ERetry CBuildSystem::fp_GeneratePrepare(CGenerateOptions const &_GenerateOptions, CGenerateEphemeralState &_GenerateState, TCFunction<bool ()> &&_fPreParse)
	{
		auto &GenerateSettings = _GenerateOptions.m_Settings;

		mp_GenerateOptions = _GenerateOptions;

		if (_GenerateOptions.m_bReconcileNoOptions)
			f_NoReconcileOptions();

		if (GenerateSettings.m_GenerationFlags & EGenerationFlag_SingleThreaded)
			g_ThreadPool.f_Construct(0);
		else
			g_ThreadPool.f_Construct();

		_GenerateState.m_pGenerator = fg_CreateRuntimeType<CBuildSystemGenerator>(NMib::NStr::CStr("CBuildSystemGenerator_") + GenerateSettings.m_Generator);

		if (!_GenerateState.m_pGenerator)
			DError(CStr::CFormat("No such generator: {}") << GenerateSettings.m_Generator);

		_GenerateState.m_FileLocation = CFile::fs_GetExpandedPath(GenerateSettings.m_SourceFile);

		if (GenerateSettings.m_OutputDir.f_IsEmpty())
			_GenerateState.m_OutputDir = "{}/BuildSystem/Default"_f << CFile::fs_GetPath(_GenerateState.m_FileLocation);
		else
			_GenerateState.m_OutputDir = CFile::fs_GetExpandedPath(GenerateSettings.m_OutputDir);

		_GenerateState.m_RelativeFileLocation = CFile::fs_MakePathRelative(_GenerateState.m_FileLocation, _GenerateState.m_OutputDir);

		mp_OutputDir = _GenerateState.m_OutputDir;
		mp_GenerateWorkspace = GenerateSettings.m_Workspace;

		{
			CUniversallyUniqueIdentifier UniqueIdentifier
				(
					EUniversallyUniqueIdentifierGenerate_StringHash
					, g_GeneratorStateUUIDNamespace
					, "Global!" + _GenerateState.m_RelativeFileLocation + GenerateSettings.m_Generator
				)
			;

			_GenerateState.m_GlobalGeneratorStateFileName = CFile::fs_AppendPath(_GenerateState.m_OutputDir, "GeneratorStates/" + UniqueIdentifier.f_GetAsString() + ".MGeneratorState");
		}
		{
			CUniversallyUniqueIdentifier UniqueIdentifier
				(
					EUniversallyUniqueIdentifierGenerate_StringHash
					, g_GeneratorStateUUIDNamespace
					, _GenerateState.m_RelativeFileLocation + GenerateSettings.m_Generator + mp_GenerateWorkspace
				)
			;

			_GenerateState.m_WorkspaceGeneratorStateFileName = CFile::fs_AppendPath(_GenerateState.m_OutputDir, "GeneratorStates/" + UniqueIdentifier.f_GetAsString() + ".MGeneratorState");
		}

		_GenerateState.m_EnvironmentStateFile = CFile::fs_AppendPath(_GenerateState.m_OutputDir, "Environment.json");

		mp_GeneratorStateFileName = _GenerateState.m_GlobalGeneratorStateFileName;
		_GenerateState.m_bUseCachedEnvironment = (GenerateSettings.m_GenerationFlags & EGenerationFlag_UseCachedEnvironment) != EGenerationFlag_None;

		if (_GenerateState.m_bUseCachedEnvironment)
		{
			if (!CFile::fs_FileExists(_GenerateState.m_EnvironmentStateFile, EFileAttrib_File))
			{
				DMibConErrOut2("Cached environment was not found at: {}. Saving current environment.\n", _GenerateState.m_EnvironmentStateFile);
				mp_Environment = mp_SaveEnvironment = fg_GetSys()->f_Environment();
				fp_SaveEnvironment();
			}

			mp_SaveEnvironment.f_Clear();
			mp_Environment.f_Clear();
			try
			{
				auto EnvironmentJSON = CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(_GenerateState.m_EnvironmentStateFile, true), _GenerateState.m_EnvironmentStateFile);
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

		CStr OverrideEnvironmentFile = CFile::fs_AppendPath(_GenerateState.m_OutputDir, "OverrideEnvironment.json");
		{
			if (CFile::fs_FileExists(OverrideEnvironmentFile, EFileAttrib_File))
			{
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

		if (_fPreParse && _fPreParse())
			return ERetry_None;

		if (CFile::fs_FileExists(_GenerateState.m_GlobalGeneratorStateFileName, EFileAttrib_File))
		{
			try
			{
				TCVector<uint8> FileData = CFile::fs_ReadFileTry(_GenerateState.m_GlobalGeneratorStateFileName);
				CBinaryStreamMemory<> Stream;
				Stream.f_Open(FileData);
				Stream >> _GenerateState.m_GlobalState;

				if (_GenerateState.m_GlobalState.m_GeneratedFiles.f_IsEmpty())
					mp_GenerateWorkspace.f_Clear();
			}
			catch (...)
			{
				_GenerateState.m_GlobalState = CGeneratorArchiveState();
				mp_GenerateWorkspace.f_Clear();
			}
		}
		else
			mp_GenerateWorkspace.f_Clear(); // If we don't have global state we need to create the whole thing to get the correct generated files

		_GenerateState.m_BeforeGlobalState = _GenerateState.m_GlobalState;

		if (mp_GenerateWorkspace.f_IsEmpty())
		{
			_GenerateState.m_GlobalState.m_GeneratedFiles.f_Clear();
		}
		else
		{
			for (auto iFile = _GenerateState.m_GlobalState.m_GeneratedFiles.f_GetIterator(); iFile; )
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

		_GenerateState.m_bDisableUserSettings = (GenerateSettings.m_GenerationFlags & EGenerationFlag_DisableUserSettings) != EGenerationFlag_None;

		CStr UserSettingsFileNameLocal = _GenerateState.m_OutputDir / "UserSettings.MSettings";
		CStr UserSettingsFileNameGlobal = CFile::fs_GetUserHomeDirectory() / "UserSettingGlobal.MSettings";
		mp_UserSettingsFileLocal = UserSettingsFileNameLocal;
		mp_UserSettingsFileGlobal = UserSettingsFileNameGlobal;

		_GenerateState.m_GeneratorValues = _GenerateState.m_pGenerator->f_GetValues(*this, _GenerateState.m_OutputDir);

		{
			CLocalGeneratorInteface LocalInterface{_GenerateState.m_OutputDir};
			auto pOldInterface = fg_Move(mp_GeneratorInterface);
			auto Cleanup = g_OnScopeExit > [&]
				{
					mp_GeneratorInterface = fg_Move(pOldInterface);
				}
			;
			mp_GeneratorInterface = &LocalInterface;

			bool bTryParsed = false;
			try
			{
				mp_FileLocation = CFile::fs_GetExpandedPath(GenerateSettings.m_SourceFile);
				mp_BaseDir = CFile::fs_GetPath(mp_FileLocation);
				mp_FileLocationFile = CFile::fs_GetFile(mp_FileLocation);

				if (!_GenerateState.m_bDisableUserSettings)
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
					Preprocessor.f_ReadFile(GenerateSettings.m_SourceFile);
				}
				if (!_GenerateState.m_bDisableUserSettings)
				{
					mp_SourceFiles[UserSettingsFileNameLocal];
					mp_SourceFiles[UserSettingsFileNameGlobal];
				}

				mp_SourceFiles[_GenerateState.m_EnvironmentStateFile];
				mp_SourceFiles[OverrideEnvironmentFile];

				bTryParsed = true;
				fp_ParseData(mp_Data.m_RootEntity, mp_Registry, &mp_Data.m_ConfigurationTypes);
			}
			catch (CException const &)
			{
				if (!bTryParsed)
					fp_ParseData(mp_Data.m_RootEntity, mp_Registry, &mp_Data.m_ConfigurationTypes);
				if (auto Retry = fp_HandleRepositories(_GenerateState.m_GeneratorValues, _GenerateOptions.m_bSkipUpdate))
					return Retry;
				else
					throw;
			}

			if (auto Retry = fp_HandleRepositories(_GenerateState.m_GeneratorValues, _GenerateOptions.m_bSkipUpdate))
				return Retry;
		}
		return ERetry_None;
	}

	bool CBuildSystem::f_Action_Generate(CGenerateOptions const &_GenerateOptions, ERetry &o_Retry)
	{
		if (_GenerateOptions.m_Settings.m_Action == "Clean")
			return false; // For now we don't support clean

		CGenerateEphemeralState GenerateState;

		o_Retry = CBuildSystem::fp_GeneratePrepare
			(
			 	_GenerateOptions
			 	, GenerateState
			 	, [&]() -> bool
				{
					if (CFile::fs_FileExists(GenerateState.m_WorkspaceGeneratorStateFileName, EFileAttrib_File))
					{
						CGeneratorArchiveState State;
						try
						{
							TCVector<uint8> FileData = CFile::fs_ReadFileTry(GenerateState.m_WorkspaceGeneratorStateFileName);
							CBinaryStreamMemory<> Stream;
							Stream.f_Open(FileData);
							Stream >> State;
						}
						catch (...)
						{
							State = CGeneratorArchiveState();
						}

						if (!State.m_SourceFiles.f_IsEmpty() && mp_GenerateOptions.m_Settings.m_Action == "Build")
						{
							// DMibScopeConOutTimer("Check source files");

							auto OldExeFile = *State.m_ExeFile.f_FindSmallest();

							if (!GenerateState.m_bDependenciesChanged)
							{
								State.m_ExeFile.f_Clear();
								auto &ExeFile = State.m_ExeFile[CFile::fs_MakePathRelative(CStr(CFile::fs_GetProgramPath()), GenerateState.m_OutputDir)];
								ExeFile = OldExeFile;
								ExeFile.f_FileChanged(GenerateState.m_bDependenciesChanged, GenerateState.m_OutputDir);
							}

							if (!GenerateState.m_bDependenciesChanged)
							{
								TCVector<TCFunctionMutable<void ()>> ToProcess;

								TCVector<CGeneratorArchiveState::CProcessedFile *> CurrentInsert;

								auto fAddFiles = [&](auto &&_Files)
									{
										for (auto iFile = _Files.f_GetIterator(); iFile; ++iFile)
										{
											CurrentInsert.f_Insert(&*iFile);
											if (CurrentInsert.f_GetLen() >= 128)
											{
												ToProcess.f_Insert
													(
														[Files = fg_Move(CurrentInsert), &GenerateState]() mutable
														{
															for (auto pFile : Files)
															{
																if (pFile->f_FileChanged(GenerateState.m_bDependenciesChanged, GenerateState.m_OutputDir))
																	break;
															}
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
										[Files = fg_Move(CurrentInsert), &GenerateState]() mutable
										{
											for (auto pFile : Files)
											{
												if (pFile->f_FileChanged(GenerateState.m_bDependenciesChanged, GenerateState.m_OutputDir))
													break;
											}
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
											if (!GenerateState.m_bDependenciesChanged.f_Exchange(true))
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

							if (!GenerateState.m_bDependenciesChanged)
							{
								for (auto iEnv = State.m_Environment.f_GetIterator(); iEnv; ++iEnv)
								{
									//DConOut("{} = {}" DNewLine, iEnv.f_GetKey() << *iEnv);
									CStr Value = f_GetEnvironmentVariable(iEnv.f_GetKey());

									if (Value != *iEnv)
									{
										DConOut("Dependency check: Regenerating build system because env var '{}' changed: '{}' != '{}'" DNewLine, iEnv.f_GetKey() << Value << *iEnv);
										GenerateState.m_bDependenciesChanged = true;
										break;
									}

								}
							}

							EGenerationFlag InterestingGenerationFlags = EGenerationFlag_AbsoluteFilePaths | EGenerationFlag_DisableUserSettings;

							if ((mp_GenerateOptions.m_Settings.m_GenerationFlags & InterestingGenerationFlags) != (State.m_GenerationFlags & InterestingGenerationFlags))
							{
								DConOut
									(
										"Dependency check: Regenerating build system because generation flags changed {} != {}" DNewLine
										, mp_GenerateOptions.m_Settings.m_GenerationFlags << State.m_GenerationFlags
									)
								;
								GenerateState.m_bDependenciesChanged = true;
							}

							if (!GenerateState.m_bDependenciesChanged)
							{
								fp64 Time1 = GenerateState.m_Clock.f_GetTime();
								DConOut("Dependency check: Checked for changes {fe2} s{\n}", Time1);

								return true;
							}
						}
						else
							DConOut("Dependency check: Regenerating build system because there are no source files in state" DNewLine, 0);
					}
					else
						DConOut("Dependency check: Regenerating build system because there is no state file" DNewLine, 0);

					fp64 Time1 = GenerateState.m_Clock.f_GetTime();

					DConOut("Checked for changes {fe2} s{\n}", Time1);
					return false;
				}
			)
		;

		if (o_Retry != ERetry_None)
			return false;

		fp64 Time1 = GenerateState.m_Clock.f_GetTime();

		// Clear out evaluated properties from repositories
		mp_Data.m_RootEntity.m_EvaluatedProperties.f_Clear();

		fp64 Time2 = GenerateState.m_Clock.f_GetTime();
		DConOut("Parsed data {fe2} s{\n}", Time2 - Time1);

		GenerateState.m_pGenerator->f_Generate(*this, mp_Data, GenerateState.m_OutputDir);

		fp64 Time3 = GenerateState.m_Clock.f_GetTime();

		if (!GenerateState.m_bDisableUserSettings)
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
			fSaveFile(mp_UserSettingsLocal.m_Registry, mp_UserSettingsFileLocal);
			fSaveFile(mp_UserSettingsGlobal.m_Registry, mp_UserSettingsFileGlobal);
		}

		if (!GenerateState.m_bUseCachedEnvironment)
			fp_SaveEnvironment();

		{
			CGeneratorArchiveState State;
			auto fl_UpdateFileInfo
				= [&](TCMap<CStr, CGeneratorArchiveState::CProcessedFile> &_Files, CStr const &_FileName) -> CGeneratorArchiveState::CProcessedFile &
				{
					CStr FileName = CFile::fs_MakePathRelative(_FileName, GenerateState.m_OutputDir);
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
				CStr FileName = CFile::fs_MakePathRelative(iFile.f_GetKey(), GenerateState.m_OutputDir);
				auto &File = State.m_GeneratedFiles[FileName];
				File.m_WriteTime = GeneratedWriteTimes[iFile.f_GetKey()];
				File.m_Flags = iFile->m_Flags;
			}

			for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
			{
				CStr FileName = CFile::fs_MakePathRelative(iFile.f_GetKey(), GenerateState.m_OutputDir);
				auto &File = GenerateState.m_GlobalState.m_GeneratedFiles[FileName];
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

			GenerateState.m_GlobalState.m_GenerationFlags = mp_GenerateOptions.m_Settings.m_GenerationFlags;
			State.m_GenerationFlags = mp_GenerateOptions.m_Settings.m_GenerationFlags;

			{
				CBinaryStreamMemory<> Stream;
				Stream << State;

				CFile::fs_CreateDirectory(CFile::fs_GetPath(GenerateState.m_WorkspaceGeneratorStateFileName));
				CFile::fs_CopyFileDiff(Stream.f_MoveVector(), GenerateState.m_WorkspaceGeneratorStateFileName, CTime::fs_NowUTC());
			}
			{
				CBinaryStreamMemory<> Stream;
				Stream << GenerateState.m_GlobalState;

				CFile::fs_CreateDirectory(CFile::fs_GetPath(GenerateState.m_GlobalGeneratorStateFileName));
				CFile::fs_CopyFileDiff(Stream.f_MoveVector(), GenerateState.m_GlobalGeneratorStateFileName, CTime::fs_NowUTC());
			}
		}

		TCVector<CStr> DeletedFiles;
		TCSet<CStr> PotentiallyOrphanedDirectories;

		for (auto iFile = GenerateState.m_BeforeGlobalState.m_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
		{
			if (iFile->m_Flags & EGeneratedFileFlag_KeepGeneratedFile)
				continue;

			if (!GenerateState.m_GlobalState.m_GeneratedFiles.f_FindEqual(iFile.f_GetKey()))
			{
				CStr FileName = CFile::fs_GetExpandedPath(iFile.f_GetKey(), GenerateState.m_OutputDir);

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
		
		fp64 Time4 = GenerateState.m_Clock.f_GetTime();
		DConOut("Saved state {fe2} s{\n}", Time4 - Time3);
		DConOut("Total time {fe2} s{\n}", Time4);
		
		return mp_FileChanged;
	}
}
