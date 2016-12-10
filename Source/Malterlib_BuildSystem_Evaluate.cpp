// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

namespace NMib::NBuildSystem
{
	CStr CBuildSystem::f_EvaluateEntityProperty
		(
			CEntity const &_Entity
			, EPropertyType _Type
			, CStr const &_Property
			, CProperty const *&_pFromProperty
		) const
	{
		CPropertyKey Key;
		Key.m_Type = _Type;
		Key.m_Name = _Property;
		DMibLock(_Entity.m_Lock);
		CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
		return fp_EvaluateEntityProperty(_Entity, _Entity, Key, EvalContext, _pFromProperty);
	}
	
	CStr CBuildSystem::f_EvaluateEntityPropertyUncached
		(
			CEntity const &_Entity
			, EPropertyType _Type
			, CStr const &_Property
			, CProperty const *&_pFromProperty
			, TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties
		) const
	{
		CPropertyKey Key;
		Key.m_Type = _Type;
		Key.m_Name = _Property;
		TCMap<CPropertyKey, CEvaluatedProperty> EvaluatedProperties;
		if (_pInitialProperties)
			EvaluatedProperties = *_pInitialProperties;
		CEvaluationContext EvalContext(&EvaluatedProperties);
		return fp_EvaluateEntityProperty(_Entity, _Entity, Key, EvalContext, _pFromProperty);
	}

	CStr CBuildSystem::f_EvaluateEntityProperty(CEntity const &_Entity, EPropertyType _Type, CStr const &_Property) const
	{
		CPropertyKey Key;
		Key.m_Type = _Type;
		Key.m_Name = _Property;
		DMibLock(_Entity.m_Lock);
		CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
		CProperty const *pFromProperty = nullptr;
		return fp_EvaluateEntityProperty(_Entity, _Entity, Key, EvalContext, pFromProperty);
	}

	bool CBuildSystem::f_EvalCondition(CEntity const &_Context, CCondition const &_Condition) const
	{
		DMibRequire(_Condition.m_Type == EConditionType_Root);
		DMibLock(_Context.m_Lock);
		CEvaluationContext EvalContext(&_Context.m_EvaluatedProperties);
		return fpr_EvalCondition(_Context, _Context, _Condition, EvalContext, false);
	}

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
	
	void CBuildSystem::fp_UsedExternal(CStr const &_Name) const
	{
		bool bRecorded;
		{
			DLockReadLocked(mp_UsedExternalsLock);
			bRecorded = mp_UsedExternals.f_FindEqual(_Name);
		}
			
		if (!bRecorded)
		{
			DMibLock(mp_UsedExternalsLock);
			mp_UsedExternals[_Name];
		}
	}
	
	void CBuildSystem::fp_TracePropertyEval(bool _bSuccess, CEntity const &_Entity, CProperty const &_Property, CStr const &_Value) const
	{
		if (_Property.m_Debug.f_Find("TraceEval") >= 0)
		{
			if (!_bSuccess)
			{
				if (_Property.m_Debug.f_Find("TraceEvalSuccess") < 0)
				{
					DConOut
						(
							DMibPFileLineFormat " !!!!!! {} {}:{} = {}" DNewLine
							, _Property.m_Position.m_FileName 
							<< _Property.m_Position.m_Line 
							<< _Entity.f_GetPath() 
							<< fg_PropertyTypeToStr(_Property.m_Key.m_Type) 
							<< _Property.m_Key.m_Name
							<< _Value
						)
					;
				}
			}
			else
			{
				DConOut
					(
						DMibPFileLineFormat "        {} {}:{} = {}" DNewLine
						, _Property.m_Position.m_FileName 
						<< _Property.m_Position.m_Line 
						<< _Entity.f_GetPath() 
						<< fg_PropertyTypeToStr(_Property.m_Key.m_Type) 
						<< _Property.m_Key.m_Name 
						<< _Value
					)
				;
			}
		}
	}
}
