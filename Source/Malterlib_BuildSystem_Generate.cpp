// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_GeneratorState.h"
#include "Malterlib_BuildSystem_Preprocessor.h"
#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Core/RuntimeType>
#include <Mib/Cryptography/UUID>
#include <Mib/Encoding/EJson>

namespace NMib::NBuildSystem
{
	constexpr CUniversallyUniqueIdentifier const g_GeneratorStateUUIDNamespace(0x8220886E, 0x63BD, 0x4F4E, 0xB32B, 0x71D68D7346D4_uint64);

	namespace
	{
		struct CLocalGeneratorInteface : public ICGeneratorInterface
		{
			CLocalGeneratorInteface(CStr const &_OutputDir)
				: m_OutputDir(_OutputDir)
			{
			}

			CValuePotentiallyByRef f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const override
			{
				if (_Value == gc_ConstKey_Builtin_GeneratedBuildSystemDir.m_Name)
				{
					if (_pStorePositions)
						_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.GeneratedBuildSystemDir")->f_AddValue(m_OutputDir, false);
					o_bSuccess = true;
					return CEJsonSorted(m_OutputDir);
				}
				return CEJsonSorted();
			}

			CStr f_GetExpandedPath(CStr const &_Path, CStr const &_Base) const override
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
			CEJsonSorted Json;
			for (auto &EnvVar : mp_SaveEnvironment)
				Json[mp_SaveEnvironment.fs_GetKey(EnvVar)] = EnvVar;

			CByteVector FileData;
			CFile::fs_WriteStringToVector(FileData, Json.f_ToString(), false);

			CStr EnvironmentStateFile = mp_OutputDir / "Environment.json";

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


	bool CBuildSystem::f_SingleThreaded() const
	{
		return mp_bSingleThreaded;
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::fp_GeneratePrepare
		(
			CGenerateOptions const &_GenerateOptions
			, CGenerateEphemeralState &_GenerateState
			, TCFunction<TCFuture<bool> ()> &&_fPreParse
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		auto &GenerateSettings = _GenerateOptions.m_Settings;

		mp_GenerateOptions = _GenerateOptions;

		if (_GenerateOptions.m_bReconcileNoOptions)
			f_NoReconcileOptions();

		mp_bApplyRepoPolicy = _GenerateOptions.m_bApplyRepoPolicy;
		mp_bApplyRepoPolicyPretend = _GenerateOptions.m_bApplyRepoPolicyPretend;
		mp_bApplyRepoPolicyCreateMissing = _GenerateOptions.m_bApplyRepoPolicyCreateMissing;
		mp_bUpdateLfsReleaseIndexes = _GenerateOptions.m_bUpdateLfsReleaseIndexes;
		mp_bUpdateLfsReleaseIndexesPretend = _GenerateOptions.m_bUpdateLfsReleaseIndexesPretend;
		mp_bUpdateLfsReleaseIndexesPruneOrphanedAssets = _GenerateOptions.m_bUpdateLfsReleaseIndexesPruneOrphanedAssets;

		mp_bSingleThreaded = (GenerateSettings.m_GenerationFlags & EGenerationFlag_SingleThreaded) != 0;

		_GenerateState.m_pGenerator = fg_CreateRuntimeType<CBuildSystemGenerator>(NMib::NStr::CStr("CBuildSystemGenerator_") + GenerateSettings.m_Generator);

		if (!_GenerateState.m_pGenerator)
			DError(CStr::CFormat("No such generator: {}") << GenerateSettings.m_Generator);

		_GenerateState.m_FileLocation = CFile::fs_GetExpandedPath(GenerateSettings.m_SourceFile);

		if (GenerateSettings.m_OutputDir.f_IsEmpty())
			_GenerateState.m_OutputDir = "{}/BuildSystem/Default"_f << CFile::fs_GetPath(_GenerateState.m_FileLocation);
		else
			_GenerateState.m_OutputDir = CFile::fs_GetExpandedPath(GenerateSettings.m_OutputDir);

		_GenerateState.m_RelativeFileLocation = CFile::fs_MakePathRelative(_GenerateState.m_FileLocation, _GenerateState.m_OutputDir);

		NRepository::CColors Colors(f_AnsiFlags());

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

			_GenerateState.m_GlobalGeneratorStateFileName = _GenerateState.m_OutputDir / "GeneratorStates" / (UniqueIdentifier.f_GetAsString() + ".MGeneratorState");
			mp_GeneratorStateFileName = _GenerateState.m_OutputDir / "GeneratorStates" / (UniqueIdentifier.f_GetAsString() + ".TouchOnly");
		}
		{
			CUniversallyUniqueIdentifier UniqueIdentifier
				(
					EUniversallyUniqueIdentifierGenerate_StringHash
					, g_GeneratorStateUUIDNamespace
					, _GenerateState.m_RelativeFileLocation + GenerateSettings.m_Generator + mp_GenerateWorkspace
				)
			;

			_GenerateState.m_WorkspaceGeneratorStateFileName = _GenerateState.m_OutputDir / "GeneratorStates" / (UniqueIdentifier.f_GetAsString() + ".MGeneratorState");
		}

		_GenerateState.m_EnvironmentStateFile = _GenerateState.m_OutputDir / "Environment.json";

		_GenerateState.m_bUseCachedEnvironment = (GenerateSettings.m_GenerationFlags & EGenerationFlag_UseCachedEnvironment) != EGenerationFlag_None;

		if (_GenerateState.m_bUseCachedEnvironment)
		{
			if (!CFile::fs_FileExists(_GenerateState.m_EnvironmentStateFile, EFileAttrib_File))
			{
				f_OutputConsole("Cached environment was not found at: {}. Saving current environment.\n"_f << _GenerateState.m_EnvironmentStateFile, true);
				mp_Environment = mp_SaveEnvironment = GenerateSettings.m_Environment;
				fp_SaveEnvironment();
			}

			mp_SaveEnvironment.f_Clear();
			mp_Environment.f_Clear();
			try
			{
				auto EnvironmentJson = CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(_GenerateState.m_EnvironmentStateFile, true), _GenerateState.m_EnvironmentStateFile);
				for (auto &EnvVar : EnvironmentJson.f_Object())
					mp_Environment[EnvVar.f_Name()] = EnvVar.f_Value().f_String();
			}
			catch (NException::CException const &_Exception)
			{
				DMibError("Failed to parse Environment.json: {}"_f << _Exception);
			}
			mp_SaveEnvironment = mp_Environment;
		}
		else
			mp_SaveEnvironment = mp_Environment = GenerateSettings.m_Environment;

		CStr OverrideEnvironmentFile = _GenerateState.m_OutputDir / "OverrideEnvironment.json";
		{
			if (CFile::fs_FileExists(OverrideEnvironmentFile, EFileAttrib_File))
			{
				try
				{
					auto EnvironmentJson = CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(OverrideEnvironmentFile, true), OverrideEnvironmentFile);
					for (auto &EnvVar : EnvironmentJson.f_Object())
						mp_Environment[EnvVar.f_Name()] = EnvVar.f_Value().f_String();
				}
				catch (NException::CException const &_Exception)
				{
					DMibError("Failed to parse Environment.json: {}"_f << _Exception);
				}
			}
		}

		if (_fPreParse && co_await _fPreParse())
			co_return ERetry_None;

		if (CFile::fs_FileExists(_GenerateState.m_GlobalGeneratorStateFileName, EFileAttrib_File))
		{
			try
			{
				CByteVector FileData = CFile::fs_ReadFileTry(_GenerateState.m_GlobalGeneratorStateFileName);
				CBinaryStreamMemory<> Stream;
				Stream.f_Open(FileData);
				Stream >> _GenerateState.m_GlobalState;

				if (_GenerateState.m_GlobalState.m_GeneratedFiles.f_IsEmpty())
				{
					if (mp_GenerateWorkspace)
					{
						DMibConErrOut2("Generating all workspaces: Empty generated files\n");
						mp_GenerateWorkspace.f_Clear();
					}
				}
			}
			catch (CException const &_Exception)
			{
				if (mp_GenerateWorkspace)
				{
					DMibConErrOut2("Generating all workspaces: Exception reading state: {}\n", _Exception);
					mp_GenerateWorkspace.f_Clear();
				}
				_GenerateState.m_GlobalState = CGeneratorArchiveState();
			}
			catch (...)
			{
				if (mp_GenerateWorkspace)
				{
					DMibConErrOut2("Generating all workspaces: Generic exception reading state\n");
					mp_GenerateWorkspace.f_Clear();
				}
				_GenerateState.m_GlobalState = CGeneratorArchiveState();
			}
		}
		else
		{
			if (mp_GenerateWorkspace)
			{
				DMibConErrOut2("Generating all workspaces: No global state found at '{}'\n", _GenerateState.m_GlobalGeneratorStateFileName);
				mp_GenerateWorkspace.f_Clear(); // If we don't have global state we need to create the whole thing to get the correct generated files
			}
		}

		_GenerateState.m_BeforeGlobalState = _GenerateState.m_GlobalState;

		if (mp_GenerateWorkspace.f_IsEmpty())
			_GenerateState.m_GlobalState.m_GeneratedFiles.f_Clear();
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
		CStr UserSettingsFileNameGlobal = CFile::fs_GetUserHomeDirectory() / "UserSettingGlobalV2.MSettings";
		mp_UserSettingsFileLocal = UserSettingsFileNameLocal;
		mp_UserSettingsFileGlobal = UserSettingsFileNameGlobal;
		mp_FileLocation = CFile::fs_GetExpandedPath(GenerateSettings.m_SourceFile);
		mp_BaseDir = CFile::fs_GetPath(mp_FileLocation.f_String());
		mp_FileLocationFile = CFile::fs_GetFile(mp_FileLocation.f_String());
		{
			CStr Ret = CFile::fs_GetProgramDirectory() / "MTool";
			#ifdef DPlatformFamily_Windows
				Ret += ".exe";
			#endif
			mp_MToolExe = Ret;
		}
		{
			CUniversallyUniqueIdentifier UUIDNamespace("{EF53758B-02E4-4DE4-88CC-43513C7F6E2E}");

			CStr CmakeProgramPath = CFile::fs_GetProgramDirectory() / "MToolCMake";

			#ifdef DPlatformFamily_Windows
				CmakeProgramPath += ".exe";
			#endif

			CUniversallyUniqueIdentifier UUID{EUniversallyUniqueIdentifierGenerate_StringHash, UUIDNamespace, CmakeProgramPath.f_LowerCase()};

			CStr CacheDirectory = CFile::fs_GetPath(CFile::fs_GetUserLocalProgramCacheDirectory()) / "MToolCMake";
			CStr CmakeRoot = fg_Format("{}/{}/CMakeRoot", CacheDirectory, UUID.f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum));

			mp_CMakeRoot = CmakeRoot;
		}
		{
			CStr Ret = CFile::fs_GetProgramDirectory() / "mib";
			#ifdef DPlatformFamily_Windows
				Ret += ".exe";
			#endif

			mp_MalterlibExe = Ret;
		}

		_GenerateState.m_GeneratorValues = _GenerateState.m_pGenerator->f_GetValues(*this, _GenerateState.m_OutputDir);
				
		TCMap<CStr, CStr> PreprocessorEnvironment = mp_Environment;
		for (auto &Value : _GenerateState.m_GeneratorValues)
		{
			auto &Key = _GenerateState.m_GeneratorValues.fs_GetKey(Value);
			if (Key.f_GetType() != EPropertyType_Property)
				continue;

			switch (Value.f_EType())
			{
			case EEJsonType_Null:
			case EEJsonType_String:
			case EEJsonType_Integer:
			case EEJsonType_Float:
			case EEJsonType_Boolean:
			case EEJsonType_Object:
			case EEJsonType_Array:
				PreprocessorEnvironment(Key.m_Name, Value.f_AsString());
			default:
				continue;
			}			
		}

		{
			_GenerateState.m_pLocalGeneratorInterface = fg_Construct<CLocalGeneratorInteface>(_GenerateState.m_OutputDir);

			_GenerateState.m_LocalGeneratorInterfaceCleanup = g_OnScopeExitShared / [this, pOldInterface = fg_Move(mp_GeneratorInterface)]() mutable
				{
					mp_GeneratorInterface = fg_Move(pOldInterface);
				}
			;
			mp_GeneratorInterface = _GenerateState.m_pLocalGeneratorInterface.f_Get();

			bool bTryParsed = false;

			CExceptionPointer pFistError;
			try
			{
				if (!_GenerateState.m_bDisableUserSettings)
				{
					if (CFile::fs_FileExists(UserSettingsFileNameGlobal))
					{
						CBuildSystemPreprocessor Preprocessor(mp_UserSettingsGlobal.m_Registry, mp_SourceFiles, mp_FindCache, PreprocessorEnvironment, mp_StringCache);
						Preprocessor.f_ReadFile(UserSettingsFileNameGlobal);
						mp_Registry = mp_UserSettingsGlobal.m_Registry;
					}
					if (CFile::fs_FileExists(UserSettingsFileNameLocal))
					{
						// Once to get only local registry
						{
							CBuildSystemPreprocessor Preprocessor(mp_UserSettingsLocal.m_Registry, mp_SourceFiles, mp_FindCache, PreprocessorEnvironment, mp_StringCache);
							Preprocessor.f_ReadFile(UserSettingsFileNameLocal);
						}
						// Once to merge with global settings
						{
							CBuildSystemPreprocessor Preprocessor(mp_Registry, mp_SourceFiles, mp_FindCache, PreprocessorEnvironment, mp_StringCache);
							Preprocessor.f_ReadFile(UserSettingsFileNameLocal);
						}
					}
				}
				{
					CBuildSystemPreprocessor Preprocessor(mp_Registry, mp_SourceFiles, mp_FindCache, PreprocessorEnvironment, mp_StringCache);
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
			catch ([[maybe_unused]] CException const &)
			{
				pFistError = NException::fg_CurrentException();
			}

			if (pFistError)
			{
				auto Result = co_await fg_CallSafe
					(
						[&]() -> TCUnsafeFuture<ERetry>
						{
							co_await ECoroutineFlag_CaptureMalterlibExceptions;
							if (!bTryParsed)
							{
								try
								{
									fp_ParseData(mp_Data.m_RootEntity, mp_Registry, &mp_Data.m_ConfigurationTypes);
								}
								catch ([[maybe_unused]] CException const &_Exception2)
								{
									f_OutputConsole
										(
											"{}Error trying to parse data to handle repositories (will try anyway):{}\n{}\n\n"_f
											<< Colors.f_StatusWarning()
											<< Colors.f_Default()
											<< _Exception2.f_GetErrorStr().f_Indent(DMibPFileLineFormatIndent)
											, true
										)
									;
								}
							}

							if (auto Retry = co_await fp_HandleRepositories(_GenerateState.m_GeneratorValues))
								co_return Retry;
							else
								co_return pFistError;
						}
					)
					.f_Wrap()
				;
				if (!Result)
				{
					f_OutputConsole
						(
							"{}Error trying to handle repositories as fallback:{}\n{}\n\n"_f
							<< Colors.f_StatusWarning()
							<< Colors.f_Default()
							<< Result.f_GetExceptionStr().f_Indent(DMibPFileLineFormatIndent)
							, true
						)
					;
					co_return pFistError;
				}
				else
					co_return *Result;
			}

			if (auto Retry = co_await fp_HandleRepositories(_GenerateState.m_GeneratorValues))
				co_return Retry;
		}

		co_return ERetry_None;
	}

	TCUnsafeFuture<bool> CBuildSystem::f_Action_Generate(CGenerateOptions const &_GenerateOptions, ERetry &o_Retry)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		if (_GenerateOptions.m_Settings.m_Action == "Clean")
			co_return false; // For now we don't support clean

		CGenerateEphemeralState GenerateState;

		NContainer::TCMap<NStr::CStr, uint32> NumWorkspaceTargets;

		o_Retry = co_await CBuildSystem::fp_GeneratePrepare
			(
				_GenerateOptions
				, GenerateState
				, [&]() -> TCUnsafeFuture<bool>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					co_await f_CheckCancelled();

					if (CFile::fs_FileExists(GenerateState.m_WorkspaceGeneratorStateFileName, EFileAttrib_File))
					{
						CGeneratorArchiveState State;
						try
						{
							CByteVector FileData = CFile::fs_ReadFileTry(GenerateState.m_WorkspaceGeneratorStateFileName);
							CBinaryStreamMemory<> Stream;
							Stream.f_Open(FileData);
							Stream >> State;
							NumWorkspaceTargets = State.m_NumWorkspaceTargets;
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
								ExeFile.f_FileChanged(GenerateState.m_bDependenciesChanged, GenerateState.m_OutputDir, *this);
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
														[this, Files = fg_Move(CurrentInsert), &GenerateState]() mutable
														{
															for (auto pFile : Files)
															{
																if (pFile->f_FileChanged(GenerateState.m_bDependenciesChanged, GenerateState.m_OutputDir, *this))
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
										[this, Files = fg_Move(CurrentInsert), &GenerateState]() mutable
										{
											for (auto pFile : Files)
											{
												if (pFile->f_FileChanged(GenerateState.m_bDependenciesChanged, GenerateState.m_OutputDir, *this))
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
												f_OutputConsole("Dependency check: Regenerating build system because search changed: {}{\n}"_f << FindOptions.m_Path);
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

								co_await fg_ParallelForEach
									(
										ToProcess
										, [&](auto &&_fToProcess) -> TCUnsafeFuture<void>
										{
											co_await ECoroutineFlag_CaptureMalterlibExceptions;

											_fToProcess();

											co_return {};
										}
										, mp_bSingleThreaded
									)
								;
							}

							if (!GenerateState.m_bDependenciesChanged)
							{
								for (auto iEnv = State.m_Environment.f_GetIterator(); iEnv; ++iEnv)
								{
									//f_OutputConsole("{} = {}{\n}"_f << iEnv.f_GetKey() << *iEnv);
									CStr Value = f_GetEnvironmentVariable(iEnv.f_GetKey());

									if (Value != *iEnv)
									{
										f_OutputConsole
											(
												"Dependency check: Regenerating build system because env var '{}' changed: '{}' != '{}'{\n}"_f
												<< iEnv.f_GetKey()
												<< Value
												<< *iEnv
											)
										;
										GenerateState.m_bDependenciesChanged = true;
										break;
									}

								}
							}

							EGenerationFlag InterestingGenerationFlags = EGenerationFlag_AbsoluteFilePaths | EGenerationFlag_DisableUserSettings;

							if ((mp_GenerateOptions.m_Settings.m_GenerationFlags & InterestingGenerationFlags) != (State.m_GenerationFlags & InterestingGenerationFlags))
							{
								f_OutputConsole
									(
										"Dependency check: Regenerating build system because generation flags changed {} != {}{\n}"_f
										<< mp_GenerateOptions.m_Settings.m_GenerationFlags
										<< State.m_GenerationFlags
									)
								;
								GenerateState.m_bDependenciesChanged = true;
							}

							if (!GenerateState.m_bDependenciesChanged)
							{
								fp64 Time1 = GenerateState.m_Clock.f_GetTime();
								f_OutputConsole("No changes found {fe2} s{\n}"_f << Time1);

								co_return true;
							}
						}
						else
							f_OutputConsole("Dependency check: Regenerating build system because there are no source files in state{\n}"_f);
					}
					else
						f_OutputConsole("Dependency check: Regenerating build system because there is no state file{\n}"_f);

					fp64 Time1 = GenerateState.m_Clock.f_GetTime();

					GenerateState.m_bDependenciesChanged = true;
					f_OutputConsole("Checked for changes {fe2} s{\n}"_f << Time1);
					co_return false;
				}
			)
		;

		if (o_Retry != ERetry_None)
			co_return false;

		if (!GenerateState.m_bDependenciesChanged)
		{
			if (!CFile::fs_FileExists(mp_GeneratorStateFileName.f_String()))
				CFile::fs_Touch(mp_GeneratorStateFileName.f_String());

			co_return false;
		}

		co_await f_CheckCancelled();

		GenerateState.m_LocalGeneratorInterfaceCleanup.f_Clear();

		fp64 Time1 = GenerateState.m_Clock.f_GetTime();

		// Clear out evaluated properties from repositories
		mp_Data.m_RootEntity.m_EvaluatedProperties.m_Properties.f_Clear();

		fp64 Time2 = GenerateState.m_Clock.f_GetTime();
		f_OutputConsole("Parsed data {fe2} s{\n}"_f << (Time2 - Time1));

		co_await GenerateState.m_pGenerator->f_Generate(this, &mp_Data, GenerateState.m_OutputDir, NumWorkspaceTargets);

		fp64 Time3 = GenerateState.m_Clock.f_GetTime();

		if (!GenerateState.m_bDisableUserSettings)
		{
			auto fSaveFile = [&](auto &_Registry, auto &_FileName)
				{
					auto StringData = _Registry.f_GenerateStr();

					CByteVector FileData;
					CFile::fs_WriteStringToVector(FileData, StringData, false);

					CFile::fs_CreateDirectory(CFile::fs_GetPath(_FileName));
					CFile::fs_CopyFileDiff(FileData, _FileName, CTime::fs_NowUTC());
				}
			;

			if (mp_UserSettingsLocal.m_bChanged)
				fSaveFile(mp_UserSettingsLocal.m_Registry, mp_UserSettingsFileLocal);

			if (mp_UserSettingsGlobal.m_bChanged)
				fSaveFile(mp_UserSettingsGlobal.m_Registry, mp_UserSettingsFileGlobal);
		}

		if (!GenerateState.m_bUseCachedEnvironment)
			fp_SaveEnvironment();

		{
			CGeneratorArchiveState State;
			auto fUpdateFileInfo = [&]
				(
					TCMap<CStr, CGeneratorArchiveState::CProcessedFile> &_Files
					, CStr const &_FileName
					, TCSharedPointer<CHashDigest_SHA256> const &_pDigest
				) -> CGeneratorArchiveState::CProcessedFile &
				{
					CStr FileName = CFile::fs_MakePathRelative(_FileName, GenerateState.m_OutputDir);
					auto &ProcessedFile = _Files[FileName];
					try
					{
						ProcessedFile.m_WriteTime = CFile::fs_GetWriteTime(_FileName);
						if (_pDigest)
						{
							ProcessedFile.m_Flags |= EGeneratedFileFlag_ByDigest;
							ProcessedFile.m_pDigest = _pDigest;
						}
					}
					catch (CExceptionFile const &)
					{
					}
					return ProcessedFile;
				}
			;

			fUpdateFileInfo(State.m_ExeFile, CFile::fs_GetProgramPath(), nullptr);

			mint nSourceFiles = 0;
			for (auto &File : mp_SourceFiles.f_Entries())
			{
				++nSourceFiles;
				fUpdateFileInfo(State.m_SourceFiles, File.f_Key(), File.f_Value());
			}

			mint nReferencedFiles = 0;
			{
				auto SourceFiles = mp_FindCache.f_GetSourceFiles();
				for (auto &File : SourceFiles.f_Entries())
				{
					++nReferencedFiles;
					fUpdateFileInfo(State.m_ReferencedFiles, File.f_Key(), File.f_Value()).m_Flags |= EGeneratedFileFlag_NoDateCheck;
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
				TCFutureVector<void> Results;
				TCVector<CToEdit> CurrentInsert;
				TCVector<TCVector<CToEdit>> ToProcess;

				for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
				{
					++nGeneratedFiles;
					auto &ToEdit = CurrentInsert.f_Insert();
					ToEdit.m_pPath = &iFile.f_GetKey();
					ToEdit.m_pWriteTime = &GeneratedWriteTimes[iFile.f_GetKey()];

					if (CurrentInsert.f_GetLen() >= 128)
						ToProcess.f_Insert(fg_Move(CurrentInsert));
				}

				if (!CurrentInsert.f_IsEmpty())
					ToProcess.f_Insert(fg_Move(CurrentInsert));

				co_await fg_ParallelForEach
					(
						ToProcess
						, [&](auto &o_Files) -> TCUnsafeFuture<void>
						{
							for (auto &File : o_Files)
							{
								try
								{
									*File.m_pWriteTime = CFile::fs_GetWriteTime(*File.m_pPath);
								}
								catch (CExceptionFile const &)
								{
								}
							}

							co_return {};
						}
						, mp_bSingleThreaded
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
			f_OutputConsole("{} source files{\n}"_f << nSourceFiles);
			f_OutputConsole("{} file searches{\n}"_f << nFileSearches);
			f_OutputConsole("{} referenced files{\n}"_f << nReferencedFiles);
			f_OutputConsole("{} generated files{\n}"_f << nGeneratedFiles);
			f_OutputConsole("{} executable launches{\n}"_f << mp_nExecuteLaunches.f_Load());

			for (auto iEnv = mp_UsedExternals.f_GetIterator(); iEnv; ++iEnv)
			{
				CStr Value = f_GetEnvironmentVariable(iEnv->m_Name);
				State.m_Environment[iEnv->m_Name] = Value;
			}

			GenerateState.m_GlobalState.m_GenerationFlags = mp_GenerateOptions.m_Settings.m_GenerationFlags;

			State.m_GenerationFlags = mp_GenerateOptions.m_Settings.m_GenerationFlags;
			State.m_NumWorkspaceTargets = NumWorkspaceTargets;

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
			CFile::fs_Touch(mp_GeneratorStateFileName.f_String());
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
					f_OutputConsole("Error deleting file: {}{\n}"_f << _Exception.f_GetErrorStr());
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
					f_OutputConsole("Error deleting directory: {}{\n}"_f << _Exception.f_GetErrorStr());
				}
			}
			PotentiallyOrphanedDirectories = fg_Move(NewPotentiallyOrphanedDirectories);
		}

		if (DeletedFiles.f_GetLen() <= 32)
		{
			for (auto &File : DeletedFiles)
				f_OutputConsole("Deleted unused file: {}{\n}"_f << File);
		}
		else
			f_OutputConsole("Deleted {} unused files{\n}"_f << DeletedFiles.f_GetLen());

		fp64 Time4 = GenerateState.m_Clock.f_GetTime();
		f_OutputConsole("Saved state {fe2} s{\n}"_f << (Time4 - Time3));
		f_OutputConsole("Total time {fe2} s{\n}"_f << Time4);

		co_return mp_FileChanged;
	}
}
