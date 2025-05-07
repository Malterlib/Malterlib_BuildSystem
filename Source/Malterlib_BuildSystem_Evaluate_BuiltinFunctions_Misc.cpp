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

	enum ESwitchType
	{
		ESwitchType_Bare
		, ESwitchType_Default
		, ESwitchType_Error
	};

	CEJsonSorted fg_SwitchValues(ESwitchType _Type, CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params)
	{
		mint iParam = 0;
		auto &Value = _Params[iParam];
		++iParam;

		if (_Type != ESwitchType_Bare)
			++iParam;

		if (!_Params.f_IsPosValid(iParam))
			CBuildSystem::fs_ThrowError(_Context, "What");

		auto &SwitchValues = _Params[iParam].f_Array();
		if (SwitchValues.f_GetLen() & 1)
			CBuildSystem::fs_ThrowError(_Context, "Switch values pairs are uneven");

		mint nSwitchValues = SwitchValues.f_GetLen();

		for (mint iSwitchValue = 0; iSwitchValue < nSwitchValues; iSwitchValue += 2)
		{
			if (SwitchValues[iSwitchValue] == Value)
				return fg_Move(SwitchValues[iSwitchValue + 1]);
		}

		switch (_Type)
		{
		case ESwitchType_Bare: CBuildSystem::fs_ThrowError(_Context, "Value not found in switch: {}"_f << Value); break;
		case ESwitchType_Error: CBuildSystem::fs_ThrowError(_Context, "{}{}"_f << _Params[1].f_String() << Value ); break;
		case ESwitchType_Default: break;
		}

		return fg_Move(_Params[1]);
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
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_SwitchValues(ESwitchType_Bare, _This, _Context, fg_Move(_Params));
							}
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
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_SwitchValues(ESwitchType_Error, _This, _Context, fg_Move(_Params));
							}
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
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_SwitchValues(ESwitchType_Default, _This, _Context, fg_Move(_Params));
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
