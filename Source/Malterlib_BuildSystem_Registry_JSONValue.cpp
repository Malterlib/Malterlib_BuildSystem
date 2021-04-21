// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Registry.h"

#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NContainer
{
	using namespace NEncoding;
	using namespace NEncoding::NJSON;
	using namespace NBuildSystem;
	using namespace NStr;

	namespace
	{
		constexpr ch8 const g_PrefixOperatorCharacters[] = "+<>=!";
		constexpr ch8 const g_PrefixOperatorCharactersStart[] = "<>=";
	}

	NTime::CTime TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::f_ParseDate(uch8 const * &o_pParse, bool _bWithinParenthesis)
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

	NContainer::CByteVector TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::f_ParseBinary(uch8 const * &o_pParse, bool _bWithinParenthesis)
	{
		if (_bWithinParenthesis)
			o_pParse += 7;

		uch8 const *pParse = o_pParse;
		NStr::fg_ParseWhiteSpaceNoLines(pParse);
		auto pStart = pParse;
		while (*pParse == '=' || NEncoding::g_Base64EncodingTableReverse[(uch8)*pParse] != -1)
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
		NEncoding::fg_Base64Decode(NStr::CStr(pStart, pEnd - pStart), Data);

		o_pParse = pParse;
		return Data;
	}

	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::f_ParseAfterValue(NEncoding::CJSON &o_Value, uch8 const *&o_pParse)
	{
	}

	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::f_ParseValue(CJSON &o_Value, uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;
		if (NStr::fg_StrStartsWith(pParse, "date("))
		{
			auto Date = f_ParseDate(pParse, true);
			if (!Date.f_IsValid())
				o_Value["$date"] = nullptr;
			else
				o_Value["$date"] = NTime::CTimeConvert(Date).f_UnixMilliseconds();
			o_pParse = pParse;
			return true;
		}
		else if (NStr::fg_StrStartsWith(pParse, "binary("))
		{
			auto Binary = f_ParseBinary(pParse, true);
			o_Value["$binary"] = NEncoding::fg_Base64Encode(Binary);
			o_pParse = pParse;
			return true;
		}
		return false;
	}

	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::fs_IsBinaryOperator(uch8 const *_pParse)
	{
		auto pParse = _pParse;

		if (NStr::fg_StrFindChar(mc_BinaryOperatorCharacters, *pParse) < 0)
			return false;

		++pParse;

		while (NStr::fg_StrFindChar(mc_BinaryOperatorCharacters, *pParse) >= 0)
			++pParse;

		return fg_CharIsWhiteSpace(*pParse) || *pParse == '{';
	}

	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::fs_IsPrefixOperator(uch8 const *_pParse)
	{
		auto pParse = _pParse;

		if (NStr::fg_StrFindChar(mc_PrefixOperatorCharacters, *pParse) < 0)
			return false;

		return true;
	}

	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseAfterValue(NEncoding::CJSON &o_Value, uch8 const *&o_pParse)
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

			if (fg_StrStartsWith(pParse, "->") || fg_StrStartsWith(pParse, "...") || (m_bSupportBinaryOperators && fs_IsBinaryOperator(pParse)) || fg_StrStartsWith(pParse, "?"))
			{
				CJSON FirstParam = fg_Move(o_Value);

				o_Value["$type"] = "BuildSystemToken";
				o_Value["$value"] = f_ParseExpression(pParse, EParseExpressionFlag_SupportAppend | EParseExpressionFlag_NoParentheses, &FirstParam);
				o_pParse = pParse;
			}
		}
	}

	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseValue(CJSON &o_Value, uch8 const *&o_pParse)
	{
		if (CEJSONParseContext::f_ParseValue(o_Value, o_pParse))
			return true;

		++m_ParseDepth;
		auto Cleanup = g_OnScopeExit > [&]
			{
				--m_ParseDepth;
			}
		;

		auto pParse = o_pParse;

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
					CJSON *pFirstParam = nullptr;
					CJSON FirstParam;

					if (o_Value.f_IsValid())
					{
						FirstParam = fg_Move(o_Value);
						pFirstParam = &FirstParam;
					}

					o_Value["$type"] = "BuildSystemToken";
					o_Value["$value"] = f_ParseExpression(pParse, EParseExpressionFlag_SupportAppend | EParseExpressionFlag_NoParentheses, pFirstParam);
					o_pParse = pParse;
					return true;
				}
				return false;
			}
		;

		if (*pParse == '~' && (pParse[1] == '\"' || pParse[1] == '\'' || pParse[1] == '`'))
		{
			++pParse;

			CJSON Value = f_ParseWildcardStringToken(pParse);

			o_Value["$type"] = "BuildSystemToken";
			o_Value["$value"] = fg_Move(Value);

			o_pParse = pParse;

			fParseExpression();

			return true;
		}
		else if (*pParse == '#')
		{
			++pParse;
			if (*pParse != '<')
				f_ThrowError("Expected < to start JSON accessor", pParse);

			auto Accessor = f_ParseJSONAccessor(pParse, nullptr);
			fg_ParseWhiteSpaceNoLines(pParse);

			if (fg_CharIsNewLine(*pParse))
				f_ThrowError("Expected constant or expression", pParse);

			CJSON RightValue;
			auto Cleanup = f_EnableBinaryOperators();
			fg_ParseJSONValue(RightValue, pParse, *this);

			o_Value["$type"] = "BuildSystemToken";
			o_Value["$value"] =
				{
					"Type"__= "RootValue"
					, "Value"__= fg_Move(RightValue)
					, "Accessors"__= fg_Move(Accessor["$value"]["Accessors"])
				}
			;

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

			if (Operator != "<" && Operator != ">" && Operator != ">=" && Operator != "<=" && Operator != "==" && Operator != "!=" && Operator != "+=" && Operator != "=+")
				f_ThrowError("Invalid operator: {}"_f << Operator, pParseStart);

			CJSON RightValue;
			auto Cleanup = f_EnableBinaryOperators();
			fg_ParseJSONValue(RightValue, pParse, *this);

			o_Value["$type"] = "BuildSystemToken";
			o_Value["$value"] =
				{
					"Type"__= "Operator"
					, "Operator"__= Operator
					, "Right"__= fg_Move(RightValue)
				}
			;

			o_pParse = pParse;
			return true;
		}
		else if (*pParse == '`')
		{
			CJSON Value = f_ParseEvalStringToken(pParse);

			o_Value["$type"] = "BuildSystemToken";
			o_Value["$value"] = fg_Move(Value);

			o_pParse = pParse;

			fParseExpression();

			return true;
		}
		else if (*pParse == '@' || *pParse == '(' || fs_IsPrefixOperator(pParse))
			return fParseExpression();
		else if (fs_CharIsStartIdentifier(*pParse))
		{
			auto pTestParse = pParse;

			CStr Identifier = f_ParseIdentifier(pTestParse);

			if (Identifier == "define")
			{
				if (m_bParsingDefine)
					f_ThrowError("Recursive define statements not supported", pParse);

				o_Value["$type"] = "BuildSystemToken";
				o_Value["$value"] = f_ParseDefine(pTestParse);

				o_pParse = pTestParse;
			}
			else if (Identifier == "function")
			{
				if (m_bParsingDefine)
					f_ThrowError("Recursive function statements not supported", pParse);

				o_Value["$type"] = "BuildSystemToken";
				o_Value["$value"] = f_ParseFunctionType(pTestParse);

				o_pParse = pTestParse;
			}
			else if (Identifier == "type")
			{
				if (!m_bParsingDefine)
					f_ThrowError("Type can only be used inside define statements"_f << Identifier, pParse);

				o_Value["$type"] = "BuildSystemToken";
				o_Value["$value"] = f_ParseType(pTestParse);

				o_pParse = pTestParse;
			}
			else if (Identifier == "one_of")
			{
				if (!m_bParsingDefine)
					f_ThrowError("one_of can only be used inside define statements"_f << Identifier, pParse);

				o_Value["$type"] = "BuildSystemToken";
				o_Value["$value"] = f_ParseOneOf(pTestParse);

				o_pParse = pTestParse;
			}
			else if
				(
					Identifier == "string"
					|| Identifier == "int"
					|| Identifier == "float"
					|| Identifier == "bool"
					|| Identifier == "date"
					|| Identifier == "binary"
					|| Identifier == "any"
					|| Identifier == "void"
				)
			{
				if (!m_bParsingDefine)
					f_ThrowError("Type '{}' can only be used inside define statements"_f << Identifier, pParse);

				o_Value["$type"] = "BuildSystemToken";
				o_Value["$value"] = f_ParseDefaultType(Identifier);

				o_pParse = pTestParse;
			}
			else
				return fParseExpression();

			return true;
		}

		return false;
	}

	template <>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::fs_Generate<CStr>
		(
		 	CStr &o_Output
		 	, CBuildSystemRegistryValue const &_Value
		 	, bool _bForceEscape
		 	, mint _Level
		 	, CStr const &_PreData
		)
	{
		if (!_Value.f_IsValid())
		{
			if (_bForceEscape)
				o_Output += "undefined";
			return;
		}

		fg_GenerateJSONValue<CJSONParseContext>(o_Output, _Value.f_ToJSON(), _Level, "\t", gc_BuildSystemJSONParseFlags);
	}

	template <>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::fs_GenerateKey<CBuildSystemRegistryValue, CStr>
		(
			CStr &o_Output
			, CBuildSystemRegistryValue const &_Value
			, bool _bForceEscape
			, mint _Level
			, CStr const &_PreData
		)
	{
		if (!_Value.f_IsValid())
		{
			if (_bForceEscape)
				o_Output += "undefined";
			return;
		}

		fg_GenerateJSONValue<CJSONParseContext>(o_Output, _Value.f_ToJSON(), _Level, "\t", gc_BuildSystemJSONParseFlags);
	}

	template <typename tf_CParseContext, typename tf_CStr>
	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::fs_GenerateValue(tf_CStr &o_String, NEncoding::CJSON const &_Value, mint _Depth, ch8 const *_pPrettySeparator)
	{
		// TODO: Handle without converting to EJSON
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
					if (Name == "$date")
					{
						CTime Time;
						if (Value.f_IsNull())
							Time = NTime::CTime();
						else if (Value.f_Type() != EJSONType_Integer)
							DMibError("Invalid EJSON: $date value must be an integer");

						Time = NTime::CTimeConvert::fs_FromCreateFromUnixMilliseconds(Value.f_Integer());

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

						o_String += typename tf_CStr::CFormat("date({tc*})") << Time << nComponents;
						return true;
					}
					else if (Name == "$binary")
					{
						if (Value.f_Type() != EJSONType_String)
							DMibError("Invalid EJSON: $binary value must be a string");

						o_String += typename tf_CStr::CFormat("binary({})") << Value.f_String();
						return true;
					}
				}
			}
		}

		return false;
	}
	
	template <typename tf_CParseContext, typename tf_CStr>
	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateValue(tf_CStr &o_String, NEncoding::CJSON const &_Value, mint _Depth, ch8 const *_pPrettySeparator)
	{
		if (CEJSONParseContext::fs_GenerateValue<CEJSONParseContext>(o_String, _Value, _Depth, _pPrettySeparator))
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
							(Name == "$type" && iMember->f_Name() == "$value")
							|| (Name == "$value" && iMember->f_Name() == "$type")
						)
					{
						++iMember;
						if (!iMember)
						{
							auto *pType = _Value.f_GetMember("$type");
							auto *pValue = _Value.f_GetMember("$value");
							DMibCheck(pType);
							DMibCheck(pValue);
							if (pType->f_Type() != EJSONType_String)
								DMibError("Invalid EJSON: $type value must be a string");

							if (pType->f_String() == "BuildSystemToken")
							{
								CJSONParseContext::fs_GenerateExpression(o_String, *pValue, true, _Depth);
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	template bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateValue<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext, NStr::CStr>
		(
	   		CStr &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	;
	template bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateValue
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, NStr::CStrNonTracked
		>
		(
	   		CStrNonTracked &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	;
	template bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateValue
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, NStr::CStrAggregate
		>
		(
	   		CStrAggregate &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	;
	template bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateValue
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, NStr::CStrAggregateNonTracked
		>
		(
	   		CStrAggregateNonTracked &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	;

	template bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::fs_GenerateValue<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext, NStr::CStr>
		(
	   		CStr &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	;
	template bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::fs_GenerateValue
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext
			, NStr::CStrNonTracked
		>
		(
	   		CStrNonTracked &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	;
	template bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::fs_GenerateValue
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext
			, NStr::CStrAggregate
		>
		(
	   		CStrAggregate &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	;
	template bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext::fs_GenerateValue
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CEJSONParseContext
			, NStr::CStrAggregateNonTracked
		>
		(
	   		CStrAggregateNonTracked &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	;
}
