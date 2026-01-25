// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/Cryptography/UUID>

namespace NMib::NBuildSystem
{
	constexpr CUniversallyUniqueIdentifier const g_GeneratorFunctionUUIDHashUUIDNamespace(0x010669A0, 0x1AEC, 0x48C9, 0x878A, 0xCFC5FFD996C6_uint64);

	CStr fg_FormatJsonValue(CEJsonSorted const &_Value, bool _bSingleLine)
	{
		bool bSingleLine = _bSingleLine;
		if (!bSingleLine)
		{
			if (!_Value.f_IsArray() && !_Value.f_IsObject() && !_Value.f_IsDate() && !_Value.f_IsBinary() && !_Value.f_IsUserType())
				bSingleLine = true;
		}

		return _Value.f_ToString(bSingleLine ? nullptr : "\t", EJsonDialectFlag_AllowUndefined | EJsonDialectFlag_AllowInvalidFloat).f_Trim();
	}

	CStr fg_FormatJsonArray(TCVector<CEJsonSorted> const &_Array, bool _bSingleLine)
	{
		if (_Array.f_GetLen() == 1 && _Array[0].f_IsString())
			return _Array[0].f_String();

		CStr ToOutput;
		bool bFirst = true;
		for (auto &Param : _Array)
		{
			bool bSingleLine = _bSingleLine;
			if (!bSingleLine)
			{
				if (!Param.f_IsArray() && !Param.f_IsObject() && !Param.f_IsDate() && !Param.f_IsBinary() && !Param.f_IsUserType())
					bSingleLine = true;
			}

			if (bFirst)
			{
				ToOutput += Param.f_ToString(bSingleLine ? nullptr : "\t", EJsonDialectFlag_AllowUndefined | EJsonDialectFlag_AllowInvalidFloat).f_Trim();
				bFirst = false;
			}
			else
			{
				ToOutput += " ";
				ToOutput += Param.f_ToString(bSingleLine ? nullptr : "\t", EJsonDialectFlag_AllowUndefined | EJsonDialectFlag_AllowInvalidFloat).f_Trim();
			}
		}

		return ToOutput;
	}

	CEJsonSorted CBuildSystem::fp_SwitchValuesLazy(ESwitchType _Type, CBuildSystem::CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CFunctionCall const &_FunctionCall) const
	{
		using namespace NStr;
		auto const &Params = _FunctionCall.m_Params;
		auto iParam = Params.f_GetIterator();

		TCVector<CEJsonSorted> ExpandedArrayStorage;
		CEJsonSorted *pExpandedArray = nullptr;
		CEJsonSorted *pExpandedArrayEnd = nullptr;

		auto fTryExpandAppendParam = [&](CBuildSystemSyntax::CParam const &_Param) -> bool
			{
				if (!_Param.m_Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>())
					return false;

				auto &Expression = _Param.m_Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>().f_Get();
				auto ToAppend = fp_EvaluatePropertyValueExpression(_Context, Expression);
				auto &ToAppendRef = ToAppend.f_Get();
				if (ToAppendRef.f_IsArray())
				{
					ExpandedArrayStorage = ToAppend.f_MoveArray();
					pExpandedArray = ExpandedArrayStorage.f_GetArray();
					pExpandedArrayEnd = pExpandedArray + ExpandedArrayStorage.f_GetLen();
				}
				else if (!ToAppendRef.f_IsValid())
				{
					// Undefined values result in empty expansion
					pExpandedArray = nullptr;
					pExpandedArrayEnd = nullptr;
				}
				else
				{
					CBuildSystem::fs_ThrowError(_Context, "Append expressions expects an array to expand. {} resulted in : {}"_f << Expression << ToAppendRef);
				}
				return true;
			}
		;

		using CNextValueResult = TCVariant<void, CEJsonSorted, CBuildSystemSyntax::CParam const *>;
		auto fGetNextValue = [&](this auto &_fThis, bool _bEvaluate) -> CNextValueResult
			{
				if (pExpandedArray != pExpandedArrayEnd)
					return fg_Move(*pExpandedArray++);

				pExpandedArray = nullptr;
				pExpandedArrayEnd = nullptr;

				if (!iParam)
					return {};

				auto &Param = *iParam;
				++iParam;

				if (fTryExpandAppendParam(Param))
				{
					if (pExpandedArray != pExpandedArrayEnd)
						return fg_Move(*pExpandedArray++);

					pExpandedArray = nullptr;
					pExpandedArrayEnd = nullptr;
					return _fThis(_bEvaluate);
				}

				if (!_bEvaluate)
					return &Param;

				return fp_EvaluatePropertyValueParam(_Context, Param).f_Move();
			}
		;

		auto fGetFunctionName = [&]() -> CStr
			{
				return (_Type == ESwitchType_Bare) ? "Switch" : (_Type == ESwitchType_Error) ? "SwitchWithError" : "SwitchWithDefault";
			}
		;

		auto ValueResult = fGetNextValue(true);
		if (ValueResult.f_IsOfType<void>())
			CBuildSystem::fs_ThrowError(_Context, "Missing parameters for function '{}'"_f << fGetFunctionName());
		CEJsonSorted Value = fg_Move(ValueResult.f_GetAsType<CEJsonSorted>());

		CNextValueResult ErrorOrDefaultResult;
		if (_Type != ESwitchType_Bare)
		{
			ErrorOrDefaultResult = fGetNextValue(false);
			if (ErrorOrDefaultResult.f_IsOfType<void>())
				CBuildSystem::fs_ThrowError(_Context, "Missing parameters for function '{}'"_f << fGetFunctionName());
		}

		auto fSkipOneValue = [&]() -> bool
			{
				if (pExpandedArray != pExpandedArrayEnd)
				{
					++pExpandedArray;
					return true;
				}

				pExpandedArray = nullptr;
				pExpandedArrayEnd = nullptr;

				if (!iParam)
					return false;

				auto &Param = *iParam;
				++iParam;

				if (fTryExpandAppendParam(Param))
				{
					if (pExpandedArray != pExpandedArrayEnd)
						++pExpandedArray;
				}

				return true;
			}
		;

		while (true)
		{
			auto CaseKeyResult = fGetNextValue(true);
			if (CaseKeyResult.f_IsOfType<void>())
				break;

			if (CaseKeyResult.f_GetAsType<CEJsonSorted>() == Value)
			{
				auto CaseValueResult = fGetNextValue(true);
				if (CaseValueResult.f_IsOfType<void>())
					CBuildSystem::fs_ThrowError(_Context, "Switch values pairs are uneven");

				return fg_Move(CaseValueResult.f_GetAsType<CEJsonSorted>());
			}
			else
			{
				if (!fSkipOneValue())
					CBuildSystem::fs_ThrowError(_Context, "Switch values pairs are uneven");
			}
		}

		if (_Type != ESwitchType_Bare)
		{
			CEJsonSorted ErrorOrDefault;
			if (ErrorOrDefaultResult.f_IsOfType<CEJsonSorted>())
				ErrorOrDefault = fg_Move(ErrorOrDefaultResult.f_GetAsType<CEJsonSorted>());
			else
				ErrorOrDefault = fp_EvaluatePropertyValueParam(_Context, *ErrorOrDefaultResult.f_GetAsType<CBuildSystemSyntax::CParam const *>()).f_Move();

			if (_Type == ESwitchType_Error)
			{
				fp_CheckValueConformToType
					(
						_Context
						, g_String
						, ErrorOrDefault
						, _Context.m_Position
						, _Context.m_Position
						, [&]() -> CStr
						{
							return "In call to function 'SwitchWithError' parameter _Error (1)"_f;
						}
						, EDoesValueConformToTypeFlag_None
					)
				;
				CBuildSystem::fs_ThrowError(_Context, "{}{}"_f << ErrorOrDefault.f_String() << Value);
			}
			else // ESwitchType_Default
			{
				return fg_Move(ErrorOrDefault);
			}
		}

		CBuildSystem::fs_ThrowError(_Context, "Value not found in switch: {}"_f << Value);
		return {};
	}

	void CBuildSystem::fp_RegisterBuiltinFunctions_Misc()
	{
		f_RegisterFunctions
			(
				{
					{
						gc_ConstString_Error
						, CBuiltinFunction
						{
							fg_FunctionType(g_Void, fg_FunctionParam(g_Any, gc_ConstString_p_ErrorValues, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								fs_ThrowError(_Context, fg_FormatJsonArray(_Params[0].f_Array(), false));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_ErrorWithPositions
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Void
								, fg_FunctionParam(fg_Array(fg_TempCopy(g_Position)), gc_ConstString__Positions)
								, fg_FunctionParam(g_Any, gc_ConstString_p_ErrorValues, g_Ellipsis)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (!_Params[0].f_Array().f_IsEmpty())
									fs_ThrowError(_Context, CBuildSystemUniquePositions::fs_FromJson(_Params[0]), fg_FormatJsonArray(_Params[1].f_Array(), false));
								else
									fs_ThrowError(_Context, fg_FormatJsonArray(_Params[1].f_Array(), false));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Log
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_Any, gc_ConstString__ToReturn), fg_FunctionParam(g_Any, gc_ConstString_p_LogValues, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params[1].f_Array().f_IsEmpty())
									_This.f_OutputConsole("{}\n"_f << fg_FormatJsonValue(_Params[0], false));
								else
									_This.f_OutputConsole("{}\n"_f << fg_FormatJsonArray(_Params[1].f_Array(), false));

								return fg_Move(_Params[0]);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_LogWithSequence
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_Any, gc_ConstString__ToReturn), fg_FunctionParam(g_Any, gc_ConstString_p_LogValues, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params[1].f_Array().f_IsEmpty())
									_This.f_OutputConsole("{}: {}\n"_f << _This.mp_LogSequence.f_FetchAdd(1) << fg_FormatJsonValue(_Params[0], false));
								else
									_This.f_OutputConsole("{}: {}\n"_f << _This.mp_LogSequence.f_FetchAdd(1) << fg_FormatJsonArray(_Params[1].f_Array(), false));

								return fg_Move(_Params[0]);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Warning
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_Any, gc_ConstString__ToReturn), fg_FunctionParam(g_Any, gc_ConstString_p_LogValues, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params[1].f_Array().f_IsEmpty())
									_This.f_OutputConsole("{}\n"_f << fg_FormatJsonValue(_Params[0], false), true);
								else
									_This.f_OutputConsole("{}\n"_f << fg_FormatJsonArray(_Params[1].f_Array(), false), true);

								return fg_Move(_Params[0]);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_MalterlibTime
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__FormatString))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params.f_GetLen() != 1 || !_Params[0].f_IsString())
									fs_ThrowError(_Context, "MalterlibTime takes one string parameter");

								NTime::CTime Time = _This.mp_NowUTC;
								return CStr::CFormat(_Params[0].f_String()) << Time.f_GetSeconds() << Time.f_GetFractionInt();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_BuildSystemEnvironment
						, CBuiltinFunction
						{
							fg_FunctionType(g_ObjectWithAny)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CEJsonSorted Environment(EEJsonType_Object);

								auto &EnvironmentObject = Environment.f_Object();
								for (auto &EnvEntry : _This.mp_Environment.f_Entries())
									EnvironmentObject[EnvEntry.f_Key()] = EnvEntry.f_Value();

								return Environment;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_DateTime
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__FormatString), fg_FunctionParam(fg_Optional(g_Date), gc_ConstString__Time, g_Optional))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr Format = _Params[0].f_String();

								Format = Format.f_Replace("{YYYY}", "{0}");
								Format = Format.f_Replace("{YY}", "{0,sl2,sf0}");
								Format = Format.f_Replace("{M}", "{1}");
								Format = Format.f_Replace("{MM}", "{1,sl2,sf0}");
								Format = Format.f_Replace("{D}", "{2}");
								Format = Format.f_Replace("{DD}", "{2,sl2,sf0}");
								Format = Format.f_Replace("{H}", "{3}");
								Format = Format.f_Replace("{HH}", "{3,sl2,sf0}");
								Format = Format.f_Replace("{MN}", "{4}");
								Format = Format.f_Replace("{MNMN}", "{4,sl2,sf0}");
								Format = Format.f_Replace("{S}", "{5}");
								Format = Format.f_Replace("{SS}", "{5,sl2,sf0}");
								Format = Format.f_Replace("{F}", "{6}");
								Format = Format.f_Replace("{WD}", "{7}");

								static constexpr const ch8 *c_WeekDays[] =
									{
										""
										, "Monday"
										, "Tuesday"
										, "Wednesday"
										, "Thursday"
										, "Friday"
										, "Saturday"
										, "Sunday"
									}
								;

								NTime::CTime Time;
								if (_Params[1].f_IsValid())
									Time = _Params[1].f_Date();
								else
									Time = _This.mp_Now;

								NTime::CTimeConvert::CDateTime DateTime;
								NTime::CTimeConvert(Time).f_ExtractDateTime(DateTime);

								CStr Ret = CStr::CFormat(Format)
									<< DateTime.m_Year
									<< DateTime.m_Month
									<< DateTime.m_DayOfMonth
									<< DateTime.m_Hour
									<< DateTime.m_Minute
									<< DateTime.m_Second
									<< DateTime.m_Fraction
									<< c_WeekDays[DateTime.m_DayOfWeek]
								;

								return fg_Move(Ret);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_HashUUID
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_ConstString__StringToHash)
								, fg_FunctionParam
								(
									fg_Optional(CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf{{gc_ConstString_Registry, gc_ConstString_Bare, gc_ConstString_AlphaNum}}})
									, gc_ConstString__OutputFormat
									, g_Optional
								)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								EUniversallyUniqueIdentifierFormat UUIDFormat = EUniversallyUniqueIdentifierFormat_Registry;

								if (_Params[1].f_IsValid())
								{
									auto &OutputFormat = _Params[1].f_String();
									if (OutputFormat == gc_ConstString_Registry.m_String)
										UUIDFormat = EUniversallyUniqueIdentifierFormat_Registry;
									else if (OutputFormat == gc_ConstString_Bare.m_String)
										UUIDFormat = EUniversallyUniqueIdentifierFormat_Bare;
									else if (OutputFormat == gc_ConstString_AlphaNum.m_String)
										UUIDFormat = EUniversallyUniqueIdentifierFormat_AlphaNum;
									else
										DMibNeverGetHere;
								}

								return CUniversallyUniqueIdentifier
									(
										EUniversallyUniqueIdentifierGenerate_StringHash
										, g_GeneratorFunctionUUIDHashUUIDNamespace
										, _Params[0].f_String()
									)
									.f_GetAsString(UUIDFormat)
								;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_HashSHA256
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_ConstString__StringToHash)
								, fg_FunctionParam(fg_Optional(g_Integer), gc_ConstString__Length, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								mint HashLen = 64;
								if (_Params[1].f_IsValid())
									HashLen = _Params[1].f_Integer();

								NCryptography::CHash_SHA256 Hash;
								if (_Params[0].f_IsString())
									Hash.f_AddData(_Params[0].f_String().f_GetStr(), _Params[0].f_String().f_GetLen());
								else
									Hash.f_AddData(_Params[0].f_Binary().f_GetArray(), _Params[0].f_Binary().f_GetLen());

								return Hash.f_GetDigest().f_GetString().f_Left(HashLen);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsDefined
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_String, gc_ConstString__Property)
							)
							, [this](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CPropertyKey PropertyKey = CPropertyKey::fs_FromString(mp_StringCache, _Params[0].f_String(), _Context);

								auto *pTypeWithPosition = fp_GetTypeForProperty(_Context, PropertyKey.f_Reference());
								return !!pTypeWithPosition;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Switch
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Any
								, fg_FunctionParam(g_Any, gc_ConstString__Value)
								, fg_FunctionParam(g_Any, gc_ConstString_p_SwitchValues, g_Ellipsis)
							)
							, FEvalPropertyFunctionLazy
							(
								[](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CFunctionCall const &_FunctionCall, CBuildSystem::CBuiltinFunction const &_Function) -> CEJsonSorted
								{
									return _This.fp_SwitchValuesLazy(ESwitchType_Bare, _Context, _FunctionCall);
								}
							)
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_SwitchWithError
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Any
								, fg_FunctionParam(g_Any, gc_ConstString__Value)
								, fg_FunctionParam(g_String, gc_ConstString__Error)
								, fg_FunctionParam(g_Any, gc_ConstString_p_SwitchValues, g_Ellipsis)
							)
							, FEvalPropertyFunctionLazy
							(
								[](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CFunctionCall const &_FunctionCall, CBuildSystem::CBuiltinFunction const &_Function) -> CEJsonSorted
								{
									return _This.fp_SwitchValuesLazy(ESwitchType_Error, _Context, _FunctionCall);
								}
							)
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_SwitchWithDefault
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Any
								, fg_FunctionParam(g_Any, gc_ConstString__Value)
								, fg_FunctionParam(g_Any, gc_ConstString__Default)
								, fg_FunctionParam(g_Any, gc_ConstString_p_SwitchValues, g_Ellipsis)
							)
							, FEvalPropertyFunctionLazy
							(
								[](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CFunctionCall const &_FunctionCall, CBuildSystem::CBuiltinFunction const &_Function) -> CEJsonSorted
								{
									return _This.fp_SwitchValuesLazy(ESwitchType_Default, _Context, _FunctionCall);
								}
							)
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
