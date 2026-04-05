// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_EvalGlobalWorkspaces
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntityMutablePointer> &_Targets
		) const
	{
		fp_EvalGlobalWorkspaces
			(
				_Destination
				, _Targets
			)
		;
	}

	TCMap<CEntity *, TCSet<CStr>> CBuildSystem::f_EvaluateTargetsInWorkspace
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntityMutablePointer> const &_Targets
			, CEntity &_Workspace
		) const
	{
		return fp_EvaluateTargetsInWorkspace
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
			CBuildSystemPropertyInfo PropertyInfo;

			auto ExtraGroups = f_EvaluateEntityPropertyUncachedStringArray(_Entity, gc_ConstKey_Workspace_ExtraGroups, PropertyInfo, nullptr, TCVector<CStr>());

			for (auto &Group : ExtraGroups)
			{
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
					Key.m_Name.m_Value = ParentGroup;
					Key.m_Type = EEntityType_Group;
					auto Child = pParentGroup->m_ChildEntitiesMap(Key, pParentGroup);
					(*Child).f_DataWritable().m_Position = pParentGroup->f_Data().m_Position;

					pParentGroup = &*Child;
				}

				CEntityKey EntityKey;
				EntityKey.m_Type = EEntityType_Group;
				EntityKey.m_Name.m_Value = OriginalGroup;

				auto pGroup = fg_Const(_Destination).m_RootEntity.m_ChildEntitiesMap.f_FindEqual(EntityKey);
				if (!pGroup)
					fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, CStr::CFormat("Extra group '{}' not found") << Group);

				auto &GroupEntity = *pGroup;
				if (Group.f_StartsWith("{"))
				{
					pParentGroup->f_CopyEntities(GroupEntity, EEntityCopyFlag_None);
					pParentGroup->f_CopyProperties(GroupEntity);
				}
				else
				{
					auto Key = GroupEntity.f_GetKey();
					Key.m_Name.m_Value = Group;

					if (pParentGroup->m_ChildEntitiesMap.f_FindEqual(Key))
						fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, CStr::CFormat("Duplicate extra group '{}'") << Group);

					pParentGroup->m_ChildEntitiesMap(Key, GroupEntity, pParentGroup, EEntityCopyFlag_CopyChildren);
				}
			}
		}
	}

	void CBuildSystem::fp_EvalGlobalWorkspaces
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntityMutablePointer> &_Targets
		) const
	{
		{
			TCFunction<void (CEntity &_Entity)> fFindTargetsRecursive;
			auto fFindTargets
				= [&fFindTargetsRecursive, &_Targets, this](CEntity &_Entity)
				{
					for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
					{
						auto &ChildEntity = *iEntity;
						switch (ChildEntity.f_GetKey().m_Type)
						{
						case EEntityType_Import:
							{
								fFindTargetsRecursive(ChildEntity);
							}
							break;
						case EEntityType_Group:
							{
								if (!f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Group_HideTargets, false))
									fFindTargetsRecursive(ChildEntity);
							}
							break;
						case EEntityType_Target:
							{
								auto Mapped = _Targets(ChildEntity.f_GetKeyName());
								if (!Mapped.f_WasCreated())
									fsp_ThrowError(**Mapped, ChildEntity.f_Data().m_Position, "Target with same name already specified in another scope");

								*Mapped = fg_Explicit(&ChildEntity);
							}
							break;
						default:
							break;
						}
					}
				}
			;

			fFindTargetsRecursive = fFindTargets;

			for (auto iChild = _Destination.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
			{
				auto &ChildEntity = *iChild;
				auto &Key = ChildEntity.f_GetKey();
				++iChild;
				if (Key.m_Type == EEntityType_Import)
					fFindTargets(ChildEntity);
				else if (Key.m_Type == EEntityType_Group)
				{
					if (!f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Group_HideTargets, false))
						fFindTargets(ChildEntity);
				}
				else if (Key.m_Type == EEntityType_Workspace)
				{
					auto &ChildEntityData = ChildEntity.f_Data();
					if (!f_EvalCondition(ChildEntity, ChildEntityData.m_Condition, ChildEntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
					{
						_Destination.m_RootEntity.m_ChildEntitiesMap.f_Remove(&ChildEntity);
						continue;
					}

					if (!mp_GenerateWorkspace.f_IsEmpty())
					{
						CStr Name = f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Workspace_Name);
						if (mp_GenerateWorkspace != Name)
						{
							_Destination.m_RootEntity.m_ChildEntitiesMap.f_Remove(&ChildEntity);
							continue;
						}
					}
				}
				else if (Key.m_Type == EEntityType_Target)
				{
					auto Mapped = _Targets(ChildEntity.f_GetKeyName());
					if (!Mapped.f_WasCreated())
						fsp_ThrowError(**Mapped, ChildEntity.f_Data().m_Position, "Target with same name already specified in another scope");
					*Mapped = fg_Explicit(&ChildEntity);
				}
			}
		}
		for (auto iChild = _Destination.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			auto &Child = *iChild;
			if (Child.f_GetKey().m_Type == EEntityType_Workspace)
				fp_EvaluateWorkspace(_Destination, Child);
			++iChild;
		}
	}

	TCMap<CEntity *, TCSet<CStr>> CBuildSystem::fp_EvaluateTargetsInWorkspace
		(
			CBuildSystemData &_Destination
			, TCMap<CStr, CEntityMutablePointer> const &_Targets
			, CEntity &_Workspace
		) const
	{
		TCLinkedList<CEntity *> ToEval;

		TCMap<CEntity *, TCSet<CStr>> AlreadyCreatedGroups;

		TCFunction<void (CEntity &_Entity)> fFindTargetsRecursive;
		auto fFindTargets
			= [&](CEntity &_Entity)
			{
				for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity;)
				{
					auto &ChildEntity = *iEntity;
					auto &ChildEntityData = ChildEntity.f_Data();
					++iEntity;

					if (!f_EvalCondition(ChildEntity, ChildEntityData.m_Condition, ChildEntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
					{
						_Entity.m_ChildEntitiesMap.f_Remove(&ChildEntity);
						continue;
					}

					switch (ChildEntity.f_GetKey().m_Type)
					{
					case EEntityType_Import:
					case EEntityType_Group:
						{
							fFindTargetsRecursive(ChildEntity);
						}
						break;
					case EEntityType_Target:
						{
							fp_EvaluateTarget(_Destination, _Targets, ToEval, ChildEntity, AlreadyCreatedGroups[&ChildEntity]);
						}
						break;
					default:
						break;
					}
				}
			}
		;

		fFindTargetsRecursive = fFindTargets;

		fFindTargets(_Workspace);

		for (auto iChild = ToEval.f_GetIterator(); iChild;)
		{
			fpr_EvaluateData(**iChild);
			++iChild;
		}

		return AlreadyCreatedGroups;
	}
}
