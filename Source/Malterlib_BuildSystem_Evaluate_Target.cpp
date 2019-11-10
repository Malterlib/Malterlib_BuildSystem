// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_EvaluateTarget
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntity *> const &_Targets
			, CEntity &_Entity
		) const
	{
		DMibLock(_Entity.m_Lock);
		TCLinkedList<CEntity *> ToEval;
		
		fp_EvaluateTarget(_Destination, _Targets, ToEval, _Entity);
		
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
			switch (ChildEntity.m_Key.m_Type)
			{
			case EEntityType_Group:
				{
					auto pRet = fpr_FindChildTarget(ChildEntity, _EntityKey);
					if (pRet)
						return pRet;
				}
				break;
			case EEntityType_Target:
				{
					if (ChildEntity.m_Key == _EntityKey)
						return &ChildEntity;
				}
				break;
			default:
				break;
			}
		}
		return nullptr;
	}

	void CBuildSystem::fp_EvaluateTarget
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntity *> const &_Targets
			, TCLinkedList<CEntity *> &_ToEval
			, CEntity &_Entity
		) const
	{
		CStr Name = _Entity.m_Key.m_Name;

		if (Name.f_IsEmpty())
			fsp_ThrowError(_Entity.m_Position, "No name specified for target reference");

		CEntityKey EntityKey;
		EntityKey.m_Type = EEntityType_Target;
		EntityKey.m_Name = Name;
		CEntity *pChildEntity = fpr_FindChildTarget(_Entity, EntityKey);
		if (!pChildEntity)
		{
			auto pTarget = _Targets.f_FindEqual(Name);
			if (!pTarget)
				fsp_ThrowError(_Entity.m_Position, CStr::CFormat("Target {} not found") << Name);

			auto &Target = **pTarget;

			// Add project as a sub entry
			TCLinkedList<CEntity *> ChildEntities;
			for (auto pSourceParent = Target.m_pParent; pSourceParent->m_Key.m_Type != EEntityType_Root; pSourceParent = pSourceParent->m_pParent)
			{
				ChildEntities.f_InsertFirst(pSourceParent);
			}

			auto pParent = &_Entity;
			CEntity *pToEval = nullptr;
			for (auto iSourceParent = ChildEntities.f_GetIterator(); iSourceParent; ++iSourceParent)
			{
				auto *pNewEntity = &pParent->m_ChildEntitiesMap((*iSourceParent)->m_Key, pParent).f_GetResult();
				pParent->m_ChildEntitiesOrdered.f_Insert(*pNewEntity);
				if ((*iSourceParent)->m_pCopiedFrom)
					pNewEntity->f_CopyFrom(*(*iSourceParent)->m_pCopiedFrom, false);
				else
					pNewEntity->f_CopyFrom(**iSourceParent, false);
				if (pToEval == nullptr)
					pToEval = pNewEntity;

				f_AddExternalProperty(*pNewEntity, CPropertyKey(EPropertyType_Property, "HiddenGroup"), "true");
				
				pParent = pNewEntity;
			}

			auto *pNewEntity = &pParent->m_ChildEntitiesMap(Target.m_Key, pParent).f_GetResult();
			pParent->m_ChildEntitiesOrdered.f_Insert(*pNewEntity);
			if (Target.m_pCopiedFrom)
				pNewEntity->f_CopyFrom(*Target.m_pCopiedFrom, true);
			else
				pNewEntity->f_CopyFrom(Target, true);
			
			if (!f_EvalCondition(*pNewEntity, pNewEntity->m_Condition))
				fsp_ThrowError(pNewEntity->m_Position, fg_Format("Target {} is disabled", Name));


			if (pToEval == nullptr)
				_ToEval.f_Insert(pNewEntity);
			else
				_ToEval.f_Insert(pToEval);

			bool bAdded = true;
			TCSet<CStr> AlreadyAdded;
			while (bAdded)
			{
				bAdded = false;
				CProperty const *pFromProp = nullptr;
				CStr ExtraGroups = f_EvaluateEntityPropertyUncached(*pNewEntity, EPropertyType_Target, "ExtraGroups", pFromProp);

				while (!ExtraGroups.f_IsEmpty())
				{
					CStr Group = fg_GetStrSep(ExtraGroups, ";");
					
					if (Group.f_IsEmpty() || AlreadyAdded.f_FindEqual(Group))
						continue;
					
					bAdded = true;
					AlreadyAdded[Group];
					
					CStr OriginalGroup = Group;

					auto pParentGroup = pNewEntity;
					
					while (Group.f_FindChar('/') >= 0)
					{
						CStr ParentGroup = fg_GetStrSep(Group, "/");
						if (ParentGroup.f_StartsWith("{"))
							continue;
						
						CEntityKey Key;
						Key.m_Name = ParentGroup;
						Key.m_Type = EEntityType_Group;
						auto Child = pParentGroup->m_ChildEntitiesMap(Key, pParentGroup);
						
						if (Child.f_WasCreated())
						{
							(*Child).m_Key = Key;
							pParentGroup->m_ChildEntitiesOrdered.f_Insert(*Child);
						}
						pParentGroup = &*Child;
					}

					CEntityKey EntityKey;
					EntityKey.m_Type = EEntityType_Group;
					EntityKey.m_Name = OriginalGroup;

					auto pGroup = fg_Const(_Destination).m_RootEntity.m_ChildEntitiesMap.f_FindEqual(EntityKey);
					if (!pGroup)
					{
						if (_Destination.m_RootEntity.m_pCopiedFrom)
							pGroup = _Destination.m_RootEntity.m_pCopiedFrom->m_ChildEntitiesMap.f_FindEqual(EntityKey);
						if (!pGroup)
							fsp_ThrowError(pFromProp ? pFromProp->m_Position : _Entity.m_Position, CStr::CFormat("Extra group '{}' not found") << Group);
					}
					
					auto &GroupEntity = *pGroup;
					
					if (Group.f_StartsWith("{"))
					{
						if (GroupEntity.m_pCopiedFrom)
						{
							pParentGroup->f_CopyEntities(*GroupEntity.m_pCopiedFrom, true);
							pParentGroup->f_CopyProperties(*GroupEntity.m_pCopiedFrom);
							pParentGroup->m_pCopiedFrom = nullptr;
						}
						else
						{
							pParentGroup->f_CopyEntities(GroupEntity, true);
							pParentGroup->f_CopyProperties(GroupEntity);
							pParentGroup->m_pCopiedFrom = nullptr;
						}
					}
					else
					{
						auto Key = GroupEntity.m_Key;
						Key.m_Name = Group;

						if (pParentGroup->m_ChildEntitiesMap.f_FindEqual(Key))
							fsp_ThrowError(pFromProp ? pFromProp->m_Position : _Entity.m_Position, CStr::CFormat("Duplicate extra group '{}'") << Group);
						auto *pNewGroup = &pParentGroup->m_ChildEntitiesMap(Key, pParentGroup).f_GetResult();
						pParentGroup->m_ChildEntitiesOrdered.f_Insert(*pNewGroup);
						if (GroupEntity.m_pCopiedFrom)
							pNewGroup->f_CopyFrom(*GroupEntity.m_pCopiedFrom, true, &Key);
						else
							pNewGroup->f_CopyFrom(GroupEntity, true, &Key);
					}
				}
			}
		}
	}
}
