// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/Cryptography/UUID>

namespace NMib::NBuildSystem
{
	CUniversallyUniqueIdentifier g_GeneratorFunctionUUIDHashUUIDNamespace("{010669A0-1AEC-48C9-878A-CFC5FFD996C6}");

	CStr fg_FormatJSONArray(TCVector<CEJSON> const &_Array, bool _bSingleLine)
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
				ToOutput += Param.f_ToString(bSingleLine ? nullptr : "\t", EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat).f_Trim();
				bFirst = false;
			}
			else
			{
				ToOutput += " ";
				ToOutput += Param.f_ToString(bSingleLine ? nullptr : "\t", EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat).f_Trim();
			}
		}

		return ToOutput;
	}

	void CBuildSystem::fp_RegisterBuiltinFunctions_Misc()
	{
		f_RegisterFunctions
			(
				{
					{
						"Error"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Void, fg_FunctionParam(g_Any, "p_ErrorValues", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								fs_ThrowError(_Context, fg_FormatJSONArray(_Params[0].f_Array(), false));
							}
						}
					}
					,
					{
						"Log"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_Any, "_ToReturn"), fg_FunctionParam(g_Any, "p_LogValues", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								_This.f_OutputConsole("{}\n"_f << fg_FormatJSONArray(_Params[1].f_Array(), false));
								return fg_Move(_Params[0]);
							}
						}
					}
					,
					{
						"Warning"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_Any, "_ToReturn"), fg_FunctionParam(g_Any, "p_LogValues", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								_This.f_OutputConsole("{}\n"_f << fg_FormatJSONArray(_Params[1].f_Array(), false), true);
								return fg_Move(_Params[0]);
							}
						}
					}
					,
					{
						"MalterlibTime"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_FormatString"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								if (_Params.f_GetLen() != 1 || !_Params[0].f_IsString())
									fs_ThrowError(_Context, "MalterlibTime takes one string parameter");

								NTime::CTime Time = _This.mp_NowUTC;
								return CStr::CFormat(_Params[0].f_String()) << Time.f_GetSeconds() << Time.f_GetFractionInt();
							}
						}
					}
					,
					{
						"DateTime"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_FormatString"), fg_FunctionParam(fg_Optional(g_Date), "_Time", g_Optional))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
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
						}
					}
					,
					{
						"HashUUID"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, "_StringToHash")
								, fg_FunctionParam(fg_Optional(CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf{{"Registry", "Bare", "AlphaNum"}}}), "_OutputFormat", g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								EUniversallyUniqueIdentifierFormat UUIDFormat = EUniversallyUniqueIdentifierFormat_Registry;

								if (_Params[1].f_IsValid())
								{
									auto &OutputFormat = _Params[1].f_String();
									if (OutputFormat == "Registry")
										UUIDFormat = EUniversallyUniqueIdentifierFormat_Registry;
									else if (OutputFormat == "Bare")
										UUIDFormat = EUniversallyUniqueIdentifierFormat_Bare;
									else if (OutputFormat == "AlphaNum")
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
						}
					}
					,
					{
						"HashSHA256"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, "_StringToHash")
								, fg_FunctionParam(fg_Optional(g_Integer), "_Length", g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
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
						}
					}
					,
					{
						"IsDefined"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_String, "_Property")
							)
							, [this](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CPropertyKey PropertyKey = CPropertyKey::fs_FromString(_Params[0].f_String(), _Context);

								auto *pTypeWithPosition = fp_GetTypeForProperty(_Context, PropertyKey);
								return !!pTypeWithPosition;
							}
						}
					}
				}
			)
		;
	}
}
