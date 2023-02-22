// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CGeneratorSettings::f_PopulateSetting
		(
			CPropertyKeyReference const &_GeneratorSetting
			, EPropertyType _PropertyType
			, CBuildSystem const &_BuildSystem
			, TCMap<CConfiguration, CEntityMutablePointer> const &_EntitiesPerConfig
			, CGeneratorSetting &o_Result
		)
	{
		auto &Config = TCMap<CConfiguration, CGeneratorSetting>::fs_GetKey(o_Result);
		auto &Entity = _EntitiesPerConfig[Config];

		auto *pConfig = Entity.f_Get();

		bool bIsFile = Entity.f_Get()->f_GetKey().m_Type == EEntityType_File;

		CEJSONSorted DefinedProperties;
		if (bIsFile)
			DefinedProperties = _BuildSystem.f_GetDefinedProperties<true>(*pConfig, _PropertyType);
		else
			DefinedProperties = _BuildSystem.f_GetDefinedProperties<false>(*pConfig, _PropertyType);

		{
			auto &EvalProperty = pConfig->m_EvaluatedProperties.m_Properties[gc_ConstKey_GeneratorSetting_DefinedProperties];
			EvalProperty.m_Value = fg_Move(DefinedProperties);
			EvalProperty.m_Type = EEvaluatedPropertyType_External;
			EvalProperty.m_pProperty = &_BuildSystem.f_ExternalProperty(EPropertyType_GeneratorSetting);
		}

		if (_PropertyType == EPropertyType_Compile)
		{
			auto &EvalProperty = pConfig->m_EvaluatedProperties.m_Properties[gc_ConstKey_GeneratorSetting_IsFile];
			EvalProperty.m_Value = bIsFile;
			EvalProperty.m_Type = EEvaluatedPropertyType_External;
			EvalProperty.m_pProperty = &_BuildSystem.f_ExternalProperty(EPropertyType_GeneratorSetting);
		}

		auto Cleanup = g_OnScopeExit / [&]
			{
				pConfig->m_EvaluatedProperties.m_Properties.f_Remove(gc_ConstKey_GeneratorSetting_DefinedProperties);
				pConfig->m_EvaluatedProperties.m_Properties.f_Remove(gc_ConstKey_GeneratorSetting_IsFile);
				pConfig->m_EvaluatedProperties.m_Properties.f_Remove(_GeneratorSetting);
			}
		;

		o_Result.m_Value = _BuildSystem.f_EvaluateEntityProperty
			(
				*pConfig
				, _GeneratorSetting
				, o_Result.m_PropertyInfo
			)
			.f_Move();
		;
	}

	TCFuture<void> CGeneratorSettings::f_PopulateSettings
		(
			CPropertyKeyReference const &_GeneratorSetting
			, EPropertyType _PropertyType
			, CBuildSystem const &_BuildSystem
			, TCMap<CConfiguration, CEntityMutablePointer> const &_EntitiesPerConfig
		)
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureExceptions);

		f_ConstructSettings();

		auto &Settings = f_Settings();

		for (auto &Entity : _EntitiesPerConfig)
			Settings[_EntitiesPerConfig.fs_GetKey(Entity)];

		co_await fg_ParallelForEach
			(
				Settings
				, [&](CGeneratorSetting &o_Result) -> TCFuture<void>
				{
					co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureExceptions);
					co_await _BuildSystem.f_CheckCancelled();

					f_PopulateSetting(_GeneratorSetting, _PropertyType, _BuildSystem, _EntitiesPerConfig, o_Result);

					co_return {};
				}
				, _BuildSystem.f_SingleThreaded()
			)
		;
		co_await _BuildSystem.f_CheckCancelled();

		co_return {};
	}
}
