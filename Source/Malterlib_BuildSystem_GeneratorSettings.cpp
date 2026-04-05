// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CGeneratorSettings::fs_PopulateSetting
		(
			CPropertyKeyReference const &_GeneratorSetting
			, EPropertyType _PropertyType
			, CBuildSystem const &_BuildSystem
			, CEntity &_Entity
			, CGeneratorSetting &o_Result
		)
	{
		bool bIsFile = _Entity.f_GetKey().m_Type == EEntityType_File;

		CEJsonSorted DefinedProperties;
		if (bIsFile)
			DefinedProperties = _BuildSystem.f_GetDefinedProperties<true>(_Entity, _PropertyType, o_Result.m_bIsFullEval);
		else
			DefinedProperties = _BuildSystem.f_GetDefinedProperties<false>(_Entity, _PropertyType, o_Result.m_bIsFullEval);

		{
			auto &EvalProperty = _Entity.m_EvaluatedProperties.m_Properties[gc_ConstKey_GeneratorSetting_DefinedProperties];
			EvalProperty.m_Value = fg_Move(DefinedProperties);
			EvalProperty.m_Type = EEvaluatedPropertyType_External;
			EvalProperty.m_pProperty = &_BuildSystem.f_ExternalProperty(EPropertyType_GeneratorSetting);
		}

		if (_PropertyType == EPropertyType_Compile)
		{
			auto &EvalProperty = _Entity.m_EvaluatedProperties.m_Properties[gc_ConstKey_GeneratorSetting_IsFile];
			EvalProperty.m_Value = bIsFile;
			EvalProperty.m_Type = EEvaluatedPropertyType_External;
			EvalProperty.m_pProperty = &_BuildSystem.f_ExternalProperty(EPropertyType_GeneratorSetting);
		}

		auto Cleanup = g_OnScopeExit / [&]
			{
				_Entity.m_EvaluatedProperties.m_Properties.f_Remove(gc_ConstKey_GeneratorSetting_DefinedProperties);
				_Entity.m_EvaluatedProperties.m_Properties.f_Remove(gc_ConstKey_GeneratorSetting_IsFile);
				_Entity.m_EvaluatedProperties.m_Properties.f_Remove(_GeneratorSetting);
			}
		;

		o_Result.m_Value = _BuildSystem.f_EvaluateEntityProperty
			(
				_Entity
				, _GeneratorSetting
				, o_Result.m_PropertyInfo
			)
			.f_Move();
		;
	}

	TCUnsafeFuture<void> CGeneratorSettings::f_PopulateSettings
		(
			CPropertyKeyReference const &_GeneratorSetting
			, EPropertyType _PropertyType
			, CBuildSystem const &_BuildSystem
			, TCMap<CConfiguration, CEntityMutablePointer> const &_EntitiesPerConfig
		)
	{
		co_await ECoroutineFlag_CaptureExceptions;

		f_ConstructSettings();

		auto &Settings = f_Settings();

		for (auto &Entity : _EntitiesPerConfig)
			Settings[_EntitiesPerConfig.fs_GetKey(Entity)];

		co_await fg_ParallelForEach
			(
				Settings
				, [&](CGeneratorSetting &o_Result) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureExceptions;
					co_await _BuildSystem.f_CheckCancelled();

					auto &Config = Settings.fs_GetKey(o_Result);

					CGeneratorSettings::fs_PopulateSetting(_GeneratorSetting, _PropertyType, _BuildSystem, **_EntitiesPerConfig.f_FindEqual(Config), o_Result);

					co_return {};
				}
				, _BuildSystem.f_SingleThreaded()
			)
		;
		co_await _BuildSystem.f_CheckCancelled();

		co_return {};
	}
}
