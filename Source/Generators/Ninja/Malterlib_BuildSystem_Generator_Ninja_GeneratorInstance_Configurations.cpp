// Copyright © 2026 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Ninja.h"

namespace NMib::NBuildSystem::NNinja
{
	TCUnsafeFuture<void> CGeneratorInstance::fp_EvaluateTargetSettings(CProject& _Project) const
	{
		co_await ECoroutineFlag_CaptureExceptions;

		for (auto &Entity : _Project.m_EnabledProjectConfigs)
		{
			auto &Config = _Project.m_EnabledProjectConfigs.fs_GetKey(Entity);
			_Project.m_EvaluatedTargetSettings[Config];
			_Project.m_CompileCommandsFilePath[Config];
		}

		co_await fg_ParallelForEach
			(
				_Project.m_EvaluatedTargetSettings
				, [&](CConfigResultTarget &o_Result) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureExceptions;
					co_await m_BuildSystem.f_CheckCancelled();

					auto &Config = TCMap<CConfiguration, CConfigResultTarget>::fs_GetKey(o_Result);
					auto &Entity = _Project.m_EnabledProjectConfigs[Config];

					fp_SetEvaluatedValuesTarget(Entity, o_Result, _Project.m_EntityName);

					_Project.m_CompileCommandsFilePath[Config] = fp_GetConfigValue
						(
							_Project.m_EnabledProjectConfigs
							, Config
							, gc_ConstKey_Target_CompileCommandsFile
							, EEJsonType_String
							, false
						)
						.m_Value.f_MoveString()
					;

					co_return {};
				}
				, m_BuildSystem.f_SingleThreaded()
			)
		;

		co_await m_BuildSystem.f_CheckCancelled();

		co_await g_Yield;

		co_return {};
	}
}
