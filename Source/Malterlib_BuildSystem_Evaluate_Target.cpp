// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_EvaluateTarget
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntityMutablePointer> const &_Targets
			, CEntity &_Entity
			, TCSet<CStr> &_AlreadyAddedGroups
		) const
	{
		TCLinkedList<CEntity *> ToEval;
		fp_EvaluateTarget(_Destination, _Targets, ToEval, _Entity, _AlreadyAddedGroups);

		for (auto iChild = ToEval.f_GetIterator(); iChild;)
		{
			fpr_EvaluateData(**iChild);
			++iChild;
		}
	}

	CEntity *CBuildSystem::fpr_FindChildTarget(CEntity &_Entity, CEntityKey const &_EntityKey) const
	{
		for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
		{
			auto &ChildEntity = *iEntity;
			auto &Key = ChildEntity.f_GetKey();
			switch (Key.m_Type)
			{
			case EEntityType_Import:
			case EEntityType_Group:
				{
					auto pRet = fpr_FindChildTarget(ChildEntity, _EntityKey);
					if (pRet)
						return pRet;
				}
				break;
			case EEntityType_Target:
				{
					if (Key == _EntityKey)
						return &ChildEntity;
				}
				break;
			default:
				break;
			}
		}
		return nullptr;
	}

	void CBuildSystem::fp_UpdateDependenciesNames(CEntity *_pTargetOuterEntity) const
	{
		TCVector<CStr> Dependencies;

		TCFunction<void (CEntity &_Entity)> fFindDependencies
			= [&](CEntity &_Entity)
			{
				for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
				{
					auto &ChildEntity = *iEntity;
					auto &Key = ChildEntity.f_GetKey();

					switch (Key.m_Type)
					{
					case EEntityType_Group:
					case EEntityType_Import:
					case EEntityType_Target:
						{
							fFindDependencies(fg_RemoveQualifiers(ChildEntity));
						}
						break;
					case EEntityType_Dependency:
						{
							if (Key.m_Name.f_IsConstantString())
								Dependencies.f_Insert(Key.m_Name.f_ConstantString());
						}
						break;
					default:
						break;
					}
				}
			}
		;

		fFindDependencies(*_pTargetOuterEntity);

		Dependencies.f_Sort();
		
		CEJSONSorted DependenciesJSON = EJSONType_Array;
		for (auto &Dependency : Dependencies)
			DependenciesJSON.f_Insert(Dependency);

		f_AddExternalProperty
			(
				*_pTargetOuterEntity
				, gc_ConstKey_Target_DependenciesNames
				, fg_Move(DependenciesJSON)
			)
		;
	}

	void CBuildSystem::fp_EvaluateTarget
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntityMutablePointer> const &_Targets
			, TCLinkedList<CEntity *> &_ToEval
			, CEntity &_Entity
			, TCSet<CStr> &_AlreadyAddedGroups
		) const
	{
		auto const &Name = _Entity.f_GetKeyName();
		auto &EntityData = _Entity.f_Data();

		if (Name.f_IsEmpty())
			fsp_ThrowError(EntityData.m_Position, "No name specified for target reference");

		CEntityKey EntityKey;
		EntityKey.m_Type = EEntityType_Target;
		EntityKey.m_Name.m_Value = Name;
		CEntity *pChildEntity = fpr_FindChildTarget(_Entity, EntityKey);
		bool bWasCreated = false;
		if (!pChildEntity)
		{
			bWasCreated = true;
			auto pTarget = _Targets.f_FindEqual(Name);
			if (!pTarget)
				fsp_ThrowError(EntityData.m_Position, CStr::CFormat("Target {} not found") << Name);

			auto &Target = **pTarget;

			// Add project as a sub entry
			TCLinkedList<CEntity *> ChildEntities;
			for (auto pSourceParent = Target.m_pParent; pSourceParent->f_GetKey().m_Type != EEntityType_Root; pSourceParent = pSourceParent->m_pParent)
				ChildEntities.f_InsertFirst(pSourceParent);

			auto pParent = &_Entity;
			CEntity *pToEval = nullptr;
			for (auto iSourceParent = ChildEntities.f_GetIterator(); iSourceParent; ++iSourceParent)
			{
				CEntity const *pSource;
				pSource = *iSourceParent;

				CEntity *pNewEntity = &pParent->m_ChildEntitiesMap(pSource->f_GetKey(), *pSource, pParent, EEntityCopyFlag_None).f_GetResult();

				if (pToEval == nullptr)
					pToEval = pNewEntity;

				f_AddExternalProperty(*pNewEntity, gc_ConstKey_HiddenGroup, true);

				pParent = pNewEntity;
			}


			CEntity const *pSource;
			pSource = &Target;

			auto *pNewEntity = &pParent->m_ChildEntitiesMap(Target.f_GetKey(), *pSource, pParent, EEntityCopyFlag_CopyChildren).f_GetResult();

			if (!f_EvalCondition(*pNewEntity, pNewEntity->f_Data().m_Condition, Target.f_Data().m_DebugFlags & EPropertyFlag_TraceCondition))
				fsp_ThrowError(pNewEntity->f_Data().m_Position, fg_Format("Target {} is disabled", Name));

			if (pToEval == nullptr)
				_ToEval.f_Insert(pNewEntity);
			else
				_ToEval.f_Insert(pToEval);

			pChildEntity = pNewEntity;
		}

		TCSet<CEntity *> ToEval;

		auto fNewCreated = [&](CEntity *_pEntity)
			{
				if (bWasCreated)
					return;

				if (ToEval.f_IsEmpty())
				{
					ToEval[_pEntity];
					return;
				}

				bool bMoreRoot = false;
				for (auto *pExisting : ToEval)
				{
					if (_pEntity->f_HasParent(pExisting))
						return;

					if (pExisting->f_HasParent(_pEntity))
					{
						bMoreRoot = true;
						break;
					}
				}
				if (bMoreRoot)
				{
					for (auto iToEval = ToEval.f_GetIterator(); iToEval;)
					{
						if ((*iToEval)->f_HasParent(_pEntity))
							iToEval.f_Remove();
						else
							++iToEval;
					}
				}
				ToEval[_pEntity];
			}
		;

		bool bAdded = true;
		while (bAdded)
		{
			fp_UpdateDependenciesNames(pChildEntity);

			bAdded = false;
			CBuildSystemPropertyInfo PropertyInfo;
			PropertyInfo.m_pFallbackPosition = &pChildEntity->f_GetFirstValidPosition();

			auto ExtraGroups = f_EvaluateEntityPropertyUncachedStringArray(*pChildEntity, gc_ConstKey_Target_ExtraGroups, PropertyInfo, nullptr, TCVector<CStr>());
			auto InjectedGroups = f_EvaluateEntityPropertyUncachedStringArray(*pChildEntity, gc_ConstKey_Target_InjectedExtraGroups, PropertyInfo, nullptr, TCVector<CStr>());
			ExtraGroups.f_Insert(fg_Move(InjectedGroups));

			for (auto &Group : ExtraGroups)
			{
				if (Group.f_IsEmpty() || _AlreadyAddedGroups.f_FindEqual(Group))
					continue;

				bAdded = true;
				_AlreadyAddedGroups[Group];

				CStr OriginalGroup = Group;

				bool bAddedCreated = false;

				auto pParentGroup = pChildEntity;

				while (Group.f_FindChar('/') >= 0)
				{
					CStr ParentGroup = fg_GetStrSep(Group, "/");
					if (ParentGroup.f_StartsWith("{"))
						continue;

					CEntityKey Key;
					Key.m_Name.m_Value = ParentGroup;
					Key.m_Type = EEntityType_Group;
					auto Child = pParentGroup->m_ChildEntitiesMap(Key, pParentGroup);
					if (!bAddedCreated)
					{
						bAddedCreated = true;
						fNewCreated(&*Child);
					}
					(*Child).f_DataWritable().m_Position = pParentGroup->f_Data().m_Position;

					pParentGroup = &*Child;
				}

				CEntityKey EntityKey;
				EntityKey.m_Type = EEntityType_Group;
				EntityKey.m_Name.m_Value = OriginalGroup;

				auto pGroup = fg_Const(_Destination).m_RootEntity.m_ChildEntitiesMap.f_FindEqual(EntityKey);
				if (!pGroup)
					fs_ThrowError(PropertyInfo, EntityData.m_Position, CStr::CFormat("Extra group '{}' not found") << Group);

				auto &GroupEntity = *pGroup;

				if (Group.f_StartsWith("{"))
				{
					if (!bAddedCreated)
					{
						bAddedCreated = true;
						fNewCreated(pParentGroup);
					}

					pParentGroup->f_CopyEntities(GroupEntity, EEntityCopyFlag_MergeEntities);
					pParentGroup->f_CopyProperties(GroupEntity);
				}
				else
				{
					auto Key = GroupEntity.f_GetKey();
					Key.m_Name.m_Value = Group;

					if (pParentGroup->m_ChildEntitiesMap.f_FindEqual(Key))
						fs_ThrowError(PropertyInfo, EntityData.m_Position, CStr::CFormat("Duplicate extra group '{}'") << Group);

					CEntity const *pSource = &GroupEntity;

					auto Child = pParentGroup->m_ChildEntitiesMap(Key, *pSource, pParentGroup, EEntityCopyFlag_CopyChildren);

					if (!bAddedCreated)
					{
						bAddedCreated = true;
						fNewCreated(&*Child);
					}
				}
			}
		}
		
		for (auto *pToEval : ToEval)
			_ToEval.f_Insert(pToEval);
	}
}
