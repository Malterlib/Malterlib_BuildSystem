// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_RegisterBuiltinFunctions_List()
	{
		f_RegisterFunctions
			(
				{
					{
						"ForEach"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Any
								, fg_FunctionParam(g_AnyArray, "_ArrayToExplode")
								, fg_FunctionParam(g_String, "_ExplodeFunction")
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), "_Properties", g_Optional)
							)
							, [this](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CPropertyKey FunctionPropertyKey = CPropertyKey::fs_FromString(_Params[1].f_String(), _Context.m_Position);

								auto *pTypeWithPosition = fp_GetTypeForProperty(_Context.m_OriginalContext, FunctionPropertyKey);
								if (!pTypeWithPosition)
									fsp_ThrowError(_Context, "Expected function as argument to ForEach, instead got: {}"_f << FunctionPropertyKey);

								auto TypePosition = pTypeWithPosition->m_Position;
								CBuildSystemSyntax::CType const *pType = fp_GetCanonicalType(_Context, &pTypeWithPosition->m_Type, TypePosition);

								if (!pType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
									fsp_ThrowError(_Context, "Expected function as argument to ForEach, instead got: {}"_f << FunctionPropertyKey);

								auto &FunctionType = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

								CEvaluatedProperties TempProperties;

								auto &ExplodeStackEntry = _Context.m_EvalContext.m_ExplodeListStack.f_InsertFirst();

								auto CleanUp = g_OnScopeExit > [&]()
									{
										_Context.m_EvalContext.m_ExplodeListStack.f_Remove(ExplodeStackEntry);
									}
								;

								CChangePropertiesScope ChangeProperties(_Context.m_EvalContext, &TempProperties);

								if (_Params[2].f_IsValid())
								{
									for (auto &Member : _Params[2].f_Object())
									{
										CPropertyKey Key = CPropertyKey::fs_FromString(Member.f_Name(), _Context.m_Position);
										auto &Property = TempProperties.m_Properties[Key];
										Property.m_Value = Member.f_Value();
										Property.m_Type = EEvaluatedPropertyType_Implicit;
										Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
									}
								}

								CEvaluatedProperty *pFunctionProperty = nullptr;

								auto fMapProperty = [&]()
									{
										if (!pFunctionProperty)
										{
											pFunctionProperty = &TempProperties.m_Properties[FunctionPropertyKey];
											pFunctionProperty->m_Type = EEvaluatedPropertyType_Implicit;
											pFunctionProperty->m_pProperty = &mp_ExternalProperty[FunctionPropertyKey.m_Type];
										}

										return pFunctionProperty;
									}
								;


								auto &Array = _Params[0].f_Array();

								for (auto &Value : Array)
								{
									CPropertyKey Key;
									Key.m_Type = EPropertyType_Property;

									ExplodeStackEntry.m_Value = Value;
									if (pFunctionProperty)
										ExplodeStackEntry.m_pExplodedValue = &pFunctionProperty->m_Value;

									CBuildSystemSyntax::CFunctionCall FunctionCall;
									FunctionCall.m_Name = FunctionPropertyKey.m_Name;
									FunctionCall.m_PropertyType = FunctionPropertyKey.m_Type;
									FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{Value});

									if (FunctionType.m_Parameters.f_GetLen() >= 2)
									{
										if (pFunctionProperty)
											FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{pFunctionProperty->m_Value});
										else
											FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{CEJSON{}});
									}

									if (FunctionType.m_Parameters.f_GetLen() >= 3)
									{
										CEJSON Stack = EJSONType_Array;
										for (auto &Entry : _Context.m_EvalContext.m_ExplodeListStack)
										{
											auto &Object = Stack.f_Insert().f_Object();
											Object["Value"] = Entry.m_Value;
											Object["Return"] = *Entry.m_pExplodedValue;
										}

										FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{fg_Move(Stack)});
									}

									auto NewValue = fp_EvaluatePropertyValueFunctionCall(_Context, FunctionCall);
									fMapProperty()->m_Value = fg_Move(NewValue);
								}

								if (Array.f_IsEmpty())
								{
									CEvalPropertyValueContext Context{_Context.m_Context, _Context.m_OriginalContext, TypePosition, _Context.m_EvalContext, &_Context};
									fp_CheckValueConformToType
										(
											Context
											, FunctionType.m_Return.f_Get()
											, fMapProperty()->m_Value
											, TypePosition
											, TypePosition
											, [&]() -> CStr
											{
												return "{}"_f << FunctionPropertyKey;
											}
											, EDoesValueConformToTypeFlag_None
										)
									;
								}

								return fMapProperty()->m_Value;
							}
						}
					}
					,
					{
						"ContainsListElement"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_AnyArray, "_ArrayToSearch"), fg_FunctionParam(g_Any, "_ElementToFind"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								for (auto &Value : _Params[0].f_Array())
								{
									if (Value == _Params[1])
										return true;
								}

								return false;
							}
						}
					}
					,
					{
						"Length"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(fg_OneOf(g_AnyArray, g_String), "_Array"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								auto &ToCheck = _Params[0];
								if (ToCheck.f_IsString())
									return ToCheck.f_String().f_GetLen();
								else
									return ToCheck.f_Array().f_GetLen();
							}
						}
					}
					,
					{
						"IsEmpty"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(fg_OneOf(g_AnyArray, g_String), "_Array"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								auto &ToCheck = _Params[0];
								if (ToCheck.f_IsString())
									return ToCheck.f_String().f_IsEmpty();
								else
									return ToCheck.f_Array().f_IsEmpty();
							}
						}
					}
					,
					{
						"Unique"
						, CBuiltinFunction
						{
							fg_FunctionType(g_AnyArray, fg_FunctionParam(g_AnyArray, "_Array"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CEJSON Return;
								auto &ReturnArray = Return.f_Array();
								TCSet<CEJSON> Added;
								for (auto &Value : _Params[0].f_Array())
								{
									if (Added(Value).f_WasCreated())
										ReturnArray.f_Insert(fg_Move(Value));
								}

								return Return;
							}
						}
					}
					,
					{
						"Concat"
						, CBuiltinFunction
						{
							fg_FunctionType(g_AnyArray, fg_FunctionParam(g_AnyArray, "p_ToConcat", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CEJSON Return;

								auto &ReturnArray = Return.f_Array();
								for (auto &ToConcat : _Params[0].f_Array())
									ReturnArray.f_Insert(fg_Move(ToConcat.f_Array()));

								return Return;
							}
						}
					}
				}
			)
		;
	}
}
