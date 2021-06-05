// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

namespace NMib::NBuildSystem
{
	CEJSON CBuildSystem::fp_BuiltinFunction_GetProperty
		(
			CEvalPropertyValueContext &_Context
			, NContainer::TCVector<NEncoding::CEJSON> &&_Params
			, EBuiltinFunctionGetProperty _Function
			, CEvalPropertyValueContext const *_pParentContext
		) const
	{
		CPropertyKey PropertyKey;
		CStr EntityName;
		mint iProperty = 0;
		if (_Function == EBuiltinFunctionGetProperty_HasEntity)
		{
			if (_Params.f_GetLen() != 1 || !_Params[iProperty].f_IsString())
				fsp_ThrowError(_Context, "HasEntity takes one string parameter");
			EntityName = _Params[iProperty].f_String();
			++iProperty;
		}
		else
		{
			PropertyKey.m_Type = fg_PropertyTypeFromStr(_Params[iProperty].f_String());
			if (PropertyKey.m_Type == EPropertyType_Invalid)
				fsp_ThrowError(_Context, CStr::CFormat("Invalid property type '{}'") << _Params[iProperty]);
			++iProperty;

			PropertyKey.m_Name = _Params[iProperty].f_String();
			++iProperty;

			EntityName = _Params[iProperty].f_String();
			++iProperty;
		}

		CEJSON Return;

		auto pEntity = &_Context.m_OriginalContext;
		{
			CStr Entity = EntityName;
			pEntity = nullptr;
			while (!Entity.f_IsEmpty())
			{
				CStr SubEntity = fg_GetStrSepEscaped(Entity, ".");
				CStr Type = fg_GetStrSep(SubEntity, ":");

				if (Type == "Parent")
				{
					if (!pEntity)
						pEntity = &_Context.m_OriginalContext;

					pEntity = pEntity->m_pParent;
					if (!pEntity)
						fsp_ThrowError(_Context, "No parent found. {}"_f << _Context.m_OriginalContext.f_GetPath());
					continue;
				}

				CEntityKey EntityKey;
				EntityKey.m_Type = fg_EntityTypeFromStr(Type);
				if (EntityKey.m_Type == EEntityType_Invalid)
					fsp_ThrowError(_Context, CStr::CFormat("Invalid entity type '{}'") << Type);

				if (SubEntity == "*")
				{
					if (!pEntity)
						pEntity = &_Context.m_OriginalContext;

					while (pEntity && pEntity->f_GetKey().m_Type != EntityKey.m_Type)
						pEntity = pEntity->m_pParent;

					if (!pEntity)
						fsp_ThrowError(_Context, fg_Format("No parent with type '{}' found", Type));
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
						pEntity = nullptr;
						break;
					}
					pEntity = pChild;
					continue;
				}

				auto pChild = pEntity->m_ChildEntitiesMap.f_FindEqual(EntityKey);
				if (!pChild)
				{
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
					fsp_ThrowError(_Context, _Context.m_Position, CStr::CFormat("Entity '{}' not found") << EntityName);
				else
					Return = false;
			}
			else
			{
				CEvaluatedProperties TempProperties;

				if (_Params[iProperty].f_IsValid())
				{
					for (auto &Member : _Params[iProperty].f_Object())
					{
						CPropertyKey Key = CPropertyKey::fs_FromString(Member.f_Name(), _Context.m_Position);
						auto &Property = TempProperties.m_Properties[Key];
						Property.m_Value = Member.f_Value();
						Property.m_Type = EEvaluatedPropertyType_Implicit;
						Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
					}
				}

				CEvaluationContext EvalContext(&TempProperties);
				CEvalPropertyValueContext Context{*pEntity, *pEntity, pEntity->f_Data().m_Position, EvalContext, &_Context};

				{
					if (_Function == EBuiltinFunctionGetProperty_HasProperty)
					{
						CProperty const *pFromProperty = nullptr;
						fp_EvaluateEntityProperty(*pEntity, *pEntity, PropertyKey, EvalContext, pFromProperty, Context.m_Position, &_Context);

						if (pFromProperty)
							Return = true;
						else
							Return = false;
					}
					else
					{
						CBuildSystemSyntax::CIdentifier Identifier;
						Identifier.m_Name = PropertyKey.m_Name;
						Identifier.m_PropertyType = PropertyKey.m_Type;

						Return = fp_EvaluatePropertyValueIdentifier(Context, Identifier);
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
						"GetProperty"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, "_PropertyType")
								, fg_FunctionParam(g_String, "_PropertyName")
								, fg_FunctionParam(g_String, "_EntityName")
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), "_Properties", g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _This.fp_BuiltinFunction_GetProperty(_Context, fg_Move(_Params), EBuiltinFunctionGetProperty_GetProperty, &_Context);
							}
						}
					}
					,
					{
						"HasProperty"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_String, "_PropertyType")
								, fg_FunctionParam(g_String, "_PropertyName")
								, fg_FunctionParam(g_String, "_EntityName")
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), "_Properties", g_Optional)
							)
 							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _This.fp_BuiltinFunction_GetProperty(_Context, fg_Move(_Params), EBuiltinFunctionGetProperty_HasProperty, &_Context);
							}
						}
					}
					,
					{
						"HasEntity"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_EntityName"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _This.fp_BuiltinFunction_GetProperty(_Context, fg_Move(_Params), EBuiltinFunctionGetProperty_HasEntity, &_Context);
							}
						}
					}
					,
					{
						"HasParentEntity"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_EntityType"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
 								auto &EntityTypeStr = _Params[0].f_String();

								EEntityType EntityType = fg_EntityTypeFromStr(EntityTypeStr);
								if (EntityType == EEntityType_Invalid)
									fsp_ThrowError(_Context, CStr::CFormat("Invalid entity type '{}'") << EntityTypeStr);

								auto pOriginalContext = &_Context.m_OriginalContext;
								while (pOriginalContext && pOriginalContext->f_GetKey().m_Type != EntityType)
									pOriginalContext = pOriginalContext->m_pParent;

								return !!pOriginalContext;
							}
						}
					}
					,
					{
						"IsString"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsString();
							}
						}
					}
					,
					{
						"IsValid"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsValid();
							}
						}
					}
					,
					{
						"IsNull"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsNull();
							}
						}
					}
					,
					{
						"IsStringArray"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsStringArray();
							}
						}
					}
					,
					{
						"IsInteger"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsInteger();
							}
						}
					}
					,
					{
						"IsFloat"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsFloat();
							}
						}
					}
					,
					{
						"IsBoolean"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsBoolean();
							}
						}
					}
					,
					{
						"IsObject"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsObject();
							}
						}
					}
					,
					{
						"IsArray"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsArray();
							}
						}
					}
					,
					{
						"IsDate"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsDate();
							}
						}
					}
					,
					{
						"IsBinary"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsBinary();
							}
						}
					}
					,
					{
						"IsUserType"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_IsUserType();
							}
						}
					}
					,
					{
						"AsString"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_AsString();
							}
						}
					}
					,
					{
						"AsInteger"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_AsInteger();
							}
						}
					}
					,
					{
						"AsFloat"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_AsFloat();
							}
						}
					}
					,
					{
						"AsBoolean"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_Any, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_AsBoolean();
							}
						}
					}
					,
					{
						"HasMember"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_ObjectWithAny, "_Value")
								, fg_FunctionParam(g_String, "_Member")
								, fg_FunctionParam(fg_Optional(g_String), "_Type", g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								if (_Params[2].f_IsValid())
								{
									CStr const &JSONTypeString = _Params[2].f_String();
									EEJSONType JSONType = EEJSONType_Invalid;
									if (JSONTypeString == "Invalid")
										JSONType = EEJSONType_Invalid;
									else if (JSONTypeString == "Null")
										JSONType = EEJSONType_Null;
									else if (JSONTypeString == "String")
										JSONType = EEJSONType_String;
									else if (JSONTypeString == "Integer")
										JSONType = EEJSONType_Integer;
									else if (JSONTypeString == "Float")
										JSONType = EEJSONType_Float;
									else if (JSONTypeString == "Boolean")
										JSONType = EEJSONType_Boolean;
									else if (JSONTypeString == "Object")
										JSONType = EEJSONType_Object;
									else if (JSONTypeString == "Array")
										JSONType = EEJSONType_Array;
									else if (JSONTypeString == "Date")
										JSONType = EEJSONType_Date;
									else if (JSONTypeString == "Binary")
										JSONType = EEJSONType_Binary;
									else if (JSONTypeString == "UserType")
										JSONType = EEJSONType_UserType;
									else
										fsp_ThrowError(_Context, CStr::CFormat("Invalid JSON type '{}'") << JSONTypeString);

									return !!_Params[0].f_GetMember(_Params[1].f_String(), JSONType);
								}
								else
									return !!_Params[0].f_GetMember(_Params[1].f_String());
							}
						}
					}
					,
					{
						"RemoveUndefined"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_ObjectWithAny, "_Value"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CEJSON Return = EJSONType_Object;
								for (auto &Entry : _Params[0].f_Object())
								{
									if (Entry.f_Value().f_IsValid())
										Return[Entry.f_Name()] = fg_Move(Entry.f_Value());
								}

								return Return;
							}
						}
					}
				}
			)
		;
	}
}
