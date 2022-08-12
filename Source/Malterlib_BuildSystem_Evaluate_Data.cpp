// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	CEntity const *CBuildSystem::f_EvaluateData
		(
			CBuildSystemData &_Destination
			, TCMap<CPropertyKey, CEJSONSorted> const &_InitialValues
			, CEntity const *_pStartEntity
			, bool _bCopyTree
		) const
	{
		auto pRet = fp_EvaluateData
			(
				_Destination
				, _InitialValues
				, _pStartEntity
				, _bCopyTree
				, true
			)
		;
		return pRet;
	}

	CEntity const *CBuildSystem::f_EvaluateDataMain
		(
			CBuildSystemData &_Destination
			, TCMap<CPropertyKey, CEJSONSorted> const &_InitialValues
		) const
	{
		auto pRet = fp_EvaluateData
			(
				_Destination
				, _InitialValues
				, nullptr
				, true
				, false
			)
		;
		
		return pRet;
	}
	
	void CBuildSystem::fpr_EvaluateData(CEntity &_Entity, TCSet<CEntity *> &o_Deleted) const
	{
		auto &EntityData = _Entity.f_Data();
		if (!f_EvalCondition(_Entity, EntityData.m_Condition, EntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
		{
			o_Deleted[&_Entity];
			_Entity.f_ForEachChild
				(
				 	[&](CEntity *_pChild)
				 	{
						o_Deleted[_pChild];
					}
				)
			;
			_Entity.m_pParent->m_ChildEntitiesMap.f_Remove(&_Entity);
			return;
		}
		
		for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			auto &Child = *iChild;
			++iChild;
			fpr_EvaluateData(Child, o_Deleted);
		}
	}

	void CBuildSystem::fpr_EvaluateData(CEntity &_Entity) const
	{
		for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			auto &Child = *iChild;
			auto &ChildEntityData = Child.f_Data();
			if (!f_EvalCondition(Child, ChildEntityData.m_Condition, ChildEntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
			{
				auto *pToRemove = &*iChild;
				++iChild;
				_Entity.m_ChildEntitiesMap.f_Remove(pToRemove);
				continue;
			}
			fpr_EvaluateData(Child);
			++iChild;
		}
	}

	CEntity const *CBuildSystem::fp_EvaluateData
		(
			CBuildSystemData &_Destination
			, TCMap<CPropertyKey, CEJSONSorted> const &_InitialValues
			, CEntity const *_pStartEntity
			, bool _bCopyTree
			, bool _bAllChildren
		) const
	{
		CEntity const *pRet = nullptr;

		if (_pStartEntity)
		{
			TCLinkedList<CEntity const *> Entities;
			
			CEntity const *pEntity = _pStartEntity;

			while (pEntity)
			{
				Entities.f_InsertFirst(pEntity);
				pEntity = pEntity->m_pParent;
			}

			auto pParent = &_Destination.m_RootEntity;

			for (auto iEntity = Entities.f_GetIterator(); iEntity; ++iEntity)
			{
				auto pEntity = *iEntity;

				CEntity *pDestination;
				if (pEntity->f_GetKey().m_Type == EEntityType_Root)
					pDestination = &_Destination.m_RootEntity;
				else
					pDestination = &pParent->m_ChildEntitiesMap(pEntity->f_GetKey(), pParent).f_GetResult();

				pDestination->f_CopyAll(*pEntity, _bCopyTree && pEntity == _pStartEntity);
				pParent = pDestination;
			}

			pRet = pParent;
		}
		else
		{
			_Destination.f_Assign(mp_Data);
			pRet = &_Destination.m_RootEntity;
		}

		f_InitEntityForEvaluation(_Destination.m_RootEntity, _InitialValues);

		for (auto iChild = _Destination.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			auto &ChildEntity = *iChild;
			auto &Key = ChildEntity.f_GetKey();
			++iChild;
			if 
			(
				!_bAllChildren
				&&
				(
					Key.m_Type == EEntityType_Workspace
					|| Key.m_Type == EEntityType_Target
					|| Key.m_Type == EEntityType_Group
					|| Key.m_Type == EEntityType_Import
				)
			)
			{
				continue;
			}

			auto &ChildEntityData = ChildEntity.f_Data();

			if (!f_EvalCondition(ChildEntity, ChildEntityData.m_Condition, ChildEntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
			{
				_Destination.m_RootEntity.m_ChildEntitiesMap.f_Remove(&ChildEntity);
				continue;
			}
		}

		for (auto iChild = _Destination.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			auto &Key = iChild->f_GetKey();
			if
			(
				_bAllChildren
				||
				(
					Key.m_Type != EEntityType_Workspace
					&& Key.m_Type != EEntityType_Target
					&& Key.m_Type != EEntityType_Group
					&& Key.m_Type != EEntityType_Import
				)
			)
			{
				fpr_EvaluateData(*iChild);
			}
				
			++iChild;
		}
		
		return pRet;
	}
}
