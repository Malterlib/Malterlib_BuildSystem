// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_EvaluateAllProperties(CEntity &_Entity) const
	{
		DMibLock(_Entity.m_Lock);
		for (auto iProperty = _Entity.m_PropertiesEvalOrder.f_GetIterator(); iProperty; ++iProperty)
		{
			CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);

			EvalContext.m_EvalStack[iProperty->m_Key][&_Entity];
			if (fpr_EvalCondition(_Entity, _Entity, iProperty->m_Condition, EvalContext, iProperty->m_Debug.f_Find("TraceCondition") >= 0))
			{
				CStr Value = fp_EvaluatePropertyValue(_Entity, _Entity, iProperty->m_Value, iProperty->m_Position, EvalContext);
				auto Mapped = _Entity.m_EvaluatedProperties(iProperty->m_Key);
				auto pEvaluated = &*Mapped;
				pEvaluated->m_Value = Value;
				pEvaluated->m_pProperty = iProperty;
				pEvaluated->m_Type = EEvaluatedPropertyType_Explicit;
				fp_TracePropertyEval(true, _Entity, *iProperty, Value);
			}
			else
				fp_TracePropertyEval(false, _Entity, *iProperty, iProperty->m_Value);
		}
	}

	CStr CBuildSystem::fp_EvaluateEntityProperty
		(
			CEntity const &_Entity
			, CEntity const &_OriginalEntity
			, CPropertyKey const &_Key
			, CEvaluationContext &_EvalContext
			, CProperty const *&_pFromProperty
		) const
	{
		auto &Stack = _EvalContext.m_EvalStack[_Key];
		auto EvalStack = Stack(&_Entity);

		if (!EvalStack.f_WasCreated())
		{
			//_Entity.f_GetRoot()->f_CheckParents();
			// Recursive, lets evaluate at parent scope
			if (_Entity.m_pParent != nullptr)
			{
				auto pParent = _Entity.m_pParent;
				while (pParent)
				{
					DLockReadLocked(pParent->m_Lock);
					auto *pValue = pParent->m_EvaluatedProperties.f_FindEqual(_Key);
					if (pValue && pValue->m_Type == EEvaluatedPropertyType_External)
						break;
					if (pParent->m_Properties.f_FindEqual(_Key))
						break;
					pParent  = pParent ->m_pParent;
				}
				if (pParent)
				{
					CStr Ret;
					TCMap<CPropertyKey, CEvaluatedProperty> Properties;
					{
						CChangePropertiesScope ChangeProperties(_EvalContext, &Properties);
						Ret = fp_EvaluateEntityProperty(*pParent, _OriginalEntity, _Key, _EvalContext, _pFromProperty);
					}
					return Ret;
				}
				if (_Key.m_Type == EPropertyType_Property)
					fp_UsedExternal(_Key.m_Name);
			}
			else
			{
				if (_Key.m_Type == EPropertyType_Property)
					fp_UsedExternal(_Key.m_Name);
				DLockReadLocked(_Entity.m_Lock);
				auto *pValue = _Entity.m_EvaluatedProperties.f_FindEqual(_Key);
				if (pValue && pValue->m_Type == EEvaluatedPropertyType_External)
					return pValue->m_Value;
			}

			return CStr();
		}

		auto Cleanup 
			= fg_OnScopeExit
			(
				[&]
				{
					Stack.f_Remove(&_Entity);
				}
			)
		;

		// Look in cache
		auto *pValue = _EvalContext.m_pEvaluatedProperties->f_FindEqual(_Key);
		if (pValue)
		{
			_pFromProperty = pValue->m_pProperty;
			return pValue->m_Value;
		}

		CEntity const *pContext = &_Entity;

		CEvaluatedProperty *pEvaluated = nullptr;
		{
			auto pToEval = _Entity.m_Properties.f_FindEqual(_Key);
			if (pToEval)
			{
				//DMibCheck(pContext != &_Entity);
				CStr Value;
				for (auto iProp = pToEval->f_GetIterator(); iProp; ++iProp)
				{
					if (fpr_EvalCondition(*pContext, _OriginalEntity, iProp->m_Condition, _EvalContext, iProp->m_Debug.f_Find("TraceCondition") >= 0))
					{
						Value = fp_EvaluatePropertyValue(*pContext, _OriginalEntity, iProp->m_Value, iProp->m_Position, _EvalContext);
						_pFromProperty = iProp;
						if (!pEvaluated)
							pEvaluated = &(*_EvalContext.m_pEvaluatedProperties)[_Key];
						pEvaluated->m_Value = Value;
						pEvaluated->m_pProperty = iProp;
						pEvaluated->m_Type = EEvaluatedPropertyType_Explicit;
						fp_TracePropertyEval(true, _OriginalEntity, *iProp, Value);
					}
					else
						fp_TracePropertyEval(false, _OriginalEntity, *iProp, iProp->m_Value);
				}
			}
		}

		// Evaluation didn't work, walk down the tree
		if (!pEvaluated)
		{
			{
				DLockReadLocked(_Entity.m_Lock);
				auto *pValue = _Entity.m_EvaluatedProperties.f_FindEqual(_Key);
				if (pValue && pValue->m_Type == EEvaluatedPropertyType_External)
				{
					_pFromProperty = pValue->m_pProperty;

					pEvaluated = &(*_EvalContext.m_pEvaluatedProperties)[_Key];
					pEvaluated->m_Value = pValue->m_Value;
					pEvaluated->m_pProperty = pValue->m_pProperty;

					return pValue->m_Value;
				}
			}

			auto pParent = _Entity.m_pParent;
			while (pParent)
			{
				DLockReadLocked(pParent->m_Lock);
				auto *pValue = pParent->m_EvaluatedProperties.f_FindEqual(_Key);
				if (pValue && pValue->m_Type == EEvaluatedPropertyType_External)
					break;

				if (pParent->m_Properties.f_FindEqual(_Key))
					break;
				pParent  = pParent ->m_pParent;
			}

			if (pParent)
				return fp_EvaluateEntityProperty(*pParent , _OriginalEntity, _Key, _EvalContext, _pFromProperty);
			else if (_Key.m_Type == EPropertyType_Property)
				fp_UsedExternal(_Key.m_Name);
		}

		if (!pEvaluated)
			pEvaluated = &(*_EvalContext.m_pEvaluatedProperties)[_Key];

		_pFromProperty = pEvaluated->m_pProperty;
		return pEvaluated->m_Value;
	}
}
