// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_ReEvaluateData(CEntity &_Entity) const
	{
		DMibCheck(_Entity.m_Key.m_Type != EEntityType_Target);
		DMibCheck(_Entity.m_Key.m_Type != EEntityType_Workspace);
		DMibCheck(_Entity.m_Key.m_Type != EEntityType_Root);
		fp_EvaluateDataOrder(_Entity);
	}

	
	CEntity const *CBuildSystem::f_EvaluateData
		(
			CBuildSystemData &_Destination
			, TCMap<CPropertyKey, CStr> const &_InitialValues
			, CEntity const *_pStartEntity
			, TCMap<CPropertyKey, CStr> const *_pStartEntityInitialValues
			, TCVector<CEntityKey> const *_pStartEntityInitialValuesLocation
			, bool _bCopyTree
			, bool _bAddEnvironment
		) const
	{
		auto pRet = fp_EvaluateData
			(
				_Destination
				, _InitialValues
				, _pStartEntity
				, _pStartEntityInitialValues
				, _pStartEntityInitialValuesLocation
				, _bCopyTree
				, _bAddEnvironment
				, true
			)
		;
		return pRet;
	}

	CEntity const *CBuildSystem::f_EvaluateDataMain
		(
			CBuildSystemData &_Destination
			, TCMap<CPropertyKey, CStr> const &_InitialValues
		) const
	{
		auto pRet = fp_EvaluateData
			(
				_Destination
				, _InitialValues
				, nullptr
				, nullptr
				, nullptr
				, true
				, true
				, false
			)
		;
		
		return pRet;
	}
	
	void CBuildSystem::fp_EvaluateDataOrder(CEntity &_Entity) const
	{
		DMibLock(_Entity.m_Lock);
		DCheck(!_Entity.m_bEvaluated);
		_Entity.m_bEvaluated = true;
		for (auto iProperty = _Entity.m_PropertiesEvalOrder.f_GetIterator(); iProperty; ++iProperty)
		{
			_Entity.m_PotentialExplicitProperties[iProperty->m_Key].f_Insert(&*iProperty);

			if (iProperty->m_Condition.f_NeedPerFile())
				_Entity.m_PerFilePotentialExplicitProperties[iProperty->m_Key].f_Insert(&*iProperty);
		}
	}

	void CBuildSystem::fpr_EvaluateData(CEntity &_Entity, TCSet<CEntity *> &o_Deleted) const
	{
		fp_EvaluateDataOrder(_Entity);

		if (!f_EvalCondition(_Entity, _Entity.m_Condition))
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
		fp_EvaluateDataOrder(_Entity);

		for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			auto &Child = *iChild;
			if (!f_EvalCondition(Child, Child.m_Condition))
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
			, TCMap<CPropertyKey, CStr> const &_InitialValues
			, CEntity const *_pStartEntity
			, TCMap<CPropertyKey, CStr> const *_pStartEntityInitialValues
			, TCVector<CEntityKey> const *_pStartEntityInitialValuesLocation
			, bool _bCopyTree
			, bool _bAddEnvironment
			, bool _bAllChildren
		) const
	{
		DMibLock(_Destination.m_RootEntity.m_Lock);
		
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
				if (pEntity->m_Key.m_Type == EEntityType_Root)
					pDestination = &_Destination.m_RootEntity;
				else
				{
					pDestination = &pParent->m_ChildEntitiesMap(pEntity->m_Key, pParent).f_GetResult();
					pParent->m_ChildEntitiesOrdered.f_Insert(*pDestination);
				}

				if (_bCopyTree && pEntity == _pStartEntity)
					pDestination->f_CopyFromWithCopyFrom(*pEntity, true);
				else
					pDestination->f_CopyFromWithCopyFrom(*pEntity, false);
				pParent = pDestination;
			}

			if (_pStartEntityInitialValues)
			{
				if (_pStartEntityInitialValuesLocation)
				{
					auto pInitialDest = &_Destination.m_RootEntity;
					for (auto iEntityKey = _pStartEntityInitialValuesLocation->f_GetIterator(); iEntityKey; ++iEntityKey)
					{
						auto pNext = pInitialDest->m_ChildEntitiesMap.f_FindEqual(*iEntityKey);
						if (!pNext)
							fsp_ThrowError(pInitialDest->m_Position, "Initial child entity not found in children");

						pInitialDest = pNext;
					}
					f_InitEntityForEvaluationNoEnv(*pInitialDest, *_pStartEntityInitialValues);
					pRet = pInitialDest;
				}
				else
				{
					pRet = pParent;
					f_InitEntityForEvaluationNoEnv(*pParent, *_pStartEntityInitialValues);
				}
			}
			else
				pRet = pParent;
		}
		else
		{
			_Destination = mp_Data;
			pRet = &_Destination.m_RootEntity;
		}

		if (_bAddEnvironment)
			f_InitEntityForEvaluation(_Destination.m_RootEntity, _InitialValues);
		else
			f_InitEntityForEvaluationNoEnv(_Destination.m_RootEntity, _InitialValues);

		// Nothing should access the root directly, just set potential
		for (auto iProperty = _Destination.m_RootEntity.m_PropertiesEvalOrder.f_GetIterator(); iProperty; ++iProperty)
		{
			_Destination.m_RootEntity.m_PotentialExplicitProperties[iProperty->m_Key].f_Insert(&*iProperty);
			if (iProperty->m_Condition.f_NeedPerFile())
				_Destination.m_RootEntity.m_PerFilePotentialExplicitProperties[iProperty->m_Key].f_Insert(&*iProperty);

		}
		
		for (auto iChild = _Destination.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			auto &ChildEntity = *iChild;
			++iChild;
			if 
			(
				!_bAllChildren
				&&
				(
					ChildEntity.m_Key.m_Type == EEntityType_Workspace 
					|| ChildEntity.m_Key.m_Type == EEntityType_Target
					|| ChildEntity.m_Key.m_Type == EEntityType_Group
				)
			)
				continue;
			
			if (!f_EvalCondition(ChildEntity, ChildEntity.m_Condition))
			{
				_Destination.m_RootEntity.m_ChildEntitiesMap.f_Remove(&ChildEntity);
				continue;
			}
		}

		for (auto iChild = _Destination.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
		{
			if 
			(
				_bAllChildren
				||
				(
					iChild->m_Key.m_Type != EEntityType_Workspace 
					&& iChild->m_Key.m_Type != EEntityType_Target 
					&& iChild->m_Key.m_Type != EEntityType_Group
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
