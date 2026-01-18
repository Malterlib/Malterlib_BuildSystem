// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Registry.h"

#include <Mib/Encoding/EJsonParse>

namespace NMib::NContainer
{
#ifndef DDocumentation_Doxygen
	using namespace NEncoding;
	using namespace NEncoding::NJson;
	using namespace NBuildSystem;
	using namespace NStr;

	namespace
	{
		constexpr ch8 const g_PrefixOperatorCharacters[] = "+<>=!";
		constexpr ch8 const g_PrefixOperatorCharactersStart[] = "<>=";
	}

	NTime::CTime TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::f_ParseDate(uch8 const * &o_pParse, bool _bWithinParenthesis)
	{
		using namespace NStr;

		if (_bWithinParenthesis)
			o_pParse += 5;
		uch8 const *pParse = o_pParse;
		auto pParseStart = pParse;

		auto fMaybeNextChar = [&](uch8 const *_pParse) -> uch8 const *
			{
				if (!*_pParse || *_pParse == ')')
					return _pParse;
				return _pParse + 1;
			}
		;

		auto fReportError = [&](NStr::CStr const &_Error, uch8 const *_pEnd)
			{
				f_ThrowError
					(
						"Failed to parse \"{}\" as a date: {}. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						<< CStr(pParseStart, _pEnd - pParseStart)
						<< _Error
						, pParseStart
					)
				;
			}
		;

		auto fParseInt = [&](uch8 const *&_pParse, auto _Type, ch8 const *_pTerminators)
			{
				auto pStart = _pParse;
				bool bFailed = false;
				auto Return = fg_StrToIntParse(_pParse, _Type, _pTerminators, false, EStrToIntParseMode_Base10, &bFailed);
				if (bFailed)
					fReportError(fg_Format("Failed to parse \"{}\" as a integer", CStr(pStart, fMaybeNextChar(_pParse) - pStart)), fMaybeNextChar(_pParse));
				if (*_pParse == _pTerminators[0])
					++_pParse;
				return Return;
			}
		;

		NStr::fg_ParseWhiteSpaceNoLines(pParse);
		if (!*pParse || *pParse == ')')
			fReportError("Missing year", pParse);
		int64 Year = fParseInt(pParse, int64(), "-)");
		if (!*pParse || *pParse == ')')
			fReportError("Missing month", pParse);
		uint32 Month = fParseInt(pParse, uint32(), "-)");
		if (!*pParse || *pParse == ')')
			fReportError("Missing day", pParse);
		uint32 Day = fParseInt(pParse, uint32(), " )");
		uint32 Hour = 0;
		uint32 Minute = 0;
		uint32 Second = 0;
		fp64 Fraction = 0;
		if (*pParse && *pParse != ')')
			Hour = fParseInt(pParse, uint32(), ":)");
		if (*pParse && *pParse != ')')
			Minute = fParseInt(pParse, uint32(), ":)");
		if (*pParse && *pParse != ')')
			Second = fParseInt(pParse, uint32(), ".)");
		if (*pParse && *pParse != ')')
		{
			--pParse;
			auto pStart = pParse;
			Fraction = fg_StrToFloatParse(pParse, fp64::fs_Inf(), ")", false, (ch8 const *)nullptr);
			if (Fraction == fp64::fs_Inf())
				fReportError(fg_Format("Failed to parse \"{}\" as a float", CStr(pStart, fMaybeNextChar(pParse) - pStart)), fMaybeNextChar(pParse));
		}

		NStr::fg_ParseWhiteSpaceNoLines(pParse);

		auto pParseEnd = pParse;
		if (_bWithinParenthesis)
		{
			if (*pParse != ')')
				fReportError(NStr::fg_Format("Unexpected charater: {}", NStr::CStr(pParse, 1)), pParse + 1);
			else
				++pParse;
		}
		else if (*pParse)
			fReportError(NStr::fg_Format("Unexpected charater: {}", NStr::CStr(pParse, 1)), pParse + 1);

		if (fg_Clamp(Month, 1u, 12u) != Month)
			fReportError("Invalid month", pParseEnd);
		if (fg_Clamp(Day, 1u, mint(NTime::CTimeConvert::fs_GetDaysInMonth(Year, Month - 1))) != Day)
			fReportError("Invalid day", pParseEnd);
		if (fg_Clamp(Hour, 0u, 23u) != Hour)
			fReportError("Invalid hour", pParseEnd);
		if (fg_Clamp(Minute, 0u, 59u) != Minute)
			fReportError("Invalid minute", pParseEnd);
		if (fg_Clamp(Second, 0u, 59u) != Second)
			fReportError("Invalid second", pParseEnd);
		if (fg_Clamp(Fraction, 0.0, 1.0) != Fraction)
			fReportError("Invalid fraction", pParseEnd);

		o_pParse = pParse;
		return NTime::CTimeConvert::fs_CreateTime(Year, Month, Day, Hour, Minute, Second, Fraction);
	}

	NContainer::CByteVector TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::f_ParseBinary(uch8 const * &o_pParse, bool _bWithinParenthesis)
	{
		if (_bWithinParenthesis)
			o_pParse += 7;

		uch8 const *pParse = o_pParse;
		NStr::fg_ParseWhiteSpaceNoLines(pParse);
		auto pStart = pParse;
		while (*pParse == '=' || g_Base64EncodingTableReverse[(uint8)*pParse] != -1)
			++pParse;
		auto pEnd = pParse;
		NStr::fg_ParseWhiteSpaceNoLines(pParse);

		if (_bWithinParenthesis)
		{
			if (*pParse != ')')
				f_ThrowError("Unexpected character in Base64 string: {}"_f << NStr::CStr(pParse, 1), o_pParse);
			else
				++pParse;
		}
		else if (*pParse)
			f_ThrowError("Unexpected character in Base64 string: {}"_f << NStr::CStr(pParse, 1), o_pParse);

		NContainer::CByteVector Data;
		fg_Base64Decode(NStr::CStr(pStart, pEnd - pStart), Data);

		o_pParse = pParse;
		return Data;
	}

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::f_ParseAfterValue(CJsonSorted &o_Value, uch8 const *&o_pParse)
	{
	}

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::f_PreParse(CJsonSorted &o_Value, uch8 const *&o_pParse)
	{
	}

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::f_PostParse(CJsonSorted &o_Value, uch8 const *&o_pParse)
	{
	}

	bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::f_ParseValue(CJsonSorted &o_Value, uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;
		if (NStr::fg_StrStartsWith(pParse, "date("))
		{
			auto Date = f_ParseDate(pParse, true);
			if (!Date.f_IsValid())
				o_Value[CEJsonConstStrings::mc_Date] = nullptr;
			else
				o_Value[CEJsonConstStrings::mc_Date] = NTime::CTimeConvert(Date).f_UnixMilliseconds();
			o_pParse = pParse;
			return true;
		}
		else if (NStr::fg_StrStartsWith(pParse, "binary("))
		{
			auto Binary = f_ParseBinary(pParse, true);
			o_Value[CEJsonConstStrings::mc_Binary] = fg_Base64Encode(Binary);
			o_pParse = pParse;
			return true;
		}
		return false;
	}

	bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::fs_IsBinaryOperator(uch8 const *_pParse)
	{
		auto pParse = _pParse;

		if (NStr::fg_StrFindChar(mc_BinaryOperatorCharacters, *pParse) < 0)
			return false;

		++pParse;

		while (NStr::fg_StrFindChar(mc_BinaryOperatorCharacters, *pParse) >= 0)
			++pParse;

		return fg_CharIsWhiteSpace(*pParse) || *pParse == '{';
	}

	bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::fs_IsPrefixOperator(uch8 const *_pParse)
	{
		auto pParse = _pParse;

		if (NStr::fg_StrFindChar(mc_PrefixOperatorCharacters, *pParse) < 0)
			return false;

		return true;
	}

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_PreParse(CJsonSorted &o_Value, uch8 const *&o_pParse)
	{
		++m_ParseDepth;
	}

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_PostParse(CJsonSorted &o_Value, uch8 const *&o_pParse)
	{
		--m_ParseDepth;
	}

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseAfterValue(CJsonSorted &o_Value, uch8 const *&o_pParse)
	{
		if (!m_bParseAfterValue)
			return;

		if (m_bParsingDefine)
			f_ParsePostDefine(o_pParse, o_Value);
		else
		{
			auto *pParse = o_pParse;
			fg_ParseWhiteSpaceNoLines(pParse);

			if (fg_StrStartsWith(pParse, "//") || fg_StrStartsWith(pParse, "/*"))
				return;

			while
				(
					fg_StrStartsWith(pParse, gc_ConstString_Symbol_AccessObject.m_String)
					|| fg_StrStartsWith(pParse, gc_ConstString_Symbol_Ellipsis.m_String)
					|| (m_bSupportBinaryOperators && fs_IsBinaryOperator(pParse)) || fg_StrStartsWith(pParse, gc_ConstString_Symbol_Optional.m_String)
					|| (*pParse == '<' && !fg_CharIsWhiteSpace(pParse[1]))
				)
			{
				CJsonSorted FirstParam = fg_Move(o_Value);

				auto &Object = o_Value.f_Object();
				Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
				Object[CEJsonConstStrings::mc_Value] = f_ParseExpression(pParse, EParseExpressionFlag_SupportAppend | EParseExpressionFlag_NoParentheses, &FirstParam);
				o_pParse = pParse;
				fg_ParseWhiteSpaceNoLines(pParse);
				if (fg_StrStartsWith(pParse, "//") || fg_StrStartsWith(pParse, "/*"))
					break;
			}
		}
	}

	bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseValue(CJsonSorted &o_Value, uch8 const *&o_pParse)
	{
		if (CEJsonParseContext::f_ParseValue(o_Value, o_pParse))
			return true;

		auto pParse = o_pParse;

		EParseExpressionFlag ExpressionFlags = EParseExpressionFlag_None;

		if (m_ParseDepth == m_ParsingFunctionParamsDepth)
			ExpressionFlags |= EParseExpressionFlag_ParsingFunctionParams;

		auto fParseExpression = [&]
			{
				if
					(
						*pParse
						&& !fg_CharIsWhiteSpace(*pParse)
						&& *pParse != ','
						&& *pParse != ')'
						&& *pParse != ']'
						&& *pParse != '}'
						&& !fg_StrStartsWith(pParse, "//")
						&& !fg_StrStartsWith(pParse, "/*")
					)
				{
					CJsonSorted *pFirstParam = nullptr;
					CJsonSorted FirstParam;

					if (o_Value.f_IsValid())
					{
						FirstParam = fg_Move(o_Value);
						pFirstParam = &FirstParam;
					}

					auto &Object = o_Value.f_Object();
					Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
					Object[CEJsonConstStrings::mc_Value] = f_ParseExpression(pParse, EParseExpressionFlag_SupportAppend | EParseExpressionFlag_NoParentheses | ExpressionFlags, pFirstParam);
					o_pParse = pParse;
					return true;
				}
				return false;
			}
		;

		if (m_bParsingNamespace)
		{
			if (!fs_CharIsStartIdentifier(*pParse))
				f_ThrowError("Expected start of namespace name", pParse);

			TCVector<CStr> AllNamespaces;
			while (*pParse)
			{
				AllNamespaces.f_Insert(f_ParseIdentifier(pParse));
				if (*pParse == ':' && pParse[1] == ':')
				{
					pParse += 2;
					if (!fs_CharIsStartIdentifier(*pParse))
						f_ThrowError("Expected start of next namespace name", pParse);
				}
				else
					break;
			}

			m_bParsingNamespace = false;

			CStr Name = CStr::fs_Join(AllNamespaces, "::");
			Name.f_SetUserData(EJsonStringType_NoQuote);
			o_Value = Name;
			o_pParse = pParse;

			return true;
		}
		else if (*pParse == '~' && (pParse[1] == '\"' || pParse[1] == '\'' || pParse[1] == '`'))
		{
			++pParse;

			CJsonSorted Value = f_ParseWildcardStringToken(pParse);

			auto &Object = o_Value.f_Object();
			Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
			Object[CEJsonConstStrings::mc_Value] = fg_Move(Value);

			o_pParse = pParse;

			fParseExpression();

			return true;
		}
		else if (*pParse == '#')
		{
			++pParse;
			if (*pParse != '<')
				f_ThrowError("Expected < to start JSON accessor", pParse);

			auto Accessor = f_ParseJsonAccessor(pParse, nullptr);
			fg_ParseWhiteSpaceNoLines(pParse);

			if (fg_CharIsNewLine(*pParse))
				f_ThrowError("Expected constant or expression", pParse);

			CJsonSorted RightValue;
			auto Cleanup = f_EnableBinaryOperators();
			fg_ParseJsonValue(RightValue, pParse, *this);

			auto &Object = o_Value.f_Object();
			Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;

			auto &ValueObject = Object[CEJsonConstStrings::mc_Value].f_Object();
			ValueObject[gc_ConstString_Type] = gc_ConstString_RootValue;
			ValueObject[gc_ConstString_Value] = fg_Move(RightValue);
			ValueObject[gc_ConstString_Accessors] = fg_Move(Accessor[CEJsonConstStrings::mc_Value][gc_ConstString_Accessors]);

			o_pParse = pParse;
			return true;
		}
		else if (*pParse == '&')
		{
			++pParse;

			CJsonSorted Identifier;

			if (fs_CharIsStartIdentifier(*pParse) || *pParse == '@')
				Identifier = f_ParseIdentifierToken(pParse);
			else
				f_ThrowError("Expected started of identifier", pParse);

			auto &Object = o_Value.f_Object();
			Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;

			auto &ValueObject = Object[CEJsonConstStrings::mc_Value].f_Object();
			ValueObject[gc_ConstString_Type] = gc_ConstString_IdentifierReference;
			ValueObject[gc_ConstString_Identifier] = fg_Move(Identifier);

			o_pParse = pParse;
			return true;
		}
		else if (NStr::fg_StrFindChar(g_PrefixOperatorCharactersStart, *pParse) >= 0 || (*pParse == '+' && pParse[1] == '=') || (*pParse == '!' && pParse[1] == '='))
		{
			auto pParseStart = pParse;
			++pParse;

			while (NStr::fg_StrFindChar(g_PrefixOperatorCharacters, *pParse) >= 0)
				++pParse;

			CStr Operator(pParseStart, pParse - pParseStart);
			if (!fg_CharIsWhiteSpaceNoLines(*pParse))
				f_ThrowError("Operator needs to be followed by whitespace", pParse);

			++pParse;

			if
				(
					Operator != gc_ConstString_Symbol_OperatorLessThan.m_String
					&& Operator != gc_ConstString_Symbol_OperatorGreaterThan.m_String
					&& Operator != gc_ConstString_Symbol_OperatorGreaterThanEqual.m_String
					&& Operator != gc_ConstString_Symbol_OperatorLessThanEqual.m_String
					&& Operator != gc_ConstString_Symbol_OperatorEqual.m_String
					&& Operator != gc_ConstString_Symbol_OperatorNotEqual.m_String
					&& Operator != gc_ConstString_Symbol_OperatorPrepend.m_String
					&& Operator != gc_ConstString_Symbol_OperatorAppend.m_String
				)
			{
				f_ThrowError("Invalid operator: {}"_f << Operator, pParseStart);
			}

			CJsonSorted RightValue;
			auto Cleanup = f_EnableBinaryOperators();
			fg_ParseJsonValue(RightValue, pParse, *this);

			auto &Object = o_Value.f_Object();
			Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;

			auto &ValueObject = Object[CEJsonConstStrings::mc_Value].f_Object();
			ValueObject[gc_ConstString_Type] = gc_ConstString_Operator;
			ValueObject[gc_ConstString_Operator] = fg_Move(Operator);
			ValueObject[gc_ConstString_Right] = fg_Move(RightValue);

			o_pParse = pParse;
			return true;
		}
		else if (*pParse == '`')
		{
			CJsonSorted Value = f_ParseEvalStringToken(pParse);

			auto &Object = o_Value.f_Object();
			Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
			Object[CEJsonConstStrings::mc_Value] = fg_Move(Value);

			o_pParse = pParse;

			fParseExpression();

			return true;
		}
		else if (*pParse == '@' || *pParse == '(' || fs_IsPrefixOperator(pParse))
			return fParseExpression();
		else if (*pParse == ':')
		{
			if (m_bParsingDefine)
				f_ThrowError("Recursive define statements not supported", pParse);

			++pParse;
			auto pTestParse = pParse;

			auto &Object = o_Value.f_Object();
			Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
			Object[CEJsonConstStrings::mc_Value] = f_ParseDefine(pTestParse, false);

			o_pParse = pTestParse;

			return true;
		}
		else if (fs_CharIsStartIdentifier(*pParse))
		{
			auto pTestParse = pParse;

			CStr Identifier = f_ParseIdentifier(pTestParse);

			if (Identifier == gc_ConstString_define.m_String)
			{
				if (m_bParsingDefine)
					f_ThrowError("Recursive define statements not supported", pParse);

				auto &Object = o_Value.f_Object();
				Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
				Object[CEJsonConstStrings::mc_Value] = f_ParseDefine(pTestParse, true);

				o_pParse = pTestParse;
			}
			else if (Identifier == gc_ConstString_function.m_String)
			{
				if (m_bParsingDefine)
					f_ThrowError("Recursive function statements not supported", pParse);

				auto &Object = o_Value.f_Object();
				Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
				Object[CEJsonConstStrings::mc_Value] = f_ParseFunctionType(pTestParse);

				o_pParse = pTestParse;
			}
			else if (Identifier == gc_ConstString_type.m_String)
			{
				if (!m_bParsingDefine)
					f_ThrowError("Type can only be used inside define statements"_f << Identifier, pParse);

				auto &Object = o_Value.f_Object();
				Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
				Object[CEJsonConstStrings::mc_Value] = f_ParseType(pTestParse);

				o_pParse = pTestParse;
			}
			else if (Identifier == gc_ConstString_one_of.m_String)
			{
				if (!m_bParsingDefine)
					f_ThrowError("one_of can only be used inside define statements"_f << Identifier, pParse);

				auto &Object = o_Value.f_Object();
				Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
				Object[CEJsonConstStrings::mc_Value] = f_ParseOneOf(pTestParse);

				o_pParse = pTestParse;
			}
			else if
				(
					Identifier == gc_ConstString_string.m_String
					|| Identifier == gc_ConstString_identifier.m_String
					|| Identifier == gc_ConstString_int.m_String
					|| Identifier == gc_ConstString_float.m_String
					|| Identifier == gc_ConstString_bool.m_String
					|| Identifier == gc_ConstString_date.m_String
					|| Identifier == gc_ConstString_binary.m_String
					|| Identifier == gc_ConstString_any.m_String
					|| Identifier == gc_ConstString_void.m_String
				)
			{
				if (!m_bParsingDefine)
					f_ThrowError("Type '{}' can only be used inside define statements"_f << Identifier, pParse);

				auto &Object = o_Value.f_Object();
				Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
				Object[CEJsonConstStrings::mc_Value] = f_ParseDefaultType(Identifier);

				o_pParse = pTestParse;
			}
			else
				return fParseExpression();

			return true;
		}

		return false;
	}

	template <>
	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_Generate<CStr>
		(
			CStr &o_Output
			, NBuildSystem::CBuildSystemSyntax::CRootValue const &_Value
			, bool _bForceEscape
			, mint _Level
			, CStr const &_PreData
		)
	{
		if (!_Value.m_Value.f_IsValid())
		{
			if (_bForceEscape)
				o_Output += gc_ConstString_undefined.m_String;
			return;
		}

		CStr::CAppender Appender(o_Output);
		fg_GenerateJsonValue<CJsonParseContext>(Appender, _Value.f_ToJson().f_ToJson(), _Level, "\t", gc_BuildSystemJsonParseFlags);
	}

	template <>
	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_GenerateKey<NBuildSystem::CBuildSystemSyntax::CRootKey, CStr>
		(
			CStr &o_Output
			, NBuildSystem::CBuildSystemSyntax::CRootKey const &_Value
			, bool _bForceEscape
			, mint _Level
			, CStr const &_PreData
		)
	{
		if (!_Value.f_IsValid())
		{
			if (_bForceEscape)
				o_Output += gc_ConstString_undefined.m_String;
			return;
		}

		CStr::CAppender Appender(o_Output);
		fg_GenerateJsonValue<CJsonParseContext>(Appender, _Value.f_ToJson().f_ToJson(), _Level, "\t", gc_BuildSystemJsonParseFlags);
	}

	template <typename tf_CParseContext, typename tf_CStr>
	bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::fs_GenerateValue
		(
			tf_CStr &o_String
			, CJsonSorted const &_Value
			, mint _Depth
			, ch8 const *_pPrettySeparator
			, EJsonDialectFlag _Flags
		)
	{
		// TODO: Handle without converting to EJson
		if (_Value.f_IsObject())
		{
			auto iMember = _Value.f_Object().f_OrderedIterator();
			if (iMember)
			{
				auto &Member = *iMember;
				auto &Name = Member.f_Name();
				auto &Value = Member.f_Value();
				++iMember;
				if (!iMember)
				{
					if (Name == CEJsonConstStrings::mc_Date)
					{
						CTime Time;
						if (Value.f_IsNull())
							Time = NTime::CTime();
						else if (Value.f_Type() != EJsonType_Integer)
							DMibError("Invalid EJSON: $date value must be an integer");

						Time = NTime::CTimeConvert::fs_FromUnixMilliseconds(Value.f_Integer());

						auto DateTime = NTime::CTimeConvert(Time).f_ExtractDateTime();

						aint nComponents = 7;

						if (DateTime.m_Fraction == 0.0)
						{
							--nComponents;
							if (DateTime.m_Second == 0)
							{
								--nComponents;
								if (DateTime.m_Hour == 0 && DateTime.m_Minute == 0)
									nComponents -= 2;
							}
						}

						{
							auto Committed = o_String.f_Commit();
							Committed.m_String += typename tf_CStr::CString::CFormat("date({tc*})") << Time << nComponents;
						}
						return true;
					}
					else if (Name == CEJsonConstStrings::mc_Binary)
					{
						if (Value.f_Type() != EJsonType_String)
							DMibError("Invalid EJSON: $binary value must be a string");

						{
							auto Committed = o_String.f_Commit();
							Committed.m_String += typename tf_CStr::CString::CFormat("binary({})") << Value.f_String();
						}
						return true;
					}
				}
			}
		}
		else if (_Value.f_IsString() && _Value.f_String().f_GetUserData() == EJsonStringType_NoQuote)
		{
			o_String += _Value.f_String();
			return true;
		}

		return false;
	}

	template <typename tf_CParseContext, typename tf_CStr>
	bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateValue
		(
			tf_CStr &o_String
			, CJsonSorted const &_Value
			, mint _Depth
			, ch8 const *_pPrettySeparator
			, EJsonDialectFlag _Flags
		)
	{
		if (CEJsonParseContext::fs_GenerateValue<CEJsonParseContext>(o_String, _Value, _Depth, _pPrettySeparator, _Flags))
			return true;

		if (_Value.f_IsObject())
		{
			auto iMember = _Value.f_Object().f_OrderedIterator();
			if (iMember)
			{
				auto &Member = *iMember;
				++iMember;
				auto &Name = Member.f_Name();

				if (iMember)
				{
					if
						(
							(Name == CEJsonConstStrings::mc_Type && iMember->f_Name() == CEJsonConstStrings::mc_Value)
							|| (Name == CEJsonConstStrings::mc_Value && iMember->f_Name() == CEJsonConstStrings::mc_Type)
						)
					{
						++iMember;
						if (!iMember)
						{
							auto *pType = _Value.f_GetMember(CEJsonConstStrings::mc_Type);
							auto *pValue = _Value.f_GetMember(CEJsonConstStrings::mc_Value);
							DMibCheck(pType);
							DMibCheck(pValue);
							if (pType->f_Type() != EJsonType_String)
								DMibError("Invalid EJSON: $type value must be a string");

							if (pType->f_String() == gc_ConstString_BuildSystemToken.m_String)
							{
								CJsonParseContext::fs_GenerateExpression(o_String, *pValue, true, _Depth);
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	template bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateValue
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext
			, NStr::CStr::CAppender
		>
		(
			CStr::CAppender &o_String
			, CJsonSorted const &_Value
			, mint _Depth
			, ch8 const *_pPrettySeparator
			, EJsonDialectFlag _Flags
		)
	;
	template bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateValue
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext
			, NStr::CStrNonTracked::CAppender
		>
		(
			CStrNonTracked::CAppender &o_String
			, CJsonSorted const &_Value
			, mint _Depth
			, ch8 const *_pPrettySeparator
			, EJsonDialectFlag _Flags
		)
	;

	template bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::fs_GenerateValue
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey
			, CBuildSystemSyntax::CRootValue>::CEJsonParseContext
			, CStr::CAppender
		>
		(
			CStr::CAppender &o_String
			, CJsonSorted const &_Value
			, mint _Depth
			, ch8 const *_pPrettySeparator
			, EJsonDialectFlag _Flags
		)
	;
	template bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext::fs_GenerateValue
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJsonParseContext
			, NStr::CStrNonTracked::CAppender
		>
		(
			CStrNonTracked::CAppender &o_String
			, CJsonSorted const &_Value
			, mint _Depth
			, ch8 const *_pPrettySeparator
			, EJsonDialectFlag _Flags
		)
	;
#endif
}
