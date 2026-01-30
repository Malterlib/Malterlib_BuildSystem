// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/Process/ProcessLaunch>
#include <Mib/Compression/Zstandard>

namespace NMib::NBuildSystem
{
	CStr fg_FormatJsonArray(TCVector<CEJsonSorted> const &_Array, bool _bSingleLine);

	void CBuildSystem::fp_RegisterBuiltinFunctions_String()
	{
		CBuiltinFunction SharedFunction_EJsonToString
			{
				fg_FunctionType(g_Any, fg_FunctionParam(g_Any, gc_ConstString__EJson), fg_FunctionParam(fg_Optional(g_String), gc_ConstString__IndentString, g_Optional))
				, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
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

						Return = ToFormat.f_ToString(pIndentString, EJsonDialectFlag_AllowUndefined | EJsonDialectFlag_AllowInvalidFloat | EJsonDialectFlag_TrimWhitespace);
					}
					catch (CException const &_Exception)
					{
						fs_ThrowError(_Context, "Failed to convert EJSON to string: {}"_f << _Exception);
					}
					return Return;
				}
				, DMibBuildSystemFilePosition
			}
		;

		CBuiltinFunction SharedFunction_JsonToString
			{
				fg_FunctionType(g_Any, fg_FunctionParam(g_Any, gc_ConstString__Json), fg_FunctionParam(fg_Optional(g_String), gc_ConstString__IndentString, g_Optional))
				, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
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

						Return = ToFormat.f_ToJsonNoConvert().f_ToString(pIndentString).f_Trim();
					}
					catch (CException const &_Exception)
					{
						fs_ThrowError(_Context, "Failed to convert JSON to string: {}"_f << _Exception);
					}
					return Return;
				}
				, DMibBuildSystemFilePosition
			}
		;

		CBuiltinFunction SharedFunction_ParseEJson
			{
				fg_FunctionType(g_Any, fg_FunctionParam(g_String, gc_ConstString__String), fg_FunctionParam(fg_Optional(g_String), gc_ConstString__FileName, g_Optional))
				, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
				{
					CStr FileName;
					if (_Params[1].f_IsValid())
						FileName = _Params[1].f_String();

					CEJsonSorted Return;
					try
					{
						Return = CEJsonSorted::fs_FromString(_Params[0].f_String(), FileName);
					}
					catch (CException const &_Exception)
					{
						fs_ThrowError(_Context, "Failed to parse EJSON string: {}"_f << _Exception);
					}
					return Return;
				}
				, DMibBuildSystemFilePosition
			}
		;

		CBuiltinFunction SharedFunction_ParseJson
			{
				fg_FunctionType(g_Any, fg_FunctionParam(g_String, gc_ConstString__String), fg_FunctionParam(fg_Optional(g_String), gc_ConstString__FileName, g_Optional))
				, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
				{
					CStr FileName;
					if (_Params[1].f_IsValid())
						FileName = _Params[1].f_String();

					CEJsonSorted Return;
					try
					{
						Return = CEJsonSorted::fs_FromJsonNoConvert(CJsonSorted::fs_FromString(_Params[0].f_String(), FileName));
					}
					catch (CException const &_Exception)
					{
						fs_ThrowError(_Context, "Failed to parse JSON string: {}"_f << _Exception);
					}
					return Return;
				}
				, DMibBuildSystemFilePosition
			}
		;

		f_RegisterFunctions
			(
				{
					{
						gc_ConstString_ToString
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Any, gc_ConstString_p_Values, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_FormatJsonArray(_Params[0].f_Array(), false);
							}
							, DMibBuildSystemFilePosition // ToString
						}
					}
					,
					{
						gc_ConstString_ToStringCompact
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Any, gc_ConstString_p_Values, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_FormatJsonArray(_Params[0].f_Array(), true);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_EJsonToString
						, SharedFunction_EJsonToString
					}
					,
					{
						gc_ConstString_EJSONToString
						, SharedFunction_EJsonToString
					}
					,
					{
						gc_ConstString_JsonToString
						, SharedFunction_JsonToString
					}
					,
					{
						gc_ConstString_JSONToString
						, SharedFunction_JsonToString
					}
					,
					{
						gc_ConstString_ParseEJson
						, SharedFunction_ParseEJson
					}
					,
					{
						gc_ConstString_ParseEJSON
						, SharedFunction_ParseEJson
					}
					,
					{
						gc_ConstString_ParseJson
						, SharedFunction_ParseJson
					}
					,
					{
						gc_ConstString_ParseJSON
						, SharedFunction_ParseJson
					}
					,
					{
						gc_ConstString_Split
						, CBuiltinFunction
						{
							fg_FunctionType(g_StringArray, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__SplitBy))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_Split<true>(_Params[1].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Join
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_StringArray, gc_ConstString__Strings), fg_FunctionParam(g_String, gc_ConstString__JoinBy))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return CStr::fs_Join(_Params[0].f_StringArray(), _Params[1].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_JoinEscaped
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_StringArray, gc_ConstString__Strings), fg_FunctionParam(g_String, gc_ConstString__JoinBy))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto const &JoinString = _Params[1].f_String();
								if (JoinString.f_GetLen() != 1)
									fs_ThrowError(_Context, "Joining escaped the join by string needs to be exactly 1 character");

								auto JoinChar = JoinString[0];

								CStr Return;
								for (auto &String : _Params[0].f_StringArray())
									fg_AddStrSepEscaped(Return, String, JoinChar);

								return Return;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Escape
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_EscapeStr();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_EscapeHost
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString_p_Source, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
#ifdef DPlatformFamily_Windows
								return CProcessLaunchParams::fs_GetParamsWindows(_Params[0].f_StringArray());
#else
								return CProcessLaunchParams::fs_GetParamsBash(_Params[0].f_StringArray());
#endif
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_EscapeWindows
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString_p_Source, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return NProcess::CProcessLaunchParams::fs_GetParamsWindows(_Params[0].f_StringArray());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_EscapeMSBuild
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__String))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto &SourceString = _Params[0].f_String();

								if (SourceString.f_StartsWith(gc_ConstString_RawMSBuild_.m_String))
									return SourceString.f_Extract(11);

								auto pParse = SourceString.f_GetStr();

								CStr Return;
								{
									CStr::CAppender StringAppender(Return);

									while (*pParse)
									{
										auto Character = *pParse;
										switch (Character)
										{
										case '%': StringAppender += gc_ConstString__25.m_String; break;
										case '$': StringAppender += gc_ConstString__24.m_String; break;
										case '@': StringAppender += gc_ConstString__40.m_String; break;
										case '\'': StringAppender += gc_ConstString__27.m_String; break;
										case '(': StringAppender += gc_ConstString__28.m_String; break;
										case ')': StringAppender += gc_ConstString__29.m_String; break;
										case ';': StringAppender += gc_ConstString__3B.m_String; break;
										case '?': StringAppender += gc_ConstString__3F.m_String; break;
										case '*': StringAppender += gc_ConstString__2A.m_String; break;
										default: StringAppender += Character; break;
										}

										++pParse;
									}
								}

								return fg_Move(Return);
							}
							, DMibBuildSystemFilePosition
						}
					}


					,
					{
						gc_ConstString_EscapeBash
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString_p_Source, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return NProcess::CProcessLaunchParams::fs_GetParamsBash(_Params[0].f_StringArray());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Trim
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_Trim();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Replace
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_ConstString__Source)
								, fg_FunctionParam(g_String, gc_ConstString__SearchFor)
								, fg_FunctionParam(g_String, gc_ConstString__ReplaceWith)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_Replace(_Params[1].f_String(), _Params[2].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_ReplaceChars
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_ConstString__String)
								, fg_FunctionParam(g_String, gc_ConstString__SearchForCharacters)
								, fg_FunctionParam(g_String, gc_ConstString__ReplaceWithString)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
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
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_FindGetLine
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__WildcardToSearchFor))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
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
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Find
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__StringToFind))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params[0].f_String().f_Find(_Params[1].f_String()) >= 0)
									return true;
								return false;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_FindNoCase
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__StringToFind))
							, [] (CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params[0].f_String().f_FindNoCase(_Params[1].f_String()) >= 0)
									return true;
								return false;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_StartsWith
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__StringToFind))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_StartsWith(_Params[1].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_EndsWith
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__StringToFind))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_EndsWith(_Params[1].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_RemoveSuffix
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__StringToRemove))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_RemoveSuffix(_Params[1].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_RemovePrefix
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__StringToRemove))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_RemovePrefix(_Params[1].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Format
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Format), fg_FunctionParam(g_Any, gc_ConstString_p_Params, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr::CFormat Format(_Params[0].f_String());

								for (auto &Param : _Params[1].f_Array())
								{
									switch (Param.f_EType())
									{
									case EEJsonType_String:
										Format << Param.f_String();
										break;
									case EEJsonType_Integer:
										Format << Param.f_Integer();
										break;
									case EEJsonType_Float:
										Format << Param.f_Float();
										break;
									case EEJsonType_Boolean:
										Format << Param.f_Boolean();
										break;
									case EEJsonType_Date:
										Format << Param.f_Date();
										break;
									case EEJsonType_Binary:
										Format << fg_Base64Encode(Param.f_Binary());
										break;
									case EEJsonType_Invalid:
									case EEJsonType_Null:
									case EEJsonType_Object:
									case EEJsonType_Array:
									case EEJsonType_UserType:
										// Use Json built in formatter
										Format << Param;
										break;
									}
								}

								return Format.f_GetStr();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_ParseFormatString
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_ConstString__StringToParse)
								, fg_FunctionParam(g_String, gc_ConstString__ParseFormat)
								, fg_FunctionParam(g_String, gc_ConstString__OutputFormat)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
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
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Parse
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_AnyArray
								, fg_FunctionParam(g_String, gc_ConstString__StringToParse)
								, fg_FunctionParam(g_String, gc_ConstString__ParseFormat)
								, fg_FunctionParam(g_String, gc_ConstString_p_Types, g_Ellipsis)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto Parse = CStr::CParse(_Params[1].f_String());

								CEJsonSorted Output = EJsonType_Array;
								auto &TypeParams = _Params[2].f_Array();
								mint nTypeParams = TypeParams.f_GetLen();
								Output.f_SetLen(nTypeParams);
								mint iType = 0;

								for (auto &TypeParam : TypeParams)
								{
									auto &OutputVar = Output[iType++];
									auto &Type = TypeParam.f_String();
									if (Type == gc_ConstString_string.m_String)
										Parse >> OutputVar.f_String();
									else if (Type == gc_ConstString_int.m_String)
										Parse >> OutputVar.f_Integer();
									else if (Type == gc_ConstString_float.m_String)
										Parse >> OutputVar.f_Float();
									else
										fs_ThrowError(_Context, "Only string, int, and float are supported");
								}

								Parse.f_Parse(_Params[0].f_String());

								return Output;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Sanitize
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_ConstString__String)
								, fg_FunctionParam
								(
									fg_Optional(CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf{{gc_ConstString_rfc1034, gc_ConstString_bash}}})
									, gc_ConstString__Format
									, g_Optional
								)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params.f_GetLen() < 1 || _Params.f_GetLen() > 2 || !_Params[0].f_IsString() || (_Params.f_GetLen() > 1 && _Params[1].f_IsString()))
									fs_ThrowError(_Context, "Sanitize takes one or two string parameters: String [Format]");

								CStr Format = gc_ConstString_rfc1034;
								if (_Params[1].f_IsValid())
									Format = _Params[1].f_String();

								CStr Return = _Params[0].f_String();
								if (Format.f_CmpNoCase(gc_ConstString_rfc1034.m_String) == 0)
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
								else if (Format.f_CmpNoCase(gc_ConstString_bash.m_String) == 0)
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
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_StringBase64Encode
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_Base64Encode(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_StringBase64Decode
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_Base64Decode(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_StringHexEncode
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr EncodedString;
								uint8 CharValue = 0;
								CStr::CFormat Format("{nfh,sj2,sf0}");
								Format << CharValue;

								for (auto &Char : _Params[0].f_String())
								{
									CharValue = Char;
									EncodedString += Format;
								}

								return EncodedString;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_StringHexDecode
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto *pParse = _Params[0].f_String().f_GetStr();
								auto *pParseEnd = pParse + _Params[0].f_String().f_GetLen();

								CStr OutString;
								{
									CStr::CAppender OutStringAppender(OutString);

									while ((pParse + 2) <= pParseEnd)
									{
										auto Value = fg_StrToIntParseHexNoSign(pParse, 2, uint8(0));
										OutStringAppender += (ch8)Value;
									}
								}

								return OutString;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_StringCompress
						, CBuiltinFunction
						{
							fg_FunctionType(g_Binary, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return NCompression::fg_CompressZstandard(CByteVector::fs_FromString(_Params[0].f_String()));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_StringDecompress
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Binary, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return NCompression::fg_DecompressZstandard(_Params[0].f_Binary()).f_ToString();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Base64Encode
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Binary, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_Base64Encode(_Params[0].f_Binary());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Base64Decode
						, CBuiltinFunction
						{
							fg_FunctionType(g_Binary, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return fg_Base64Decode(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_HexEncode
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Binary, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr EncodedString;
								uint8 CharValue = 0;
								CStr::CFormat Format("{nfh,sj2,sf0}");
								Format << CharValue;

								for (auto &Char : _Params[0].f_Binary())
								{
									CharValue = Char;
									EncodedString += Format;
								}

								return EncodedString;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_HexDecode
						, CBuiltinFunction
						{
							fg_FunctionType(g_Binary, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto *pParse = _Params[0].f_String().f_GetStr();
								auto *pParseEnd = pParse + _Params[0].f_String().f_GetLen();

								CByteVector Out;
								while ((pParse + 2) <= pParseEnd)
								{
									auto Value = fg_StrToIntParseHexNoSign(pParse, 2, uint8(0));
									Out.f_Insert(Value);
								}

								return Out;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Compress
						, CBuiltinFunction
						{
							fg_FunctionType(g_Binary, fg_FunctionParam(g_Binary, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return NCompression::fg_CompressZstandard(_Params[0].f_Binary());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Decompress
						, CBuiltinFunction
						{
							fg_FunctionType(g_Binary, fg_FunctionParam(g_Binary, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return NCompression::fg_DecompressZstandard(_Params[0].f_Binary());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_TrimLeft
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_TrimLeft();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_TrimRight
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_TrimRight();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_UpperCase
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_UpperCase();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_LowerCase
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_LowerCase();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Left
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_Integer, gc_ConstString__Length))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_Left(_Params[1].f_Integer());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Right
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_Integer, gc_ConstString__Length))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_Right(_Params[1].f_Integer());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Extract
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_ConstString__Source)
								, fg_FunctionParam(g_Integer, gc_ConstString__Start)
								, fg_FunctionParam(fg_Optional(g_Integer), gc_ConstString__Length, g_Optional)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params[2].f_IsValid())
									return _Params[0].f_String().f_Extract(_Params[1].f_Integer(), _Params[2].f_Integer());
								else
									return _Params[0].f_String().f_Extract(_Params[1].f_Integer());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Indent
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_String, gc_ConstString__String))
							, [](CBuildSystem const& _This, CBuildSystem::CEvalPropertyValueContext& _Context, TCVector<CEJsonSorted>&& _Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_Indent(_Params[1].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_ChunkString
						, CBuiltinFunction
						{
							fg_FunctionType(g_StringArray, fg_FunctionParam(g_String, gc_ConstString__Source), fg_FunctionParam(g_Integer, gc_ConstString__Length))
							, [](CBuildSystem const& _This, CBuildSystem::CEvalPropertyValueContext& _Context, TCVector<CEJsonSorted>&& _Params) -> CEJsonSorted
							{
								CUStr SourceString = _Params[0].f_String();

								mint ChunkLength = _Params[1].f_Integer();
								mint SourceLength = SourceString.f_GetLen();

								CEJsonSorted Output = EJsonType_Array;

								if (SourceLength < ChunkLength)
									Output.f_Insert(CStr(SourceString));
								else
								{
									for (mint i = 0; i < SourceLength; i += ChunkLength)
										Output.f_Insert(CStr(SourceString.f_Extract(i, ChunkLength)));
								}

								return Output;
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
