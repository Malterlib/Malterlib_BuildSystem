// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_SetEnablePositions()
	{
		mp_bEnablePositions = true;
	}

	bool CBuildSystem::f_EnablePositions() const
	{
		return mp_bEnablePositions;
	}

	bool CBuildSystem::f_EnableValues() const
	{
		return mp_bEnableValues;
	}

	void CBuildSystem::f_SetEnableValues()
	{
		mp_bEnableValues = true;
	}

	bool CBuildSystem::f_ApplyRepoPolicy() const
	{
		return mp_bApplyRepoPolicy;
	}

	bool CBuildSystem::f_ApplyRepoPolicyPretend() const
	{
		return mp_bApplyRepoPolicyPretend;
	}

	bool CBuildSystem::f_ApplyRepoPolicyCreateMissing() const
	{
		return mp_bApplyRepoPolicyCreateMissing;
	}

	bool CBuildSystem::f_UpdateLfsReleaseIndexes() const
	{
		return mp_bUpdateLfsReleaseIndexes;
	}

	bool CBuildSystem::f_UpdateLfsReleaseIndexesPretend() const
	{
		return mp_bUpdateLfsReleaseIndexesPretend;
	}

	bool CBuildSystem::f_UpdateLfsReleaseIndexesPruneOrphanedAssets() const
	{
		return mp_bUpdateLfsReleaseIndexesPruneOrphanedAssets;
	}

	CStringCache &CBuildSystem::f_StringCache() const
	{
		return mp_StringCache;
	}

	CBuildSystemUniquePositions *CBuildSystem::f_EnablePositions(NStorage::TCSharedPointer<CBuildSystemUniquePositions> &o_pPositions) const
	{
		if (mp_bEnablePositions)
		{
			if (!o_pPositions)
				o_pPositions = fg_Construct();
			return o_pPositions.f_Get();
		}

		return nullptr;
	}

	CBuildSystemUniquePositions *CBuildSystem::f_EnablePositions(CBuildSystemUniquePositions *_pPositions) const
	{
		if (mp_bEnablePositions)
			return _pPositions;

		return nullptr;
	}

	CValuePotentiallyByRef CBuildSystem::f_EvaluateEntityProperty
		(
			CEntity &_Entity
			, CPropertyKeyReference const &_PropertyKey
			, CBuildSystemPropertyInfo &o_PropertyInfo
			, CEvaluatedProperties *_pEvaluatedProperties
		) const
	{
		CEvaluationContext EvalContext(_pEvaluatedProperties ? _pEvaluatedProperties : &_Entity.m_EvaluatedProperties);
		return fp_EvaluateEntityProperty(_Entity, _Entity, _PropertyKey, EvalContext, o_PropertyInfo, o_PropertyInfo.f_FallbackPosition(), nullptr, false);
	}

	CValuePotentiallyByRef CBuildSystem::f_EvaluateEntityPropertyNoDefault
		(
			CEntity &_Entity
			, CPropertyKeyReference const &_PropertyKey
			, CBuildSystemPropertyInfo &o_PropertyInfo
			, CEvaluatedProperties *_pEvaluatedProperties
		) const
	{
		CEvaluationContext EvalContext(_pEvaluatedProperties ? _pEvaluatedProperties : &_Entity.m_EvaluatedProperties);
		EvalContext.m_bFailUndefinedTypeCheck = false;
		return fp_EvaluateEntityProperty(_Entity, _Entity, _PropertyKey, EvalContext, o_PropertyInfo, o_PropertyInfo.f_FallbackPosition(), nullptr, false);
	}

	NEncoding::CEJsonSorted CBuildSystem::f_EvaluateEntityPropertyUncached
		(
			CEntity &_Entity
			, CPropertyKeyReference const &_PropertyKey
			, CBuildSystemPropertyInfo &o_PropertyInfo
			, TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties
			, CEvaluatedProperties *_pEvaluatedProperties
		) const
	{
		CEvaluatedProperties EvaluatedProperties;
		if (_pInitialProperties)
			EvaluatedProperties.m_Properties = *_pInitialProperties;
		CEvaluationContext EvalContext(&EvaluatedProperties);
		return fp_EvaluateEntityProperty(_Entity, _Entity, _PropertyKey, EvalContext, o_PropertyInfo, o_PropertyInfo.f_FallbackPosition(), nullptr, false).f_Move();
	}

	NContainer::TCVector<NStr::CStr> CBuildSystem::f_EvaluateEntityPropertyUncachedStringArray
		(
			CEntity &_Entity
			, CPropertyKeyReference const &_PropertyKey
			, CBuildSystemPropertyInfo &o_PropertyInfo
			, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties
			, NStorage::TCOptional<NContainer::TCVector<NStr::CStr>> const &_Default
		) const
	{
		auto Value = f_EvaluateEntityPropertyUncached(_Entity, _PropertyKey, o_PropertyInfo, _pInitialProperties);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!Value.f_IsStringArray())
			fs_ThrowError(o_PropertyInfo, _Entity.f_Data().m_Position, "Expected a string array value");

		return fg_Move(Value).f_StringArray();
	}

	NStr::CStr CBuildSystem::f_EvaluateEntityPropertyUncachedString
		(
			CEntity &_Entity
			, CPropertyKeyReference const &_PropertyKey
			, CBuildSystemPropertyInfo &o_PropertyInfo
			, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties
			, NStorage::TCOptional<NStr::CStr> const &_Default
		) const
	{
		auto Value = f_EvaluateEntityPropertyUncached(_Entity, _PropertyKey, o_PropertyInfo, _pInitialProperties);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!Value.f_IsString())
			fs_ThrowError(o_PropertyInfo, _Entity.f_Data().m_Position, "Expected a string value");

		return fg_Move(Value.f_String());
	}


	CValuePotentiallyByRef CBuildSystem::f_EvaluateEntityProperty(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey) const
	{
		CBuildSystemPropertyInfo PropertyInfo;
		return f_EvaluateEntityProperty(_Entity, _PropertyKey, PropertyInfo);
	}

	CValuePotentiallyByRef CBuildSystem::f_EvaluateEntityPropertyObject(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<CEJsonSorted> &&_Default) const
	{
		CBuildSystemPropertyInfo PropertyInfo;
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, PropertyInfo);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return CEJsonSorted(fg_Move(*_Default));
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsObject())
			fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, "Expected an object value");

		return Value;
	}

	CStr CBuildSystem::f_EvaluateEntityPropertyString(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<NStr::CStr> &&_Default) const
	{
		CBuildSystemPropertyInfo PropertyInfo;
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, PropertyInfo);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return fg_Move(*_Default);
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsString())
			fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, "Expected a string value");

		return Value.f_MoveString();
	}

	CStr CBuildSystem::f_EvaluateEntityPropertyString
		(
			CEntity &_Entity
			, CPropertyKeyReference const &_PropertyKey
			, CBuildSystemPropertyInfo &o_PropertyInfo
			, NStorage::TCOptional<NStr::CStr> &&_Default
		) const
	{
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, o_PropertyInfo);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return fg_Move(*_Default);
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsString())
			fs_ThrowError(o_PropertyInfo, _Entity.f_Data().m_Position, "Expected a string value");

		return Value.f_MoveString();
	}

	bool CBuildSystem::f_EvaluateEntityPropertyTryBool(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<bool> const &_Default) const
	{
		CBuildSystemPropertyInfo PropertyInfo;
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, PropertyInfo);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsBoolean())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, "Expected a boolean value");
		}

		return ValueRef.f_Boolean();
	}

	bool CBuildSystem::f_EvaluateEntityPropertyBool(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<bool> const &_Default) const
	{
		CBuildSystemPropertyInfo PropertyInfo;
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, PropertyInfo);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsBoolean())
			fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, "Expected a boolean value");

		return ValueRef.f_Boolean();
	}

	bool CBuildSystem::f_EvaluateEntityPropertyBool
		(
			CEntity &_Entity
			, CPropertyKeyReference const &_PropertyKey
			, CBuildSystemPropertyInfo &o_PropertyInfo
			, NStorage::TCOptional<bool> const &_Default
			, CEvaluatedProperties *_pEvaluatedProperties
		) const
	{
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, o_PropertyInfo, _pEvaluatedProperties);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsBoolean())
			fs_ThrowError(o_PropertyInfo, _Entity.f_Data().m_Position, "Expected a boolean value");

		return ValueRef.f_Boolean();
	}

	int64 CBuildSystem::f_EvaluateEntityPropertyInteger(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<int64> const &_Default) const
	{
		CBuildSystemPropertyInfo PropertyInfo;
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, PropertyInfo);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsInteger())
			fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, "Expected an integer value");

		return ValueRef.f_Integer();
	}

	fp64 CBuildSystem::f_EvaluateEntityPropertyFloat(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<fp64> const &_Default) const
	{
		CBuildSystemPropertyInfo PropertyInfo;
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, PropertyInfo);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsFloat())
			fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, "Expected a float value");

		return ValueRef.f_Float();
	}

	TCVector<CStr> CBuildSystem::f_EvaluateEntityPropertyStringArray
		(
			CEntity &_Entity
			, CPropertyKeyReference const &_PropertyKey
			, TCOptional<TCVector<NStr::CStr>> &&_Default
		) const
	{
		CBuildSystemPropertyInfo PropertyInfo;
		auto Value = f_EvaluateEntityProperty(_Entity, _PropertyKey, PropertyInfo);
		auto &ValueRef = Value.f_Get();

		if (!ValueRef.f_IsValid())
		{
			if (_Default)
				return fg_Move(*_Default);
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {}"_f << _PropertyKey);
		}
		else if (!ValueRef.f_IsStringArray())
			fs_ThrowError(PropertyInfo, _Entity.f_Data().m_Position, "Expected a string array value");

		return Value.f_MoveStringArray();
	}

	void CBuildSystem::fp_EvaluateAllProperties(CEntity &_Entity, bool _bDoTypeChecks) const
	{
		for (auto &Properties : _Entity.f_Data().m_Properties)
		{
			CEvaluatedProperty *pEvaluated = nullptr;
			CFilePosition LastPropertyPosition;
			auto &PropertyKey = _Entity.f_Data().m_Properties.fs_GetKey(Properties);
			for (auto &Property : Properties)
			{
				CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
				EvalContext.m_EvalStack[PropertyKey][&_Entity];

				bool bTypeAlreadyChecked = false;

				CBuildSystemUniquePositions Positions;
				if
					(
						!Property.m_pCondition
						|| fpr_EvalCondition(_Entity, _Entity, *Property.m_pCondition, EvalContext, Property.m_Flags & EPropertyFlag_TraceCondition, nullptr, f_EnablePositions(&Positions))
					)
				{
					CEvalPropertyValueContext Context{_Entity, _Entity, Property.m_Position, EvalContext, nullptr, f_EnablePositions(&Positions)};
					auto Value = fp_EvaluateRootValue(Context, Property.m_Value, &PropertyKey, bTypeAlreadyChecked);
					auto &ValueRef = Value.f_Get();
					fp_TracePropertyEval(true, _Entity, PropertyKey, Property, ValueRef);

					LastPropertyPosition = Property.m_Position;

					if (!pEvaluated)
						pEvaluated = &_Entity.m_EvaluatedProperties.m_Properties[PropertyKey];

					pEvaluated->m_Value = Value.f_Move();
					pEvaluated->m_pProperty = &Property;
					pEvaluated->m_Type = EEvaluatedPropertyType_Explicit;
					if (f_EnablePositions())
					{
						if (!pEvaluated->m_pPositions)
							pEvaluated->m_pPositions = fg_Construct();
						pEvaluated->m_pPositions->f_AddPositions(Positions);
					}
				}
				else
					fp_TracePropertyEval(false, _Entity, PropertyKey, Property, {});
			}
			if (pEvaluated && _bDoTypeChecks)
			{
				CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
				CEvalPropertyValueContext Context{_Entity, _Entity, LastPropertyPosition, EvalContext, nullptr, f_EnablePositions(pEvaluated->m_pPositions.f_Get())};
				fp_CheckValueConformToPropertyType(Context, _Entity.f_Data().m_Properties.fs_GetKey(Properties).f_Reference(), pEvaluated->m_Value, LastPropertyPosition, EDoesValueConformToTypeFlag_None);
			}
		}
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluateEntityProperty
		(
			CEntity &_Entity
			, CEntity &_OriginalEntity
			, CPropertyKeyReference const &_Key
			, CEvaluationContext &_EvalContext
			, CBuildSystemPropertyInfo &o_PropertyInfo
			, CFilePosition const &_FallbackPosition
			, CEvalPropertyValueContext const *_pParentContext
			, bool _bMoveCache
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
					auto *pValue = pParent->m_EvaluatedProperties.m_Properties.f_FindEqual(_Key);
					if (pValue && pValue->f_IsExternal())
						break;
					if (pParent->f_Data().m_Properties.f_FindEqual(_Key))
						break;
					pParent = pParent->m_pParent;
				}
				if (pParent)
				{
					CBuildSystemPropertyInfo PropertyInfo;

					DMibFastCheck(!_EvalContext.m_pEvaluatedProperties->m_Properties.f_FindEqual(_Key));
					auto Cleanup = g_OnScopeExit / [&]
						{
							_EvalContext.m_pEvaluatedProperties->m_Properties.f_Remove(_Key);
						}
					;
					return fp_EvaluateEntityProperty(*pParent, _OriginalEntity, _Key, _EvalContext, PropertyInfo, _FallbackPosition, _pParentContext, false).f_Move();
				}
				if (_Key.f_GetType() == EPropertyType_Property)
					fp_UsedExternal(_Key);
			}
			else
			{
				if (_Key.f_GetType() == EPropertyType_Property)
					fp_UsedExternal(_Key);
				auto *pValue = _Entity.m_EvaluatedProperties.m_Properties.f_FindEqual(_Key);
				if (pValue && pValue->f_IsExternal())
					return &pValue->m_Value;
			}

			return CEJsonSorted();
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
		auto *pValue = _EvalContext.m_pEvaluatedProperties->m_Properties.f_FindEqual(_Key);
		if (pValue)
		{
			o_PropertyInfo.m_pProperty = pValue->m_pProperty;
			if (pValue->m_pPositions)
				o_PropertyInfo.m_pPositions = pValue->m_pPositions;
			return &pValue->m_Value;
		}

		CEntity *pContext = &_Entity;

		CEvaluatedProperty *pEvaluated = nullptr;
		{
			auto pToEval = _Entity.f_Data().m_Properties.f_FindEqual(_Key);
			if (pToEval)
			{
				auto &PropertyKey = _Entity.f_Data().m_Properties.fs_GetKey(*pToEval);
				//DMibCheck(pContext != &_Entity);
				CEJsonSorted Value;
				CFilePosition const *pLastPropertyPosition = nullptr;
				bool bTypeAlreadyChecked = false;
				for (auto &Prop : *pToEval)
				{
					CBuildSystemUniquePositions Positions;
					if
						(
							!Prop.m_pCondition
							|| fpr_EvalCondition
							(
								*pContext
								, _OriginalEntity
								, *Prop.m_pCondition
								, _EvalContext
								, Prop.m_Flags & EPropertyFlag_TraceCondition
								, _pParentContext
								, f_EnablePositions(&Positions)
							)
						)
					{
						CEvalPropertyValueContext Context{*pContext, _OriginalEntity, Prop.m_Position, _EvalContext, _pParentContext, f_EnablePositions(&Positions)};

						pLastPropertyPosition = &Prop.m_Position;
						auto Value = fp_EvaluateRootValue(Context, Prop.m_Value, &PropertyKey, bTypeAlreadyChecked);
						auto &ValueRef = Value.f_Get();
						fp_TracePropertyEval(true, _OriginalEntity, PropertyKey, Prop, ValueRef);
						o_PropertyInfo.m_pProperty = &Prop;

						if (!pEvaluated)
							pEvaluated = &_EvalContext.m_pEvaluatedProperties->m_Properties[_Key];

						pEvaluated->m_Value = Value.f_Move();
						if (f_EnablePositions())
						{
							if (!pEvaluated->m_pPositions)
								pEvaluated->m_pPositions = fg_Construct();
							auto pAdded = pEvaluated->m_pPositions->f_AddPosition(Prop.m_Position, "{}"_f << _Key);
							if (pAdded)
								pAdded->f_AddValue(pEvaluated->m_Value, f_EnableValues());
							pEvaluated->m_pPositions->f_AddPositions(Positions);
						}
						pEvaluated->m_pProperty = &Prop;
						pEvaluated->m_Type = EEvaluatedPropertyType_Explicit;
					}
					else
						fp_TracePropertyEval(false, _OriginalEntity, PropertyKey, Prop, {});
				}
				if (pEvaluated && !bTypeAlreadyChecked)
				{
					CEvalPropertyValueContext Context
						{
							*pContext
							, _OriginalEntity
							, pLastPropertyPosition ? *pLastPropertyPosition : CFilePosition::fs_Default()
							, _EvalContext
							, _pParentContext
							, f_EnablePositions(pEvaluated->m_pPositions.f_Get())
						}
					;
					fp_CheckValueConformToPropertyType(Context, _Key, pEvaluated->m_Value, *pLastPropertyPosition, EDoesValueConformToTypeFlag_None);
				}
			}
		}

		// Evaluation didn't work, walk down the tree
		if (!pEvaluated)
		{
			{
				auto *pValue = _Entity.m_EvaluatedProperties.m_Properties.f_FindEqual(_Key);
				if (pValue && pValue->f_IsExternal())
				{
					o_PropertyInfo.m_pProperty = pValue->m_pProperty;

					pEvaluated = &_EvalContext.m_pEvaluatedProperties->m_Properties[_Key];
					pEvaluated->m_Value = pValue->m_Value;
					pEvaluated->m_pProperty = pValue->m_pProperty;
					if (f_EnablePositions())
					{
						if (!pEvaluated->m_pPositions)
							pEvaluated->m_pPositions = fg_Construct();
						pEvaluated->m_pPositions->f_AddPositions(pValue->m_pPositions);
					}

					CFilePosition Position = _FallbackPosition.f_IsValid() ? _FallbackPosition : _Entity.f_GetFirstValidPosition();
					CEvalPropertyValueContext Context{*pContext, _OriginalEntity, Position, _EvalContext, _pParentContext, f_EnablePositions(pEvaluated->m_pPositions)};
					fp_CheckValueConformToPropertyType(Context, _Key, pEvaluated->m_Value, Position, EDoesValueConformToTypeFlag_ConvertFromString);

					if (pValue->m_pPositions)
						o_PropertyInfo.m_pPositions = pValue->m_pPositions;

					return &pEvaluated->m_Value;
				}
			}

			auto pParent = _Entity.m_pParent;
			while (pParent)
			{
				auto *pValue = pParent->m_EvaluatedProperties.m_Properties.f_FindEqual(_Key);
				if (pValue && pValue->f_IsExternal())
					break;

				if (pParent->f_Data().m_Properties.f_FindEqual(_Key))
					break;
				pParent = pParent->m_pParent;
			}

			if (pParent)
				return fp_EvaluateEntityProperty(*pParent, _OriginalEntity, _Key, _EvalContext, o_PropertyInfo, _FallbackPosition, _pParentContext, _bMoveCache);
			else if (_Key.f_GetType() == EPropertyType_Property)
				fp_UsedExternal(_Key);
		}

		if (!pEvaluated)
		{
			// Check for inherited evaluated properties
			for
				(
					auto *pEvaluatedProperties = _EvalContext.m_pEvaluatedProperties->m_pParentProperties
					; pEvaluatedProperties
					; pEvaluatedProperties = pEvaluatedProperties->m_pParentProperties
				)
			{
				if (auto pValue = pEvaluatedProperties->m_Properties.f_FindEqual(_Key))
				{
					o_PropertyInfo.m_pProperty = pValue->m_pProperty;
					if (pValue->m_pPositions)
						o_PropertyInfo.m_pPositions = pValue->m_pPositions;
					return &pValue->m_Value;
				}
			}

			auto Mapped = _EvalContext.m_pEvaluatedProperties->m_Properties(_Key);
			pEvaluated = &*Mapped;

			if (Mapped.f_WasCreated())
			{
				CFilePosition Position = _FallbackPosition.f_IsValid() ? _FallbackPosition : _Entity.f_GetFirstValidPosition();
				if (!Position.f_IsValid())
					Position = _OriginalEntity.f_Data().m_Position;
				CEvalPropertyValueContext Context{*pContext, _OriginalEntity, Position, _EvalContext, _pParentContext, f_EnablePositions(pEvaluated->m_pPositions)};
				fp_CheckValueConformToPropertyType(Context, _Key, pEvaluated->m_Value, Position, EDoesValueConformToTypeFlag_None);
			}
		}

		o_PropertyInfo.m_pProperty = pEvaluated->m_pProperty;
		if (pEvaluated->m_pPositions)
			o_PropertyInfo.m_pPositions = pEvaluated->m_pPositions;

		if (_bMoveCache)
			return CValuePotentiallyByRef(&pEvaluated->m_Value, true);
		else
			return &pEvaluated->m_Value;
	}
}
