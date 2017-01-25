// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <Mib/XML/XML>

namespace NMib::NStr
{
	template <typename tf_CStr, typename tf_CSplitChar>
	NContainer::TCVector<tf_CStr> fg_StrSplit(tf_CStr const& _ToSplit, tf_CSplitChar _SplitChar)
	{
		TCVector<tf_CStr> Ret;
		aint iLast = 0;
		aint iFind = _ToSplit.f_FindChar(_SplitChar);
		while (iFind >= 0)
		{
			Ret.f_Insert(_ToSplit.f_Extract(iLast, iFind - iLast));
			iLast = iFind + 1;
			iFind = _ToSplit.f_FindCharOffset(iLast, _SplitChar);
		}
		
		tf_CStr Last = _ToSplit.f_Extract(iLast);
		if (!Last.f_IsEmpty())
			Ret.f_Insert(Last);
		
		return fg_Move(Ret);
	}
}
namespace NMib::NBuildSystem
{
	namespace NXcode
	{
		void CGeneratorInstance::fp_GenerateBuildConfigurationFiles(CProject& _Project, CStr const& _OutputDir, TCVector<CBuildConfiguration>& _ConfigList, bint _bProject) const
		{
			CStr OutputDir = CFile::fs_AppendPath(_OutputDir, _Project.f_GetName());
			
			auto &ThreadLocal = *m_ThreadLocal;
			ThreadLocal.f_CreateDirectory(OutputDir);
			
			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				CConfiguration const& Configuration = iConfig.f_GetKey();

				CStr OutputFile = CFile::fs_AppendPath(OutputDir, CStr("Configurations"));
				ThreadLocal.f_CreateDirectory(OutputFile);

				OutputFile = CFile::fs_AppendPath(OutputFile, (CStr::CFormat("{} {}.xcconfig") << Configuration.m_Platform << Configuration.m_Configuration).f_GetStr());

				if (!_bProject)
					fp_GenerateBuildConfigurationFile(_Project, Configuration, OutputFile, _OutputDir);

				CBuildConfiguration& BuildConfig = _ConfigList.f_Insert();
				BuildConfig.m_Name = CFile::fs_GetFileNoExt(OutputFile);
				BuildConfig.m_Path = OutputFile;
				BuildConfig.m_bProject = _bProject;
			}
		}

		void CGeneratorInstance::fp_EvaluateTargetSettings(CProject& _Project) const
		{
			auto & ThreadLocal = *m_ThreadLocal;
			{
				TCMap<CConfiguration, CConfigResult> TargetTypes;
				TCVector<CStr> SearchList;
				SearchList.f_Insert("Root");
				
				fp_GetConfigValue(
					_Project.m_EnabledProjectConfigs
					, _Project.m_EnabledProjectConfigs
					, (*_Project.m_EnabledProjectConfigs.f_GetIterator())->m_Position
					, EPropertyType_Target
					, "Type"
					, false
					, false
					, &SearchList
					, nullptr
					, CStr()
					, CStr()
					, TargetTypes);		

				for (auto iTarget = TargetTypes.f_GetIterator(); iTarget; ++iTarget)
				{
					CStr ThisTargetType = iTarget->m_Element["Type"].f_GetValue();
					if (_Project.m_NativeTarget.m_Type.f_IsEmpty())
					{
						_Project.m_NativeTarget.m_Type = ThisTargetType;
					}
					else if (ThisTargetType != _Project.m_NativeTarget.m_Type)
					{
						m_BuildSystem.fs_ThrowError((*_Project.m_EnabledProjectConfigs.f_GetIterator())->m_Position, CStr::CFormat("Multiple target types found when only one is allowed '{}' != '{}'") << ThisTargetType << _Project.m_NativeTarget.m_Type);
					}
				}
			}

			TCVector<CStr> SearchList;
			SearchList.f_Insert(_Project.m_NativeTarget.m_Type + CStr::fs_ToStr(m_XcodeVersion));
			SearchList.f_Insert(_Project.m_NativeTarget.m_Type);
			if (_Project.m_NativeTarget.m_Type != "StaticLibrary" && _Project.m_NativeTarget.m_Type != "Makefile")
				SearchList.f_Insert("SharedApplication");
			else if (_Project.m_NativeTarget.m_Type == "Makefile")
				SearchList.f_Insert("SharedMakefile");

			SearchList.f_Insert("SharedTarget");

			fp_SetEvaluatedValues(
				_Project.m_EnabledProjectConfigs
				, _Project.m_EnabledProjectConfigs
				, false
				, EPropertyType_Target
				, &SearchList
				, nullptr
				, CStr()
				, false
				, false
				, ThreadLocal.mp_EvaluatedTargetSettings);	

			// Build scripts
			{
				bint bFoundPostBuildScript = false;
				CBuildScript PostBuildScript;
				PostBuildScript.m_bPostBuild = true;
				mint nPostbuildInputs = 0;
				mint nPostbuildOutputs = 0;

				bint bFoundPreBuildScript = false;
				CBuildScript PreBuildScript;
				PreBuildScript.m_bPreBuild = true;
				mint nPrebuildInputs = 0;
				mint nPrebuildOutputs = 0;

				for (auto iConfig = ThreadLocal.mp_EvaluatedTargetSettings.f_GetIterator(); iConfig; ++iConfig)
				{
					if (iConfig->m_Element.f_Exists("PostBuildScript"))
					{
						auto & InputsElement = iConfig->m_Element["PostBuildScriptInputs"];
						auto & OutputsElement = iConfig->m_Element["PostBuildScriptOutputs"];
						TCVector<CStr> Inputs = fg_StrSplit(InputsElement.f_GetValue(), ';');
						TCVector<CStr> Outputs = fg_StrSplit(OutputsElement.f_GetValue(), ';');
						nPostbuildInputs = fg_Max(Inputs.f_GetLen(), nPostbuildInputs);
						nPostbuildOutputs = fg_Max(Outputs.f_GetLen(), nPostbuildOutputs);
					}
					if (iConfig->m_Element.f_Exists("PreBuildScript"))
					{
						auto & InputsElement = iConfig->m_Element["PreBuildScriptInputs"];
						auto & OutputsElement = iConfig->m_Element["PreBuildScriptOutputs"];
						TCVector<CStr> Inputs = fg_StrSplit(InputsElement.f_GetValue(), ';');
						TCVector<CStr> Outputs = fg_StrSplit(OutputsElement.f_GetValue(), ';');
						nPrebuildInputs = fg_Max(Inputs.f_GetLen(), nPrebuildInputs);
						nPrebuildOutputs = fg_Max(Outputs.f_GetLen(), nPrebuildOutputs);
					}
				}

				for (mint iInput = 0; iInput < nPostbuildInputs; ++iInput)
					PostBuildScript.m_Inputs.f_Insert(fg_Format("$(PostBuildScriptInput{})", iInput));
				for (mint iOutput = 0; iOutput < nPostbuildOutputs; ++iOutput)
					PostBuildScript.m_Outputs.f_Insert(fg_Format("$(PostBuildScriptOutput{})", iOutput));
				
				for (mint iInput = 0; iInput < nPrebuildInputs; ++iInput)
					PreBuildScript.m_Inputs.f_Insert(fg_Format("$(PreBuildScriptInput{})", iInput));
				for (mint iOutput = 0; iOutput < nPrebuildOutputs; ++iOutput)
					PreBuildScript.m_Outputs.f_Insert(fg_Format("$(PreBuildScriptOutput{})", iOutput));
				
				for (auto iConfig = ThreadLocal.mp_EvaluatedTargetSettings.f_GetIterator(); iConfig; ++iConfig)
				{
					if (iConfig->m_Element.f_Exists("PostBuildScript"))
					{
						
						CElement const& BuildScript = iConfig->m_Element["PostBuildScript"];
						PostBuildScript.m_Name = BuildScript.m_Property;
						PostBuildScript.m_Script[iConfig.f_GetKey()] = BuildScript.f_GetValue();
						
						auto & InputsElement = iConfig->m_Element["PostBuildScriptInputs"];
						auto & OutputsElement = iConfig->m_Element["PostBuildScriptOutputs"];
						
						TCVector<CStr> Inputs = fg_StrSplit(InputsElement.f_GetValue(), ';');
						TCVector<CStr> Outputs = fg_StrSplit(OutputsElement.f_GetValue(), ';');
						
						{
							{
								mint iInput = 0;
								for (auto & Input : Inputs)
								{
									auto &Element = iConfig->m_Element[fg_Format("PostBuildScriptInput{}", iInput)];
									Element.m_Property = fg_Format("PostBuildScriptInput{}", iInput);
									Element.f_SetValue(Input);
									++iInput;
								}
								for (; !Inputs.f_IsEmpty() && iInput < nPostbuildInputs; ++iInput)
								{
									auto &Element = iConfig->m_Element[fg_Format("PostBuildScriptInput{}", iInput)];
									Element.m_Property = fg_Format("PostBuildScriptInput{}", iInput);
									Element.f_SetValue(Inputs[0]);
								}
							}
							{
								mint iOutput = 0;
								for (auto & Output : Outputs)
								{
									auto &Element = iConfig->m_Element[fg_Format("PostBuildScriptOutput{}", iOutput)];
									Element.m_Property = fg_Format("PostBuildScriptOutput{}", iOutput);
									Element.f_SetValue(Output);
									++iOutput;
								}
								for (; !Outputs.f_IsEmpty() && iOutput < nPostbuildOutputs; ++iOutput)
								{
									auto &Element = iConfig->m_Element[fg_Format("PostBuildScriptOutput{}", iOutput)];
									Element.m_Property = fg_Format("PostBuildScriptOutput{}", iOutput);
									Element.f_SetValue(Outputs[0]);
								}
							}
						}
						
						bFoundPostBuildScript = true;
						
					}
					if (iConfig->m_Element.f_Exists("PreBuildScript"))
					{

						CElement const& BuildScript = iConfig->m_Element["PreBuildScript"];
						PreBuildScript.m_Name = BuildScript.m_Property;
						PreBuildScript.m_Script[iConfig.f_GetKey()] = BuildScript.f_GetValue();

						auto & InputsElement = iConfig->m_Element["PreBuildScriptInputs"];
						auto & OutputsElement = iConfig->m_Element["PreBuildScriptOutputs"];
						
						TCVector<CStr> Inputs = fg_StrSplit(InputsElement.f_GetValue(), ';');
						TCVector<CStr> Outputs = fg_StrSplit(OutputsElement.f_GetValue(), ';');
						
						{
							{
								mint iInput = 0;
								for (auto & Input : Inputs)
								{
									auto &Element = iConfig->m_Element[fg_Format("PreBuildScriptInput{}", iInput)];
									Element.m_Property = fg_Format("PreBuildScriptInput{}", iInput);
									Element.f_SetValue(Input);
									++iInput;
								}
								for (; !Inputs.f_IsEmpty() && iInput < nPrebuildInputs; ++iInput)
								{
									auto &Element = iConfig->m_Element[fg_Format("PreBuildScriptInput{}", iInput)];
									Element.m_Property = fg_Format("PreBuildScriptInput{}", iInput);
									Element.f_SetValue(Inputs[0]);
								}
							}
							{
								mint iOutput = 0;
								for (auto & Output : Outputs)
								{
									auto &Element = iConfig->m_Element[fg_Format("PreBuildScriptOutput{}", iOutput)];
									Element.m_Property = fg_Format("PreBuildScriptOutput{}", iOutput);
									Element.f_SetValue(Output);
									++iOutput;
								}
								for (; !Outputs.f_IsEmpty() && iOutput < nPrebuildOutputs; ++iOutput)
								{
									auto &Element = iConfig->m_Element[fg_Format("PreBuildScriptOutput{}", iOutput)];
									Element.m_Property = fg_Format("PreBuildScriptOutput{}", iOutput);
									Element.f_SetValue(Outputs[0]);
								}
							}
						}
						
						bFoundPreBuildScript = true;
					}
				}

				if (bFoundPostBuildScript)
					_Project.m_NativeTarget.m_BuildScripts[PostBuildScript.m_Name] = fg_Move(PostBuildScript);
				if (bFoundPreBuildScript)
					_Project.m_NativeTarget.m_BuildScripts[PreBuildScript.m_Name] = fg_Move(PreBuildScript);
			}
		}

		void CGeneratorInstance::fp_EvaluateDependencies(CProject& _Project) const
		{
			for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
			{
				auto &Dependency = *iDependency;
				bool bInvalid = false;
				for (auto iConfig = Dependency.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					if (!_Project.m_EnabledProjectConfigs.f_FindEqual(iConfig.f_GetKey()))
					{
						bInvalid = true;
						break;
					}
				}
				for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					if (!Dependency.m_EnabledConfigs.f_FindEqual(iConfig.f_GetKey()))
					{
						bInvalid = true;
						break;
					}
				}
				if (bInvalid)
				{
					CStr Configs0;
					for (auto iConfig = Dependency.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
						fg_AddStrSep(Configs0, iConfig.f_GetKey().f_GetFullName(), "\n");
					CStr Configs1;
					for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
						fg_AddStrSep(Configs1, iConfig.f_GetKey().f_GetFullName(), "\n");
					m_BuildSystem.fs_ThrowError(Dependency.m_Position, fg_Format("Dependencies cannot be varied per configuration ({}):\n{}\n!=\n{}\n", _Project.f_GetName(), Configs0, Configs1));
				}

				CStr ProjectDependency = iDependency->f_GetName();
				auto pDependProject = _Project.m_pSolution->m_Projects.f_FindEqual(ProjectDependency);

				if (!pDependProject)
					m_BuildSystem.fs_ThrowError(Dependency.m_Position, CStr::CFormat("Dependency {} not found in workspace") << ProjectDependency);

				for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					if (!pDependProject->m_EnabledProjectConfigs.f_FindEqual(iConfig.f_GetKey()))
						m_BuildSystem.fs_ThrowError(
						Dependency.m_Position
						, CStr::CFormat("Dependency project does not have required configuration {} - {}") 
						<< iConfig.f_GetKey().m_Platform 
						<< iConfig.f_GetKey().m_Configuration
						);
				}

				TCVector<CStr> SearchLists;
				SearchLists.f_Insert("Dependency");

				TCMap<CConfiguration, CConfigResult> Entities;

				fp_SetEvaluatedValues(
					Dependency.m_EnabledConfigs
					, _Project.m_EnabledProjectConfigs
					, false
					, EPropertyType_Dependency
					, &SearchLists
					, nullptr
					, CStr()
					, false
					, false
					, Entities);

				fp_CalculateDependencyProductPath(*pDependProject, Dependency);
			}
		}

		void CGeneratorInstance::fp_AddExcludedFile(CConfiguration const &_Config, CProjectFile &_File) const
		{
			auto & ThreadLocal = *m_ThreadLocal;
			auto Mapped = ThreadLocal.mp_XcodeExcludedFileRefs[_Config][_File.f_GetFileRefGUID()];
		}

		CStr CGeneratorInstance::fp_MakeNiceSharedFlagValue(CStr const& _Type) const
		{
			return _Type.f_Replace("+", "P");
		}

		void CGeneratorInstance::fp_GenerateCompilerFlags(CProject& _Project) const
		{     
			auto & ThreadLocal = *m_ThreadLocal;
			
			auto fl_GetSharedFlags = [&] (CStr const& _Type, mint _SettingsNumber, bint _bKey) -> CStr
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
				TCMap<CStr, CConfigResult> MergedSharedSettings;
				TCMap<CStr, bool> PrefixHeadersEnabled;
				{
					for (auto iFlag = ThreadLocal.mp_EvaluatedTypeCompileFlags.f_GetIterator(); iFlag; ++iFlag)
					{
						CStr const& Type = iFlag.f_GetKey();
						CConfigResult const& ConfigData = (*iFlag)[Configuration];

						CStr TranslatedType = Type;
						
						auto MergedMap = MergedSharedSettings(TranslatedType);
						
						if (MergedMap.f_WasCreated())
							*MergedMap = ConfigData;
						else
							DError("Internal error, duplicate compiler type");
						{
							auto pPrecompilePrefixHeader = ConfigData.m_Element.f_FindEqual("GCC_PRECOMPILE_PREFIX_HEADER");
							if (pPrecompilePrefixHeader && pPrecompilePrefixHeader->f_GetValue() == "YES")
							{
								auto pPrefixHeader = ConfigData.m_Element.f_FindEqual("GCC_PREFIX_HEADER");
								if (!pPrefixHeader->f_GetValue().f_IsEmpty())
									PrefixHeadersEnabled[TranslatedType] = true;
							}
						}
					}
				}
				
				auto &OtherCPPFlags = ThreadLocal.mp_OtherCPPFlags[Configuration];
				auto &OtherCFlags = ThreadLocal.mp_OtherCFlags[Configuration];
				auto &OtherObjCPPFlags = ThreadLocal.mp_OtherObjCPPFlags[Configuration];
				auto &OtherObjCFlags = ThreadLocal.mp_OtherObjCFlags[Configuration];
				auto &OtherAssemblerFlags = ThreadLocal.mp_OtherAssemblerFlags[Configuration];
				auto &SettingsFromTypes = ThreadLocal.mp_XcodeSettingsFromTypes[Configuration];
				
				TCMap<CStr, zmint> SettingsNumber;
				TCMap<CStr, TCMap<CStr, mint>> GeneratedOverriddenFlags;
				
				TCMap<CStr, TCMap<CStr, CStr>> SharedFlags;
				TCMap<CStr, TCMap<CStr, TCSet<CStr>>> SharedFlagsSet;
				TCMap<CStr, TCSet<CStr>> NonSharedFlags;
				TCMap<CStr, TCMap<CStr, CElement const*>> GlobalNonShared;
				TCMap<CStr, TCMap<CStr, TCSet<CStr>>> NonSharedFlagsSet;
				TCMap<CStr, TCMap<CStr, CElement>> GlobalNonSharedSet;
				
				{
					
					for (auto iFile = _Project.m_Files.f_GetIterator(); iFile; ++iFile)
					{
						if (iFile->m_bHasCompilerFlags)
						{
							CStr TranslatedType = iFile->m_Type;
							
							CConfigResult const& ConfigData = ThreadLocal.mp_EvaluatedCompileFlags[iFile->f_GetBuildRefGUID()][Configuration];
							CConfigResult const& TypeConfigData = MergedSharedSettings[TranslatedType];

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
											m_BuildSystem.fs_ThrowError(iFlag->m_Position, "Inconsistent use of value set");
											
										for (auto &Value : pType->m_ValueSet)
										{
											if (!iFlag->m_ValueSet.f_FindEqual(Value))
											{
												if (bPrefixHeadersEnabled)
													m_BuildSystem.fs_ThrowError(iFlag->m_Position, "Per file settings cannot remove a compile flag when precompile prefix headers are enabled");
												else
												{
													NonSharedSet[iFlag->m_Property][Value];
													auto Mapped = GlobalNonSharedTypeSet(iFlag->m_Property);
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
											m_BuildSystem.fs_ThrowError(iFlag->m_Position, "Per file settings cannot have a different value for a compile flag when precompile prefix headers are enabled");
										else
										{
											NonShared[iFlag->m_Property];
											GlobalNonSharedType[iFlag->m_Property] = pType;
										}
										continue;											
									}
								}
								
								if (iFlag->m_bUseValues)
								{
									auto Mapped = FileSet(iFlag->m_Property);
									if (Mapped.f_WasCreated())
									{
										if (pType)
											*Mapped = pType->m_ValueSet;
									}
									
									{
										auto &Set = NonSharedSet[iFlag->m_Property];
										TCSet<CStr> ToRemove;
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
									auto Mapped = File(iFlag->m_Property);
									if (!Mapped.f_WasCreated())
									{
										if (*Mapped != iFlag->f_GetValue())
											NonShared[iFlag->m_Property];
									}
									else
										*Mapped = iFlag->f_GetValue();
								}
							}
						}
					}
				}

				auto fl_GenerateFlags = [&] (CConfigResult const& _ConfigData, CStr const& _FileType, bint _bFileType) -> CStr
				{
					CStr CompileFlags;

					auto fAddFlag = [&](CStr const &_Value)
						{
							if (_bFileType)
							{
								if (_FileType == "C++")
								{
									OtherCPPFlags += CStr::CFormat(" {}") << _Value;
									return;
								}
								else if (_FileType == "ObjC++")
								{
									OtherObjCPPFlags += CStr::CFormat(" {}") << _Value;
									return;
								}
								else if (_FileType == "C")
								{
									OtherCFlags += CStr::CFormat(" {}") << _Value;
									return;
								}
								else if (_FileType == "ObjC")
								{
									OtherObjCFlags += CStr::CFormat(" {}") << _Value;
									return;
								}
								else if (_FileType == "Assembler")
								{
									OtherAssemblerFlags += CStr::CFormat(" {}") << _Value;
									return;
								}
							}
							CompileFlags += (CStr::CFormat(" {}") << _Value);
						}
					;
					auto fl_BuildFlags = [&] (CElement const& _Element, TCSet<CStr> const *_pValueSet = nullptr)
					{
						if (_Element.m_Property == "GCC_PRECOMPILE_PREFIX_HEADER")
							return;
						else if (_Element.m_Property == "GCC_PREFIX_HEADER")
							return;

						if (_Element.m_bUseValues)
						{
							if (_pValueSet)
							{
								for (auto &Value : *_pValueSet)
									fAddFlag(Value);
							}
							else
							{
								for (auto &Value : _Element.m_ValueSet)
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
					
					CConfigResult const& TypeConfigData = MergedSharedSettings[TranslatedType];

					for (auto iElement = _ConfigData.m_Element.f_GetIterator(); iElement; ++iElement)
					{
						if (iElement->m_bXcodeProperty)
						{
							if (_bFileType)
							{
								if (iElement->m_Property == "GCC_PRECOMPILE_PREFIX_HEADER" || iElement->m_Property == "GCC_PREFIX_HEADER")
								{									
									if (SettingsFromTypes.f_Exists(iElement->m_Property))
									{
										if (iElement->f_GetValue() != SettingsFromTypes[iElement->m_Property])
											m_BuildSystem.fs_ThrowError(_Project.m_ProjectPosition, "Prefix headers and PrecompilePrefixHeaders per file type are not supported");
									}
								}
								if (iElement->m_bUseValues)
								{
									CStr Values;
									for (auto &Value : iElement->m_ValueSet)
										fg_AddStrSep(Values, Value, " ");
									SettingsFromTypes[iElement->m_Property] = Values;
								}
								else
									SettingsFromTypes[iElement->m_Property] = iElement->f_GetValue();
							}

							continue;
						}

						if (_bFileType)
						{
							if (iElement->m_bUseValues)
							{
								auto pNonShared = NonSharedSet.f_FindEqual(iElement->m_Property);
								TCSet<CStr> ToSet;
								TCSet<CStr> *pToSet = nullptr;
								if (pNonShared)
								{
									for (auto &Value : iElement->m_ValueSet)
									{
										if (!pNonShared->f_FindEqual(Value))
											ToSet[Value];
									}
									pToSet = &ToSet;
								}

								fl_BuildFlags(*iElement, pToSet);
								continue;
							}
							else
							{
								if (NonShared.f_FindEqual(iElement->m_Property))
									continue;
							}
						}
						else
						{
							if (iElement->m_bUseValues)
							{
								if (TypeConfigData.m_Element.f_FindEqual(iElement.f_GetKey()))
								{
									auto pNonShared = NonSharedSet.f_FindEqual(iElement->m_Property);
									TCSet<CStr> ToSet;
									if (pNonShared)
									{
										for (auto &Value : iElement->m_ValueSet)
										{
											if (pNonShared->f_FindEqual(Value))
												ToSet[Value];
										}
									}

									fl_BuildFlags(*iElement, &ToSet);
									continue;
								}
							}
							else
							{
								if (!NonShared.f_FindEqual(iElement->m_Property) && TypeConfigData.m_Element.f_FindEqual(iElement.f_GetKey()))
									continue;
							}
						}

						fl_BuildFlags(*iElement);
					}
					
					if (!_bFileType)
					{
						auto const &GlobalNonSharedType = GlobalNonShared[TranslatedType];
						for (auto iNonShared = GlobalNonSharedType.f_GetIterator(); iNonShared; ++iNonShared)
						{
							if (!_ConfigData.m_Element.f_FindEqual(iNonShared.f_GetKey()))
							{
								fl_BuildFlags(**iNonShared);
							}
						}
						auto const &GlobalNonSharedTypeSet = GlobalNonSharedSet[TranslatedType];
						for (auto iNonShared = GlobalNonSharedTypeSet.f_GetIterator(); iNonShared; ++iNonShared)
						{
							if (!_ConfigData.m_Element.f_FindEqual(iNonShared.f_GetKey()))
							{
								fl_BuildFlags(*iNonShared);
							}
						}
					}

					return CompileFlags;
				};

				// Compiler flags per file
				{
					auto &ExcludedFileRefs = ThreadLocal.mp_XcodeExcludedFileRefs[Configuration];
					for (auto iFile = _Project.m_Files.f_GetIterator(); iFile; ++iFile)
					{
						if (!iFile->m_EnabledConfigs.f_Exists(Configuration))
							fp_AddExcludedFile(Configuration, *iFile);
						else if (iFile->m_bHasCompilerFlags)
						{
							CConfigResult const& ConfigData = ThreadLocal.mp_EvaluatedCompileFlags[iFile->f_GetBuildRefGUID()][Configuration];
							
							// Generate compiler flags for this file for any properties that exist in the overridden properties map (take them from
							// the shared flags if they do not override them)
							CStr FlagsForFile = fl_GenerateFlags(ConfigData, iFile->m_Type, false);
							if (!FlagsForFile.f_IsEmpty())
							{
								auto & OverriddenType = GeneratedOverriddenFlags[iFile->m_Type];
								auto MappedOveridden = OverriddenType(FlagsForFile);
								if (MappedOveridden.f_WasCreated())
									*MappedOveridden = ++SettingsNumber[iFile->m_Type];

								ThreadLocal.mp_CompileFlagsValues[iFile->f_GetCompileFlagsGUID()][Configuration] = fl_GetSharedFlags(iFile->m_Type, *MappedOveridden, false);
							}			

							// Some special flags
							{
								for (auto iFlag = ConfigData.m_Element.f_GetIterator(); iFlag; ++iFlag)
								{
									if (iFlag->m_bXcodeProperty)
									{
										if (iFlag->m_Property == "GCC_PRECOMPILE_PREFIX_HEADER" || iFlag->m_Property == "GCC_PREFIX_HEADER")
										{
											if (SettingsFromTypes.f_Exists(iFlag->m_Property))
											{
												if (iFlag->f_GetValue() != SettingsFromTypes[iFlag->m_Property])
													m_BuildSystem.fs_ThrowError(iFile->m_Position, "Prefix headers and PrecompilePrefixHeaders per file type are not supported");
											}
											
											SettingsFromTypes[iFlag->m_Property] = iFlag->f_GetValue();
										}
										else if (iFlag->m_Property == "MOC_OUTPUT_PATTERN")
										{
											if (iFlag->f_GetValue().f_Find(CFile::fs_GetFile(iFile->f_GetName())) > -1)
												m_BuildSystem.fs_ThrowError(iFile->m_Position, "Moc output pattern settings per file are not supported");
											else if (CFile::fs_GetExtension(iFile->f_GetName()) == "cpp")
											{
												if (!ThreadLocal.mp_MocOutputPatternCPP.f_IsEmpty() && ThreadLocal.mp_MocOutputPatternCPP != iFlag->f_GetValue())
													m_BuildSystem.fs_ThrowError(iFile->m_Position, "Moc output pattern settings per file are not supported");
												else if (ThreadLocal.mp_MocOutputPatternCPP.f_IsEmpty())
													ThreadLocal.mp_MocOutputPatternCPP = iFlag->f_GetValue();
											}
											else
											{
												ThreadLocal.mp_EvaluatedTypeCompileFlags[iFile->m_Type][Configuration].m_Element["MOC_OUTPUT_PATTERN"].f_SetValue(iFlag->f_GetValue());
											}
										}
										else if (iFlag->m_Property == "FilesExcludedFromCompile")
										{
											if (iFlag->f_GetValue() == "true" && iFile->m_LastKnownFileType.f_Find("nolink") == -1)
												iFile->m_LastKnownFileType += "nolink";
										}
										else if (iFlag->m_Property == "EXCLUDED_FILE_REFS")
										{
											if (iFlag->f_GetValue() == "true")
												ExcludedFileRefs[iFile->f_GetFileRefGUID()];
										}
										else
										{
											//DMibTrace("Excluded {} = {}\n", iFlag->m_Property << iFlag->m_Value);
										}
									}
								}
							}
						}

					}
				}

				// Store overridden flags
				{
					auto &EvaluatedOverriddenCompileFlags = ThreadLocal.mp_EvaluatedOverriddenCompileFlags[Configuration];
					for (auto iFileType = GeneratedOverriddenFlags.f_GetIterator(); iFileType; ++iFileType)
					{
						for (auto iGeneratedOverriddenFlags = iFileType->f_GetIterator(); iGeneratedOverriddenFlags; ++iGeneratedOverriddenFlags)
						{
							CStr Key = fl_GetSharedFlags(iFileType.f_GetKey(), (*iGeneratedOverriddenFlags), true);
							EvaluatedOverriddenCompileFlags[Key] = iGeneratedOverriddenFlags.f_GetKey();
						}
					}
				}

				// Base compile flags per file type
				{
					for (auto iFlag = ThreadLocal.mp_EvaluatedTypeCompileFlags.f_GetIterator(); iFlag; ++iFlag)
					{
						CStr const& Type = iFlag.f_GetKey();
						CConfigResult const& ConfigData = (*iFlag)[Configuration];
						
						// Generate any flags that do not appear in the overridden properties map
						CStr CompileFlags = fl_GenerateFlags(ConfigData, Type, true);
						CStr Flags = fl_GetSharedFlags(Type, 0, true);
						ThreadLocal.mp_CompileFlagsValues[Flags][Configuration] = CompileFlags;
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
						if (ThreadLocal.mp_CompileFlagsValues.f_Exists(iFile->f_GetCompileFlagsGUID()))
						{
							TCMap<CConfiguration, CStr>& SpecificSettings = ThreadLocal.mp_CompileFlagsValues[iFile->f_GetCompileFlagsGUID()];
							SpecificSettings[Configuration];
						}
					}
				}
			}
		}

		void CGeneratorInstance::fp_GenerateBuildConfigurationScriptFile(CProject& _Project, CConfiguration const& _Configuration, CStr const& _OutputFile, CStr const& _OutputDir, CStr const &_Contents) const
		{
			CStr FileData = _Contents.f_Replace("\r\n", "\n");

			{
				bool bWasCreated;
				if (!m_BuildSystem.f_AddGeneratedFile(_OutputFile, FileData, _Project.m_pSolution->f_GetName(), bWasCreated, false))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << _OutputFile));

				if (bWasCreated)
				{
					TCVector<uint8> FileDataVector;
					CFile::fs_WriteStringToVector(FileDataVector, CStr(FileData), false);
					m_BuildSystem.f_WriteFile(FileDataVector, _OutputFile, EFileAttrib_Executable);
				}
			}
		}

		struct CLinkGroup
		{
			DLinkDS_Link(CLinkGroup, m_Link);
			TCLinkedList<CProjectDependency::CPerConfig *> m_Configs;
			CStr m_Name;
		};

		void CGeneratorInstance::fp_GenerateBuildConfigurationFile(CProject& _Project, CConfiguration const& _Configuration, CStr const& _OutputFile, CStr const& _OutputDir) const
		{
			auto &ThreadLocal = *m_ThreadLocal;
			CStr FileData;

			FileData += "ESCSLASH = /\n";
			
			auto fl_EscVar
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
				FileData += (CStr::CFormat("OTHER_CFLAGS_ONLY = {}\n") << fl_EscVar(ThreadLocal.mp_OtherCFlags[_Configuration]));
				FileData += (CStr::CFormat("OTHER_OBJCFLAGS_ONLY = {}\n") << fl_EscVar(ThreadLocal.mp_OtherObjCFlags[_Configuration]));
				FileData += (CStr::CFormat("OTHER_CPLUSPLUSFLAGS_ONLY = {}\n") << fl_EscVar(ThreadLocal.mp_OtherCPPFlags[_Configuration]));
				FileData += (CStr::CFormat("OTHER_OBJCPLUSPLUSFLAGS_ONLY = {}\n") << fl_EscVar(ThreadLocal.mp_OtherObjCPPFlags[_Configuration]));
				FileData += (CStr::CFormat("OTHER_ASSEMBLERFLAGS_ONLY = {}\n") << fl_EscVar(ThreadLocal.mp_OtherAssemblerFlags[_Configuration]));
				FileData += (CStr::CFormat("MOC_OUTPUT_PATTERN_CPP = {}\n") << fl_EscVar(ThreadLocal.mp_MocOutputPatternCPP));
				FileData += "MOC_OUTPUT_PATTERN_NOLINK = $(MOC_OUTPUT_PATTERN)\nMOC_OUTPUT_PATTERN_CPP_NOLINK = $(MOC_OUTPUT_PATTERN_CPP)\n";
			}

			// Overridden flags
			{
				for (auto iFlag = ThreadLocal.mp_EvaluatedOverriddenCompileFlags[_Configuration].f_GetIterator(); iFlag; ++iFlag)
					FileData += (CStr::CFormat("{} = {}\n") << iFlag.f_GetKey() << fl_EscVar(*iFlag));
			}

			// Compile flags
			{
				for (auto iFlag = ThreadLocal.mp_CompileFlagsValues.f_GetIterator(); iFlag; ++iFlag)
					FileData += (CStr::CFormat("{} = {}\n") << iFlag.f_GetKey() << fl_EscVar((*iFlag)[_Configuration]));
			}

			// Xcode settings
			{
				for (auto ISetting = ThreadLocal.mp_XcodeSettingsFromTypes[_Configuration].f_GetIterator(); ISetting; ++ISetting)
					FileData += CStr::CFormat("{} = {}\n") << ISetting.f_GetKey() << fl_EscVar(*ISetting);
			}
			
			// Excluded files
			{
				FileData += "EXCLUDED_FILE_REFS =";
				for (auto &FileRef : ThreadLocal.mp_XcodeExcludedFileRefs[_Configuration])
					FileData += CStr::CFormat(" {}") << FileRef;
				FileData += "\n";
			}

			// MLSRCROOT
			{
				FileData += (CStr::CFormat("MLSRCROOT = {}\n") << fl_EscVar(m_RelativeBasePathAbsolute));
			}

			bint bDoneAdditionalLibraries = false;
//				bint bDoneSearchPaths = false;

			// Per configuration target settings
			{
				CConfigResult& ConfigData = ThreadLocal.mp_EvaluatedTargetSettings[_Configuration];
				for (auto iElement = ConfigData.m_Element.f_GetIterator(); iElement; ++iElement)
				{
					bint bSearchPaths = false;
					bint bAdditionalLibraries = false;

					if (iElement->m_Property == "LIBRARY_SEARCH_PATHS")
					{
						CStr NewValue;
						CStr Value = iElement->f_GetValue();
						while(!Value.f_IsEmpty())
						{
							CStr ThisValue = fg_GetStrSep(Value, ";");
							if (ThisValue.f_IsEmpty())
								continue;
							NewValue += CStr::CFormat("\"{}\" ") << ThisValue;
						}
						iElement->f_SetValue(NewValue);
						
						if (NewValue.f_IsEmpty())
							continue;

						bSearchPaths = true;
					}
					else if (!bDoneAdditionalLibraries && iElement->m_Property == "OTHER_LDFLAGS")
						bAdditionalLibraries = true;
					else if (!bDoneAdditionalLibraries && iElement->m_Property == "OTHER_LIBTOOLFLAGS" && m_XcodeVersion >= 6)
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
						Extra = "$DependencyLibraries ";
						ExtraAfter = " $DependencyLibraries";
					}

					if (auto pBuildScript = _Project.m_NativeTarget.m_BuildScripts.f_FindEqual(iElement->m_Property))
					{
						CStr ScriptName = _OutputFile + "." + iElement->m_Property + ".sh";
						pBuildScript->m_ScriptNames[_Configuration] = ScriptName;
						fp_GenerateBuildConfigurationScriptFile(_Project, _Configuration, ScriptName, _OutputDir, iElement->f_GetValue());
						FileData += (CStr::CFormat("{} = {}\n") << iElement->m_Property << ScriptName);
					}
					else if (!iElement->m_Property.f_IsEmpty())
					{
						iElement->f_SetValue(iElement->f_GetValue().f_Replace("\r\n", "\n").f_Replace("\n", "\\n"));
						FileData += (CStr::CFormat("{} = {}{}{}\n") << iElement->m_Property << Extra << fl_EscVar(iElement->f_GetValue()) << ExtraAfter);
					}
				}
			}

			// Project dependencies
			{
				CStr LinkSearchPaths;
				CStr Link;
				
				DLinkDS_List(CLinkGroup, m_Link) LinkerGroupsOrdered;
				TCMap<CStr, CLinkGroup> LinkerGroups;
				TCLinkedList<CLinkGroup> NonLinkerGroups;
				for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
				{
					auto &PerConfig = iDependency->m_PerConfig[_Configuration];
					
					if (PerConfig.m_bLink)
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
						Group.m_Configs.f_Insert(&PerConfig);
						LinkSearchPaths += (CStr::CFormat("\"{}\" ") << PerConfig.m_SearchPath);
					}
				}
				
				for (auto iLinkerGroup = LinkerGroupsOrdered.f_GetIterator(); iLinkerGroup; ++iLinkerGroup)
				{
					if (!iLinkerGroup->m_Name.f_IsEmpty())
						Link += "-Xlinker -( ";
					for (auto iDependency = iLinkerGroup->m_Configs.f_GetIterator(); iDependency; ++iDependency)
						Link += (CStr::CFormat("\"{}/{}\" ") << (*iDependency)->m_SearchPath << (*iDependency)->m_CalculatedPath);
					if (!iLinkerGroup->m_Name.f_IsEmpty())
						Link += "-Xlinker -) ";
				}

				//FileData += (CStr::CFormat("DependencySearchPaths = {}\n") << fl_EscVar(LinkSearchPaths));
				FileData += (CStr::CFormat("DependencyLibraries = {}\n") << fl_EscVar(Link));

				if (!bDoneAdditionalLibraries)
				{
					if (m_XcodeVersion >= 6)
						FileData += "OTHER_LIBTOOLFLAGS = $DependencyLibraries\n";
					FileData += "OTHER_LDFLAGS = $DependencyLibraries\n";
				}
				//if (!bDoneSearchPaths)
				//	FileData += "LIBRARY_SEARCH_PATHS = $DependencySearchPaths\n";
			}

			{
				bool bWasCreated;
				if (!m_BuildSystem.f_AddGeneratedFile(_OutputFile, FileData, _Project.m_pSolution->f_GetName(), bWasCreated, false))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << _OutputFile));

				if (bWasCreated)
				{
					TCVector<uint8> FileDataVector;
					CFile::fs_WriteStringToVector(FileDataVector, CStr(FileData), false);
					m_BuildSystem.f_WriteFile(FileDataVector, _OutputFile);
				}
			}
		}
	}
}
