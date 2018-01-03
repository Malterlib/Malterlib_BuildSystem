// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_EvalGlobalWorkspaces
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntity *> &_Targets
		) const
	{
		fp_EvalGlobalWorkspaces
			(
				_Destination
				, _Targets
			)
		;
	}
	
	void CBuildSystem::f_EvaluateTargetsInWorkspace
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntity *> const &_Targets
			, CEntity &_Workspace
		) const
	{
		fp_EvaluateTargetsInWorkspace
			(
				_Destination
				, _Targets
				, _Workspace
			)
		;
	}
	
	void CBuildSystem::fp_EvaluateWorkspace
		(
			CBuildSystemData &_Destination
			, CEntity &_Entity
		) const
	{
		bool bAdded = true;
		TCSet<CStr> AlreadyAdded;
		while (bAdded)
		{
			bAdded = false;
			CProperty const *pFromProp = nullptr;
			CStr ExtraGroups = f_EvaluateEntityPropertyUncached(_Entity, EPropertyType_Workspace, "ExtraGroups", pFromProp);
		
			while (!ExtraGroups.f_IsEmpty())
			{
				CStr Group = fg_GetStrSep(ExtraGroups, ";");
				
				if (Group.f_IsEmpty() || AlreadyAdded.f_FindEqual(Group))
					continue;
				
				bAdded = true;
				AlreadyAdded[Group];
				
				CStr OriginalGroup = Group;

				auto *pParentGroup = &_Entity;
				
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
						pParentGroup->f_CopyEntities(*GroupEntity.m_pCopiedFrom);
						pParentGroup->f_CopyProperties(*GroupEntity.m_pCopiedFrom);
						pParentGroup->m_pCopiedFrom = nullptr;
						//fp_EvaluateDataOrder(*pParentGroup);
					}
					else
					{
						pParentGroup->f_CopyEntities(GroupEntity);
						pParentGroup->f_CopyProperties(GroupEntity);
						pParentGroup->m_pCopiedFrom = nullptr;
						//fp_EvaluateDataOrder(*pParentGroup);
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
	
	void CBuildSystem::fp_EvalGlobalWorkspaces
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntity *> &_Targets
		) const
	{
		DMibLock(_Destination.m_RootEntity.m_Lock);
		{
			TCFunction<void (CEntity &_Entity)> flr_FindTargets;
			auto fl_FindTargets
				= [&flr_FindTargets, &_Targets, this](CEntity &_Entity)
				{
					for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
					{
						auto &ChildEntity = *iEntity;
						switch (ChildEntity.m_Key.m_Type)
						{
						case EEntityType_Group:
							{
								if (f_EvaluateEntityProperty(ChildEntity, EPropertyType_Group, "HideTargets") != "true")
									flr_FindTargets(ChildEntity);
							}
							break;
						case EEntityType_Target:
							{
								auto Mapped = _Targets(ChildEntity.m_Key.m_Name);
								if (!Mapped.f_WasCreated())
									fsp_ThrowError(**Mapped, ChildEntity.m_Position, "Target with same name already specified in another scope");

								*Mapped = &ChildEntity;
							}
							break;
						}
					}
				}
			;

			flr_FindTargets = fl_FindTargets;

			for (auto iChild = _Destination.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
			{
				auto &ChildEntity = *iChild;
				++iChild;
				if (ChildEntity.m_Key.m_Type == EEntityType_Group)
				{
					if (f_EvaluateEntityProperty(ChildEntity, EPropertyType_Group, "HideTargets") != "true")
						fl_FindTargets(ChildEntity);
				}
				else if (ChildEntity.m_Key.m_Type == EEntityType_Workspace)
				{
					if (!f_EvalCondition(ChildEntity, ChildEntity.m_Condition))
					{
						_Destination.m_RootEntity.m_ChildEntitiesMap.f_Remove(&ChildEntity);
						continue;
					}

					if (!mp_GenerateWorkspace.f_IsEmpty())
					{
						CStr Name = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Workspace, "Name");
						if (mp_GenerateWorkspace == Name)
						{
							fp_EvaluateDataOrder(ChildEntity);
						}
						else
						{
							_Destination.m_RootEntity.m_ChildEntitiesMap.f_Remove(&ChildEntity);
							continue;
						}
					}
					else
					{
						fp_EvaluateDataOrder(ChildEntity);
					}
				}
				else if (ChildEntity.m_Key.m_Type == EEntityType_Target)
				{
					auto Mapped = _Targets(ChildEntity.m_Key.m_Name);
					if (!Mapped.f_WasCreated())
						fsp_ThrowError(**Mapped, ChildEntity.m_Position, "Target with same name already specified in another scope");
					*Mapped = &ChildEntity;
				}
			}
		}
		for (auto iChild = _Destination.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			auto &Child = *iChild;
			if (Child.m_Key.m_Type == EEntityType_Workspace)
				fp_EvaluateWorkspace(_Destination, Child);
			++iChild;
		}
	}
	
	void CBuildSystem::fp_EvaluateTargetsInWorkspace
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntity *> const &_Targets
			, CEntity &_Workspace
		) const
	{
		TCLinkedList<CEntity *> ToEval;

		TCFunction<void (CEntity &_Entity)> flr_FindTargets;
		auto fl_FindTargets
			= [this, &flr_FindTargets, &_Targets, &ToEval, &_Destination](CEntity &_Entity)
			{
				for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity;)
				{
					auto &ChildEntity = *iEntity;
					++iEntity;
					
					if (!f_EvalCondition(ChildEntity, ChildEntity.m_Condition))
					{
						_Entity.m_ChildEntitiesMap.f_Remove(&ChildEntity);
						continue;
					}
					
					fp_EvaluateDataOrder(ChildEntity);
					
					switch (ChildEntity.m_Key.m_Type)
					{
					case EEntityType_Group:
						{
							flr_FindTargets(ChildEntity);
						}
						break;
					case EEntityType_Target:
						{
							fp_EvaluateTarget(_Destination, _Targets, ToEval, ChildEntity);
						}
						break;
					}
				}
			}
		;

		flr_FindTargets = fl_FindTargets;

		fl_FindTargets(_Workspace);

		for (auto iChild = ToEval.f_GetIterator(); iChild;)
		{
			fpr_EvaluateData(**iChild);
			++iChild;
		}
	}
}
