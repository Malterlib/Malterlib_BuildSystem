// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>
#include <Mib/Storage/Reference>

namespace NMib::NBuildSystem::NVisualStudio
{
	TCUnsafeFuture<TCMap<CStr, CGeneratorSettingsVSType>> CGeneratorInstance::f_GenerateProjectFile_FileTypes
		(
			CProject &_Project
			, CProjectState &_ProjectState
			, TCMap<CStr, CCompileType> const &_CompileTypes
		) const
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		TCMap<CStr, CGeneratorSettings> CompileTypeSettings;
		for (auto &CompileType : _CompileTypes)
		{
			auto &Type = _CompileTypes.fs_GetKey(CompileType);

			if (CompileType.m_EnabledConfigs.f_IsEmpty())
				continue;

			auto &OutSettings = CompileTypeSettings[Type];

			auto &Settings = OutSettings.f_ConstructSettings();

			for (auto &Config : CompileType.m_EnabledConfigs)
				Settings[CompileType.m_EnabledConfigs.fs_GetKey(Config)];
		}

		co_await fg_ParallelForEach
			(
				_Project.m_EnabledProjectConfigs
				, [&](auto &_pEntity) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					co_await m_BuildSystem.f_CheckCancelled();

					auto &Configuration = _Project.m_EnabledProjectConfigs.fs_GetKey(_pEntity);

					for (auto &Settings : CompileTypeSettings)
					{
						auto &Type = CompileTypeSettings.fs_GetKey(Settings);

						auto &ResultSettings = Settings.f_Settings();
						auto *pResult = ResultSettings.f_FindEqual(Configuration);
						if (!pResult)
							continue;

						TCMap<CPropertyKey, CEJsonSorted> StartValuesCompile;
						StartValuesCompile[gc_ConstKey_Compile_Type] = Type;

						TCVector<CEntity *> ToRemove;
						auto Cleanup = g_OnScopeExit / [&]
							{
								for (auto &pToRemove : ToRemove)
									pToRemove->m_pParent->m_ChildEntitiesMap.f_Remove(pToRemove->f_GetKey());
							}
						;

						auto &ConfigEntity = *_pEntity;
						CEntityKey NewEntityKey = ConfigEntity.f_GetKey();

						auto NewEntityMap = ConfigEntity.m_ChildEntitiesMap(NewEntityKey, &ConfigEntity);
						auto &TempEntity = *NewEntityMap;
						TempEntity.f_DataWritable().m_Position = ConfigEntity.f_Data().m_Position;

						ToRemove.f_Insert(&TempEntity);

						m_BuildSystem.f_InitEntityForEvaluationNoEnv(TempEntity, StartValuesCompile, EEvaluatedPropertyType_External);

						CGeneratorSettings::fs_PopulateSetting(gc_ConstKey_GeneratorSetting_Compile, EPropertyType_Compile, m_BuildSystem, TempEntity, *pResult);
					}

					co_return {};
				}
				, m_BuildSystem.f_SingleThreaded()
			)
		;
		co_await m_BuildSystem.f_CheckCancelled();

		TCMap<CStr, CGeneratorSettingsVSType> CompileTypeSettingsPerVSType;

		// Compile types
		for (auto &Settings : CompileTypeSettings)
		{
			auto &Type = CompileTypeSettings.fs_GetKey(Settings);

			auto ParsedSettings = Settings.f_GetParsedVSSettings<true, true>(nullptr);
			auto &VSType = ParsedSettings.m_VSType;

			auto &SettingsVSType = CompileTypeSettingsPerVSType[VSType];
			if (!SettingsVSType.m_SharedSettings.f_SettingsConstructed())
				SettingsVSType.m_SharedSettings.f_ConstructSettings();
			SettingsVSType.m_SharedSettings.m_VSType = VSType;

			SettingsVSType.m_SpecificSettings[Type] = fg_Move(ParsedSettings);
		}

		for (auto &SettingsVSType : CompileTypeSettingsPerVSType)
		{
			if (SettingsVSType.m_SpecificSettings.f_HasOneElement())
			{
				SettingsVSType.m_SharedSettings = fg_Move(*SettingsVSType.m_SpecificSettings.f_FindAny());
				SettingsVSType.m_SpecificSettings.f_Clear();
				continue;
			}

			TCMap<CConfiguration, TCMap<CStr, TCMap<TCReference<TCVector<CGeneratorSettings::CVS_Setting>>, zmint>>> AllProperties;

			umint nSettings = SettingsVSType.m_SpecificSettings.f_GetLen();

			for (auto &Settings : SettingsVSType.m_SpecificSettings)
			{
				for (auto &Setting : Settings.f_Settings())
				{
					auto &Configuration = TCMap<CConfiguration, CGeneratorSettings::CParsedGeneratorSetting>::fs_GetKey(Setting);
					auto &Properties = AllProperties[Configuration];

					for (auto &VSSetting : Setting.m_Properties)
						++Properties[Setting.m_Properties.fs_GetKey(VSSetting)][fg_Reference(VSSetting.m_VSSettings)];
				}
			}

			for (auto &Settings : SettingsVSType.m_SpecificSettings)
			{
				for (auto &Setting : Settings.f_Settings())
				{
					auto &Configuration = TCMap<CConfiguration, CGeneratorSettings::CParsedGeneratorSetting>::fs_GetKey(Setting);
					auto &Properties = fg_Const(AllProperties)[Configuration];
					auto &SharedParsedSettings = SettingsVSType.m_SharedSettings.f_Settings()[Configuration];

					for (auto iVSSetting = Setting.m_Properties.f_GetIterator(); iVSSetting;)
					{
						auto &SettingsName = iVSSetting.f_GetKey();
						auto &VSSetting = *iVSSetting;
						auto &Aggregated = Properties[SettingsName];
						if (Aggregated.f_HasOneElement() && (*Aggregated.f_FindAny()) == nSettings)
						{
							SharedParsedSettings.m_Properties(SettingsName, fg_Move(VSSetting));
							iVSSetting.f_Remove();
						}
						else
							++iVSSetting;
					}
				}
			}
		}

		co_return fg_Move(CompileTypeSettingsPerVSType);
	}

	void CGeneratorInstance::f_GenerateProjectFile_AddToXML_FileTypes
		(
			CProject &_Project
			, CProjectState &_ProjectState
			, CProjectXMLState &_XMLState
			, TCMap<CStr, CGeneratorSettingsVSType> &&_Compile
		) const
	{
		for (auto &SettingsVSType : _Compile)
			CGeneratorSettings::fs_AddToXMLFiles<true, false>(_XMLState, _Project, fg_Move(SettingsVSType.m_SharedSettings), nullptr);
	}
}
