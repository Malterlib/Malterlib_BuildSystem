// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <AOCC/AOXMLUtils.h>

namespace NMib::NBuildSystem::NXcode
{
	void CGeneratorInstance::fp_SetEvaluatedValues
		(
			TCMap<CConfiguration, CEntityPointer> const &_Configs
			, TCMap<CConfiguration, CEntityPointer> const &_AllConfigs
			, bool _bFile
			, EPropertyType _PropertyType
			, TCVector<CStr> const *_pSearchList
			, TCMap<CConfiguration, TCVector<CStr>> const *_pSearchListPerConfig
			, CStr const &_DefaultEntity
			, bool _bPropertyCondition
			, bool _bAddPropertyDefined
			, TCMap<CConfiguration, CGeneratorInstance::CConfigResult>& _Result
		) const
	{
		struct CPropretyEval
		{
			TCMap<CConfiguration, CEntity const *> m_Configs;
			CFilePosition m_Position;
		};
		CPropertyKey FullEvalKey;
		FullEvalKey.m_Type = _PropertyType;
		FullEvalKey.m_Name = "FullEval";
		TCMap<CPropertyKey, CPropretyEval> Properties;
		for (auto iConfig = _Configs.f_GetIterator(); iConfig; ++iConfig)
		{
			auto *pConfig = iConfig->f_Get();
			auto pTopConfig = pConfig;
			bool bBelowFileLevel = false;
			bool bFullFileSettings = false;
			if (_bFile)
			{
				auto *pConfig = iConfig->f_Get();
				while (pConfig)
				{
					if (pConfig->m_PotentialExplicitProperties.f_FindEqual(FullEvalKey))
					{
						CProperty const *pFromProperty = nullptr;
						CStr Value = m_BuildSystem.f_EvaluateEntityProperty(*pTopConfig, _PropertyType, FullEvalKey.m_Name, pFromProperty);
						bFullFileSettings = Value == "true";
						break;
					}
					
					pConfig = pConfig->m_pParent;
				}
			}
			
			while (pConfig)
			{
				if (_bFile && !bBelowFileLevel)
				{
					if 
						(
							pConfig->m_Key.m_Type != EEntityType_Group 
							&& pConfig->m_Key.m_Type != EEntityType_GenerateFile 
							&& pConfig->m_Key.m_Type != EEntityType_File
						)
					{
						bBelowFileLevel = true;
					}
				}
				auto iProperty = pConfig->m_PotentialExplicitProperties.f_GetIterator();
				if (bBelowFileLevel && !bFullFileSettings)
					iProperty = pConfig->m_PerFilePotentialExplicitProperties.f_GetIterator();

				for (; iProperty; ++iProperty)
				{
					auto Type = iProperty.f_GetKey().m_Type;

					if (Type != _PropertyType)
						continue;

					if (bBelowFileLevel)
					{
						bool bFoundProperty = false;
						for (auto iSource = iProperty->f_GetIterator(); iSource; ++iSource)
						{
							if (m_BuildSystem.f_EvalCondition(*pTopConfig, (*iSource)->m_Condition))
							{
								bFoundProperty = true;
								break;
							}
						}
						if (!bFoundProperty)
							continue;
					}

					CProperty const *pFromProperty = nullptr;
					m_BuildSystem.f_EvaluateEntityProperty(*pTopConfig, _PropertyType, iProperty.f_GetKey().m_Name, pFromProperty);
					
					if (pFromProperty == nullptr)
						continue; // This means that the property is not valid at the top level

					auto &Eval = Properties[iProperty.f_GetKey()];
					auto MapConfig = Eval.m_Configs(iConfig.f_GetKey());
					if (MapConfig.f_WasCreated())
					{
						if (!iProperty->f_IsEmpty())
							Eval.m_Position = ((*iProperty)[0])->m_Position;
						MapConfig.f_GetResult() = iConfig->f_Get();
					}
				}
				pConfig = pConfig->m_pParent;
			}
		}

		for (auto iProperty = Properties.f_GetIterator(); iProperty; ++iProperty)
		{
			fp_GetConfigValue(
				iProperty->m_Configs
				, _Configs
				, iProperty->m_Position
				, iProperty.f_GetKey().m_Type
				, iProperty.f_GetKey().m_Name
				, false
				, false
				, _pSearchList
				, _pSearchListPerConfig
				, _DefaultEntity
				, CStr()
				, _Result);
		}
	}

	CGeneratorInstance::CSingleValue CGeneratorInstance::fp_GetSingleConfigValue
		(
			TCMap<CConfiguration, CEntityPointer> const &_Configs
			, EPropertyType _PropType
			, CStr const &_Property
		) const
	{
		CProperty const *pSavedFromProperty = nullptr;
		
		bool bFirst = true;
		
		CGeneratorInstance::CSingleValue Ret;
		
		for (auto iConfig = _Configs.f_GetIterator(); iConfig; ++iConfig)
		{
			CProperty const *pFromProperty = nullptr;
			CStr Value = m_BuildSystem.f_EvaluateEntityProperty(**iConfig, _PropType, _Property, pFromProperty);
			
			if (bFirst)
			{
				bFirst = false;
				Ret.m_Value = Value;
				if (pFromProperty)
					Ret.m_Position = pFromProperty->m_Position;
				else
					Ret.m_Position = (*iConfig)->m_Position;
				pSavedFromProperty = pFromProperty;
			}
			else
			{
				if (Value != Ret.m_Value)
				{
					CFilePosition Position;
					if (pFromProperty)
						Position = pFromProperty->m_Position;
					else
						Position = (*iConfig)->m_Position;
					TCVector<CBuildSystemError> OtherErrors;
					CBuildSystemError &Error = OtherErrors.f_Insert();
					Error.m_Position = Ret.m_Position;
					Error.m_Error = CStr::CFormat("note: Differs from: {}") << Ret.m_Value;
					m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Property value cannot be varied per configuration: {}") << Value << Ret.m_Value, OtherErrors);
				}
			}
		}
		
		return Ret;
	}
	
	void CGeneratorInstance::fp_GetConfigValue
		(
			TCMap<CConfiguration, CEntityPointer> const &_Configs
			, TCMap<CConfiguration, CEntityPointer> const &_AllConfigs
			, CFilePosition const &_Position
			, EPropertyType _PropType
			, CStr const &_SourceType
			, bint _bFile
			, bool _bExcludeFromBuildCondition
			, TCVector<CStr> const *_pSearchList
			, TCMap<CConfiguration, TCVector<CStr>> const *_pSearchListPerConfig
			, CStr const &_DefaultEntity
			, CStr const &_ExtraCondition
			, TCMap<CConfiguration, CGeneratorInstance::CConfigResult>& _Result
		) const
	{
		auto Position = _Position;
		if (Position.m_FileName.f_IsEmpty())
			Position = m_pGeneratorSettings->m_Position;

		CValueProperties SingleProperties;

		auto fl_GetValueProperties
			= [&](CConfiguration const &_Configuration) -> CValueProperties
			{
				CEntityKey Key;
				Key.m_Type = EEntityType_GeneratorSetting;
				TCVector<CStr> const *pSearchList = _pSearchList;
				if (_pSearchListPerConfig)
				{
					pSearchList = _pSearchListPerConfig->f_FindEqual(_Configuration);
					DMibCheck(pSearchList);
				}
				CEntity const *pSettings;
				zbool bDisabled;
				{
					for (auto iSearch = pSearchList->f_GetIterator(); iSearch; ++iSearch)
					{
						Key.m_Name = *iSearch;
						pSettings = m_pGeneratorSettings->m_ChildEntitiesMap.f_FindEqual(Key);
						if (!pSettings)
							continue;

						CPropertyKey PropertyKey;
						PropertyKey.m_Type = EPropertyType_Property;
						PropertyKey.m_Name = "Disabled";
						if (pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey))
						{
							bDisabled = true;
							break;
						}

						Key.m_Name = fg_PropertyTypeToStr(_PropType);
						pSettings = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
						if (!pSettings)
							continue;

						Key.m_Name = _SourceType;
						pSettings = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
						if (!pSettings)
							continue;
						else
							break;
					}

					if (bDisabled)
					{
						CValueProperties Ret;
						Ret.m_bDisabled = true;
						return Ret;
					}

					if (!pSettings)
					{
						CStr FindIn;
						for (auto iSearch = pSearchList->f_GetIterator(); iSearch; ++iSearch)
							fg_AddStrSep(FindIn, *iSearch, ", ");
						m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("No generator setting for property '{}' in {}") << _SourceType << FindIn);

						CValueProperties Ret;
						Ret.m_bDisabled = true;
						return Ret;

					}
				}

				CValueProperties Ret;

				CPropertyKey PropertyKey;
				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Disabled";
				if (pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey))
					Ret.m_bDisabled = true;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Name";
				Ret.m_pTranslatedPropertyName = pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey);

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Substitute";
				auto pSubstitute = pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey);
				if (pSubstitute)
					Ret.m_Substitute = pSubstitute->m_Value;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "ConvertSeperator";
				if (pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey))
					Ret.m_bConvertSeperator = true;
				
				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "IgnoreEmpty";
				if (pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey))
					Ret.m_bIgnoreEmtpy = true;
				
				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "RemoveLastSlash";
				if (pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey))
					Ret.m_bRemoveLastSlash = true;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Prefix";
				auto pPrefix = pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey);
				if (pPrefix)
					Ret.m_Prefix = pPrefix->m_Value;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "QuoteSeperatedValues";
				if (pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey))
					Ret.m_bQuoteSeperatedValues = true;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "QuoteAfterEquals";
				if (pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey))
					Ret.m_bQuoteAfterEquals = true;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Seperator";
				auto pSeperator = pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey);
				if (pSeperator)
					Ret.m_Seperator = pSeperator->m_Value;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "OldSeperator";
				auto pOldSeperator = pSettings->m_EvaluatedProperties.f_FindEqual(PropertyKey);
				if (pOldSeperator)
					Ret.m_OldSeperator = pOldSeperator->m_Value;

				Key.m_Name = "Value";
				auto pTranslators = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
				if (pTranslators)
					Ret.m_pTranslators = fg_Explicit(pTranslators);

				Key.m_Name = "ValueSet";
				auto pValueSet = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
				if (pValueSet)
					Ret.m_pValueSet = fg_Explicit(pValueSet);

				Key.m_Name = "Properties";
				auto pProperties = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
				if (pProperties)
					Ret.m_pProperties = fg_Explicit(pProperties);

				return Ret;
			}
		;

		if (_pSearchList)
		{
			SingleProperties = fl_GetValueProperties(CConfiguration());
			if (!_bFile && SingleProperties.m_bDisabled)
				return;
		}

		TCMap<TCSet<CConfigValue>, CValueConfigs> ConfigOptions;
		auto fl_AddConfig
			= [&](CConfiguration const &_Config, CEntity const &_Entity)
			{
				CStr Value = m_BuildSystem.f_EvaluateEntityProperty(_Entity, _PropType, _SourceType);

				CConfigValue ConfigValue;
				CValueProperties PropertiesValue;

				if (_pSearchList)
					PropertiesValue = SingleProperties;
				else
					PropertiesValue = fl_GetValueProperties(_Config);

				if (PropertiesValue.m_bDisabled && !_bFile)
					return;

				ConfigValue.m_Entity = _DefaultEntity;

				if (PropertiesValue.m_pTranslatedPropertyName)
				{
					ConfigValue.m_Property = PropertiesValue.m_pTranslatedPropertyName->m_Value;
					ConfigValue.m_bXcodeProperty = true;
				}
				else
					ConfigValue.m_Property = _SourceType;

				ConfigValue.m_Value = Value;

				if (PropertiesValue.m_pTranslators)
				{
					CPropertyKey PropertyKey;
					PropertyKey.m_Type = EPropertyType_Property;
					PropertyKey.m_Name = Value;
					auto pVSProperty = PropertiesValue.m_pTranslators->m_EvaluatedProperties.f_FindEqual(PropertyKey);

					if (!pVSProperty)
						m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("No translated property found for value {}") << Value);
					ConfigValue.m_Value = pVSProperty->m_Value;
				}

				if (PropertiesValue.m_bRemoveLastSlash)
				{
					if (!ConfigValue.m_Value.f_IsEmpty() && ConfigValue.m_Value[ConfigValue.m_Value.f_GetLen() - 1] == '/')
						ConfigValue.m_Value = ConfigValue.m_Value.f_Left(ConfigValue.m_Value.f_GetLen() - 1);
				}
				
				if (_SourceType == "FileExtension")
					ConfigValue.m_Value = ConfigValue.m_Value.f_Replace(".", "");
				else if (_SourceType == "ToolPath")
					ConfigValue.m_Value = ConfigValue.m_Value.f_Replace(";", ":");

				if (!PropertiesValue.m_Substitute.f_IsEmpty())
				{
					if (_SourceType == "PostBuildScript" || _SourceType == "PreBuildScript")
						PropertiesValue.m_Substitute = PropertiesValue.m_Substitute.f_Replace(";", ":");

					CStr NewValue = CStr::CFormat(PropertiesValue.m_Substitute) << ConfigValue.m_Value;
					ConfigValue.m_Value = NewValue;
				}

				if (PropertiesValue.m_bConvertSeperator &&
					PropertiesValue.m_OldSeperator != "" &&
					PropertiesValue.m_Seperator != "")
				{
					if (PropertiesValue.m_Seperator == " ")
					{
						ConfigValue.m_bUseValues = true;
						while(!ConfigValue.m_Value.f_IsEmpty())
						{
							CStr Value = fg_GetStrSep(ConfigValue.m_Value, PropertiesValue.m_OldSeperator);
							if (PropertiesValue.m_bIgnoreEmtpy && Value.f_IsEmpty())
								continue;

							if (PropertiesValue.m_bQuoteAfterEquals && (Value.f_Find(" ") > -1 || Value.f_Find("\"") > -1))
							{
								Value = Value.f_Replace("\"", "");
								CStr NotQuoted = fg_GetStrSep(Value, "=");
								Value = (CStr::CFormat("{}=\"\\\"{}\\\"\"") << NotQuoted << Value).f_GetStr();
							}

							if (PropertiesValue.m_bQuoteSeperatedValues)
								ConfigValue.m_Values.f_Insert(fg_Format("{}{}\"{}\"", PropertiesValue.m_Seperator, PropertiesValue.m_Prefix, Value));
							else
								ConfigValue.m_Values.f_Insert(fg_Format("{}{}{}", PropertiesValue.m_Seperator, PropertiesValue.m_Prefix, Value));
						}
					}
					else
					{
						CStr NewValue;
						while(!ConfigValue.m_Value.f_IsEmpty())
						{
							CStr Value = fg_GetStrSep(ConfigValue.m_Value, PropertiesValue.m_OldSeperator);
							if (PropertiesValue.m_bIgnoreEmtpy && Value.f_IsEmpty())
								continue;

							if (PropertiesValue.m_bQuoteAfterEquals && (Value.f_Find(" ") > -1 || Value.f_Find("\"") > -1))
							{
								Value = Value.f_Replace("\"", "");
								CStr NotQuoted = fg_GetStrSep(Value, "=");
								Value = (CStr::CFormat("{}=\"\\\"{}\\\"\"") << NotQuoted << Value).f_GetStr();
							}

							if (PropertiesValue.m_bQuoteSeperatedValues)
								NewValue += (CStr::CFormat("{}{}\"{}\"") << PropertiesValue.m_Seperator << PropertiesValue.m_Prefix << Value);
							else
								NewValue += (CStr::CFormat("{}{}{}") << PropertiesValue.m_Seperator << PropertiesValue.m_Prefix << Value);
						}
						ConfigValue.m_Value = NewValue;
					}
				}

				CStr OriginalValue = ConfigValue.m_Value;

				TCSet<CConfigValue> ConfigValues;

				if (ConfigValue.m_Property.f_IsEmpty())
					ConfigValue.m_bMainValue = false;
				else
				{
					ConfigValue.m_bMainValue = true;
					ConfigValues[ConfigValue];
				}

				if (PropertiesValue.m_pProperties)
				{
					for (auto iParent = PropertiesValue.m_pProperties->m_ChildEntitiesOrdered.f_GetIterator(); iParent; ++iParent)
					{
						for (auto iEntity = iParent->m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
						{
							for (auto iProperty = iEntity->m_ChildEntitiesOrdered.f_GetIterator(); iProperty; ++iProperty)
							{
								CConfigValue ExtraConfigValue = ConfigValue;
								if (!ConfigValue.m_bMainValue)
								{
									ConfigValue.m_bMainValue = true;
									ExtraConfigValue.m_bMainValue = true;
								}
								else
									ExtraConfigValue.m_bMainValue = false;
								ExtraConfigValue.m_Parent = iParent->m_Key.m_Name;
								ExtraConfigValue.m_Entity = iEntity->m_Key.m_Name;
								ExtraConfigValue.m_bXcodeProperty = ConfigValue.m_bXcodeProperty;
								if (ExtraConfigValue.m_Entity.f_IsEmpty())
									ExtraConfigValue.m_Entity = _DefaultEntity;
								ExtraConfigValue.m_Property = iProperty->m_Key.m_Name;
								ExtraConfigValue.m_Value = OriginalValue;
								ConfigValues[ExtraConfigValue];
							}
						}
					}
				}
				if (PropertiesValue.m_pValueSet)
				{
					CEntityKey Key;
					Key.m_Type = EEntityType_GeneratorSetting;
					Key.m_Name = Value;
					auto pToSet = PropertiesValue.m_pValueSet->m_ChildEntitiesMap.f_FindEqual(Key);
					if (pToSet)
					{
						for (auto iParent = pToSet->m_ChildEntitiesOrdered.f_GetIterator(); iParent; ++iParent)
						{
							for (auto iEntity = iParent->m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
							{
								for (auto iProperty = iEntity->m_EvaluatedProperties.f_GetIterator(); iProperty; ++iProperty)
								{
									CConfigValue ExtraConfigValue;
									if (!ConfigValue.m_bMainValue)
									{
										ConfigValue.m_bMainValue = true;
										ExtraConfigValue.m_bMainValue = true;
									}
									else
										ExtraConfigValue.m_bMainValue = false;
									ExtraConfigValue.m_Parent = iParent->m_Key.m_Name;
									ExtraConfigValue.m_Entity = iEntity->m_Key.m_Name;
									ExtraConfigValue.m_bXcodeProperty = ConfigValue.m_bXcodeProperty;
									if (ExtraConfigValue.m_Entity.f_IsEmpty())
										ExtraConfigValue.m_Entity = _DefaultEntity;
									ExtraConfigValue.m_Property = iProperty.f_GetKey().m_Name;
									ExtraConfigValue.m_Value = iProperty->m_Value;
									ConfigValues[ExtraConfigValue];
								}
							}
						}
					}								
				}

				if (ConfigValues.f_IsEmpty())
				{
					ConfigValue.m_bMainValue = true;
					ConfigValues[ConfigValue];
				}
				auto &ValueConfigs = ConfigOptions[ConfigValues];
				ValueConfigs.m_ByPlatform[_Config.m_Platform].f_Insert(_Config);
				ValueConfigs.m_Configurations[_Config];
				ValueConfigs.m_OriginalValues[Value];
			}
		;

		for (auto iConfig = _Configs.f_GetIterator(); iConfig; ++iConfig)
			fl_AddConfig(iConfig.f_GetKey(), **iConfig);

		if (ConfigOptions.f_IsEmpty())
			return;

		auto fl_AddValue
			= [&](CConfigValue const &_Value, CStr const &_Condition, CValueConfigs const &_ValueConfigs) -> CElement
			{
				CElement Element;

//					if (!_Value.m_Property.f_IsEmpty())
//						m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Empty property {} {}") << _Value.m_Entity << _Value.m_Value);
				if (!_Value.m_Parent.f_IsEmpty())
					m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Parent not supported"));

				Element.m_Property = _Value.m_Property;
				if (_Value.m_bUseValues)
				{
					Element.m_bUseValues = true;
					for (auto &Value : _Value.m_Values)
						Element.m_ValueSet[Value];
				}
				else
					Element.f_SetValue(_Value.m_Value);
				Element.m_bXcodeProperty = _Value.m_bXcodeProperty;
				Element.m_Position = Position;
				
				return Element;
			}
		;

		if (ConfigOptions.f_GetLen() == 1 && fp_IsSameConfig(_AllConfigs, ConfigOptions.f_GetIterator()->m_Configurations))
		{
			for (auto iRealValue = ConfigOptions.f_GetIterator().f_GetKey().f_GetIterator(); iRealValue; ++iRealValue)
			{
				auto Element = fl_AddValue(*iRealValue, _ExtraCondition, *ConfigOptions.f_GetIterator());
				auto &Configs = *ConfigOptions.f_GetIterator();
				for (auto iConfig = Configs.m_Configurations.f_GetIterator(); iConfig; ++iConfig)
				{
					auto &RetValue = _Result[iConfig.f_GetKey()];
					RetValue.m_Element[Element.m_Property] = Element;
				}
			}
		}
		else
		{
			if (_bFile)
			{
				if (ConfigOptions.f_GetLen() != 1)
					m_BuildSystem.fs_ThrowError(Position, "This property cannot be varied by configuration");
			}

			for (auto iValue = ConfigOptions.f_GetIterator(); iValue; ++iValue)
			{
				for (auto iRealValue = iValue.f_GetKey().f_GetIterator(); iRealValue; ++iRealValue)
				{
					auto Element = fl_AddValue(*iRealValue, _ExtraCondition, *iValue);

					for (auto iConfig = iValue->m_Configurations.f_GetIterator(); iConfig; ++iConfig)
					{
						auto &RetValue = _Result[iConfig.f_GetKey()];
						RetValue.m_Element[Element.m_Property] = Element;
					}
				}
			}
		}
	}
}
