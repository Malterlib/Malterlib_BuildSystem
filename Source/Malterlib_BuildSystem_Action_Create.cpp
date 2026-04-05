// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Process/StdIn>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
		struct CCreateTemplate
		{
			CFilePosition m_Position;
			CEntity *m_pEntity = nullptr;
		};

		TCMap<CStr, CCreateTemplate> fg_GetCreateTemplates(CBuildSystem &_BuildSystem, CBuildSystemData &_Data)
		{
			TCMap<CStr, CCreateTemplate> CreateTemplates;

			for (auto &ChildEntity : _Data.m_RootEntity.m_ChildEntitiesOrdered)
			{
				if (ChildEntity.f_GetKey().m_Type != EEntityType_CreateTemplate)
					continue;

				auto &ChildEntityData = ChildEntity.f_Data();

				if (!_BuildSystem.f_EvalCondition(ChildEntity, ChildEntityData.m_Condition, ChildEntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
					continue;

				CStr Name = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_CreateTemplate_Name, CStr());

				if (Name.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntityData.m_Position, "You have to specify CreateTemplate.Name");

				auto Mapped = CreateTemplates(Name);
				if (!Mapped.f_WasCreated())
				{
					_BuildSystem.fs_ThrowError
						(
							ChildEntityData.m_Position
							, "Create template with name '{}' already exists"_f << Name
							, TCVector<CBuildSystemError>
							{
								CBuildSystemError
								{
									CBuildSystemUniquePositions((*Mapped).m_Position, "Create template")
									, "Previous version"
								}
							}
						)
					;
				}

				auto &CreateTemplate = *Mapped;

				CreateTemplate.m_Position = ChildEntityData.m_Position;
				CreateTemplate.m_pEntity = &ChildEntity;
			}

			return CreateTemplates;
		}
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Create(CGenerateOptions const &_GenerateOptions)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		f_InitEntityForEvaluation(mp_Data.m_RootEntity, GenerateState.m_GeneratorValues);
		f_ExpandCreateTemplateEntities(mp_Data);

		TCMap<CStr, CCreateTemplate> CreateTemplates = fg_GetCreateTemplates(*this, mp_Data);

/*		for (auto &CreateTemplate : CreateTemplates)
		{

		}*/
		co_return ERetry_None;
	}
}
