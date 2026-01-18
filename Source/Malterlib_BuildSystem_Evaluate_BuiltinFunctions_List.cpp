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
						gc_ConstString_ForEach
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Any
								, fg_FunctionParam(CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf{{g_AnyArray, g_ObjectWithAny}}}, gc_ConstString__ArrayOrObjectToExplode)
								, fg_FunctionParam(g_Identifier, gc_ConstString__ExplodeFunction)
								, fg_FunctionParam(fg_Optional(g_ObjectWithAny), gc_ConstString__Properties, g_Optional)
							)
							, [this](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CBuildSystemSyntax::CIdentifier Identifier = CBuildSystem::fp_IdentifierFromJson(_Context, _Params[1], false);

								auto FunctionPropertyKey = Identifier.f_PropertyKeyReferenceConstant();

								auto *pTypeWithPosition = fp_GetTypeForProperty(_Context, FunctionPropertyKey);
								if (!pTypeWithPosition)
									fs_ThrowError(_Context, "Expected function as argument to ForEach, instead got: {}"_f << FunctionPropertyKey);

								auto pTypePosition = &pTypeWithPosition->m_Position;
								CBuildSystemSyntax::CType const *pType = fp_GetCanonicalType(_Context, &pTypeWithPosition->m_Type, pTypePosition);

								if (!pType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
									fs_ThrowError(_Context, "Expected function as argument to ForEach, instead got: {}"_f << FunctionPropertyKey);

								auto &FunctionType = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

								CEvaluatedProperties TempProperties;
								TempProperties.m_pParentProperties = _Context.m_EvalContext.m_pEvaluatedProperties;

								auto &ExplodeStackEntry = _Context.m_EvalContext.m_ExplodeListStack.f_InsertFirst();

								auto CleanUp = g_OnScopeExit / [&]()
									{
										_Context.m_EvalContext.m_ExplodeListStack.f_Remove(ExplodeStackEntry);
									}
								;

								CChangePropertiesScope ChangeProperties(_Context.m_EvalContext, &TempProperties);

								if (_Params[2].f_IsValid())
								{
									for (auto &Member : _Params[2].f_Object())
									{
										CPropertyKey Key = CPropertyKey::fs_FromString(mp_StringCache, Member.f_Name(), _Context);
										auto &Property = TempProperties.m_Properties[Key];
										Property.m_Value = Member.f_Value();
										Property.m_Type = EEvaluatedPropertyType_Implicit;
										Property.m_pProperty = &mp_ExternalProperty[Key.f_GetType()];
									}
								}

								CEvaluatedProperty *pFunctionProperty = nullptr;

								auto fMapProperty = [&]()
									{
										if (!pFunctionProperty)
										{
											pFunctionProperty = &TempProperties.m_Properties[FunctionPropertyKey];
											pFunctionProperty->m_Type = EEvaluatedPropertyType_Implicit;
											pFunctionProperty->m_pProperty = &mp_ExternalProperty[FunctionPropertyKey.f_GetType()];
										}

										return pFunctionProperty;
									}
								;

								if (_Params[0].f_IsArray())
								{
									auto &Array = _Params[0].f_Array();

									for (auto &Value : Array)
									{
										ExplodeStackEntry.m_Value = Value;
										if (pFunctionProperty)
											ExplodeStackEntry.m_pExplodedValue = &pFunctionProperty->m_Value;

										CBuildSystemSyntax::CFunctionCall FunctionCall;
										FunctionCall.m_PropertyKey = CPropertyKey(FunctionPropertyKey);

										// Params:
										// 0: Array value
										// 1: The current value of the function return
										// 2: An array of objects with previous ForEach current key and values and current returns

										FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{Value});

										if (FunctionType.m_Parameters.f_GetLen() >= 2)
										{
											if (pFunctionProperty)
												FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{pFunctionProperty->m_Value});
											else
												FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{CEJsonSorted{}});
										}

										if (FunctionType.m_Parameters.f_GetLen() >= 3)
										{
											CEJsonSorted Stack = EJsonType_Array;
											for (auto &Entry : _Context.m_EvalContext.m_ExplodeListStack)
											{
												auto &Object = Stack.f_Insert().f_Object();
												Object[gc_ConstString_Key] = Entry.m_Key;
												Object[gc_ConstString_Value] = Entry.m_Value;
												Object[gc_ConstString_Return] = *Entry.m_pExplodedValue;
											}

											FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{fg_Move(Stack)});
										}

										fMapProperty()->m_Value = fp_EvaluatePropertyValueFunctionCall(_Context, FunctionCall, true);
									}

									if (Array.f_IsEmpty())
									{
										CEvalPropertyValueContext Context
											{
												_Context.m_Context
												, _Context.m_OriginalContext
												, *pTypePosition
												, _Context.m_EvalContext
												, &_Context
												, _Context.m_pStorePositions
											}
										;

										fp_CheckValueConformToType
											(
												Context
												, FunctionType.m_Return.f_Get()
												, fMapProperty()->m_Value
												, *pTypePosition
												, *pTypePosition
												, [&]() -> CStr
												{
													return "{}"_f << FunctionPropertyKey;
												}
												, EDoesValueConformToTypeFlag_None
											)
										;
									}
								}
								else
								{
									auto &Object = _Params[0].f_Object();

									for (auto &Entry : Object)
									{
										ExplodeStackEntry.m_Key = Entry.f_Name();
										ExplodeStackEntry.m_Value = Entry.f_Value();
										if (pFunctionProperty)
											ExplodeStackEntry.m_pExplodedValue = &pFunctionProperty->m_Value;

										CBuildSystemSyntax::CFunctionCall FunctionCall;
										FunctionCall.m_PropertyKey = CPropertyKey(FunctionPropertyKey);

										// Params:
										// 0: Object entry key
										// 1: Object entry value
										// 2: The current value of the function return
										// 3: An array of objects with previous ForEach current key and values and current returns

										FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{Entry.f_Name()});
										FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{Entry.f_Value()});

										if (FunctionType.m_Parameters.f_GetLen() >= 3)
										{
											if (pFunctionProperty)
												FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{pFunctionProperty->m_Value});
											else
												FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{CEJsonSorted{}});
										}

										if (FunctionType.m_Parameters.f_GetLen() >= 4)
										{
											CEJsonSorted Stack = EJsonType_Array;
											for (auto &Entry : _Context.m_EvalContext.m_ExplodeListStack)
											{
												auto &Object = Stack.f_Insert().f_Object();
												Object[gc_ConstString_Key] = Entry.m_Key;
												Object[gc_ConstString_Value] = Entry.m_Value;
												Object[gc_ConstString_Return] = *Entry.m_pExplodedValue;
											}

											FunctionCall.m_Params.f_Insert(CBuildSystemSyntax::CParam{fg_Move(Stack)});
										}

										fMapProperty()->m_Value = fp_EvaluatePropertyValueFunctionCall(_Context, FunctionCall, true);
									}

									if (Object.f_IsEmpty())
									{
										CEvalPropertyValueContext Context
											{
												_Context.m_Context
												, _Context.m_OriginalContext
												, *pTypePosition
												, _Context.m_EvalContext
												, &_Context
												, _Context.m_pStorePositions
											}
										;

										fp_CheckValueConformToType
											(
												Context
												, FunctionType.m_Return.f_Get()
												, fMapProperty()->m_Value
												, *pTypePosition
												, *pTypePosition
												, [&]() -> CStr
												{
													return "{}"_f << FunctionPropertyKey;
												}
												, EDoesValueConformToTypeFlag_None
											)
										;
									}
								}

								return fMapProperty()->m_Value;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_ContainsListElement
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_AnyArray, gc_ConstString__ArrayToSearch), fg_FunctionParam(g_Any, gc_ConstString__ElementToFind))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								for (auto &Value : _Params[0].f_Array())
								{
									if (Value == _Params[1])
										return true;
								}

								return false;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Length
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(fg_OneOf(g_AnyArray, g_String), gc_ConstString__Array))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto &ToCheck = _Params[0];
								if (ToCheck.f_IsString())
									return ToCheck.f_String().f_GetLen();
								else
									return ToCheck.f_Array().f_GetLen();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsEmpty
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(fg_OneOf(g_AnyArray, g_String), gc_ConstString__Array))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto &ToCheck = _Params[0];
								if (ToCheck.f_IsString())
									return ToCheck.f_String().f_IsEmpty();
								else
									return ToCheck.f_Array().f_IsEmpty();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Unique
						, CBuiltinFunction
						{
							fg_FunctionType(g_AnyArray, fg_FunctionParam(g_AnyArray, gc_ConstString__Array))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CEJsonSorted Return;
								auto &ReturnArray = Return.f_Array();
								TCSet<CEJsonSorted> Added;
								for (auto &Value : _Params[0].f_Array())
								{
									if (Added(Value).f_WasCreated())
										ReturnArray.f_Insert(fg_Move(Value));
								}

								return Return;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_RemoveDuplicates
						, CBuiltinFunction
						{
							fg_FunctionType(g_AnyArray, fg_FunctionParam(g_AnyArray, gc_ConstString__Array), fg_FunctionParam(g_Any, gc_ConstString__Value))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto &ToUniqueValue = _Params[1];

								CEJsonSorted Return;
								auto &ReturnArray = Return.f_Array();
								mint iLastLocation = TCLimitsInt<mint>::mc_Max;
								mint iLocation = 0;
								for (auto &Value : _Params[0].f_Array())
								{
									if (Value == ToUniqueValue)
										iLastLocation = iLocation;
									else
									{
										ReturnArray.f_Insert(fg_Move(Value));
										++iLocation;
									}
								}

								if (iLastLocation != TCLimitsInt<mint>::mc_Max)
									ReturnArray.f_InsertAfter(iLastLocation, fg_Move(ToUniqueValue));

								return Return;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Sort
						, CBuiltinFunction
						{
							fg_FunctionType(g_AnyArray, fg_FunctionParam(g_AnyArray, gc_ConstString__Array))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								TCVector<CEJsonSorted> Return = fg_Move(_Params[0].f_Array());
								Return.f_Sort();
								return fg_Move(Return);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Concat
						, CBuiltinFunction
						{
							fg_FunctionType(g_AnyArray, fg_FunctionParam(g_AnyArray, gc_ConstString_p_ToConcat, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CEJsonSorted Return;

								auto &ReturnArray = Return.f_Array();
								for (auto &ToConcat : _Params[0].f_Array())
									ReturnArray.f_Insert(fg_Move(ToConcat.f_Array()));

								return Return;
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
