// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Registry.h"

namespace NMib::NContainer
{
	using namespace NEncoding;
	using namespace NBuildSystem;

	NTime::CTime TCRegistry_CustomValue<CBuildSystemRegistryValue>::fs_ParseDate(ch8 const * &o_pParse)
	{
		using namespace NStr;

		o_pParse += 5;
		ch8 const *pParse = o_pParse;
		auto pParseStart = pParse;

		auto fMaybeNextChar = [&](ch8 const *_pParse) -> ch8 const *
			{
				if (!*_pParse || *_pParse == ')')
					return _pParse;
				return _pParse + 1;
			}
		;

		auto fReportError = [&](NStr::CStr const &_Error, ch8 const *_pEnd)
			{
				DMibError
					(
						fg_Format
						(
							"Failed to parse \"{}\" as a date: {}. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"
							, CStr(pParseStart, _pEnd - pParseStart)
							, _Error
						)
					)
				;
			}
		;

		auto fParseInt = [&](ch8 const *&_pParse, auto _Type, ch8 const *_pTerminators)
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
		if (*pParse != ')')
			fReportError(NStr::fg_Format("Unexpected charater: {}", NStr::CStr(pParse, 1)), pParse + 1);
		else
			++pParse;

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

	NContainer::CByteVector TCRegistry_CustomValue<CBuildSystemRegistryValue>::fs_ParseBinary(ch8 const * &o_pParse)
	{
		o_pParse += 7;

		ch8 const *pParse = o_pParse;
		NStr::fg_ParseWhiteSpaceNoLines(pParse);
		auto pStart = pParse;
		while (*pParse == '=' || NEncoding::g_Base64EncodingTableReverse[(uch8)*pParse] != -1)
			++pParse;
		auto pEnd = pParse;
		NStr::fg_ParseWhiteSpaceNoLines(pParse);

		if (*pParse != ')')
			DMibError(NStr::fg_Format("Unexpected character in Base64 string: {}", NStr::CStr(pParse, 1)));
		else
			++pParse;

		NContainer::CByteVector Data;
		NEncoding::fg_Base64Decode(NStr::CStr(pStart, pEnd - pStart), Data);

		o_pParse = pParse;
		return Data;
	}

	template <>
	CBuildSystemRegistryValue TCRegistry_CustomValue<CBuildSystemRegistryValue>::fs_Parse<CBuildSystemRegistry::CParseContext>
		(
		 	ch8 const * &o_pParse
		 	, CBuildSystemRegistry::CParseContext &o_ParseContext
		 	, bool &o_bWasEscaped
		)
	{
		o_bWasEscaped = false;

		if (NStr::fg_StrStartsWith(o_pParse, "Date("))
			return {fs_ParseDate(o_pParse)};
		else if (NStr::fg_StrStartsWith(o_pParse, "Binary("))
			return {fs_ParseBinary(o_pParse)};

		NEncoding::CJSON Output;

		uch8 const *pParse = (uch8 const *)(o_pParse);

		CJSONParseContext Context;
		Context.m_pStartParse = (uch8 const *)o_ParseContext.f_GetStartParse();
		if (!Context.m_pStartParse)
			Context.m_pStartParse = pParse;
		Context.m_FileName = o_ParseContext.m_File;
		Context.m_bConvertNullToSpace = false;
		Context.m_bAllowUndefined = true;

		NEncoding::NJSON::fg_ParseJSONValue(Output, pParse, Context);

		o_pParse = (ch8 const *)(pParse);

		return {NEncoding::CEJSON::fs_FromJSON(Output)};
	}

	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::fs_ValueIsEmpty(CBuildSystemRegistryValue const &_Value, bool _bForceEscape)
	{
		if (!_Value.f_IsValid())
		{
			if (_bForceEscape)
				return false;
			return true;
		}
		return false;
	}
}
