// Copyright © 2026 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Ninja.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>
#include <Mib/Storage/Reference>

namespace NMib::NBuildSystem::NNinja
{
	TCUnsafeFuture<void> CGeneratorInstance::fp_GenerateProjectFile_FileTypes
		(
			CProject &_Project
			, TCMap<CStr, CCompileType> const &_CompileTypes
		) const
	{
		co_await ECoroutineFlag_CaptureExceptions;

		for (auto &CompileType : _CompileTypes)
		{
			auto &Type = _CompileTypes.fs_GetKey(CompileType);

			if (CompileType.m_EnabledConfigs.f_IsEmpty())
				continue;

			for (auto &Config : CompileType.m_EnabledConfigs)
				_Project.m_Builds[Type].m_Builds[Config];
		}

		co_await fg_ParallelForEach
			(
				_Project.m_EnabledProjectConfigs
				, [&](auto &_pEntity) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureExceptions;
					co_await m_BuildSystem.f_CheckCancelled();

					auto &Configuration = _Project.m_EnabledProjectConfigs.fs_GetKey(_pEntity);

					auto *pTargetSettings = _Project.m_EvaluatedTargetSettings.f_FindEqual(Configuration);
					DMibFastCheck(pTargetSettings);
					auto &TargetSettings = *pTargetSettings;

					CGeneratorThreadLocalConfigScope ConfigScope(*ms_ThreadLocal, &TargetSettings);

					NTime::CCyclesStopwatch YieldStopwatch(true);

					auto OnResume = co_await fg_OnResume
						(
							[&]() -> CExceptionPointer
							{
								YieldStopwatch.f_Start();
								return {};
							}
						)
					;

					for (auto &Builds : _Project.m_Builds)
					{
						auto &Type = _Project.m_Builds.fs_GetKey(Builds);

						auto *pBuildConfig = Builds.m_Builds.f_FindEqual(Configuration);
						if (!pBuildConfig)
							continue;

						if (YieldStopwatch.f_GetCycles() > g_CooperativeTimeSliceCycles)
							co_await g_Yield;

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

						CGeneratorSetting Result;
						CGeneratorSettings::fs_PopulateSetting(gc_ConstKey_GeneratorSetting_Compile, EPropertyType_Compile, m_BuildSystem, TempEntity, Result);

						CBuildSystemPropertyInfo PropertyInfoValidateSettings;
						m_BuildSystem.f_EvaluateEntityProperty
							(
								TempEntity
								, gc_ConstKey_GeneratorSetting_CompileValidateSettings
								, PropertyInfoValidateSettings
							)
						;

						pBuildConfig->f_FromGeneratorSetting(m_BuildSystem, fg_Move(Result));
					}

					co_return {};
				}
				, m_BuildSystem.f_SingleThreaded()
			)
		;

		co_await m_BuildSystem.f_CheckCancelled();

		co_return {};
	}
}
