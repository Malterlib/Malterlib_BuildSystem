// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	CEJSON CBuildSystem::f_EvaluateEntityProperty
		(
			CEntity &_Entity
			, EPropertyType _Type
			, CStr const &_Property
			, CProperty const *&_pFromProperty
 		) const
	{
		CPropertyKey Key;
		Key.m_Type = _Type;
		Key.m_Name = _Property;

		CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
		return fp_EvaluateEntityProperty(_Entity, _Entity, Key, EvalContext, _pFromProperty, {}, nullptr);
	}

	CEJSON CBuildSystem::f_EvaluateEntityPropertyUncached
		(
			CEntity &_Entity
			, EPropertyType _Type
			, CStr const &_Property
			, CProperty const *&_pFromProperty
			, TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties
		) const
	{
		CPropertyKey Key;
		Key.m_Type = _Type;
		Key.m_Name = _Property;
		CEvaluatedProperties EvaluatedProperties;
		if (_pInitialProperties)
			EvaluatedProperties.m_Properties = *_pInitialProperties;
		CEvaluationContext EvalContext(&EvaluatedProperties);
		return fp_EvaluateEntityProperty(_Entity, _Entity, Key, EvalContext, _pFromProperty, {}, nullptr);
	}

	NContainer::TCVector<NStr::CStr> CBuildSystem::f_EvaluateEntityPropertyUncachedStringArray
		(
			CEntity &_Entity
			, EPropertyType _Type
			, NStr::CStr const &_Property
			, CProperty const *&_pFromProperty
			, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties
			, NStorage::TCOptional<NContainer::TCVector<NStr::CStr>> const &_Default
		) const
	{
		auto Value = f_EvaluateEntityPropertyUncached(_Entity, _Type, _Property, _pFromProperty, _pInitialProperties);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsStringArray())
			fs_ThrowError(_pFromProperty ? _pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a string array value");

		return fg_Move(Value).f_StringArray();
	}

	NStr::CStr CBuildSystem::f_EvaluateEntityPropertyUncachedString
		(
			CEntity &_Entity
			, EPropertyType _Type
			, NStr::CStr const &_Property
			, CProperty const *&_pFromProperty
			, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties
			, NStorage::TCOptional<NStr::CStr> const &_Default
		) const
	{
		auto Value = f_EvaluateEntityPropertyUncached(_Entity, _Type, _Property, _pFromProperty, _pInitialProperties);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsString())
			fs_ThrowError(_pFromProperty ? _pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a string value");

		return fg_Move(Value.f_String());
	}


	CEJSON CBuildSystem::f_EvaluateEntityProperty(CEntity &_Entity, EPropertyType _Type, CStr const &_Property) const
	{
		CProperty const *pFromProperty = nullptr;
		return f_EvaluateEntityProperty(_Entity, _Type, _Property, pFromProperty);
	}

	CEJSON CBuildSystem::f_EvaluateEntityPropertyObject(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<CEJSON> const &_Default) const
	{
		CProperty const *pFromProperty = nullptr;
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsObject())
			fs_ThrowError(pFromProperty ? pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected an object value");

		return fg_Move(Value);
	}

	CStr CBuildSystem::f_EvaluateEntityPropertyString(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<NStr::CStr> const &_Default) const
	{
		CProperty const *pFromProperty = nullptr;
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsString())
			fs_ThrowError(pFromProperty ? pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a string value");

		return fg_Move(Value.f_String());
	}

	CStr CBuildSystem::f_EvaluateEntityPropertyString
		(
			CEntity &_Entity
			, EPropertyType _Type
			, NStr::CStr const &_Property
			, CProperty const *&_pFromProperty
			, NStorage::TCOptional<NStr::CStr> const &_Default
		) const
	{
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, _pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsString())
			fs_ThrowError(_pFromProperty ? _pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a string value");

		return fg_Move(Value.f_String());
	}

	bool CBuildSystem::f_EvaluateEntityPropertyTryBool(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<bool> const &_Default) const
	{
		CProperty const *pFromProperty = nullptr;
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsBoolean())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(pFromProperty ? pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a boolean value");
		}

		return Value.f_Boolean();
	}

	bool CBuildSystem::f_EvaluateEntityPropertyBool(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<bool> const &_Default) const
	{
		CProperty const *pFromProperty = nullptr;
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsBoolean())
			fs_ThrowError(pFromProperty ? pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a boolean value");

		return Value.f_Boolean();
	}

	bool CBuildSystem::f_EvaluateEntityPropertyBool
		(
			CEntity &_Entity
			, EPropertyType _Type
			, NStr::CStr const &_Property
			, CProperty const *&_pFromProperty
			, NStorage::TCOptional<bool> const &_Default
		) const
	{
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, _pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsBoolean())
			fs_ThrowError(_pFromProperty ? _pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a boolean value");

		return Value.f_Boolean();
	}

	int64 CBuildSystem::f_EvaluateEntityPropertyInteger(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<int64> const &_Default) const
	{
		CProperty const *pFromProperty = nullptr;
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsInteger())
			fs_ThrowError(pFromProperty ? pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected an integer value");

		return Value.f_Integer();
	}

	fp64 CBuildSystem::f_EvaluateEntityPropertyFloat(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<fp64> const &_Default) const
	{
		CProperty const *pFromProperty = nullptr;
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsFloat())
			fs_ThrowError(pFromProperty ? pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a float value");

		return Value.f_Float();
	}

	TCVector<CStr> CBuildSystem::f_EvaluateEntityPropertyStringArray
		(
			CEntity &_Entity
			, EPropertyType _Type
			, CStr const &_Property
			, TCOptional<TCVector<NStr::CStr>> const &_Default
		) const
	{
		CProperty const *pFromProperty = nullptr;
		auto Value = f_EvaluateEntityProperty(_Entity, _Type, _Property, pFromProperty);

		if (!Value.f_IsValid())
		{
			if (_Default)
				return *_Default;
			else
				fs_ThrowError(_Entity.f_Data().m_Position, "No value found for {} {}"_f << fg_PropertyTypeToStr(_Type) << _Property);
		}
		else if (!Value.f_IsStringArray())
			fs_ThrowError(pFromProperty ? pFromProperty->m_Position : _Entity.f_Data().m_Position, "Expected a string array value");

		return Value.f_StringArray();
	}

	void CBuildSystem::fp_EvaluateAllProperties(CEntity &_Entity, bool _bDoTypeChecks) const
	{
		for (auto &Properties : _Entity.f_Data().m_Properties)
		{
			CEvaluatedProperty *pEvaluated = nullptr;
			CFilePosition LastPropertyPosition;
			for (auto &Property : Properties)
			{
				CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
				EvalContext.m_EvalStack[Property.m_Key][&_Entity];

				if (fpr_EvalCondition(_Entity, _Entity, Property.m_Condition, EvalContext, Property.m_Debug.f_Find("TraceCondition") >= 0, nullptr))
				{
					CEvalPropertyValueContext Context{_Entity, _Entity, Property.m_Position, EvalContext, nullptr};
					CEJSON Value = fp_EvaluatePropertyValue(Context, Property.m_Value, &Property.m_Key);
					fp_TracePropertyEval(true, _Entity, Property, Value);

					LastPropertyPosition = Property.m_Position;

					if (!pEvaluated)
						pEvaluated = &_Entity.m_EvaluatedProperties.m_Properties[Property.m_Key];
					pEvaluated->m_Value = fg_Move(Value);
					pEvaluated->m_pProperty = &Property;
					pEvaluated->m_Type = EEvaluatedPropertyType_Explicit;
				}
				else
					fp_TracePropertyEval(false, _Entity, Property, {});
			}
			if (pEvaluated && _bDoTypeChecks)
			{
				CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
				CEvalPropertyValueContext Context{_Entity, _Entity, LastPropertyPosition, EvalContext, nullptr};
				fp_CheckValueConformToPropertyType(Context, _Entity.f_Data().m_Properties.fs_GetKey(Properties), pEvaluated->m_Value, LastPropertyPosition, EDoesValueConformToTypeFlag_None);
			}
		}
	}

	CEJSON CBuildSystem::fp_EvaluateEntityProperty
		(
			CEntity &_Entity
			, CEntity &_OriginalEntity
			, CPropertyKey const &_Key
			, CEvaluationContext &_EvalContext
			, CProperty const *&_pFromProperty
			, CFilePosition const &_FallbackPosition
			, CEvalPropertyValueContext const *_pParentContext
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
					CEJSON Ret;
					CEvaluatedProperties Properties;
					{
						CChangePropertiesScope ChangeProperties(_EvalContext, &Properties);
						Ret = fp_EvaluateEntityProperty(*pParent, _OriginalEntity, _Key, _EvalContext, _pFromProperty, _FallbackPosition, _pParentContext);
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
				auto *pValue = _Entity.m_EvaluatedProperties.m_Properties.f_FindEqual(_Key);
				if (pValue && pValue->f_IsExternal())
					return pValue->m_Value;
			}

			return {};
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
			_pFromProperty = pValue->m_pProperty;
			return pValue->m_Value;
		}

		CEntity *pContext = &_Entity;

		CEvaluatedProperty *pEvaluated = nullptr;
		{
			auto pToEval = _Entity.f_Data().m_Properties.f_FindEqual(_Key);
			if (pToEval)
			{
				//DMibCheck(pContext != &_Entity);
				CEJSON Value;
				CFilePosition LastPropertyPosition;
				for (auto iProp = pToEval->f_GetIterator(); iProp; ++iProp)
				{
					if (fpr_EvalCondition(*pContext, _OriginalEntity, iProp->m_Condition, _EvalContext, iProp->m_Debug.f_Find("TraceCondition") >= 0, _pParentContext))
					{
						CEvalPropertyValueContext Context{*pContext, _OriginalEntity, iProp->m_Position, _EvalContext, _pParentContext};
						LastPropertyPosition = iProp->m_Position;
						Value = fp_EvaluatePropertyValue(Context, iProp->m_Value, &iProp->m_Key);
						fp_TracePropertyEval(true, _OriginalEntity, *iProp, Value);
						_pFromProperty = iProp;
						if (!pEvaluated)
							pEvaluated = &_EvalContext.m_pEvaluatedProperties->m_Properties[_Key];
						pEvaluated->m_Value = fg_Move(Value);
						pEvaluated->m_pProperty = iProp;
						pEvaluated->m_Type = EEvaluatedPropertyType_Explicit;
					}
					else
						fp_TracePropertyEval(false, _OriginalEntity, *iProp, {});
				}
				if (pEvaluated)
				{
					CEvalPropertyValueContext Context{*pContext, _OriginalEntity, LastPropertyPosition, _EvalContext, _pParentContext};
					fp_CheckValueConformToPropertyType(Context, _Key, pEvaluated->m_Value, LastPropertyPosition, EDoesValueConformToTypeFlag_None);
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
					_pFromProperty = pValue->m_pProperty;

					pEvaluated = &_EvalContext.m_pEvaluatedProperties->m_Properties[_Key];
					pEvaluated->m_Value = pValue->m_Value;
					pEvaluated->m_pProperty = pValue->m_pProperty;

					CFilePosition Position = _FallbackPosition.f_IsValid() ? _FallbackPosition : _Entity.f_GetFirstValidPosition();
					CEvalPropertyValueContext Context{*pContext, _OriginalEntity, Position, _EvalContext, _pParentContext};
					fp_CheckValueConformToPropertyType(Context, _Key, pEvaluated->m_Value, Position, EDoesValueConformToTypeFlag_ConvertFromString);

					return pEvaluated->m_Value;
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
				return fp_EvaluateEntityProperty(*pParent, _OriginalEntity, _Key, _EvalContext, _pFromProperty, _FallbackPosition, _pParentContext);
			else if (_Key.m_Type == EPropertyType_Property)
				fp_UsedExternal(_Key.m_Name);
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
					_pFromProperty = pValue->m_pProperty;
					return pValue->m_Value;
				}
			}

			auto Mapped = _EvalContext.m_pEvaluatedProperties->m_Properties(_Key);
			pEvaluated = &*Mapped;

			if (Mapped.f_WasCreated())
			{
				CFilePosition Position = _FallbackPosition.f_IsValid() ? _FallbackPosition : _Entity.f_GetFirstValidPosition();
				if (!Position.f_IsValid())
					Position = _OriginalEntity.f_Data().m_Position;
				CEvalPropertyValueContext Context{*pContext, _OriginalEntity, Position, _EvalContext, _pParentContext};
				fp_CheckValueConformToPropertyType(Context, _Key, pEvaluated->m_Value, Position, EDoesValueConformToTypeFlag_None);
			}
		}

		_pFromProperty = pEvaluated->m_pProperty;
		return pEvaluated->m_Value;
	}
}
