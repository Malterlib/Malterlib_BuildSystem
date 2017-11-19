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
	
	bool CBuildSystem::f_Generate(CGenerateSettings const &_GenerateSettings, bool &o_bRetry)
	{
		o_bRetry = false;
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
				DMibError("Cached environment not found at: {}"_f << EnvironmentStateFile);

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
		}
		else
			mp_Environment = fg_GetSys()->f_Environment();

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

					auto fl_FileChanged
						= [&bChanged, &OutputDir](CGeneratorState::CProcessedFile const &_File) -> bool
						{
							CStr FileName = CFile::fs_GetExpandedPath(_File.f_GetFileName(), OutputDir);

							try
							{
								if (_File.m_bNoDateCheck)
								{
									if (!CFile::fs_FileExists(FileName))
									{
										if (!bChanged.f_Exchange(true))
											DConOut("Regenerating build system because file is missing: {}{\n}", FileName);
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
													"Regenerating build system because file changed ({} != {}): {}{\n}"
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
								if (!bChanged.f_Exchange(true))
									DConOut("Regenerating build system because file is missing: {}{\n}", FileName);
								return true;
							}
							return false;
						}
					;

					auto OldExeFile = *State.m_ExeFile.f_FindSmallest();

					if (!bChanged)
					{
						//CTimerConOutScope Scope("Source files");
						State.m_ExeFile.f_Clear();
						auto &ExeFile = State.m_ExeFile[CFile::fs_MakePathRelative(CStr(CFile::fs_GetProgramPath()), OutputDir)];
						ExeFile = OldExeFile;
						if (fl_FileChanged(ExeFile))
							bChanged = true;
						for (auto iFile = State.m_SourceFiles.f_GetIterator(); iFile && !bChanged; ++iFile)
						{
							if (fl_FileChanged(*iFile))
							{
								bChanged = true;
								break;
							}
						}
					}

					if (!bChanged)
					{
						//CTimerConOutScope Scope("Source files");
						for (auto iFile = State.m_ReferencedFiles.f_GetIterator(); iFile && !bChanged; ++iFile)
						{
							if (fl_FileChanged(*iFile))
							{
								bChanged = true;
								break;
							}
						}
					}

					if (!bChanged)
					{
						//CTimerConOutScope Scope("Generated files");
						TCVector<TCVector<CGeneratorState::CProcessedFile *>> ToProcess;
						auto *pCurrentInsert = &ToProcess.f_Insert();
						mint nFiles = 0;
						for (auto iFile = State.m_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
						{
							++nFiles;
							pCurrentInsert->f_Insert(&*iFile);
							if (pCurrentInsert->f_GetLen() >= 1024)
								pCurrentInsert = &ToProcess.f_Insert();
						}
						fg_ParallellForEach
							(
								ToProcess
								, [&](TCVector<CGeneratorState::CProcessedFile *> const &_Files)
								{
									for (auto pFile : _Files)
										fl_FileChanged(*pFile);
								}
							)
						;
						/*
						for (auto iFile = State.m_GeneratedFiles.f_GetIterator(); iFile && !bChanged; ++iFile)
						{
							if (fl_FileChanged(*iFile))
							{
								bChanged = true;
							}
						}*/
					}

					if (!bChanged)
					{
						//CTimerConOutScope Scope("Source searches");
						for (auto iSearch = State.m_SourceSearches.f_GetIterator(); iSearch; ++iSearch)
						{
							auto &Search = *iSearch;
							auto &FindOptions = iSearch.f_GetKey();
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
								DConOut("Regenerating build system because search changed: {}" DNewLine, FindOptions.m_Path);
								bChanged = true;
								break;
							}
						}
					}

					if (!bChanged)
					{
						for (auto iEnv = State.m_Environment.f_GetIterator(); iEnv; ++iEnv)
						{
							//DConOut("{} = {}" DNewLine, iEnv.f_GetKey() << *iEnv);
							CStr Value = f_GetEnvironmentVariable(iEnv.f_GetKey());

							if (Value != *iEnv)
							{
								DConOut("Regenerating build system because env var '{}' changed: '{}' != '{}'" DNewLine, iEnv.f_GetKey() << Value << *iEnv);
								bChanged = true;
								break;
							}

						}
					}

					if (uint32(mp_GenerateSettings.m_GenerationFlags) != State.m_GenerationFlags)
					{
						DConOut("Regenerating build system because generation flags changed {} != {}" DNewLine, mp_GenerateSettings.m_GenerationFlags << State.m_GenerationFlags);
						bChanged = true;
					}

					if (!bChanged)
					{
						fp64 Time1 = Clock.f_GetTime();
						DConOut("Checked for changes {fe2} s{\n}", Time1);

						return false;
					}
				}
				else
					DConOut("Regenerating build system because there are no source files in state" DNewLine, 0);
			}
			else
				DConOut("Regenerating build system because there is no state file" DNewLine, 0);

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

		CStr UserSettingsFileName = CStr::CFormat("{}/UserSettings.MSettings") << OutputDir;
		mp_UserSettingsFile = UserSettingsFileName;

		auto GeneratorValues = pGenerator->f_GetValues(*this, OutputDir);

		bool bTryParsed = false;
		try
		{
			if (!bDisableUserSettings && CFile::fs_FileExists(UserSettingsFileName))
			{
				CBuildSystemPreprocessor Preprocessor(mp_UserSettingsRegistry, mp_SourceFiles, mp_FindCache);
				Preprocessor.f_ReadFile(UserSettingsFileName);
				mp_Registry = mp_UserSettingsRegistry;
			}

			{
				CBuildSystemPreprocessor Preprocessor(mp_Registry, mp_SourceFiles, mp_FindCache);
				Preprocessor.f_ReadFile(_GenerateSettings.m_SourceFile);
				mp_FileLocation = Preprocessor.f_GetFileLocation();
			}
			if (!bDisableUserSettings)
				mp_SourceFiles[UserSettingsFileName];

			mp_SourceFiles[EnvironmentStateFile];

			mp_BaseDir = CFile::fs_GetPath(mp_FileLocation);
			mp_FileLocationFile = CFile::fs_GetFile(mp_FileLocation);

			bTryParsed = true;
			fp_ParseData(mp_Data.m_RootEntity, mp_Registry, &mp_Data.m_ConfigurationTypes);
		}
		catch (CException const &)
		{
			if (!bTryParsed)
				fp_ParseData(mp_Data.m_RootEntity, mp_Registry, &mp_Data.m_ConfigurationTypes);
			if (fp_HandleRepositories(GeneratorValues))
			{
				o_bRetry = true;
				return false;
			}
			else
				throw;
		}

		if (fp_HandleRepositories(GeneratorValues))
		{
			o_bRetry = true;
			return false;
		}

		if (!bBuildAction)
		{
			fp_HandleAction(_GenerateSettings.m_Action, _GenerateSettings.m_ActionParams);
			return false;
		}

		// Clear out evaluated properties from repositories
		mp_Data.m_RootEntity.m_EvaluatedProperties.f_Clear();

		fp64 Time2 = Clock.f_GetTime();
		DConOut("Parsed data {fe2} s{\n}", Time2 - Time1);

		pGenerator->f_Generate(*this, mp_Data, OutputDir);

		fp64 Time3 = Clock.f_GetTime();

		if (!bDisableUserSettings)
		{
			auto StringData = mp_UserSettingsRegistry.f_GenerateStr();

			TCVector<uint8> FileData;
			CFile::fs_WriteStringToVector(FileData, StringData);

			CFile::fs_CreateDirectory(CFile::fs_GetPath(UserSettingsFileName));
			CFile::fs_CopyFileDiff(FileData, UserSettingsFileName, CTime::fs_NowUTC());
		}

		if (!bUseCachedEnvironment)
		{
			try
			{
				CEJSON JSON;
				for (auto &EnvVar : mp_Environment)
					JSON[mp_Environment.fs_GetKey(EnvVar)] = EnvVar;

				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, JSON.f_ToString());

				CFile::fs_CreateDirectory(CFile::fs_GetPath(EnvironmentStateFile));
				CFile::fs_CopyFileDiff(FileData, EnvironmentStateFile, CTime::fs_NowUTC());
			}
			catch (NException::CException const &_Exception)
			{
				DMibError("Failed to save Environment.json: {}"_f << _Exception);
			}
		}

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
					fl_UpdateFileInfo(State.m_ReferencedFiles, *iFile).m_bNoDateCheck = true;
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
				File.m_bNoDateCheck = iFile->m_bNoDateCheck;
			}

			for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
			{
				CStr FileName = CFile::fs_MakePathRelative(iFile.f_GetKey(), OutputDir);
				auto &File = GlobalState.m_GeneratedFiles[FileName];
				File.m_WriteTime = GeneratedWriteTimes[iFile.f_GetKey()];
				File.m_Workspaces += iFile->m_Workspaces;
				File.m_bNoDateCheck = iFile->m_bNoDateCheck;
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
			if (!GlobalState.m_GeneratedFiles.f_FindEqual(iFile.f_GetKey()))
			{
				CStr FileName = CFile::fs_GetExpandedPath(iFile.f_GetKey(), OutputDir);

				try
				{
					if (CFile::fs_FileExists(FileName))
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
