// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem
{
	namespace NXcode
	{
		void CGeneratorInstance::fp_GenerateBuildConfigurationFiles(CProjectState &_ProjectState, CProject& _Project, CStr const &_OutputDir) const
		{
			CStr OutputDir = CFile::fs_AppendPath(_OutputDir, _Project.f_GetName());

			_ProjectState.f_CreateDirectory(OutputDir);

			CStr OutputFileDirectory = CFile::fs_AppendPath(OutputDir, CStr("Configurations"));
			_ProjectState.f_CreateDirectory(OutputFileDirectory);

			for (auto &NativeTargets : _Project.m_NativeTargets)
			{
				auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);

				for (auto &NativeTarget : NativeTargets)
				{
					CStr OutputFile;

					if (NativeTarget.m_bDefaultTarget)
						OutputFile = CFile::fs_AppendPath(OutputFileDirectory, "{} {}.xcconfig"_f << Configuration.m_Platform << Configuration.m_Configuration);
					else
						OutputFile = CFile::fs_AppendPath(OutputFileDirectory, "{} {} {}.xcconfig"_f << Configuration.m_Platform << Configuration.m_Configuration << NativeTarget.m_CType);

					fp_GenerateBuildConfigurationFile(_ProjectState, _Project, Configuration, OutputFile, _OutputDir, NativeTarget);

					CBuildConfiguration& BuildConfig = NativeTarget.m_BuildConfiguration;
					BuildConfig.m_ConfigName = "{} {}"_f << Configuration.m_Platform << Configuration.m_Configuration;
					BuildConfig.m_ConfigFileName = CFile::fs_GetFile(OutputFile);
					BuildConfig.m_Path = OutputFile;
					BuildConfig.m_bProject = true;
				}
			}
		}

		void CGeneratorInstance::fp_GenerateBuildConfigurationFilesList
			(
				CProjectState &_ProjectState
				, CProject& _Project
				, CStr const &_OutputDir
				, TCVector<CBuildConfiguration>& _ConfigList
			) const
		{
			CStr OutputDir = CFile::fs_AppendPath(_OutputDir, _Project.f_GetName());

			_ProjectState.f_CreateDirectory(OutputDir);

			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				CConfiguration const& Configuration = iConfig.f_GetKey();

				CStr OutputFile = CFile::fs_AppendPath(OutputDir, CStr("Configurations"));
				_ProjectState.f_CreateDirectory(OutputFile);

				OutputFile = CFile::fs_AppendPath(OutputFile, (CStr::CFormat("{} {}.xcconfig") << Configuration.m_Platform << Configuration.m_Configuration).f_GetStr());

				CBuildConfiguration& BuildConfig = _ConfigList.f_Insert();
				BuildConfig.m_ConfigName = "{} {}"_f << Configuration.m_Platform << Configuration.m_Configuration;
				BuildConfig.m_ConfigFileName = CFile::fs_GetFile(OutputFile);
				BuildConfig.m_Path = OutputFile;
				BuildConfig.m_bProject = false;
			}
		}

		TCUnsafeFuture<void> CGeneratorInstance::fp_EvaluateTargetSettings(CProjectState &_ProjectState, CProject& _Project) const
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			for (auto &Entity : _Project.m_EnabledProjectConfigs)
			{
				auto &Config = _Project.m_EnabledProjectConfigs.fs_GetKey(Entity);
				_ProjectState.m_EvaluatedTargetSettings[Config];
			}

			co_await fg_ParallelForEach
				(
					_ProjectState.m_EvaluatedTargetSettings
					, [&](CConfigResultTarget &o_Result) -> TCUnsafeFuture<void>
					{
						co_await ECoroutineFlag_CaptureMalterlibExceptions;
						co_await m_BuildSystem.f_CheckCancelled();

						auto &Config = TCMap<CConfiguration, CConfigResultTarget>::fs_GetKey(o_Result);
						auto &Entity = _Project.m_EnabledProjectConfigs[Config];

						fp_SetEvaluatedValuesTarget(Entity, o_Result, _Project.m_EntityName);

						co_return {};
					}
					, m_BuildSystem.f_SingleThreaded()
				)
			;
			co_await m_BuildSystem.f_CheckCancelled();
			co_await g_Yield;

			for (auto &TargetSetting : _ProjectState.m_EvaluatedTargetSettings)
			{
				auto &Configuration = _ProjectState.m_EvaluatedTargetSettings.fs_GetKey(TargetSetting);
				auto &NativeTarget = _Project.f_GetDefaultNativeTarget(Configuration);

				NativeTarget.m_ProductType = TargetSetting.m_ProductType.m_Value;
				NativeTarget.m_Type = TargetSetting.m_TargetType.m_Value;
			}

			for (auto &NativeTargets : _Project.m_NativeTargets)
			{
				co_await g_Yield;

				auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);
				auto &NativeTarget = _Project.f_GetDefaultNativeTarget(Configuration);

				auto *pEnabledConfig = _Project.m_EnabledProjectConfigs.f_FindEqual(Configuration);
				DCheck(pEnabledConfig);

				TCMap<CConfiguration, CEntityMutablePointer> EnabledConfigs;
				EnabledConfigs[Configuration] = *pEnabledConfig;

				NativeTarget.m_ScriptExport = fp_GetSingleConfigValue(EnabledConfigs, gc_ConstKey_Target_ExportScriptEnvironmentContents, EEJSONType_String, false)
					.m_Value.f_MoveString()
				;
				NativeTarget.m_bGenerateScheme = fp_GetSingleConfigValue(EnabledConfigs, gc_ConstKey_Target_GenerateScheme, EEJSONType_Boolean, false).m_Value.f_Get().f_Boolean();

				// Build scripts
				{
					bool bFoundPostBuildScript = false;
					CBuildScript PostBuildScript;
					PostBuildScript.m_bPostBuild = true;

					bool bFoundPreBuildScript = false;
					CBuildScript PreBuildScript;
					PreBuildScript.m_bPreBuild = true;

					bool bFoundToolBuildScript = false;
					CBuildScript ToolBuildScript;

					auto pEvaluated = _ProjectState.m_EvaluatedTargetSettings.f_FindEqual(Configuration);

					if (pEvaluated)
					{
						if (pEvaluated->m_Element.f_Exists(gc_ConstString_PostBuildScript.m_String))
						{
							bFoundPostBuildScript = true;

							auto &InputsElement = pEvaluated->m_Element[gc_ConstKey_Target_PostBuildScriptInputs.m_Name];
							auto &OutputsElement = pEvaluated->m_Element[gc_ConstKey_Target_PostBuildScriptOutputs.m_Name];
							PostBuildScript.m_Inputs = InputsElement.f_ValueArray();
							PostBuildScript.m_Outputs = OutputsElement.f_ValueArray();

							CElement const& BuildScript = pEvaluated->m_Element[gc_ConstString_PostBuildScript.m_String];
							PostBuildScript.m_Name = BuildScript.f_GetProperty();
							PostBuildScript.m_Script = BuildScript.f_GetValue().f_Replace("\r\n", "\n");
						}
						if (pEvaluated->m_Element.f_Exists(gc_ConstString_ToolBuildScript.m_String))
						{
							bFoundToolBuildScript = true;

							auto &InputsElement = pEvaluated->m_Element[gc_ConstString_ToolBuildScriptInputs.m_String];
							auto &OutputsElement = pEvaluated->m_Element[gc_ConstString_ToolBuildScriptOutputs.m_String];
							ToolBuildScript.m_Inputs = InputsElement.f_ValueArray();
							ToolBuildScript.m_Outputs = OutputsElement.f_ValueArray();

							CStr DependencyFile;
							if (auto pDependencyFile = pEvaluated->m_Element.f_FindEqual(gc_ConstKey_Target_DependencyFile.m_Name))
								DependencyFile = pDependencyFile->f_GetValue();

							CElement const& BuildScript = pEvaluated->m_Element[gc_ConstString_ToolBuildScript.m_String];
							ToolBuildScript.m_Name = BuildScript.f_GetProperty();
							ToolBuildScript.m_Script =
								"export MalterlibDependencyFile=\"{}\"\n"
								"set -eo pipefail\n"
								"cd \"${{PROJECT_DIR}\"\n"
								"{}\n"
								"{}\n"_f
								<< DependencyFile
								<< NativeTarget.m_ScriptExport
								<< BuildScript.f_GetValue().f_Replace("\r\n", "\n")
							;

							if (!DependencyFile.f_IsEmpty())
							{
								ToolBuildScript.m_Script += CStr::CFormat("if [ ! -f \"{}\" ]; then\n") << DependencyFile;
								ToolBuildScript.m_Script += CStr::CFormat("\tMTool TouchOrCreate \"{}\"\n") << DependencyFile;
								ToolBuildScript.m_Script += "fi\n";
							}
						}
						if (pEvaluated->m_Element.f_Exists(gc_ConstString_PreBuildScript.m_String))
						{
							bFoundPreBuildScript = true;

							auto &InputsElement = pEvaluated->m_Element[gc_ConstKey_Target_PreBuildScriptInputs.m_Name];
							auto &OutputsElement = pEvaluated->m_Element[gc_ConstKey_Target_PreBuildScriptOutputs.m_Name];
							PreBuildScript.m_Inputs = InputsElement.f_ValueArray();
							PreBuildScript.m_Outputs = OutputsElement.f_ValueArray();

							CElement const& BuildScript = pEvaluated->m_Element[gc_ConstString_PreBuildScript.m_String];
							PreBuildScript.m_Name = BuildScript.f_GetProperty();
							PreBuildScript.m_Script = BuildScript.f_GetValue().f_Replace("\r\n", "\n");
						}
					}

					if (bFoundPostBuildScript)
						NativeTarget.m_BuildScripts[PostBuildScript.m_Name] = fg_Move(PostBuildScript);
					if (bFoundToolBuildScript)
						NativeTarget.m_BuildScripts[ToolBuildScript.m_Name] = fg_Move(ToolBuildScript);
					if (bFoundPreBuildScript)
						NativeTarget.m_BuildScripts[PreBuildScript.m_Name] = fg_Move(PreBuildScript);
				}
			}

			co_return {};
		}

		void CGeneratorInstance::fp_EvaluateDependencies(CProject& _Project) const
		{
			for (auto &Dependency : _Project.m_DependenciesOrdered)
			{
				if (Dependency.m_bExternal)
					continue;

				TCMap<CConfiguration, CEntityMutablePointer> EnabledConfigurations;

				for (auto iConfig = Dependency.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					if (_Project.m_EnabledProjectConfigs.f_FindEqual(iConfig.f_GetKey()))
						EnabledConfigurations[iConfig.f_GetKey()] = *iConfig;
				}

				if (EnabledConfigurations.f_IsEmpty())
					continue;

				CStr ProjectDependency = Dependency.f_GetName();
				auto pDependProject = _Project.m_pSolution->m_Projects.f_FindEqual(ProjectDependency);

				if (!pDependProject)
					m_BuildSystem.fs_ThrowError(Dependency.m_Position, CStr::CFormat("Dependency {} not found in workspace") << ProjectDependency);

				for (auto &Entity : EnabledConfigurations)
				{
					auto &Configuration = EnabledConfigurations.fs_GetKey(Entity);
					if (!pDependProject->m_EnabledProjectConfigs.f_FindEqual(Configuration))
					{
						m_BuildSystem.fs_ThrowError
							(
								Dependency.m_Position
								, "Dependency project does not have required configuration {} - {}"_f << Configuration.m_Platform << Configuration.m_Configuration
							)
						;
					}
				}


				fp_CalculateDependencyProductPath(*pDependProject, Dependency, EnabledConfigurations);
			}
		}

		CStr CGeneratorInstance::fp_MakeNiceSharedFlagValue(CStr const &_Type) const
		{
			return _Type.f_Replace("+", "P");
		}

		void CGeneratorInstance::fp_GenerateCompilerFlags(CProjectState &_ProjectState, CProject& _Project) const
		{
			auto fGetSharedFlags = [&] (CStr const &_Type, mint _SettingsNumber, bool _bKey) -> CStr
			{
				if (!_bKey)
				{
					if (_SettingsNumber != 0)
						return (CStr::CFormat("$({0}SharedFlags) $({0}Flags{1})") << fp_MakeNiceSharedFlagValue(_Type) << _SettingsNumber);
					return (CStr::CFormat("$({}SharedFlags)") << fp_MakeNiceSharedFlagValue(_Type));
				}
				else
				{
					if (_SettingsNumber != 0)
						return (CStr::CFormat("{}Flags{}") << fp_MakeNiceSharedFlagValue(_Type) << _SettingsNumber);
					return (CStr::CFormat("{}SharedFlags") << fp_MakeNiceSharedFlagValue(_Type));
				}
			};

			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				CConfiguration const& Configuration = iConfig.f_GetKey();
				TCMap<CStr, CConfigResultCompile> MergedSharedSettings;
				TCMap<CStr, bool> PrefixHeadersEnabled;
				{
					for (auto iFlag = _ProjectState.m_EvaluatedTypeCompileFlags.f_GetIterator(); iFlag; ++iFlag)
					{
						CStr const& Type = iFlag.f_GetKey();
						auto const& ConfigData = (*iFlag)[Configuration];

						CStr TranslatedType = Type;

						auto MergedMap = MergedSharedSettings(TranslatedType);

						if (MergedMap.f_WasCreated())
							*MergedMap = ConfigData;
						else
							DError("Internal error, duplicate compiler type");
						{
							auto pPrecompilePrefixHeader = ConfigData.m_Element.f_FindEqual(gc_ConstString_GCC_PRECOMPILE_PREFIX_HEADER.m_String);
							if (pPrecompilePrefixHeader && pPrecompilePrefixHeader->f_GetValue() == gc_ConstString_YES.m_String)
							{
								auto pPrefixHeader = ConfigData.m_Element.f_FindEqual(gc_ConstString_GCC_PREFIX_HEADER.m_String);
								if (!pPrefixHeader->f_GetValue().f_IsEmpty())
									PrefixHeadersEnabled[TranslatedType] = true;
							}
						}
					}
				}

				auto &OtherCPPFlags = _ProjectState.m_OtherCPPFlags[Configuration];
				auto &OtherCFlags = _ProjectState.m_OtherCFlags[Configuration];
				auto &OtherObjCPPFlags = _ProjectState.m_OtherObjCPPFlags[Configuration];
				auto &OtherObjCFlags = _ProjectState.m_OtherObjCFlags[Configuration];
				auto &OtherAssemblerFlags = _ProjectState.m_OtherAssemblerFlags[Configuration];
				auto &SettingsFromTypes = _ProjectState.m_XcodeSettingsFromTypes[Configuration];

				TCMap<CStr, zmint> SettingsNumber;
				TCMap<CStr, TCMap<CStr, mint>> GeneratedOverriddenFlags;

				TCMap<CStr, TCMap<CStr, CStr>> SharedFlags;
				TCMap<CStr, TCMap<CStr, TCSet<TCVariant<CStr, TCVector<CStr>>>>> SharedFlagsSet;
				TCMap<CStr, TCSet<CStr>> NonSharedFlags;
				TCMap<CStr, TCMap<CStr, CElement const*>> GlobalNonShared;
				TCMap<CStr, TCMap<CStr, TCSet<TCVariant<CStr, TCVector<CStr>>>>> NonSharedFlagsSet;
				TCMap<CStr, TCMap<CStr, CElement>> GlobalNonSharedSet;

				{

					for (auto iFile = _Project.m_Files.f_GetIterator(); iFile; ++iFile)
					{
						if (iFile->m_bHasCompilerFlags)
						{
							CStr TranslatedType = iFile->m_Type;

							auto const &ConfigData = _ProjectState.m_EvaluatedCompileFlags[iFile->f_GetCompileFlagsGUID()][Configuration];
							auto const &TypeConfigData = MergedSharedSettings[TranslatedType];

							auto &File = SharedFlags[TranslatedType];
							auto &FileSet = SharedFlagsSet[TranslatedType];
							auto &NonShared = NonSharedFlags[TranslatedType];
							auto &NonSharedSet = NonSharedFlagsSet[TranslatedType];
							auto &GlobalNonSharedType = GlobalNonShared[TranslatedType];
							auto &GlobalNonSharedTypeSet = GlobalNonSharedSet[TranslatedType];
							bool bPrefixHeadersEnabled = PrefixHeadersEnabled[TranslatedType];

							// Evaluate flags overridden by this file.
							for (auto iFlag = ConfigData.m_Element.f_GetIterator(); iFlag; ++iFlag)
							{
								auto pType = TypeConfigData.m_Element.f_FindEqual(iFlag.f_GetKey());
								if (pType)
								{
									if (pType->m_bUseValues)
									{
										if (!iFlag->m_bUseValues)
											m_BuildSystem.fs_ThrowError(iFlag->m_Positions, "Inconsistent use of value set");

										for (auto &Value : pType->m_ValueSet)
										{
											if (!iFlag->m_ValueSet.f_FindEqual(Value))
											{
												if (bPrefixHeadersEnabled)
													m_BuildSystem.fs_ThrowError(iFlag->m_Positions, "Per file settings cannot remove a compile flag when precompile prefix headers are enabled: {} in {}"_f << iFlag->m_ValueSet << Value);
												else
												{
													NonSharedSet[iFlag->f_GetProperty()][Value];
													auto Mapped = GlobalNonSharedTypeSet(iFlag->f_GetProperty());
													if (Mapped.f_WasCreated())
													{
														*Mapped = *pType;
														(*Mapped).m_ValueSet.f_Clear();
													}
													(*Mapped).m_ValueSet[Value];
												}
											}
										}
									}
									else if (!pType->f_IsSameValue(*iFlag))
									{
										if (bPrefixHeadersEnabled)
											m_BuildSystem.fs_ThrowError(iFlag->m_Positions, "Per file settings cannot have a different value for a compile flag when precompile prefix headers are enabled");
										else
										{
											NonShared[iFlag->f_GetProperty()];
											GlobalNonSharedType[iFlag->f_GetProperty()] = pType;
										}
										continue;
									}
								}

								if (iFlag->m_bUseValues)
								{
									auto Mapped = FileSet(iFlag->f_GetProperty());
									if (Mapped.f_WasCreated())
									{
										if (pType)
											*Mapped = pType->m_ValueSet;
									}

									{
										auto &Set = NonSharedSet[iFlag->f_GetProperty()];
										TCSet<TCVariant<CStr, TCVector<CStr>>> ToRemove;
										for (auto &Value : *Mapped)
										{
											if (!iFlag->m_ValueSet.f_FindEqual(Value))
											{
												ToRemove[Value];
												Set[Value];
											}
										}
										for (auto &Value : iFlag->m_ValueSet)
										{
											if (!(*Mapped).f_FindEqual(Value))
												Set[Value];
										}
										for (auto &Value : ToRemove)
											(*Mapped).f_Remove(Value);
									}
								}
								else
								{
									auto Mapped = File(iFlag->f_GetProperty());
									if (!Mapped.f_WasCreated())
									{
										if (*Mapped != iFlag->f_GetValue())
											NonShared[iFlag->f_GetProperty()];
									}
									else
										*Mapped = iFlag->f_GetValue();
								}
							}
						}
					}
				}

				auto fGenerateFlags = [&] (CConfigResultCompile const &_ConfigData, CStr const &_FileType, bool _bFileType) -> CStr
				{
					CStr CompileFlags;

					auto fAddFlag = [&](CStr const &_Value)
						{
							if (_bFileType)
							{
								if (_FileType == gc_ConstString_Symbol_Cpp.m_String)
								{
									OtherCPPFlags += CStr::CFormat(" {}") << fs_EscapeCommandLineArgument(_Value);
									return;
								}
								else if (_FileType == gc_ConstString_Symbol_ObjCpp.m_String)
								{
									OtherObjCPPFlags += CStr::CFormat(" {}") << fs_EscapeCommandLineArgument(_Value);
									return;
								}
								else if (_FileType == gc_ConstString_C.m_String)
								{
									OtherCFlags += CStr::CFormat(" {}") << fs_EscapeCommandLineArgument(_Value);
									return;
								}
								else if (_FileType == gc_ConstString_ObjC.m_String)
								{
									OtherObjCFlags += CStr::CFormat(" {}") << fs_EscapeCommandLineArgument(_Value);
									return;
								}
								else if (_FileType == gc_ConstString_Assembler.m_String)
								{
									OtherAssemblerFlags += CStr::CFormat(" {}") << fs_EscapeCommandLineArgument(_Value);
									return;
								}
							}
							CompileFlags += (CStr::CFormat(" {}") << fs_EscapeCommandLineArgument(_Value));
						}
					;
					auto fBuildFlags = [&] (CElement const &_Element, TCSet<TCVariant<CStr, TCVector<CStr>>> const *_pValueSet = nullptr)
					{
						if (_Element.f_GetProperty() == gc_ConstString_GCC_PRECOMPILE_PREFIX_HEADER.m_String)
							return;
						else if (_Element.f_GetProperty() == gc_ConstString_GCC_PREFIX_HEADER.m_String)
							return;

						if (_Element.m_bUseValues)
						{
							if (_pValueSet)
							{
								for (auto &Value : CElement::fs_ValueArray(*_pValueSet))
									fAddFlag(Value);
							}
							else
							{
								for (auto &Value : _Element.f_ValueArray())
									fAddFlag(Value);
							}
						}
						else
						{
							CStr Value = _Element.f_GetValue();
							if (!Value.f_IsEmpty())
								fAddFlag(Value);
						}
					};

					CStr TranslatedType = _FileType;

					auto const &NonShared = NonSharedFlags[TranslatedType];
					auto const &NonSharedSet = NonSharedFlagsSet[TranslatedType];

					CConfigResultCompile const &TypeConfigData = MergedSharedSettings[TranslatedType];

					for (auto iElement = _ConfigData.m_Element.f_GetIterator(); iElement; ++iElement)
					{
						if (iElement->m_bXcodeProperty)
						{
							if (_bFileType)
							{
								if
									(
										iElement->f_GetProperty() == gc_ConstString_GCC_PRECOMPILE_PREFIX_HEADER.m_String
										|| iElement->f_GetProperty() == gc_ConstString_GCC_PREFIX_HEADER.m_String
									)
								{
									if (SettingsFromTypes.f_Exists(iElement->f_GetProperty()))
									{
										if (iElement->f_GetValue() != SettingsFromTypes[iElement->f_GetProperty()])
											m_BuildSystem.fs_ThrowError(_Project.m_ProjectPosition, "Prefix headers and PrecompilePrefixHeaders per file type are not supported");
									}
								}
								if (iElement->m_bUseValues)
								{
									CStr Values;
									for (auto &Value : iElement->f_ValueArray())
										fg_AddStrSep(Values, Value, " ");
									SettingsFromTypes[iElement->f_GetProperty()] = Values;
								}
								else
									SettingsFromTypes[iElement->f_GetProperty()] = iElement->f_GetValue();
							}

							continue;
						}

						if (_bFileType)
						{
							if (iElement->m_bUseValues)
							{
								auto pNonShared = NonSharedSet.f_FindEqual(iElement->f_GetProperty());
								TCSet<TCVariant<CStr, TCVector<CStr>>> ToSet;
								TCSet<TCVariant<CStr, TCVector<CStr>>> *pToSet = nullptr;
								if (pNonShared)
								{
									for (auto &Value : iElement->m_ValueSet)
									{
										if (!pNonShared->f_FindEqual(Value))
											ToSet[Value];
									}
									pToSet = &ToSet;
								}

								fBuildFlags(*iElement, pToSet);
								continue;
							}
							else
							{
								if (NonShared.f_FindEqual(iElement->f_GetProperty()))
									continue;
							}
						}
						else
						{
							if (iElement->m_bUseValues)
							{
								if (TypeConfigData.m_Element.f_FindEqual(iElement.f_GetKey()))
								{
									auto pNonShared = NonSharedSet.f_FindEqual(iElement->f_GetProperty());
									TCSet<TCVariant<CStr, TCVector<CStr>>> ToSet;
									if (pNonShared)
									{
										for (auto &Value : iElement->m_ValueSet)
										{
											if (pNonShared->f_FindEqual(Value))
												ToSet[Value];
										}
									}

									fBuildFlags(*iElement, &ToSet);
									continue;
								}
							}
							else
							{
								if (!NonShared.f_FindEqual(iElement->f_GetProperty()) && TypeConfigData.m_Element.f_FindEqual(iElement.f_GetKey()))
									continue;
							}
						}

						fBuildFlags(*iElement);
					}

					if (!_bFileType)
					{
						auto const &GlobalNonSharedType = GlobalNonShared[TranslatedType];
						for (auto iNonShared = GlobalNonSharedType.f_GetIterator(); iNonShared; ++iNonShared)
						{
							if (!_ConfigData.m_Element.f_FindEqual(iNonShared.f_GetKey()))
							{
								fBuildFlags(**iNonShared);
							}
						}
						auto const &GlobalNonSharedTypeSet = GlobalNonSharedSet[TranslatedType];
						for (auto iNonShared = GlobalNonSharedTypeSet.f_GetIterator(); iNonShared; ++iNonShared)
						{
							if (!_ConfigData.m_Element.f_FindEqual(iNonShared.f_GetKey()))
							{
								fBuildFlags(*iNonShared);
							}
						}
					}

					return CompileFlags;
				};

				// Compiler flags per file
				{
					for (auto iFile = _Project.m_Files.f_GetIterator(); iFile; ++iFile)
					{
						if (!iFile->m_EnabledConfigs.f_Exists(Configuration))
							continue;

						if (!iFile->m_bHasCompilerFlags)
							continue;

						CConfigResultCompile const &ConfigData = _ProjectState.m_EvaluatedCompileFlags[iFile->f_GetCompileFlagsGUID()][Configuration];

						// Generate compiler flags for this file for any properties that exist in the overridden properties map (take them from
						// the shared flags if they do not override them)
						CStr FlagsForFile = fGenerateFlags(ConfigData, iFile->m_Type, false);
						if (!FlagsForFile.f_IsEmpty())
						{
							auto & OverriddenType = GeneratedOverriddenFlags[iFile->m_Type];
							auto MappedOveridden = OverriddenType(FlagsForFile);
							if (MappedOveridden.f_WasCreated())
								*MappedOveridden = ++SettingsNumber[iFile->m_Type];

							_ProjectState.m_CompileFlagsValues[iFile->f_GetCompileFlagsGUID()][Configuration] = fGetSharedFlags(iFile->m_Type, *MappedOveridden, false);
						}

						// Some special flags
						{
							for (auto iFlag = ConfigData.m_Element.f_GetIterator(); iFlag; ++iFlag)
							{
								if (!iFlag->m_bXcodeProperty)
									continue;

								if (iFlag->f_GetProperty() == gc_ConstString_GCC_PRECOMPILE_PREFIX_HEADER.m_String || iFlag->f_GetProperty() == gc_ConstString_GCC_PREFIX_HEADER.m_String)
								{
									if (SettingsFromTypes.f_Exists(iFlag->f_GetProperty()))
									{
										if (iFlag->f_GetValue() != SettingsFromTypes[iFlag->f_GetProperty()])
											m_BuildSystem.fs_ThrowError(iFile->m_Position, "Prefix headers and PrecompilePrefixHeaders per file type are not supported");
									}

									SettingsFromTypes[iFlag->f_GetProperty()] = iFlag->f_GetValue();
								}
								else if (iFlag->f_GetProperty() == gc_ConstString_MOC_OUTPUT_PATTERN.m_String)
								{
									if (iFlag->f_GetValue().f_Find(CFile::fs_GetFile(iFile->f_GetName())) > -1)
										m_BuildSystem.fs_ThrowError(iFile->m_Position, "Moc output pattern settings per file are not supported");
									else if (CFile::fs_GetExtension(iFile->f_GetName()) == "cpp")
									{
										if (!_ProjectState.m_MocOutputPatternCPP.f_IsEmpty() && _ProjectState.m_MocOutputPatternCPP != iFlag->f_GetValue())
											m_BuildSystem.fs_ThrowError(iFile->m_Position, "Moc output pattern settings per file are not supported");
										else if (_ProjectState.m_MocOutputPatternCPP.f_IsEmpty())
											_ProjectState.m_MocOutputPatternCPP = iFlag->f_GetValue();
									}
									else
									{
										_ProjectState.m_EvaluatedTypeCompileFlags[iFile->m_Type][Configuration]
											.m_Element[gc_ConstString_MOC_OUTPUT_PATTERN.m_String]
											.f_SetValue(iFlag->f_GetValue())
										;
									}
								}
								else if (iFlag->f_GetProperty() == gc_ConstString_FilesExcludedFromCompile.m_String)
								{
									if (iFlag->f_GetValue() == gc_ConstString_true.m_String && iFile->m_LastKnownFileType.f_Find(gc_ConstString_nolink.m_String) == -1)
										iFile->m_LastKnownFileType += ".nolink";
								}
								else
								{
									//DMibTrace("Excluded {} = {}\n", iFlag->f_GetProperty() << iFlag->m_Value);
								}
							}
						}
					}
				}

				// Store overridden flags
				{
					auto &EvaluatedOverriddenCompileFlags = _ProjectState.m_EvaluatedOverriddenCompileFlags[Configuration];
					for (auto iFileType = GeneratedOverriddenFlags.f_GetIterator(); iFileType; ++iFileType)
					{
						for (auto iGeneratedOverriddenFlags = iFileType->f_GetIterator(); iGeneratedOverriddenFlags; ++iGeneratedOverriddenFlags)
						{
							CStr Key = fGetSharedFlags(iFileType.f_GetKey(), (*iGeneratedOverriddenFlags), true);
							EvaluatedOverriddenCompileFlags[Key] = iGeneratedOverriddenFlags.f_GetKey();
						}
					}
				}

				// Base compile flags per file type
				{
					for (auto iFlag = _ProjectState.m_EvaluatedTypeCompileFlags.f_GetIterator(); iFlag; ++iFlag)
					{
						CStr const& Type = iFlag.f_GetKey();
						CConfigResultCompile const &ConfigData = (*iFlag)[Configuration];

						// Generate any flags that do not appear in the overridden properties map
						CStr CompileFlags = fGenerateFlags(ConfigData, Type, true);
						CStr Flags = fGetSharedFlags(Type, 0, true);
						_ProjectState.m_CompileFlagsValues[Flags][Configuration] = CompileFlags;
					}
				}
			}

			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				CConfiguration const& Configuration = iConfig.f_GetKey();

				for (auto iFile = _Project.m_Files.f_GetIterator(); iFile; ++iFile)
				{
					if (iFile->m_bHasCompilerFlags)
					{
						if (_ProjectState.m_CompileFlagsValues.f_Exists(iFile->f_GetCompileFlagsGUID()))
						{
							TCMap<CConfiguration, CStr>& SpecificSettings = _ProjectState.m_CompileFlagsValues[iFile->f_GetCompileFlagsGUID()];
							SpecificSettings[Configuration];
						}
					}
				}
			}
		}

		struct CLinkConfig
		{
			CProjectDependency::CPerConfig *m_pPerConfig = nullptr;
			bool m_bInternal = false;
		};
		struct CLinkGroup
		{
			DLinkDS_Link(CLinkGroup, m_Link);
			TCLinkedList<CLinkConfig> m_Configs;
			CStr m_Name;
		};

		TCSet<CStr> g_PathModSettings =
			{
				gc_ConstString_BUILD_DIR
				, gc_ConstString_BUILD_ROOT
				, gc_ConstString_CONFIGURATION_TEMP_DIR
				, gc_ConstString_OBJROOT
				, gc_ConstString_PROJECT_DERIVED_FILE_DIR
				, gc_ConstString_PROJECT_TEMP_DIR
				, gc_ConstString_PROJECT_TEMP_ROOT
				, gc_ConstString_SHARED_PRECOMPS_DIR
				, gc_ConstString_SYMROOT
				, gc_ConstString_TARGET_TEMP_DIR
				, gc_ConstString_TEMP_FILES_DIR
				, gc_ConstString_TEMP_FILE_DIR
				, gc_ConstString_TEMP_ROOT
				, gc_ConstString_MODULE_CACHE_DIR
			}
		;

		void CGeneratorInstance::fp_GenerateBuildConfigurationFile
			(
				CProjectState &_ProjectState
				, CProject& _Project
				, CConfiguration const &_Configuration
				, CStr const &_OutputFile
				, CStr const &_OutputDir
				, CNativeTarget const &_NativeTarget
			) const
		{
			CStr FileData;

			FileData += "ESCSLASH = /\n";

			auto fEscVar
				= [](CStr const &_Var) -> CStr
				{
					if (_Var.f_Find("//") >= 0)
					{
						return _Var.f_Replace("//", "${ESCSLASH}${ESCSLASH}");
					}
					return _Var;
				}
			;

			// Other CPP Flags
			{
				if (_NativeTarget.m_CType == gc_ConstString_C.m_String)
					FileData += (CStr::CFormat("OTHER_CFLAGS = {}\n") << fEscVar(_ProjectState.m_OtherCFlags[_Configuration]));
				else if (_NativeTarget.m_CType == gc_ConstString_ObjC.m_String)
					FileData += (CStr::CFormat("OTHER_CFLAGS = {}\n") << fEscVar(_ProjectState.m_OtherObjCFlags[_Configuration]));
				else if (_NativeTarget.m_CType == gc_ConstString_Symbol_Cpp.m_String)
					FileData += (CStr::CFormat("OTHER_CFLAGS = {}\n") << fEscVar(_ProjectState.m_OtherCPPFlags[_Configuration]));
				else if (_NativeTarget.m_CType == gc_ConstString_Symbol_ObjCpp.m_String)
					FileData += (CStr::CFormat("OTHER_CFLAGS = {}\n") << fEscVar(_ProjectState.m_OtherObjCPPFlags[_Configuration]));
				else if (_NativeTarget.m_CType == gc_ConstString_Assembler.m_String)
					FileData += (CStr::CFormat("OTHER_CFLAGS = {}\n") << fEscVar(_ProjectState.m_OtherAssemblerFlags[_Configuration]));

				FileData += (CStr::CFormat("MOC_OUTPUT_PATTERN_CPP = {}\n") << fEscVar(_ProjectState.m_MocOutputPatternCPP));
				FileData += "MOC_OUTPUT_PATTERN_NOLINK = $(MOC_OUTPUT_PATTERN)\nMOC_OUTPUT_PATTERN_CPP_NOLINK = $(MOC_OUTPUT_PATTERN_CPP)\n";
			}

			auto fOutputConfigValue = [&](CStr const &_Key, CStr const &_Value)
				{
					if (_Key == "MALTERLIB_GENERATOR_CLANG")
					{
						CStr Value = fEscVar(_Value);
						CStr ValuePlusPlus = fEscVar(_Value + "++");
						if (CFile::fs_GetFileNoExt(Value) == "MTool")
							ValuePlusPlus = Value;

						FileData += "MALTERLIB_GENERATOR_CLANG = {}\n"_f << Value;
						FileData += "CC = {}\n"_f << Value;
						FileData += "CPLUSPLUS = {}\n"_f << ValuePlusPlus;
						FileData += "LD = {}\n"_f << Value;
						FileData += "LDPLUSPLUS = {}\n"_f << ValuePlusPlus;
						FileData += "CLANG = {}\n"_f << Value;
						FileData += "CLANG_ANALYZER_EXEC = {}\n"_f << Value;
					}

					if (!_NativeTarget.m_bDefaultTarget)
					{
						if (g_PathModSettings.f_FindEqual(_Key))
						{
							FileData += "{} = {}\n"_f << _Key << fEscVar("{}/{}"_f << _Value << _NativeTarget.m_CType);
							return;
						}
						else if (_Key == "EXECUTABLE_FOLDER_PATH")
						{
							FileData += "{} = \n"_f << _Key;
							return;
						}
						else if (_Key == gc_ConstString_EXECUTABLE_EXTENSION.m_String)
						{
							FileData += "{} = a\n"_f << _Key;
							return;
						}
						else if (_Key == gc_ConstString_PRODUCT_NAME.m_String)
						{
							FileData += "{} = {}\n"_f << _Key << fEscVar(_Value + _NativeTarget.m_CType);
							return;
						}
						else if (_Key == "MACH_O_TYPE")
						{
							FileData += "{} = {}\n"_f << _Key << fEscVar("staticlib");
							return;
						}
					}

					FileData += "{} = {}\n"_f << _Key << fEscVar(_Value);
				}
			;

			// Overridden flags
			{
				for (auto iFlag = _ProjectState.m_EvaluatedOverriddenCompileFlags[_Configuration].f_GetIterator(); iFlag; ++iFlag)
					fOutputConfigValue(iFlag.f_GetKey(), *iFlag);
			}

			// Compile flags
			{
				for (auto iFlag = _ProjectState.m_CompileFlagsValues.f_GetIterator(); iFlag; ++iFlag)
					fOutputConfigValue(iFlag.f_GetKey(), (*iFlag)[_Configuration]);
			}

			// Xcode settings
			{
				for (auto ISetting = _ProjectState.m_XcodeSettingsFromTypes[_Configuration].f_GetIterator(); ISetting; ++ISetting)
					fOutputConfigValue(ISetting.f_GetKey(), *ISetting);
			}

			// MLSRCROOT
			{
				FileData += (CStr::CFormat("MLSRCROOT = {}\n") << fEscVar(m_RelativeBasePathAbsolute.f_String()));
			}

			bool bDoneAdditionalLibraries = false;
			CStr LDFlagsFirst;
//				bool bDoneSearchPaths = false;

			// Per configuration target settings
			{
				CConfigResultTarget &ConfigData = _ProjectState.m_EvaluatedTargetSettings[_Configuration];
				for (auto iElement = ConfigData.m_Element.f_GetIterator(); iElement; ++iElement)
				{
					bool bSearchPaths = false;
					bool bAdditionalLibraries = false;

					if (iElement->f_GetProperty() == gc_ConstString_LIBRARY_SEARCH_PATHS.m_String)
						bSearchPaths = true;
					else if (!bDoneAdditionalLibraries && iElement->f_GetProperty() == gc_ConstString_OTHER_LDFLAGS.m_String)
						bAdditionalLibraries = true;
					else if (!bDoneAdditionalLibraries && iElement->f_GetProperty() == gc_ConstString_LDFlagsFirst.m_String)
						LDFlagsFirst = CStr::fs_Join(iElement->f_ValueArray(), " ");
					else if (!bDoneAdditionalLibraries && iElement->f_GetProperty() == gc_ConstString_OTHER_LIBTOOLFLAGS.m_String && m_XcodeVersion >= 6)
						bAdditionalLibraries = true;

					CStr Extra;
					CStr ExtraAfter;
					if (bSearchPaths)
					{
//							bDoneSearchPaths = true;
						//Extra = "$DependencySearchPaths ";
					}
					else if (bAdditionalLibraries)
					{
						bDoneAdditionalLibraries = true;
						Extra = "$LDFlagsFirst $DependencyLibrariesForced $DependencyLibraries ";
						ExtraAfter = " $DependencyLibraries";
					}

					if (_Project.f_GetDefaultNativeTarget(_Configuration).m_BuildScripts.f_FindEqual(iElement->f_GetProperty()))
						;
					else if (!iElement->f_GetProperty().f_IsEmpty())
					{
						if (bAdditionalLibraries)
							FileData += (CStr::CFormat("{} = {}{}{}\n") << iElement->f_GetProperty() << Extra << fEscVar(iElement->f_GetCombinedValue()) << ExtraAfter);
						else
							fOutputConfigValue(iElement->f_GetProperty(), iElement->f_GetCombinedValue());
					}
				}
			}

			// Project dependencies
			{
				CStr LinkSearchPaths;
				CStr Link;
				CStr LinkForced;

				DLinkDS_List(CLinkGroup, m_Link) LinkerGroupsOrdered;
				TCMap<CStr, CLinkGroup> LinkerGroups;
				TCLinkedList<CLinkGroup> NonLinkerGroups;
				for (auto &Dependency : _Project.m_DependenciesOrdered)
				{
					if (Dependency.m_bExternal)
						continue;

					auto *pPerConfig = Dependency.m_PerConfig.f_FindEqual(_Configuration);
					if (!pPerConfig)
						continue;

					auto &PerConfig = *pPerConfig;

					if (PerConfig.m_bLink && _NativeTarget.m_bDefaultTarget)
					{
						CLinkGroup *pGroup;
						if (PerConfig.m_LinkerGroup.f_IsEmpty())
							pGroup = &NonLinkerGroups.f_Insert();
						else
							pGroup = &LinkerGroups[PerConfig.m_LinkerGroup];
						auto &Group = *pGroup;
						if (!Group.m_Link.f_IsInList())
						{
							Group.m_Name = PerConfig.m_LinkerGroup;
							LinkerGroupsOrdered.f_Insert(Group);
						}
						CLinkConfig LinkConfig;
						LinkConfig.m_pPerConfig = &PerConfig;
						LinkConfig.m_bInternal = Dependency.m_bInternal;
						Group.m_Configs.f_Insert(LinkConfig);
						LinkSearchPaths += (CStr::CFormat("\"{}\" ") << PerConfig.m_SearchPath);
					}
				}

				for (auto iLinkerGroup = LinkerGroupsOrdered.f_GetIterator(); iLinkerGroup; ++iLinkerGroup)
				{
					if (!iLinkerGroup->m_Name.f_IsEmpty() && _NativeTarget.m_ProductType != "com.apple.product-type.library.static")
						Link += "-Xlinker -( ";
					for (auto &LinkConfig : iLinkerGroup->m_Configs)
					{
						if ((LinkConfig.m_bInternal || LinkConfig.m_pPerConfig->m_bObjectLibrary) && _NativeTarget.m_ProductType != "com.apple.product-type.library.static")
						{
							if (_Configuration.m_PlatformBase.f_StartsWith("macOS"))
								LinkForced += "-force_load \"{}/{}\" "_f << LinkConfig.m_pPerConfig->m_SearchPath << LinkConfig.m_pPerConfig->m_CalculatedPath;
							else
								LinkForced += "-Xlinker --whole-archive \"{}/{}\" -Xlinker --no-whole-archive "_f << LinkConfig.m_pPerConfig->m_SearchPath << LinkConfig.m_pPerConfig->m_CalculatedPath;
						}
						else
							Link += "\"{}/{}\" "_f << LinkConfig.m_pPerConfig->m_SearchPath << LinkConfig.m_pPerConfig->m_CalculatedPath;
					}
					if (!iLinkerGroup->m_Name.f_IsEmpty() && _NativeTarget.m_ProductType != "com.apple.product-type.library.static")
						Link += "-Xlinker -) ";
				}

				//FileData += (CStr::CFormat("DependencySearchPaths = {}\n") << fEscVar(LinkSearchPaths));
				FileData += "DependencyLibrariesForced = {}\n"_f << fEscVar(LinkForced);
				FileData += "DependencyLibraries = {}\n"_f << fEscVar(Link);
				FileData += "LDFlagsFirst = {}\n"_f << fEscVar(LDFlagsFirst);
				FileData += "MalterlibXcodeObjectFileDirName = $(OBJECT_FILE_DIR_$CURRENT_VARIANT:file)\n"_f << fEscVar(Link);

				if (!bDoneAdditionalLibraries)
				{
					if (m_XcodeVersion >= 6)
						FileData += "OTHER_LIBTOOLFLAGS = $DependencyLibrariesForced $DependencyLibraries\n";
					FileData += "OTHER_LDFLAGS = $LDFlagsFirst $DependencyLibrariesForced $DependencyLibraries\n";
				}
				//if (!bDoneSearchPaths)
				//	FileData += "LIBRARY_SEARCH_PATHS = $DependencySearchPaths\n";
			}

			{
				bool bWasCreated;
				if (!m_BuildSystem.f_AddGeneratedFile(_OutputFile, FileData, _Project.m_pSolution->f_GetName(), bWasCreated))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << _OutputFile));

				if (bWasCreated)
				{
					CByteVector FileDataVector;
					CFile::fs_WriteStringToVector(FileDataVector, CStr(FileData), false);
					m_BuildSystem.f_WriteFile(FileDataVector, _OutputFile);
				}
			}
		}
	}
}
