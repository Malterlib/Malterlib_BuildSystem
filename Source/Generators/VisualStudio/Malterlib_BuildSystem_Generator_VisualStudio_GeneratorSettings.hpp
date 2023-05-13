// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem::NVisualStudio
{
	template <typename t_CType>
	struct TCRemoveOptional
	{
		using CType = t_CType;
	};

	template <typename t_CType>
	struct TCRemoveOptional<TCOptional<t_CType>>
	{
		using CType = t_CType;
	};

	template <typename tf_CStr>
	void CGeneratorSettings::CVS_SettingShared::f_Format(tf_CStr &o_String) const
	{
		o_String += typename tf_CStr::CFormat
			(
				"Key: \"{}\" Value:\n"
				"            -----------------------------------------------------\n"
				"{}\n"
				"            -----------------------------------------------------\n"
			) << m_Key << m_Value.f_Indent("            | ")
		;
	}

	template <typename tf_CStr>
	void CGeneratorSettings::CVS_Setting_PropertyGroup::f_Format(tf_CStr &o_String) const
	{
		o_String += typename tf_CStr::CFormat("PropertyGroup Label: \"{}\" ") << m_Label;
		CVS_SettingShared::f_Format(o_String);
	}

	template <typename tf_CStr>
	void CGeneratorSettings::CVS_Setting_ItemDefinitionGroup::f_Format(tf_CStr &o_String) const
	{
		o_String += typename tf_CStr::CFormat("ItemDefinitionGroup Name: \"{}\" ") << m_Name;
		CVS_SettingShared::f_Format(o_String);
	}

	template <typename tf_CStr>
	void CGeneratorSettings::CVS_Setting_Item::f_Format(tf_CStr &o_String) const
	{
		o_String += "FileType ";
		CVS_SettingShared::f_Format(o_String);
	}

	template <typename tf_CStr>
	void CGeneratorSettings::CVSSettingAggregateProperties::f_Format(tf_CStr &o_String) const
	{
		o_String += "            Configurations\n";
				
		for (auto &Configuration : m_Configurations)
			o_String += typename tf_CStr::CFormat("                {}\n") << Configuration;

		o_String += "            Positions\n";

		TCVector<CParseError> ParseErrors;
		for (auto &Position : m_Positions.m_Positions)
			ParseErrors.f_Insert({.m_Error = Position.f_GetMessage(), .m_Location = Position.m_Key.m_Position});

		o_String += CParseError::fs_ToString(ParseErrors).f_Indent("                ");
	}

	template <typename tf_CStr>
	void CGeneratorSettings::CVSSettingAggregate::f_Format(tf_CStr &o_String) const
	{
		for (auto &Setting : m_Settings)
		{
			auto &Key = m_Settings.fs_GetKey(Setting);
			for (auto &KeyItem : Key)
				o_String += typename tf_CStr::CFormat("    {}\n") << KeyItem;

			o_String += "        Applied To\n";
			o_String += typename tf_CStr::CFormat("{}\n") << Setting;
		}
	}

	template <typename tf_CStr>
	void CGeneratorSettings::CVSSettingAggregated::f_Format(tf_CStr &o_String) const
	{
		for (auto &Aggregate : m_AggregatedSettings)
		{
			auto &Key = m_AggregatedSettings.fs_GetKey(Aggregate);

			o_String += typename tf_CStr::CFormat("{}\n{}\n") << Key << Aggregate;
		}
	}

	template <bool tf_bType, bool tf_bIsItem>
	auto CGeneratorSettings::f_GetParsedVSSettings(TCMap<CStr, CGeneratorSettingsVSType> *_pCompileSettings) -> CParsedGeneratorSettings
	{
		CParsedGeneratorSettings Return;

		CGeneratorSettingsVSType *pSharedCompileSettings = nullptr;
		if constexpr (tf_bType)
		{
			Return.m_VSType = fg_Move(*this).f_GetSingleSetting<CStr>("VSType").m_Value;
			Return.m_Type = fg_Move(*this).f_GetSingleSetting<CStr>("Type").m_Value;
			if constexpr (tf_bIsItem)
			{
				if (_pCompileSettings)
					pSharedCompileSettings = _pCompileSettings->f_FindEqual(Return.m_VSType);
			}
		}

		auto &Settings = f_Settings();
		auto &ReturnSettingsMap = Return.f_ConstructSettings();

		for (auto &Setting : Settings)
		{
			auto &Configuration = Settings.fs_GetKey(Setting);

			auto pVSSettings = Setting.m_Value.f_GetMember(gc_ConstString_VSSettings, EEJSONType_Object);

			if (!pVSSettings)
				continue;

			auto &ReturnSettings = ReturnSettingsMap[Configuration];

			for (auto &VSSetting : pVSSettings->f_Object())
			{
				auto &SettingName = VSSetting.f_Name();

				auto &JSON = VSSetting.f_Value();

				CBuildSystemUniquePositions Positions = CBuildSystemUniquePositions::fs_FromJson(fg_Move(JSON[gc_ConstString_Positions]));
				TCVector<CVS_Setting> VSSettingsVector;

				if constexpr (tf_bIsItem)
				{
					JSON[gc_ConstString_Settings].f_Object().f_ExtractAll
						(
							[&](auto &&_Key, auto &&_Value)
							{
								VSSettingsVector.f_Insert(CVS_Setting_Item{{.m_Key = fg_Move(_Key), .m_Value = fg_Move(_Value.f_String())}});
							}
						)
					;
				}
				else
				{
					for (auto &SettingJSON : JSON[gc_ConstString_Settings].f_Array())
					{
						auto &Type = SettingJSON[gc_ConstString_Type].f_String();
						if (Type == gc_ConstString_PropertyGroup.m_String)
							VSSettingsVector.f_Insert(CVS_Setting_PropertyGroup::fs_FromJson(fg_Move(SettingJSON)));
						else if (Type == gc_ConstString_ItemDefinitionGroup.m_String)
							VSSettingsVector.f_Insert(CVS_Setting_ItemDefinitionGroup::fs_FromJson(fg_Move(SettingJSON)));
						else
							CBuildSystem::fs_ThrowError(Setting.m_PropertyInfo, CStr::CFormat("Unsupported VSSetting type: {}") << Type);
					}
				}

				if (pSharedCompileSettings)
				{
					auto *pSharedProperties = pSharedCompileSettings->m_SharedSettings.f_Settings().f_FindEqual(Configuration);
					if (pSharedProperties)
					{
						auto pSharedProperty = pSharedProperties->m_Properties.f_FindEqual(SettingName);
						if (pSharedProperty)
						{
							if (VSSettingsVector == pSharedProperty->m_VSSettings)
								continue;
							else
								pSharedProperties->m_NonSharedProperties.f_ExtractAndInsert(pSharedProperties->m_Properties, pSharedProperty);
						}
					}
				}

				auto &OutSettings = ReturnSettings.m_Properties[SettingName];
				OutSettings.m_Positions = fg_Move(Positions);
				OutSettings.m_VSSettings = fg_Move(VSSettingsVector);
			}
		}

		f_DestructSettings();

		return Return;
	}

	template <typename tf_CAllConfigs>
	CStr CGeneratorSettings::fs_GetConditionString(TCSet<CConfiguration> const &_Configurations, tf_CAllConfigs const &_AllConfigurations, CProject const &_Project)
	{
		bool bAllFound = true;
		for (auto &Config : _AllConfigurations)
		{
			auto &AllConfig = _AllConfigurations.fs_GetKey(Config);
			if (!_Configurations.f_FindEqual(AllConfig))
			{
				bAllFound = false;
				break;
			}
		}

		if (bAllFound)
			return {};

		TCSet<CConfiguration> const *pConfigurations = &_Configurations;
		TCSet<CConfiguration> TempConfigurations;
		CStr Condition;

		bool bInverted = false;

		if (_Configurations.f_GetLen() > _AllConfigurations.f_GetLen() / 2)
		{
			TempConfigurations = _AllConfigurations.f_KeySet().f_Difference(_Configurations);
			pConfigurations = &TempConfigurations;
			Condition = "!(";
			bInverted = true;
		}

		auto &Configurations = *pConfigurations;

		TCMap<CStr, TCLinkedList<CConfiguration const *>> ByPlatform;

		for (auto &Configuration : Configurations)
			ByPlatform[_Project.m_Platforms[Configuration]].f_Insert(&Configuration);

		bool bFirstPlatform = true;
		for (auto iPlatform = ByPlatform.f_GetIterator(); iPlatform; ++iPlatform)
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

				Condition += CStr::CFormat("'$(Configuration)'=='{}'") << (*iConfig)->m_Configuration;
				bFirst = false;
			}

			Condition += "))";
		}

		if (bInverted)
			Condition += ")";

		return Condition;
	}

	template <bool tf_bCompile, bool tf_bIsItem>
	void CGeneratorSettings::fs_AddToXMLFiles(CProjectXMLState &_XMLState, CProject const &_Project, CParsedGeneratorSettings &&_Parsed, CItemState const *_pItemState)
	{
		auto &Settings = _Parsed.f_Settings();

		auto const *pAllConfigurations = [&]
			{
				if constexpr (tf_bCompile && tf_bIsItem)
					return &_Project.m_EnabledProjectConfigs;
				else
					return &Settings;
			}
			()
		;		

		CVSSettingAggregated AggregatedSettings;

		CGeneratorSettingsVSType const *pSharedCompileSettings = nullptr;
		if constexpr (tf_bIsItem && tf_bCompile)
			pSharedCompileSettings = _pItemState->m_pCompile->f_FindEqual(_Parsed.m_VSType);

		for (auto &Setting : Settings)
		{
			auto &Configuration = Settings.fs_GetKey(Setting);
						
			if constexpr (tf_bIsItem && tf_bCompile)
			{
				do
				{
					if (!pSharedCompileSettings)
						break;

					if (auto *pSharedProperties = pSharedCompileSettings->m_SharedSettings.f_Settings().f_FindEqual(Configuration))
					{
						for (auto &Property : pSharedProperties->m_NonSharedProperties)
						{
							auto &SettingName = pSharedProperties->m_NonSharedProperties.fs_GetKey(Property);

							if (Setting.m_Properties.f_FindEqual(SettingName))
								continue;

							auto &OutSetting = AggregatedSettings.m_AggregatedSettings[SettingName];
							auto &OutSettings = OutSetting.m_Settings[Property.m_VSSettings];
							OutSettings.m_Positions.f_AddPositions(Property.m_Positions);
							OutSettings.m_Configurations[Configuration];
						}				
					}

					auto pSpecificSettings = pSharedCompileSettings->m_SpecificSettings.f_FindEqual(_Parsed.m_Type);
					if (!pSpecificSettings)
						break;

					auto *pProperties = pSpecificSettings->f_Settings().f_FindEqual(Configuration);
					if (!pProperties)
						break;

					for (auto &Property : pProperties->m_Properties)
					{
						auto &SettingName = pProperties->m_Properties.fs_GetKey(Property);

						if (Setting.m_Properties.f_FindEqual(SettingName))
							continue;

						auto &OutSetting = AggregatedSettings.m_AggregatedSettings[SettingName];
						auto &OutSettings = OutSetting.m_Settings[Property.m_VSSettings];
						OutSettings.m_Positions.f_AddPositions(Property.m_Positions);
						OutSettings.m_Configurations[Configuration];
					}
				}
				while (false)
					;
			}

			Setting.m_Properties.f_ExtractAll
				(
					[&](auto &&_Node)
					{
						auto &SettingName = _Node.f_Key();
						auto &Setting = _Node.f_Value();
						auto &OutSetting = AggregatedSettings.m_AggregatedSettings[fg_Move(SettingName)];
						auto &OutSettings = OutSetting.m_Settings[fg_Move(Setting.m_VSSettings)];
						OutSettings.m_Positions.f_AddPositions(fg_Move(Setting.m_Positions));
						OutSettings.m_Configurations[Configuration];
					}
				)
			;
		}

		TCSet<CConfiguration> AllConfigurationsNonDisabled;
		bool bAllConfigurationsNonDisabledValid = false;

		if constexpr (tf_bCompile && tf_bIsItem)
		{
			for (auto &Value : *pAllConfigurations)
			{
				auto &Configuration = pAllConfigurations->fs_GetKey(Value);

				if (Settings.f_FindEqual(Configuration))
					continue;

				auto &OutSetting = AggregatedSettings.m_AggregatedSettings[gc_ConstString_Disabled.m_String];
				auto &OutSettings = OutSetting.m_Settings[ms_ExcludedFromBuildVSSettingsTrue];
				OutSetting.m_Settings.f_Remove(ms_ExcludedFromBuildVSSettingsFalse);

				OutSettings.m_Configurations[Configuration];
			}

			if (auto *pDisabled = AggregatedSettings.m_AggregatedSettings.f_FindEqual(gc_ConstString_Disabled.m_String))
			{
				if (auto *pSetting = pDisabled->m_Settings.f_FindEqual(ms_ExcludedFromBuildVSSettingsTrue))
				{
					AllConfigurationsNonDisabled = pAllConfigurations->f_KeySet();
					AllConfigurationsNonDisabled -= pSetting->m_Configurations;
					bAllConfigurationsNonDisabledValid = true;
				}
			}
		}

		CXMLElement *pDefaultParentElement = nullptr;
		if constexpr (tf_bIsItem)
			pDefaultParentElement = _pItemState->m_pItemElement;
		else if constexpr (tf_bCompile)
		{
			if (_Project.m_LanguageType == ELanguageType_Native)
			{
				auto *pGroup = _XMLState.m_ItemDefinitionGroups.f_FindEqual(_Parsed.m_VSType);
				if (pGroup)
					pDefaultParentElement = *pGroup;
				else
				{
					pDefaultParentElement = CXMLDocument::f_CreateElement(_XMLState.m_pPropsItemDefinitionGroup, _Parsed.m_VSType);
					_XMLState.m_ItemDefinitionGroups[_Parsed.m_VSType] = pDefaultParentElement;
				}
			}
			else
				pDefaultParentElement = _XMLState.m_pPropsPropertyGroup;
		}

		for (auto &Aggregate : AggregatedSettings.m_AggregatedSettings)
		{
			for (auto &AppliedTo : Aggregate.m_Settings)
			{
				auto &Settings = Aggregate.m_Settings.fs_GetKey(AppliedTo);

				CStr Condition;

				if constexpr (tf_bCompile && tf_bIsItem)
				{
					if (bAllConfigurationsNonDisabledValid)
						Condition = fs_GetConditionString(AppliedTo.m_Configurations, AllConfigurationsNonDisabled, _Project);
					else
						Condition = fs_GetConditionString(AppliedTo.m_Configurations, *pAllConfigurations, _Project);
				}
				else
					Condition = fs_GetConditionString(AppliedTo.m_Configurations, *pAllConfigurations, _Project);

				for (auto &Setting : Settings)
				{
					CVS_SettingShared const *pSetting = nullptr;
					CXMLElement *pParentElement = pDefaultParentElement;
					CStr Value;
					Setting.f_Visit
						(
							[&](auto &_Setting)
							{
								Value = _Setting.m_Value;
								pSetting = &_Setting;

								using CType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Setting)>::CType;
								if constexpr (TCIsSame<CType, CVS_Setting_Item>::mc_Value)
								{
									if constexpr (tf_bCompile || tf_bIsItem)
										Value = Value.f_Replace(gc_ConstString_Symbol_Inherit.m_String, (CFStr128::CFormat("%({})") << _Setting.m_Key).f_GetStr());
									else
										DMibNeverGetHere; // Only supported for compile
								}
								else if constexpr (TCIsSame<CType, CVS_Setting_PropertyGroup>::mc_Value)
								{
									if constexpr (tf_bCompile || tf_bIsItem)
										DMibNeverGetHere; // not supported for compile
									else
									{
										Value = Value.f_Replace(gc_ConstString_Symbol_Inherit.m_String, (CFStr128::CFormat("$({})") << _Setting.m_Key).f_GetStr());

										if (!_Setting.m_Label)
											pParentElement = _XMLState.m_pPropsPropertyGroup;
										else if (_Setting.m_Label == gc_ConstString_Configuration.m_String)
											pParentElement = _XMLState.m_pConfiguration;
										else if (_Setting.m_Label == gc_ConstString_Globals.m_String)
											pParentElement = _XMLState.m_pGlobals;
										else if (_Setting.m_Label == gc_ConstString_PreProject.m_String)
											pParentElement = _XMLState.m_pPreProject;
										else
										{
											auto pLabelGroup = _XMLState.m_PropertyGroupLabels.f_FindEqual(_Setting.m_Label);
											if (!pLabelGroup)
											{
												pParentElement = CXMLDocument::f_CreateElement(_XMLState.m_pPropsProject, gc_ConstString_PropertyGroup);
												CXMLDocument::f_SetAttribute(pParentElement, gc_ConstString_Label, _Setting.m_Label);
												_XMLState.m_PropertyGroupLabels[_Setting.m_Label] = pParentElement;
											}
											else
												pParentElement = *pLabelGroup;
										}
									}
								}
								else if constexpr (TCIsSame<CType, CVS_Setting_ItemDefinitionGroup>::mc_Value)
								{
									if constexpr (tf_bCompile || tf_bIsItem)
										DMibNeverGetHere; // not supported for compile
									else
									{
										Value = Value.f_Replace(gc_ConstString_Symbol_Inherit.m_String, (CFStr128::CFormat("%({}.{})") << _Setting.m_Name << _Setting.m_Key).f_GetStr());
										auto *pGroup = _XMLState.m_ItemDefinitionGroups.f_FindEqual(_Setting.m_Name);
										if (pGroup)
											pParentElement = *pGroup;
										else
										{
											pParentElement = CXMLDocument::f_CreateElement(_XMLState.m_pPropsItemDefinitionGroup, _Setting.m_Name);
											_XMLState.m_ItemDefinitionGroups[_Setting.m_Name] = pParentElement;
										}
									}
								}
								else
								{
									static_assert(TCIsSame<CType, void>::mc_Value, "Implement this");
								}
							}
						)
					;

					if constexpr (tf_bIsItem)
					{
						if (pSetting->m_Key.f_StartsWith("@"))
						{
							if (!Condition.f_IsEmpty())
								CBuildSystem::fs_ThrowError(AppliedTo.m_Positions, "Attribute propreties cannot be varied per configuration");

							CXMLDocument::f_SetAttribute(pParentElement, pSetting->m_Key.f_Extract(1), Value);
							continue;
						}
					}

					auto pMainValue = CXMLDocument::f_AddElementAndText(pParentElement, pSetting->m_Key, Value);
					if (!Condition.f_IsEmpty())
						CXMLDocument::f_SetAttribute(pMainValue, gc_ConstString_Condition, Condition);
				}
			}
		}

		_Parsed.f_DestructSettings();
	}
}
