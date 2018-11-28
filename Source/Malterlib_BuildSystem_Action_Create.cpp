// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
				if (ChildEntity.m_Key.m_Type != EEntityType_CreateTemplate)
					continue;
				if (!_BuildSystem.f_EvalCondition(ChildEntity, ChildEntity.m_Condition))
					continue;

				CStr Name = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_CreateTemplate, "Name");

				if (Name.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify CreateTemplate.Name");

				auto Mapped = CreateTemplates(Name);
				if (!Mapped.f_WasCreated())
				{
					_BuildSystem.fs_ThrowError
						(
						 	ChildEntity.m_Position
						 	, "Create template with name '{}' already exists"_f << Name
						  	, TCVector<CBuildSystemError>
						 	{
								CBuildSystemError
								{
									(*Mapped).m_Position
									, "Previous version"
								}
							}
						)
					;
				}

				auto &CreateTemplate = *Mapped;

				CreateTemplate.m_Position = ChildEntity.m_Position;
				CreateTemplate.m_pEntity = &ChildEntity;
			}

			return CreateTemplates;
		}
	}

	CBuildSystem::ERetry CBuildSystem::f_Action_Create(CGenerateOptions const &_GenerateOptions)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;

		f_InitEntityForEvaluation(mp_Data.m_RootEntity, GenerateState.m_GeneratorValues);
		f_ExpandCreateTemplateEntities(mp_Data);

		TCMap<CStr, CCreateTemplate> CreateTemplates = fg_GetCreateTemplates(*this, mp_Data);

/*		for (auto &CreateTemplate : CreateTemplates)
		{

		}*/
		return ERetry_None;
	}
}
