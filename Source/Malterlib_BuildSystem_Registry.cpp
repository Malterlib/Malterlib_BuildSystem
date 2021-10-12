// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Registry.h"
#include "Malterlib_BuildSystem_FilePosition.h"

#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NContainer
{
	using namespace NEncoding;
	using namespace NBuildSystem;

	TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CEJSONParseContext::CEJSONParseContext()
	{
		m_bConvertNullToSpace = false;
		m_Flags = gc_BuildSystemJSONParseFlags;
	}

	TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJSONParseContext::CJSONParseContext()
	{
		m_bConvertNullToSpace = false;
		m_Flags = gc_BuildSystemJSONParseFlags;
	}

	NTime::CTime TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_ParseDate(ch8 const * &o_pParse)
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

	NContainer::CByteVector TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_ParseBinary(ch8 const * &o_pParse)
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
	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_ParseKey
		(
			ch8 const * &o_pParse
			, CBuildSystemRegistry::CParseContext &o_ParseContext
			, bool &o_bWasEscaped
			, NBuildSystem::CBuildSystemSyntax::CRootKey &o_Key
			, CBuildSystemRegistry::CLocation const &_Location
		)
	{
		o_bWasEscaped = false;

		uch8 const *pParse = (uch8 const *)(o_pParse);

		auto fParseValue = [&]() -> NEncoding::CJSON
			{
				NEncoding::CJSON Value;

				CJSONParseContext Context;
				Context.m_pStartParse = (uch8 const *)o_ParseContext.f_GetStartParse();
				if (!Context.m_pStartParse)
					Context.m_pStartParse = pParse;
				Context.m_FileName = o_ParseContext.m_File;
				Context.m_bSupportBinaryOperators = false;

				NEncoding::NJSON::fg_ParseJSONValue(Value, pParse, Context);

				return Value;
			}
		;

		auto fParsePrefixOperator = [&](CStr _Operator)
			{
				CEJSON Temp;
				auto &UserType = Temp.f_UserType();
				UserType.m_Type = "BuildSystemToken";
				UserType.m_Value =
					{
						"Type"__= "KeyPrefixOperator"
						, "Operator"__= fg_Move(_Operator)
						, "Right"__= fParseValue()
					}
				;

				o_Key = NBuildSystem::CBuildSystemSyntax::CRootKey::fs_FromJSON(Temp, _Location);
			}
		;

		auto fParseLogicalOperator = [&](CStr _Operator)
			{
				if (!fg_CharIsWhiteSpace(*pParse) && *pParse != '{')
				{
					auto ParseLocation = o_ParseContext.f_GetLocation((ch8 const *)pParse);
					DMibError(NStr::CStr::CFormat("{}Didn't expect anything after {} operator") << o_ParseContext.f_FormatLocation(ParseLocation) << _Operator);
				}

				CEJSON Temp;
				auto &UserType = Temp.f_UserType();
				UserType.m_Type = "BuildSystemToken";
				UserType.m_Value =
					{
						"Type"__= "KeyLogicalOperator"
						, "Operator"__= fg_Move(_Operator)
					}
				;

				o_Key = NBuildSystem::CBuildSystemSyntax::CRootKey::fs_FromJSON(Temp, _Location);
			}
		;

		if (pParse[0] == '!' && pParse[1] == '!')
		{
			pParse += 2;
			fParsePrefixOperator("!!");
		}
		else if (pParse[0] == '!')
		{
			pParse += 1;
			if (*pParse == '{' || fg_CharIsWhiteSpace(*pParse))
				fParseLogicalOperator("!");
			else
				fParsePrefixOperator("!");
		}
		else if (pParse[0] == '&')
		{
			pParse += 1;
			fParseLogicalOperator("&");
		}
		else if (pParse[0] == '|')
		{
			pParse += 1;
			fParseLogicalOperator("|");
		}
		else if (pParse[0] == '*')
		{
			pParse += 1;
			fParsePrefixOperator("*");
		}
		else if (pParse[0] == '%')
		{
			pParse += 1;
			fParsePrefixOperator("%");
		}
		else if (pParse[0] == '#')
		{
			pParse += 1;
 			fParsePrefixOperator("#");
		}
		else
			o_Key = NBuildSystem::CBuildSystemSyntax::CRootKey::fs_FromJSON(CEJSON::fs_FromJSON(fParseValue()), _Location);

		o_pParse = (ch8 const *)(pParse);
	}

	template <>
	NBuildSystem::CBuildSystemSyntax::CRootValue TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_Parse<CBuildSystemRegistry::CParseContext>
		(
		 	ch8 const * &o_pParse
		 	, CBuildSystemRegistry::CParseContext &o_ParseContext
		 	, bool &o_bWasEscaped
		)
	{
		o_bWasEscaped = false;

		NEncoding::CJSON Output;

		uch8 const *pParse = (uch8 const *)(o_pParse);

		CJSONParseContext Context;
		Context.m_pStartParse = (uch8 const *)o_ParseContext.f_GetStartParse();
		if (!Context.m_pStartParse)
			Context.m_pStartParse = pParse;
		Context.m_FileName = o_ParseContext.m_File;

		NEncoding::NJSON::fg_ParseJSONValue(Output, pParse, Context);

		auto ParseLocation = o_ParseContext.f_GetLocation(o_pParse);

		o_pParse = (ch8 const *)(pParse);

		return NBuildSystem::CBuildSystemSyntax::CRootValue::fs_FromJSON(NEncoding::CEJSON::fs_FromJSON(fg_Move(Output)), ParseLocation, true);
	}

	bool TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_ValueIsEmpty(NBuildSystem::CBuildSystemSyntax::CRootValue const &_Value, bool _bForceEscape)
	{
		if (!_Value.m_Value.f_IsValid())
		{
			if (_bForceEscape)
				return false;
			return true;
		}
		return false;
	}
}

namespace NMib::NBuildSystem
{
	CStr const &fg_RegistryNameStringForPath(CBuildSystemSyntax::CRootKey const &_Key)
	{
		if (!_Key.f_IsValue() || !_Key.f_Value().f_IsConstantString())
			DMibError("Only string keys supported for paths");

		return _Key.f_Value().f_ConstantString();
	}
}
