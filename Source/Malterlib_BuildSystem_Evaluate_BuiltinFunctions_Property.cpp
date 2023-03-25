// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"
#include "Malterlib_BuildSystem_DefinedProperties.hpp"

namespace NMib::NBuildSystem
{
	CBuildSystemSyntax::CIdentifier CBuildSystem::fp_IdentifierFromJson(CBuildSystem::CEvalPropertyValueContext &_Context, CEJSONSorted const &_Value, bool _bSupportEntityType) const
	{
		CBuildSystemSyntax::CIdentifier Identifier;

		auto &ValueObject = _Value.f_Object();

		Identifier.m_PropertyType = fg_PropertyTypeFromStr(ValueObject[gc_ConstString_Type].f_String());
		if (Identifier.m_PropertyType == EPropertyType_Invalid)
			fs_ThrowError(_Context, "Invalid property type '{}'"_f << ValueObject[gc_ConstString_Type].f_String());

		if (auto *pEntityType = ValueObject.f_GetMember(gc_ConstString_EntityType))
		{
			if (!_bSupportEntityType)
				fs_ThrowError(_Context, "Entity type not supported here");

			Identifier.m_EntityType = fg_EntityTypeFromStr(pEntityType->f_String());
			if (Identifier.m_EntityType == EEntityType_Invalid)
				fs_ThrowError(_Context, "Invalid entity type '{}'"_f << pEntityType->f_String());
		}

		auto &NameString = ValueObject[gc_ConstString_Name].f_String();
		Identifier.m_Name = CStringAndHash(mp_StringCache, NameString, NameString.f_Hash());

		return Identifier;
	}

	NEncoding::CEJSONSorted CBuildSystem::fp_BuiltinFunction_OverridingType
		(
			CBuildSystem::CEvalPropertyValueContext &_Context
			, TCVector<CEJSONSorted> &&_Params
			, bool _bPositions
			, bool _bType
		) const
	{
		CTypeWithPosition const *pOverrideType = nullptr;

		mint iParam = 0;

		auto &IdentifierJson = _Params[iParam];
		++iParam;

		CBuildSystemSyntax::CIdentifier Identifier = CBuildSystem::fp_IdentifierFromJson(_Context, IdentifierJson, true);

		if (_bType)
		{
			CBuildSystemSyntax::CIdentifier GetFromIdentifier = CBuildSystem::fp_IdentifierFromJson(_Context, _Params[iParam], false);
			auto GetFrom = GetFromIdentifier.f_PropertyKeyReferenceConstant();

			++iParam;

			pOverrideType = fp_GetTypeForProperty(_Context, GetFrom);

			if (!pOverrideType)
				fs_ThrowError(_Context, "No type found for property '{}'"_f << GetFrom);
		}

		CEJSONSorted Return;

		auto &Entity = _Context.m_OriginalContext;

		CBuildSystemUniquePositions Positions;
		Positions.m_pParentPositions = _Context.m_pStorePositions;

		if (_bPositions)
			_Context.m_pStorePositions = f_EnablePositions(&Positions);

		auto CleanupPositions = g_OnScopeExit / [&]
			{
				if (Positions.m_pParentPositions && _bPositions)
					Positions.m_pParentPositions->f_AddPositions(Positions);

				_Context.m_pStorePositions = Positions.m_pParentPositions;
			}
		;

		auto fReturnPosition = [](CEJSONSorted &&_Value, CBuildSystemUniquePositions const &_Positions)
			{
				CEJSONSorted Return;

				Return[gc_ConstString_Value] = fg_Move(_Value);

				auto &PositionsJson = Return[gc_ConstString_Positions].f_Array();

				for (auto &Position : _Positions.m_Positions)
				{
					auto &PositionJson = PositionsJson.f_Insert();
					PositionJson[gc_ConstString_File] = Position.m_Key.m_Position.m_File;
					PositionJson[gc_ConstString_Line] = Position.m_Key.m_Position.m_Line;
					PositionJson[gc_ConstString_Column] = Position.m_Key.m_Position.m_Column;
					PositionJson[gc_ConstString_Identifier] = Position.m_Key.m_Identifier;
					PositionJson[gc_ConstString_Message] = Position.m_Message;
				}

				return fg_Move(Return);
			}
		;

		auto PropertyKey = Identifier.f_PropertyKeyReferenceConstant();

		if (!_Params[iParam].f_IsValid())
		{
			TCOptional<CTypeWithPosition> OldOverridden;
			if (pOverrideType)
			{
				auto *pOverridden = _Context.m_EvalContext.m_OverriddenTypes.f_FindEqual(PropertyKey);
				if (pOverridden)
				{
					OldOverridden = fg_Move(*pOverridden);
					*pOverridden = *pOverrideType;
				}
				else
					_Context.m_EvalContext.m_OverriddenTypes[PropertyKey] = *pOverrideType;
			}

			auto Cleanup = g_OnScopeExit / [&]
				{
					if (!pOverrideType)
						return;

					if (OldOverridden)
						_Context.m_EvalContext.m_OverriddenTypes[PropertyKey] = fg_Move(*OldOverridden);
					else
						_Context.m_EvalContext.m_OverriddenTypes.f_Remove(PropertyKey);
				}
			;

			auto Value = fp_EvaluatePropertyValueIdentifier(_Context, Identifier, false);

			if (_bPositions)
				return fReturnPosition(Value.f_Move(), Positions);
			else
				return Value.f_Move();
		}

		CEvaluatedProperties TempProperties;
		for (auto &Member : _Params[iParam].f_Object())
		{
			CPropertyKey Key = CPropertyKey::fs_FromString(mp_StringCache, Member.f_Name(), _Context);
			auto &Property = TempProperties.m_Properties[Key];
			Property.m_Value = Member.f_Value();
			Property.m_Type = EEvaluatedPropertyType_Implicit;
			Property.m_pProperty = &mp_ExternalProperty[Key.f_GetType()];
		}

		CEvaluationContext EvalContext(&TempProperties);

		if (pOverrideType)
			EvalContext.m_OverriddenTypes[PropertyKey] = *pOverrideType;

		CEvalPropertyValueContext Context{Entity, Entity, Entity.f_Data().m_Position, EvalContext, &_Context, _Context.m_pStorePositions};

		Return = fp_EvaluatePropertyValueIdentifier(Context, Identifier, true).f_Move();

		if (_bPositions)
			return fReturnPosition(fg_Move(Return), Positions);
		else
			return Return;
	}

	CEJSONSorted CBuildSystem::fp_BuiltinFunction_GetProperty
		(
			CEvalPropertyValueContext &_Context
			, NContainer::TCVector<NEncoding::CEJSONSorted> &&_Params
			, EBuiltinFunctionGetProperty _Function
			, CEvalPropertyValueContext const *_pParentContext
		) const
	{
		CBuildSystemSyntax::CIdentifier Identifier;
		CStr EntityName;
		mint iParam = 0;

		if (_Function == EBuiltinFunctionGetProperty_HasEntity)
		{
			if (_Params.f_GetLen() != 1 || !_Params[iParam].f_IsString())
				fs_ThrowError(_Context, "HasEntity takes one string parameter");
			EntityName = _Params[iParam].f_String();
			++iParam;
		}
		else
		{
			Identifier = CBuildSystem::fp_IdentifierFromJson(_Context, _Params[iParam], true);
			++iParam;

			EntityName = _Params[iParam].f_String();
			++iParam;
		}

		CEJSONSorted Return;
		CStr NotFoundError;

		CEntity *pEntity = nullptr;
		{
			CStr Entity = EntityName;
			while (!Entity.f_IsEmpty())
			{
				CStr SubEntity = fg_GetStrSepEscaped(Entity, ".");
				CStr Type = fg_GetStrSep(SubEntity, ":");

				if (Type == gc_ConstString_Parent.m_String)
				{
					if (!pEntity)
						pEntity = &_Context.m_OriginalContext;

					pEntity = pEntity->m_pParent;
					if (!pEntity)
						fs_ThrowError(_Context, "No parent found. {}"_f << _Context.m_OriginalContext.f_GetPath());
					continue;
				}

				CEntityKey EntityKey;
				EntityKey.m_Type = fg_EntityTypeFromStr(Type);
				if (EntityKey.m_Type == EEntityType_Invalid)
					fs_ThrowError(_Context, "Invalid entity type '{}'"_f << Type);

				if (SubEntity == gc_ConstString_Symbol_OperatorMultiply.m_String)
				{
					if (!pEntity)
						pEntity = &_Context.m_OriginalContext;

					bool bFound = false;
					for (auto &Entity : pEntity->m_ChildEntitiesOrdered)
					{
						if (Entity.f_GetKey().m_Type == EntityKey.m_Type)
						{
							bFound = true;
							pEntity = &Entity;
							break;
						}
					}

					if (bFound)
						continue;

					while (pEntity && pEntity->f_GetKey().m_Type != EntityKey.m_Type)
						pEntity = pEntity->m_pParent;

					if (!pEntity)
						fs_ThrowError(_Context, fg_Format("No child or parent with type '{}' found", Type));
					continue;
				}

				EntityKey.m_Name.m_Value = SubEntity;

				if (!pEntity)
				{
					pEntity = &_Context.m_OriginalContext;

					auto pChild = pEntity->m_ChildEntitiesMap.f_FindEqual(EntityKey);
					while (!pChild && pEntity->m_pParent)
					{
						pEntity = pEntity->m_pParent;
						pChild = pEntity->m_ChildEntitiesMap.f_FindEqual(EntityKey);
					}
					if (!pChild)
					{
						NotFoundError = "Root not found";
						pEntity = nullptr;
						break;
					}
					pEntity = pChild;
					continue;
				}

				auto pChild = pEntity->m_ChildEntitiesMap.f_FindEqual(EntityKey);
				if (!pChild)
				{
					NotFoundError = "Children that do exists at '{}':"_f << pEntity->f_GetPathForGetProperty();
					for (auto &ChildEntity : pEntity->m_ChildEntitiesMap)
					{
						auto &Key = ChildEntity.f_GetKey();
						NotFoundError += "\n    {}:{}"_f << fg_EntityTypeToStr(Key.m_Type) << Key.m_Name;
					}
					pEntity = nullptr;
					break;
				}
				pEntity = pChild;
			}
		}

		if (_Function == EBuiltinFunctionGetProperty_HasEntity)
		{
			if (pEntity)
				Return = true;
			else
				Return = false;
		}
		else
		{
			if (!pEntity)
			{
				if (_Function == EBuiltinFunctionGetProperty_GetProperty)
					fs_ThrowError(_Context, _Context.m_Position, "Entity '{}' not found. {}"_f << EntityName << NotFoundError);
				else
					Return = false;
			}
			else
			{
				CEvaluatedProperties TempProperties;

				if (_Params[iParam].f_IsValid())
				{
					for (auto &Member : _Params[iParam].f_Object())
					{
						CPropertyKey Key = CPropertyKey::fs_FromString(mp_StringCache, Member.f_Name(), _Context);
						auto &Property = TempProperties.m_Properties[Key];
						Property.m_Value = Member.f_Value();
						Property.m_Type = EEvaluatedPropertyType_Implicit;
						Property.m_pProperty = &mp_ExternalProperty[Key.f_GetType()];
					}
				}

				CEvaluationContext EvalContext(&TempProperties);

				{
					if (_Function == EBuiltinFunctionGetProperty_HasProperty)
					{
						CBuildSystemPropertyInfo PropertyInfo;
						auto PropertyKey = Identifier.f_PropertyKeyReferenceConstant();

						fp_EvaluateEntityProperty(*pEntity, *pEntity, PropertyKey, EvalContext, PropertyInfo, _Context.m_Position, &_Context, true);

						if (_Context.m_pStorePositions && PropertyInfo.m_pPositions)
							_Context.m_pStorePositions->f_AddPositions(*PropertyInfo.m_pPositions);

						if (PropertyInfo.m_pProperty)
							Return = true;
						else
							Return = false;
					}
					else
					{
						CEvalPropertyValueContext Context{*pEntity, *pEntity, _Context.m_Position, EvalContext, &_Context, _Context.m_pStorePositions};

						Return = fp_EvaluatePropertyValueIdentifier(Context, Identifier, true).f_Move();
					}
				}
			}
		}

		return Return;
	}

	void CBuildSystem::fp_RegisterBuiltinFunctions_Property()
	{
		f_RegisterFunctions
			(
				{
					{
						gc_ConstString_GetProperty
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Any
								, fg_FunctionParam(g_Identifier, gc_ConstString__Identifier)
								, fg_FunctionParam(g_String, gc_ConstString__EntityName)
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), gc_ConstString__Properties, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_GetProperty(_Context, fg_Move(_Params), EBuiltinFunctionGetProperty_GetProperty, &_Context);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_OverridingType
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Any
								, fg_FunctionParam(g_Identifier, gc_ConstString__Identifier)
								, fg_FunctionParam(g_Identifier, gc_ConstString__VariableToGetTypeFrom)
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), gc_ConstString__Properties, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_OverridingType(_Context, fg_Move(_Params), false, true);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_WithPositionOverridingType
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								CBuildSystemSyntax::CType
								{
									CBuildSystemSyntax::CClassType
									(
										{
											{gc_ConstString_Value, {g_Any}}
											, {gc_ConstString_Positions, {fg_Array(fg_TempCopy(g_Position))}}
										}
										, {}
									)
								}
								, fg_FunctionParam(g_Identifier, gc_ConstString__Identifier)
								, fg_FunctionParam(g_Identifier, gc_ConstString__VariableToGetTypeFrom)
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), gc_ConstString__Properties, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_OverridingType(_Context, fg_Move(_Params), true, true);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_WithPosition
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								CBuildSystemSyntax::CType
								{
									CBuildSystemSyntax::CClassType
									(
										{
											{gc_ConstString_Value, {g_Any}}
											, {gc_ConstString_Positions, {fg_Array(fg_TempCopy(g_Position))}}
										}
										, {}
									)
								}
								, fg_FunctionParam(g_Identifier, gc_ConstString__Identifier)
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), gc_ConstString__Properties, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_OverridingType(_Context, fg_TempCopy(_Params), true, false);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_HasProperty
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Identifier, gc_ConstString__Identifier)
								, fg_FunctionParam(g_String, gc_ConstString__EntityName)
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), gc_ConstString__Properties, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_GetProperty(_Context, fg_Move(_Params), EBuiltinFunctionGetProperty_HasProperty, &_Context);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_HasEntity
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__EntityName))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_GetProperty(_Context, fg_Move(_Params), EBuiltinFunctionGetProperty_HasEntity, &_Context);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_HasParentEntity
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__EntityType))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								auto &EntityTypeStr = _Params[0].f_String();

								EEntityType EntityType = fg_EntityTypeFromStr(EntityTypeStr);
								if (EntityType == EEntityType_Invalid)
									fs_ThrowError(_Context, "Invalid entity type '{}'"_f << EntityTypeStr);

								auto pOriginalContext = &_Context.m_OriginalContext;
								while (pOriginalContext && pOriginalContext->f_GetKey().m_Type != EntityType)
									pOriginalContext = pOriginalContext->m_pParent;

								return !!pOriginalContext;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsString
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsString();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsValid
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsValid();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsNull
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsNull();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsStringArray
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsStringArray();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsInteger
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsInteger();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsFloat
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsFloat();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsBoolean
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsBoolean();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsObject
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsObject();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsArray
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsArray();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsDate
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsDate();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsBinary
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsBinary();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsUserType
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_IsUserType();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_AsString
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_AsString();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_AsInteger
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_AsInteger();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_AsFloat
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_AsFloat();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_AsBoolean
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _Params[0].f_AsBoolean();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_HasMember
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_ObjectWithAny, gc_ConstString__Value)
								, fg_FunctionParam(g_String, gc_ConstString__Member)
								, fg_FunctionParam(fg_Optional(g_String), gc_ConstString__Type, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								if (_Params[2].f_IsValid())
								{
									CStr const &JSONTypeString = _Params[2].f_String();
									EEJSONType JSONType = EEJSONType_Invalid;
									if (JSONTypeString == gc_ConstString_Invalid.m_String)
										JSONType = EEJSONType_Invalid;
									else if (JSONTypeString == gc_ConstString_Null.m_String)
										JSONType = EEJSONType_Null;
									else if (JSONTypeString == gc_ConstString_String.m_String)
										JSONType = EEJSONType_String;
									else if (JSONTypeString == gc_ConstString_Integer.m_String)
										JSONType = EEJSONType_Integer;
									else if (JSONTypeString == gc_ConstString_Float.m_String)
										JSONType = EEJSONType_Float;
									else if (JSONTypeString == gc_ConstString_Boolean.m_String)
										JSONType = EEJSONType_Boolean;
									else if (JSONTypeString == gc_ConstString_Object.m_String)
										JSONType = EEJSONType_Object;
									else if (JSONTypeString == gc_ConstString_Array.m_String)
										JSONType = EEJSONType_Array;
									else if (JSONTypeString == gc_ConstString_Date.m_String)
										JSONType = EEJSONType_Date;
									else if (JSONTypeString == gc_ConstString_Binary.m_String)
										JSONType = EEJSONType_Binary;
									else if (JSONTypeString == gc_ConstString_UserType.m_String)
										JSONType = EEJSONType_UserType;
									else
										fs_ThrowError(_Context, "Invalid JSON type '{}'"_f << JSONTypeString);

									return !!_Params[0].f_GetMember(_Params[1].f_String(), JSONType);
								}
								else
									return !!_Params[0].f_GetMember(_Params[1].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_RemoveUndefined
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_ObjectWithAny, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								CEJSONSorted Return = EJSONType_Object;
								for (auto &Entry : _Params[0].f_Object())
								{
									if (Entry.f_Value().f_IsValid())
										Return[Entry.f_Name()] = fg_Move(Entry.f_Value());
								}

								return Return;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_GetDefaultProperties
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								fg_Array
								(
									CBuildSystemSyntax::CType
									{
										CBuildSystemSyntax::CClassType
										(
											{
												{gc_ConstString_Type, {g_String}}
												, {gc_ConstString_Name, {g_String}}
											}
											, {}
										)
									}
								)
								, fg_FunctionParam(g_String, gc_ConstString__PropertyType)
								, fg_FunctionParam(fg_Optional(g_Boolean), gc_ConstString__bFile, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								auto PropertyType = fg_PropertyTypeFromStr(_Params[0].f_String());
								if (PropertyType == EPropertyType_Invalid)
									fs_ThrowError(_Context, "Invalid property type '{}'"_f << _Params[0].f_String());

								bool bFile = false;
								if (_Params[1].f_IsValid())
									bFile = _Params[1].f_Boolean();

								if (bFile)
									return _This.f_GetDefinedProperties<true>(_Context.m_Context, PropertyType);
								else
									return _This.f_GetDefinedProperties<false>(_Context.m_Context, PropertyType);
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
