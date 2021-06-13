// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/Process/ProcessLaunch>

namespace NMib::NBuildSystem
{
	CStr fg_FormatJSONArray(TCVector<CEJSON> const &_Array, bool _bSingleLine);

	void CBuildSystem::fp_RegisterBuiltinFunctions_String()
	{
		f_RegisterFunctions
			(
				{
					{
						"ToString"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Any, "p_Values", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return fg_FormatJSONArray(_Params[0].f_Array(), false);
							}
						}
					}
					,
					{
						"ToStringCompact"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Any, "p_Values", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return fg_FormatJSONArray(_Params[0].f_Array(), true);
							}
						}
					}
					,
					{
						"EJSONToString"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_Any, "_EJSON"), fg_FunctionParam(fg_Optional(g_String), "_IndentString", g_Optional))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr Return;
								try
								{
									auto &ToFormat = _Params[0];
									ch8 const *pIndentString = "\t";
									if (!ToFormat.f_IsArray() && !ToFormat.f_IsObject() && !ToFormat.f_IsDate() && !ToFormat.f_IsBinary() && !ToFormat.f_IsUserType())
										pIndentString = nullptr;

									if (_Params[1].f_IsValid())
									{
										if (_Params[1].f_String().f_IsEmpty())
											pIndentString = nullptr;
										else
											pIndentString = _Params[1].f_String();
									}

									Return = ToFormat.f_ToString(pIndentString, EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat).f_Trim();
								}
								catch (CException const &_Exception)
								{
									fsp_ThrowError(_Context, "Failed to convert EJSON to string: {}"_f << _Exception);
								}
								return Return;
							}
						}
					}
					,
					{
						"JSONToString"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_Any, "_JSON"), fg_FunctionParam(fg_Optional(g_String), "_IndentString", g_Optional))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr Return;
								try
								{
									auto &ToFormat = _Params[0];
									ch8 const *pIndentString = "\t";
									if (!ToFormat.f_IsArray() && !ToFormat.f_IsObject())
										pIndentString = nullptr;

									if (_Params[1].f_IsValid())
									{
										if (_Params[1].f_String().f_IsEmpty())
											pIndentString = nullptr;
										else
											pIndentString = _Params[1].f_String();
									}

									Return = ToFormat.f_ToJSONNoConvert().f_ToString(pIndentString).f_Trim();
								}
								catch (CException const &_Exception)
								{
									fsp_ThrowError(_Context, "Failed to convert JSON to string: {}"_f << _Exception);
								}
								return Return;
							}
						}
					}
					,
					{
						"ParseEJSON"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_String, "_String"), fg_FunctionParam(fg_Optional(g_String), "_FileName", g_Optional))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr FileName;
								if (_Params[1].f_IsValid())
									FileName = _Params[1].f_String();

								CEJSON Return;
								try
								{
									Return = CEJSON::fs_FromString(_Params[0].f_String(), FileName);
								}
								catch (CException const &_Exception)
								{
									fsp_ThrowError(_Context, "Failed to parse EJSON string: {}"_f << _Exception);
								}
								return Return;
							}
						}
					}
					,
					{
						"ParseJSON"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Any, fg_FunctionParam(g_String, "_String"), fg_FunctionParam(fg_Optional(g_String), "_FileName", g_Optional))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr FileName;
								if (_Params[1].f_IsValid())
									FileName = _Params[1].f_String();

								CEJSON Return;
								try
								{
									Return = CEJSON::fs_FromJSONNoConvert(CJSON::fs_FromString(_Params[0].f_String(), FileName));
								}
								catch (CException const &_Exception)
								{
									fsp_ThrowError(_Context, "Failed to parse JSON string: {}"_f << _Exception);
								}
								return Return;
							}
						}
					}
					,
					{
						"Split"
						, CBuiltinFunction
						{
							fg_FunctionType(g_StringArray, fg_FunctionParam(g_String, "_Source"), fg_FunctionParam(g_String, "_SplitBy"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_String().f_Split<true>(_Params[1].f_String());
							}
						}
					}
					,
					{
						"Join"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_StringArray, "_Strings"), fg_FunctionParam(g_String, "_JoinBy"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CStr::fs_Join(_Params[0].f_StringArray(), _Params[1].f_String());
							}
						}
					}
					,
					{
						"Escape"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Source"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_String().f_EscapeStr();
							}
						}
					}
					,
					{
						"EscapeHost"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "p_Source", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
#ifdef DPlatformFamily_Windows
								return CProcessLaunchParams::fs_GetParamsWindows(_Params[0].f_StringArray());
#else
								return CProcessLaunchParams::fs_GetParamsBash(_Params[0].f_StringArray());
#endif
							}
						}
					}
					,
					{
						"EscapeWindows"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "p_Source", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return NProcess::CProcessLaunchParams::fs_GetParamsWindows(_Params[0].f_StringArray());
							}
						}
					}
					,
					{
						"EscapeBash"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "p_Source", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return NProcess::CProcessLaunchParams::fs_GetParamsBash(_Params[0].f_StringArray());
							}
						}
					}
					,
					{
						"Trim"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Source"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_String().f_Trim();
							}
						}
					}
					,
					{
						"Replace"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Source"), fg_FunctionParam(g_String, "_SearchFor"), fg_FunctionParam(g_String, "_ReplaceWith"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_String().f_Replace(_Params[1].f_String(), _Params[2].f_String());
							}
						}
					}
					,
					{
						"ReplaceChars"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, "_String")
								, fg_FunctionParam(g_String, "_SearchForCharacters")
								, fg_FunctionParam(g_String, "_ReplaceWithString")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr Return = _Params[0].f_String();
								auto &SearchForChars = _Params[1].f_String();
								auto &ToReplaceWith = _Params[2].f_String();

								TCVector<ch8> Characters;
								Characters.f_Insert(SearchForChars.f_GetStr(), SearchForChars.f_GetLen());

								for (auto &Character : Characters)
								{
									ch8 ToFind[] = {Character, 0};
									Return = Return.f_Replace(ToFind, ToReplaceWith);
								}

								return Return;
							}
						}
					}
					,
					{
						"FindGetLine"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Source"), fg_FunctionParam(g_String, "_WildcardToSearchFor"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								ch8 const *pPattern = _Params[1].f_String().f_GetStr();
								ch8 const *pParse = _Params[0].f_String().f_GetStr();

								while (*pParse)
								{
									auto pParseStart = pParse;
									fg_ParseToEndOfLine(pParse);

									CStr Line(pParseStart, pParse - pParseStart);

									if (fg_StrMatchWildcard(Line.f_GetStr(), pPattern) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
										return Line;

									fg_ParseEndOfLine(pParse);
								}

								return {};
							}
						}
					}
					,
					{
						"Find"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_Source"), fg_FunctionParam(g_String, "_StringToFind"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								if (_Params[0].f_String().f_Find(_Params[1].f_String()) >= 0)
									return true;
								return false;
							}
						}
					}
					,
					{
						"FindNoCase"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_Source"), fg_FunctionParam(g_String, "_StringToFind"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								if (_Params[0].f_String().f_FindNoCase(_Params[1].f_String()) >= 0)
									return true;
								return false;
							}
						}
					}
					,
					{
						"StartsWith"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_Source"), fg_FunctionParam(g_String, "_StringToFind"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_String().f_StartsWith(_Params[1].f_String());
							}
						}
					}
					,
					{
						"EndsWith"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_Source"), fg_FunctionParam(g_String, "_StringToFind"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_String().f_EndsWith(_Params[1].f_String());
							}
						}
					}
					,
					{
						"Format"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Format"), fg_FunctionParam(g_Any, "p_Params", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr::CFormat Format(_Params[0].f_String());

								for (auto &Param : _Params[1].f_Array())
								{
									switch (Param.f_EType())
									{
									case EEJSONType_String:
										Format << Param.f_String();
										break;
									case EEJSONType_Integer:
										Format << Param.f_Integer();
										break;
									case EEJSONType_Float:
										Format << Param.f_Float();
										break;
									case EEJSONType_Boolean:
										Format << Param.f_Boolean();
										break;
									case EEJSONType_Date:
										Format << Param.f_Date();
										break;
									case EEJSONType_Binary:
										Format << fg_Base64Encode(Param.f_Binary());
										break;
									case EEJSONType_Invalid:
									case EEJSONType_Null:
									case EEJSONType_Object:
									case EEJSONType_Array:
									case EEJSONType_UserType:
										// Use JSON built in formatter
										Format << Param;
										break;
									}
								}

								return Format.f_GetStr();
							}
						}
					}
					,
					{
						"ParseFormatString"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_StringToParse"), fg_FunctionParam(g_String, "_ParseFormat"), fg_FunctionParam(g_String, "_OutputFormat"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr Params[32];
								auto Parse = CStr::CParse(_Params[1].f_String());

								for (auto i = 0; i < 32; ++i)
									Parse >> Params[i];

								Parse.f_Parse(_Params[0].f_String());

								auto Format = CStr::CFormat(_Params[2].f_String());

								for (auto i = 0; i < 32; ++i)
									Format << Params[i];

								return Format.f_GetStr();
							}
						}
					}
					,
					{
						"Parse"
						, CBuiltinFunction
						{
							fg_FunctionType(g_AnyArray, fg_FunctionParam(g_String, "_StringToParse"), fg_FunctionParam(g_String, "_ParseFormat"), fg_FunctionParam(g_String, "p_Types", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								auto Parse = CStr::CParse(_Params[1].f_String());

								CEJSON Output = EJSONType_Array;
								auto &TypeParams = _Params[2].f_Array();
								mint nTypeParams = TypeParams.f_GetLen();
								Output.f_SetLen(nTypeParams);
								mint iType = 0;

								for (auto &TypeParam : TypeParams)
								{
									auto &OutputVar = Output[iType++];
									auto &Type = TypeParam.f_String();
									if (Type == "string")
										Parse >> OutputVar.f_String();
									else if (Type == "int")
										Parse >> OutputVar.f_Integer();
									else if (Type == "float")
										Parse >> OutputVar.f_Float();
									else
										fsp_ThrowError(_Context, "Only string, int, and float are supported");
								}

								Parse.f_Parse(_Params[0].f_String());

								return Output;
							}
						}
					}
					,
					{
						"Sanitize"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, "_String")
								, fg_FunctionParam(fg_Optional(CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf{{"rfc1034", "bash"}}}), "_Format", g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								if (_Params.f_GetLen() < 1 || _Params.f_GetLen() > 2 || !_Params[0].f_IsString() || (_Params.f_GetLen() > 1 && _Params[1].f_IsString()))
									fsp_ThrowError(_Context, "Sanitize takes one or two string parameters: String [Format]");

								CStr Format = "rfc1034";
								if (_Params[1].f_IsValid())
									Format = _Params[1].f_String();

								CStr Return = _Params[0].f_String();
								if (Format.f_CmpNoCase("rfc1034") == 0)
								{
									CStr::CChar *pChar = Return.f_GetStrUniqueWritable();
									CStr::CChar CurChar;

									while (*pChar)
									{
										CurChar = *pChar;

										if
											(
												(CurChar >= 'a' && CurChar <= 'z')
												|| (CurChar >= 'A' && CurChar <= 'Z')
												|| (CurChar >= '0' && CurChar <= '9')
												|| (CurChar == '.' || CurChar == '-')
											)
										{
										}
										else
											*pChar = '-';

										++pChar;
									}
								}
								else if (Format.f_CmpNoCase("bash") == 0)
								{
									CStr::CChar const *pChar = Return.f_GetStr();
									CStr::CChar CurChar;

									CStr::CChar const *pStartRun = pChar;

									CStr Out;

									while (*pChar)
									{
										CurChar = *pChar;

										if
											(
												CurChar == ' '
												|| CurChar == '"'
												|| CurChar == '\''
											)
										{
											if (pStartRun < pChar)
												Out.f_AddStr(pStartRun, pChar - pStartRun);
											Out += "\\";
											Out += CurChar;
											pStartRun = pChar + 1;
										}

										++pChar;
									}

									if (pStartRun < pChar)
										Out.f_AddStr(pStartRun, pChar - pStartRun);

									Return = Out;
								}
								else
									DMibNeverGetHere;

								return fg_Move(Return);
							}
						}
					}
				}
			)
		;
	}
}
