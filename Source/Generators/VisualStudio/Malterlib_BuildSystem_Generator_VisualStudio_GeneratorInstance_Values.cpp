// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <Mib/XML/XML>

#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsFilePath>

namespace
{
	void fg_ShortenPath(CStr &o_Path)
	{
		if (o_Path.f_GetLen() > 200)
		{
			CStr EndPath;
			while (!CFile::fs_FileExists(o_Path))
			{
				EndPath = CFile::fs_GetFile(o_Path) / EndPath;
				o_Path = CFile::fs_GetPath(o_Path);
			}
			bool bExists = CFile::fs_FileExists(o_Path);
										
			if (o_Path)
				o_Path = NFile::NPlatform::fg_ConvertToShortWindowsPath<CWStr, CStr>(o_Path, false) / EndPath;
			else
				o_Path = EndPath;
		}
	}
}
#endif

namespace NMib::NBuildSystem::NVisualStudio
{
	void CGeneratorInstance::f_SetEvaluatedValues
		(
			TCMap<CStr, CXMLElement *> const &_Parents
			, TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
			, TCMap<CConfiguration, CEntityMutablePointer> const &_AllConfigs
			, bool _bFile
			, EPropertyType _PropertyType
			, TCVector<CStr> const *_pSearchList
			, TCMap<CConfiguration, TCVector<CStr>> const *_pSearchListPerConfig
			, CStr const &_DefaultEntity
			, bool _bPropertyCondition
			, bool _bAddPropertyDefined
			, bool _bDontAllowRedefinition
			, CProject &_Project
		) const
	{
		struct CPropretyEval
		{
			TCMap<CConfiguration, CEntity *> m_Configs;
			CFilePosition m_Position;
		};

		TCMap<CPropertyKey, CPropretyEval> Properties;
		for (auto iConfig = _Configs.f_GetIterator(); iConfig; ++iConfig)
		{
			auto *pConfig = iConfig->f_Get();
			auto pTopConfig = pConfig;
			bool bBelowFileLevel = false;
			bool bFullFileSettings = false;
			if (_bFile)
			{
				auto pConfig = (*iConfig);
				while (pConfig)
				{
					if (pConfig->f_HasFullEval(_PropertyType))
					{
						CProperty const *pFromProperty = nullptr;
						bFullFileSettings = m_BuildSystem.f_EvaluateEntityPropertyBool(*pTopConfig, _PropertyType, "FullEval", pFromProperty, false);
						break;
					}

					pConfig = fg_Explicit(pConfig->m_pParent);
				}
			}

			while (pConfig)
			{
				auto &Key = pConfig->f_GetKey();
				if (_bFile && !bBelowFileLevel)
				{
					if
						(
							Key.m_Type != EEntityType_Group
							&& Key.m_Type != EEntityType_GenerateFile
							&& Key.m_Type != EEntityType_File
						)
					{
						bBelowFileLevel = true;
					}
				}
				bool bUseExplitPerFile = bBelowFileLevel && !bFullFileSettings;

				for (auto iProperty = pConfig->f_Data().m_Properties.f_GetIterator(); iProperty; ++iProperty)
				{
					auto &SourceProperties = *iProperty;
					auto Type = iProperty.f_GetKey().m_Type;

					if (Type != _PropertyType)
						continue;

					if (bUseExplitPerFile)
					{
						bool bNeedPerFile = false;
						for (auto &Property : SourceProperties)
						{
							if (Property.m_bNeedPerFile)
								bNeedPerFile = true;
						}
						if (!bNeedPerFile)
							continue;
					}

					if (bBelowFileLevel)
					{
						bool bFoundProperty = false;
						for (auto &Property : SourceProperties)
						{
							if (m_BuildSystem.f_EvalCondition(*pTopConfig, Property.m_Condition, Property.m_Debug.f_Find("TraceCondition") >= 0))
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
						if (!SourceProperties.f_IsEmpty())
							Eval.m_Position = SourceProperties.f_GetFirst().m_Position;
						MapConfig.f_GetResult() = iConfig->f_Get();
					}
				}

				for (auto &VariableDefinition : pConfig->f_Data().m_VariableDefinitions)
				{
					auto &Key = pConfig->f_Data().m_VariableDefinitions.fs_GetKey(VariableDefinition);
					if (Key.m_Type != _PropertyType)
						continue;

					auto TypePosition = VariableDefinition.m_Type.m_Position;
					CBuildSystemSyntax::CType const *pType = m_BuildSystem.f_GetCanonicalDefaultedType(*pConfig, &VariableDefinition.m_Type.m_Type, TypePosition);

					if (!pType->f_IsDefaulted())
						continue;

					if (!m_BuildSystem.f_EvalCondition(*pTopConfig, *VariableDefinition.m_pConditions, false))
						continue;

					CProperty const *pFromProperty = nullptr;
					auto Value = m_BuildSystem.f_EvaluateEntityProperty(*pTopConfig, _PropertyType, Key.m_Name, pFromProperty);

					if (!Value.f_IsValid())
						continue;

					auto &Eval = Properties[Key];
					auto MapConfig = Eval.m_Configs(iConfig.f_GetKey());
					if (MapConfig.f_WasCreated())
					{
						Eval.m_Position = VariableDefinition.m_Type.m_Position;
						MapConfig.f_GetResult() = pTopConfig;
					}
					else if (!Eval.m_Position.f_IsValid())
						Eval.m_Position = VariableDefinition.m_Type.m_Position;
				}

				pConfig = pConfig->m_pParent;
			}
		}

		for (auto iProperty = Properties.f_GetIterator(); iProperty; ++iProperty)
		{
			f_AddConfigValue
				(
					iProperty->m_Configs
					, _Configs
					, iProperty->m_Position
					, iProperty.f_GetKey().m_Type
					, iProperty.f_GetKey().m_Name
					, _Parents
					, CStr()
					, false
					, _pSearchList
					, _pSearchListPerConfig
					, _DefaultEntity
					, _bPropertyCondition
					, _bAddPropertyDefined
					, _bDontAllowRedefinition
					, _bFile
					, _Project
				)
			;
		}
	}

	TCMap<CConfiguration, CGeneratorInstance::CConfigResult> CGeneratorInstance::f_AddConfigValue
		(
			TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
			, TCMap<CConfiguration, CEntityMutablePointer> const &_AllConfigs
			, CFilePosition const &_Position
			, EPropertyType _PropType 
			, CStr const &_SourceType
			, TCMap<CStr, CXMLElement *> const &_Parents
			, CStr const &_AddAsAttribute
			, bool _bExcludeFromBuildCondition
			, TCVector<CStr> const *_pSearchList
			, TCMap<CConfiguration, TCVector<CStr>> const *_pSearchListPerConfig
			, CStr const &_DefaultEntity
			, bool _bPropertyCondition
			, bool _bAddPropertyDefined
			, bool _bDontAllowRedefinition
			, bool _bFile
			, CProject &_Project
		) const
	{
		auto Position = _Position;
		if (Position.m_File.f_IsEmpty())
			Position = m_pGeneratorSettings->f_Data().m_Position;

		auto & ThreadLocal = *m_ThreadLocal;

		bool bFile = !_AddAsAttribute.f_IsEmpty();

		CValueProperties SingleProperties;

		auto fGetValueProperties
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
				bool bDisabled = false;
				{
					for (auto iSearch = pSearchList->f_GetIterator(); iSearch; ++iSearch)
					{
						Key.m_Name.m_Value = *iSearch;
						pSettings = m_pGeneratorSettings->m_ChildEntitiesMap.f_FindEqual(Key);
						if (!pSettings)
							continue;

						CPropertyKey PropertyKey;
						PropertyKey.m_Type = EPropertyType_Property;
						PropertyKey.m_Name = "Disabled";
						if (pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey))
						{
							bDisabled = true;
							break;
						}

						Key.m_Name.m_Value = fg_PropertyTypeToStr(_PropType);
						pSettings = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
						if (!pSettings)
							continue;

						Key.m_Name.m_Value = _SourceType;
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
					}
				}

				//CStr

				CValueProperties Ret;

				CPropertyKey PropertyKey;
				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Disabled";
				if (pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey))
					Ret.m_bDisabled = true;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Parent";
				Ret.m_pVSParentName = pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey);

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Entity";
				Ret.m_pVSEntityName = pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey);

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Name";
				Ret.m_pVSPropertyName = pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey);

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "ConvertPath";
				if (pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey))
					Ret.m_bConvertPath = true;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "EscapeSeparated";
				if (pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey))
					Ret.m_bEscapeSeparated = true;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "ShortenFilenames";
				if (pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey))
					Ret.m_bShortenFilenames = true;

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Validity";
				if (auto pProp = pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey))
				{
					if (pProp->m_Value.f_String() == "File")
						Ret.m_Validity = EPropertyValidity_File;
					else if (pProp->m_Value.f_String() == "NotFile")
						Ret.m_Validity = EPropertyValidity_NotFile;
				}

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Substitute";
				auto pSubstitute = pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey);
				if (pSubstitute)
					Ret.m_Substitute = pSubstitute->m_Value.f_String();

				PropertyKey.m_Type = EPropertyType_Property;
				PropertyKey.m_Name = "Separator";
				auto pSeparator = pSettings->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey);
				if (pSeparator)
					Ret.m_Separator = pSeparator->m_Value.f_String();

				Key.m_Name.m_Value = "Value";
				auto pTranslators = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
				if (pTranslators)
					Ret.m_pTranslators = fg_Explicit(pTranslators);

				Key.m_Name.m_Value = "ValueSet";
				auto pValueSet = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
				if (pValueSet)
					Ret.m_pValueSet = fg_Explicit(pValueSet);

				Key.m_Name.m_Value = "Properties";
				auto pProperties = pSettings->m_ChildEntitiesMap.f_FindEqual(Key);
				if (pProperties)
					Ret.m_pProperties = fg_Explicit(pProperties);

				return Ret;
			}
		;

		TCSet<CStr> ProperityGroupParents;
		for (auto iParent = _Parents.f_GetIterator(); iParent; ++iParent)
		{
			if (CXMLDocument::f_GetValue(*iParent) == "PropertyGroup")
				ProperityGroupParents[iParent.f_GetKey()];
		}

		if (_pSearchList)
		{
			SingleProperties = fGetValueProperties(CConfiguration());
			if (!bFile && SingleProperties.m_bDisabled)
				return fg_Default();
		}

		auto fGetInheritedValue
			= [&](CStr const &_Parent, CStr const &_Property, CStr const &_Entity) -> CStr
			{
				if (ProperityGroupParents.f_FindEqual(_Parent))
					return CStr::CFormat("$({})") << _Property;
				else
				{
					if (_Entity.f_IsEmpty())
						return CStr::CFormat("%({})") << _Property;
					else
						return CStr::CFormat("%({}.{})") << _Entity << _Property;
				}
			}
		;

		auto fGetInherited
			= [&](CStr const &_String, CStr const &_Parent, CStr const &_Property, CStr const &_Entity) -> CStr
			{
				return _String.f_Replace("{578185E0-2E2A-4481-A34E-BCC3F64CDCA2}", fGetInheritedValue(_Parent, _Property, _Entity));
			}
		;

		TCMap<TCSet<CConfigValue>, CValueConfigs> ConfigOptions;
		auto fAddConfig
			= [&](CConfiguration const &_Config, CEntity &_Entity)
			{
				CValueProperties PropertiesValue;

				if (_pSearchList)
					PropertiesValue = SingleProperties;
				else
					PropertiesValue = fGetValueProperties(_Config);

				ch8 const *pSeparator = PropertiesValue.m_Separator ? PropertiesValue.m_Separator.f_GetStr() : ";";

				CStr Value;
				{
					CProperty const *pFromProperty = nullptr;
					auto BuildSystemValue = m_BuildSystem.f_EvaluateEntityProperty(_Entity, _PropType, _SourceType, pFromProperty);
					if (BuildSystemValue.f_IsValid())
					{
						if (BuildSystemValue.f_IsString())
							Value = fg_Move(BuildSystemValue.f_String());
						else if (BuildSystemValue.f_IsStringArray())
						{
							auto Values = BuildSystemValue.f_StringArray();
#ifdef DPlatformFamily_Windows
							if (PropertiesValue.m_bShortenFilenames)
							{
								for (auto &Value : Values)
									fg_ShortenPath(Value);
							}
#endif
							Value = CStr::fs_Join(Values, pSeparator);
						}
						else if (BuildSystemValue.f_IsBoolean())
							Value = BuildSystemValue.f_AsString();
						else if (BuildSystemValue.f_IsInteger())
							Value = BuildSystemValue.f_AsString();
						else if (BuildSystemValue.f_IsFloat())
							Value = BuildSystemValue.f_AsString();
						else if (BuildSystemValue.f_IsArray())
						{
							TCVector<CStr> Values;

							for (auto &Value : BuildSystemValue.f_Array())
							{
								if (Value.f_IsArray())
									Values.f_Insert(Value.f_StringArray());
								else
									Values.f_Insert(Value.f_String());
							}

#ifdef DPlatformFamily_Windows
							if (PropertiesValue.m_bShortenFilenames)
							{
								for (auto &Value : Values)
									fg_ShortenPath(Value);
							}
#endif
							Value = CStr::fs_Join(Values, pSeparator);
						}
						else
							m_BuildSystem.fs_ThrowError(pFromProperty ? pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a string or string array value");
					}
				}

				CConfigValue ConfigValue;

				switch (PropertiesValue.m_Validity)
				{
				case EPropertyValidity_Any:
					break;
				case EPropertyValidity_File:
					{
						if (!_bFile)
							m_BuildSystem.fs_ThrowError(Position, "This property is only valid on files");
					}
					break;
				case EPropertyValidity_NotFile:
					{
						if (_bFile)
							m_BuildSystem.fs_ThrowError(Position, "This property is only valid on target, not at file or file group level");
					}
					break;
				}

				if (PropertiesValue.m_bDisabled && !bFile)
					return;

				if (PropertiesValue.m_pVSParentName)
					ConfigValue.m_Parent = PropertiesValue.m_pVSParentName->m_Value.f_String();
				if (PropertiesValue.m_pVSEntityName)
					ConfigValue.m_Entity = PropertiesValue.m_pVSEntityName->m_Value.f_String();
				else
					ConfigValue.m_Entity = _DefaultEntity;

				CStr DestType;
				if (PropertiesValue.m_pVSPropertyName)
					ConfigValue.m_Property = PropertiesValue.m_pVSPropertyName->m_Value.f_String();
				else
					ConfigValue.m_Property = _SourceType;

				ConfigValue.m_Value = Value;
				if (PropertiesValue.m_pTranslators)
				{
					CPropertyKey PropertyKey;
					PropertyKey.m_Type = EPropertyType_Property;
					PropertyKey.m_Name = Value;
					auto pVSProperty = PropertiesValue.m_pTranslators->m_EvaluatedProperties.m_Properties.f_FindEqual(PropertyKey);

					if (!pVSProperty)
						m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("No translated property found for value {}") << Value);
					ConfigValue.m_Value = pVSProperty->m_Value.f_String();
				}

				if (PropertiesValue.m_bConvertPath)
					ConfigValue.m_Value = ConfigValue.m_Value.f_ReplaceChar('/', '\\');
				if (PropertiesValue.m_bEscapeSeparated)
				{
					CStr NewValue;
					CStr OldValue = fg_Move(ConfigValue.m_Value);
					while (!OldValue.f_IsEmpty())
					{
						CStr Value = fg_GetStrSepNoTrim(OldValue, ";");

						fg_AddStrSepEscaped(NewValue, Value, ';', "\"\\ ");
					}
					ConfigValue.m_Value = fg_Move(NewValue);
				}

				if (!PropertiesValue.m_Substitute.f_IsEmpty())
				{
					CStr NewValue = CStr::CFormat(PropertiesValue.m_Substitute) << ConfigValue.m_Value;
					ConfigValue.m_Value = NewValue;
				}

				CStr OriginalValue = ConfigValue.m_Value;
				ConfigValue.m_Value = fGetInherited(ConfigValue.m_Value, ConfigValue.m_Parent, ConfigValue.m_Property, ConfigValue.m_Entity);

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
								ExtraConfigValue.m_Parent = iParent->f_GetKeyName();
								ExtraConfigValue.m_Entity = iEntity->f_GetKeyName();
								if (ExtraConfigValue.m_Entity.f_IsEmpty())
									ExtraConfigValue.m_Entity = _DefaultEntity;
								ExtraConfigValue.m_Property = iProperty->f_GetKeyName();
								ExtraConfigValue.m_Value = fGetInherited(OriginalValue, ExtraConfigValue.m_Parent, ExtraConfigValue.m_Property, ExtraConfigValue.m_Entity);
								ConfigValues[ExtraConfigValue];
							}
						}
					}
				}
				if (PropertiesValue.m_pValueSet)
				{
					CEntityKey Key;
					Key.m_Type = EEntityType_GeneratorSetting;
					Key.m_Name.m_Value = Value;
					auto pToSet = PropertiesValue.m_pValueSet->m_ChildEntitiesMap.f_FindEqual(Key);
					if (pToSet)
					{
						for (auto iParent = pToSet->m_ChildEntitiesOrdered.f_GetIterator(); iParent; ++iParent)
						{
							for (auto iEntity = iParent->m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
							{
								for (auto iProperty = iEntity->m_EvaluatedProperties.m_Properties.f_GetIterator(); iProperty; ++iProperty)
								{
									CConfigValue ExtraConfigValue;
									if (!ConfigValue.m_bMainValue)
									{
										ConfigValue.m_bMainValue = true;
										ExtraConfigValue.m_bMainValue = true;
									}
									else
										ExtraConfigValue.m_bMainValue = false;
									ExtraConfigValue.m_Parent = iParent->f_GetKeyName();
									ExtraConfigValue.m_Entity = iEntity->f_GetKeyName();
									if (ExtraConfigValue.m_Entity.f_IsEmpty())
										ExtraConfigValue.m_Entity = _DefaultEntity;
									ExtraConfigValue.m_Property = iProperty.f_GetKey().m_Name;
									ExtraConfigValue.m_Value = fGetInherited
										(
											iProperty->m_Value.f_String()
											, ExtraConfigValue.m_Parent
											, ExtraConfigValue.m_Property
											, ExtraConfigValue.m_Entity
										)
									;
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

				ValueConfigs.m_ByPlatform[_Project.m_Platforms[_Config]].f_Insert(_Config);
				auto &pConfig = ValueConfigs.m_Configurations[_Config];
				ValueConfigs.m_OriginalValues[Value];

				if (_SourceType == "PrecompilePrefixHeader")
				{
					if (ConfigValue.m_Value == "Use")
					{
						CProperty const *pFromProperty = nullptr;
						CStr PrefixHeader = m_BuildSystem.f_EvaluateEntityPropertyString(_Entity, EPropertyType_Compile, "PrefixHeader", pFromProperty, CStr());
						if (!PrefixHeader.f_IsEmpty())
						{
							CStr FilePath = CFile::fs_GetPath(_Entity.f_Data().m_Position.m_File);
							CStr FullPrefixHeader = f_GetExpandedPath(PrefixHeader, FilePath);

							if (!ThreadLocal.f_FileExists(FullPrefixHeader))
							{
								// Try relative to output
								FullPrefixHeader = f_GetExpandedPath(PrefixHeader, ThreadLocal.m_CurrentOutputDir);
							}

							DCheck(!ThreadLocal.m_CurrentCompileTypes.f_IsEmpty());

							auto &PrefixHeaderMap = ThreadLocal.m_PrefixHeaders[ThreadLocal.m_CurrentCompileTypes][FullPrefixHeader];
							PrefixHeaderMap.m_Configurations[_Config];
							if (pFromProperty)
								PrefixHeaderMap.m_Position = pFromProperty->m_Position;
							DCheck(pConfig == nullptr);
							pConfig = &PrefixHeaderMap;
						}
					}
				}

			}
		;

		for (auto iConfig = _Configs.f_GetIterator(); iConfig; ++iConfig)
			fAddConfig(iConfig.f_GetKey(), **iConfig);

		if (ConfigOptions.f_IsEmpty())
			return TCMap<CConfiguration, CConfigResult>();
			//m_BuildSystem.fs_ThrowError(CFilePosition(), "Internal Error");

		auto fAddValue
			= [&](TCSet<CConfigValue> const &_Value, CStr const &_Condition, CValueConfigs const &_ValueConfigs) -> CXMLElement *
			{
				CXMLElement *pAddToElement = nullptr;
				if (_AddAsAttribute.f_IsEmpty())
				{
					for (auto iValue = _Value.f_GetIterator(); iValue; ++iValue)
					{

						auto pParent = _Parents.f_FindEqual(iValue->m_Parent);

						if (!pParent)
							m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Parent element {} not found") << iValue->m_Parent);

						auto pParentElement = *pParent;
						if (!iValue->m_Entity.f_IsEmpty())
						{
							bool bCreated;
							pParentElement = CXMLDocument::f_GetOrCreateElement(pParentElement, iValue->m_Entity, bCreated);
						}
						CStr Condition;
						if (!_Condition.f_IsEmpty() || _bPropertyCondition)
						{
							if (_bPropertyCondition)
							{
								Condition = CStr::CFormat("'%({}.DefinedProperty_{})' != 'true'") << iValue->m_Entity << iValue->m_Property;
								if (!_Condition.f_IsEmpty())
								{
									Condition += " and (";
									Condition += _Condition;
									Condition += ")";
								}
							}
							else
								Condition = _Condition;
						}

						CXMLElement *pMainValue = nullptr;
						if (_bDontAllowRedefinition)
						{
							TCSet<CConfiguration> MatchedConfigs;
							for (auto iPlatform = _ValueConfigs.m_ByPlatform.f_GetIterator(); iPlatform; ++iPlatform)
							{
								for (auto iConfig = iPlatform->f_GetIterator(); iConfig; ++iConfig)
									MatchedConfigs[*iConfig];
							}

							auto XMLName = iValue->m_Property;
							for (auto pElement = pParentElement->FirstChildElement(""); pElement; pElement = pElement->NextSiblingElement(""))
							{
								if (pElement->Value() == XMLName)
								{
									CStr OtherCondition = CXMLDocument::f_GetAttribute(pElement, "Condition");

									if (OtherCondition == Condition)
									{
										if (CXMLDocument::f_GetNodeText(pElement) != iValue->m_Value)
											m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Same property specified differently for different compile types not supported (Value) {}: {} != {}") << iValue->m_Property << CXMLDocument::f_GetNodeText(pElement) << iValue->m_Value);
										pMainValue = pElement;
										continue;
									}

									if (OtherCondition.f_IsEmpty() || _Condition.f_IsEmpty())
									{
										// Always matches
										m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Same property specified differently for different compile types not supported (No condition) {} = {}") << iValue->m_Property << iValue->m_Value);
									}
									ch8 const *pParse = OtherCondition;
									ch8 const *pParseStart = pParse;
									while (*pParse)
									{
										fg_ParseWhiteSpace(pParse);
										if (*pParse == '(')
										{
											++pParse;
											if (fg_StrStartsWith(pParse, "'$(Platform)'=='"))
											{
												auto pInnerParse = pParse + 16;
												auto pStart = pInnerParse;
												while (*pInnerParse && *pInnerParse != '\'')
													++pInnerParse;
												CStr Platform = OtherCondition.f_Extract(pStart - pParseStart, pInnerParse - pStart);
												while (*pInnerParse && *pInnerParse != '(')
													++pInnerParse;
												if (*pInnerParse == '(')
												{
													++pInnerParse;
													while (*pInnerParse && *pInnerParse != ')')
													{
														if (fg_StrStartsWith(pInnerParse, "'$(Configuration)'=='"))
														{
															pInnerParse += 21;
															auto pStart = pInnerParse;
															while (*pInnerParse && *pInnerParse != '\'')
																++pInnerParse;
															CStr Config = OtherCondition.f_Extract(pStart - pParseStart, pInnerParse - pStart);
															CConfiguration Config2;
															Config2.m_Platform = Platform;
															Config2.m_Configuration = Config;
															auto Mapped = MatchedConfigs(Config2);
															if (!Mapped.f_WasCreated())
																m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Same property specified differently for different compile types not supported (Config mapped twice, {}, {}, {}) {} = {}") << Platform << Config << OtherCondition << iValue->m_Property << iValue->m_Value);
														}
														++pInnerParse;
													}
												}
											}
											mint ParenDepth = 1;
											while (*pParse)
											{
												if (*pParse == '(')
													++ParenDepth;
												else if (*pParse == ')')
												{
													if (--ParenDepth == 0)
													{
														++pParse;
														break;
													}
												}
												++pParse;
											}
										}
										else
											++pParse;
									}
								}
							}

							if (!pMainValue)
								pMainValue = CXMLDocument::f_AddElementAndText(pParentElement, iValue->m_Property, iValue->m_Value);
						}
						else
							pMainValue = CXMLDocument::f_AddElementAndText(pParentElement, iValue->m_Property, iValue->m_Value);

						if (iValue->m_bMainValue)
						{
							pAddToElement = pMainValue;
							if (_SourceType == "PrecompilePrefixHeader")
							{
								if (iValue->m_Value == "Use")
								{
									for (auto iConfig = _ValueConfigs.m_Configurations.f_GetIterator(); iConfig; ++iConfig)
									{
										if ((*iConfig))
										{
											(*iConfig)->m_Elements[_Condition][pParentElement];
										}
									}
								}
							}
						}
						if (!Condition.f_IsEmpty())
							CXMLDocument::f_SetAttribute(pMainValue, "Condition", Condition);
						if (_bAddPropertyDefined)
						{
							CStr InheritedValue = fGetInheritedValue(iValue->m_Parent, iValue->m_Property, iValue->m_Entity);

							if (iValue->m_Value.f_Find(InheritedValue) < 0)
							{
								CXMLDocument::f_AddElementAndText(pParentElement, CStr::CFormat("DefinedProperty_{}") << iValue->m_Property, "true");
							}
						}
					}
				}
				else
				{
					//if (_Value.f_GetLen() != 1)
					//	m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Mutiple values not supported at file level"));

					CConfigValue const *pMainConfigValue = nullptr;
					for (auto iValue = _Value.f_GetIterator(); iValue; ++iValue)
					{
						if (iValue->m_bMainValue)
						{
							pMainConfigValue = iValue;
							break;
						}
					}
					CConfigValue const &Value = *pMainConfigValue;

					auto pParent = _Parents.f_FindEqual(Value.m_Parent);

					if (!pParent)
						m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Parent element {} not found") << Value.m_Parent);

					pAddToElement = *pParent;

					if (!Value.m_Entity.f_IsEmpty())
						m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Entity cannot be specified at file level"));

					auto pMainValue = CXMLDocument::f_CreateElement(pAddToElement, Value.m_Value);
					CStr FileName = (*_Configs.f_GetIterator())->f_GetKeyName();

					if (m_BuildSystem.f_GetGenerateSettings().m_GenerationFlags & EGenerationFlag_AbsoluteFilePaths)
						FileName = FileName.f_ReplaceChar('/', '\\');
					else
					{
						CStr RelativeName = CFile::fs_MakePathRelative(FileName, ThreadLocal.m_CurrentOutputDir);
						CStr PotentialName = CFile::fs_AppendPath(ThreadLocal.m_CurrentOutputDir, RelativeName);
						// Use absolute paths if it is going to save us from hitting _MAX_PATH or if absolute path is shorter than relative path
						if ((PotentialName.f_GetLen() >= 256 && FileName.f_GetLen() < PotentialName.f_GetLen()) || FileName.f_GetLen() < RelativeName.f_GetLen())
							FileName = FileName.f_ReplaceChar('/', '\\');
						else
							FileName = RelativeName.f_ReplaceChar('/', '\\');
					}

					CXMLDocument::f_SetAttribute(pMainValue, _AddAsAttribute, FileName);
					pAddToElement = pMainValue;
					if (_bExcludeFromBuildCondition)
					{
						if (!_Condition.f_IsEmpty())
						{
							auto pExcludeValue = CXMLDocument::f_AddElementAndText(pAddToElement, "ExcludedFromBuild", "true");
							CXMLDocument::f_SetAttribute(pExcludeValue, "Condition", "!(" + _Condition + ")");
						}
					}
					for (auto iValue = _Value.f_GetIterator(); iValue; ++iValue)
					{
						if (iValue->m_bMainValue)
							continue;
						auto pParent = _Parents.f_FindEqual(iValue->m_Parent);

						if (!pParent)
							m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Parent element {} not found") << iValue->m_Parent);

						auto pParentElement = pMainValue;
						if (!iValue->m_Entity.f_IsEmpty())
						{
							bool bCreated;
							pParentElement = CXMLDocument::f_GetOrCreateElement(pParentElement, iValue->m_Entity, bCreated);
						}
						auto pMainValue = CXMLDocument::f_AddElementAndText(pParentElement, iValue->m_Property, iValue->m_Value);

						if (!_Condition.f_IsEmpty())
							CXMLDocument::f_SetAttribute(pMainValue, "Condition", _Condition);

						if (_bAddPropertyDefined)
						{
							CStr InheritedValue = fGetInheritedValue(iValue->m_Parent, iValue->m_Property, iValue->m_Entity);

							if (iValue->m_Value.f_Find(InheritedValue) < 0)
							{
								CXMLDocument::f_AddElementAndText(pParentElement, CStr::CFormat("DefinedProperty_{}") << iValue->m_Property, "true");
							}
						}
					}

				}
				return pAddToElement;
			}
		;

		TCMap<CConfiguration, CConfigResult> Ret;
		bool bCanUseSingleConfig = ConfigOptions.f_GetLen() == 1 && fp_IsSameConfig(_AllConfigs, ConfigOptions.f_GetIterator()->m_Configurations);

		if (bFile)
		{
			if (ConfigOptions.f_GetLen() != 1)
				m_BuildSystem.fs_ThrowError(Position, "This property cannot be varied by configuration");
		}

		for (auto iValue = ConfigOptions.f_GetIterator(); iValue; ++iValue)
		{
			CStr Condition;

			if (!bCanUseSingleConfig || iValue.f_GetKey().f_FindSmallest()->m_Property == "ExcludedFromBuild")
			{
				bool bFirstPlatform = true;
				for (auto iPlatform = iValue->m_ByPlatform.f_GetIterator(); iPlatform; ++iPlatform)
				{
					if (!bFirstPlatform)
						Condition += " or ";
					bFirstPlatform = false;

					Condition += CStr::CFormat("('$(Platform)'=='{}' and (") << iPlatform.f_GetKey();
					bool bFirst = true;
					for (auto iConfig = iPlatform->f_GetIterator(); iConfig; ++iConfig)
					{
						if (!bFirst)
							Condition += " or ";

						Condition += CStr::CFormat("'$(Configuration)'=='{}'") << iConfig->m_Configuration;
						bFirst = false;
					}

					Condition += "))";
				}
			}

			auto pEntity = fAddValue(iValue.f_GetKey(), Condition, *iValue);
			for (auto iConfig = iValue->m_Configurations.f_GetIterator(); iConfig; ++iConfig)
			{
				auto &RetValue = Ret[iConfig.f_GetKey()];
				RetValue.m_pElement = pEntity;
				RetValue.m_UntranslatedValues += iValue->m_OriginalValues;
			}
		}

		return Ret;
	}
}
