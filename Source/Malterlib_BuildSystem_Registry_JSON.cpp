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

	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseEvalStringString(NStr::CStr &o_Key, uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;
		auto pParseStart = pParse;
		CStr TempKey;
		if
			(
				!fg_ParseJSONString<'`', NEncoding::NJSON::EParseJSONStringFlag_AllowMultiLine>
				(
					TempKey
					, pParse
					, *this
					, [&](uch8 const *&o_pParse) -> bool
					{
						auto *pParse = o_pParse;
						if (*pParse != '@')
							return false;

						++pParse;
						if (*pParse == '@')
						{
							o_pParse = pParse;
							return false;
						}

						fg_ParseWhiteSpace(pParse);
						f_ParseExpression(pParse, EParseExpressionFlag_None);

						o_pParse = pParse;
						return true;
					}
				)
			)
		{
			f_ThrowError("End of eval string character ` not found for string", pParseStart);
		}

		o_pParse = pParse;
		o_Key.f_AddStr(pParseStart + 1, (pParse - pParseStart) - 2);
		o_Key.f_SetUserData(EJSONStringType_Custom);
	}

	template <>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseKey<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext>
		(
		 	CStr &o_Key
		 	, uch8 const *&o_pParse
		)
	{
		auto pParse = o_pParse;
		if (*pParse == '`')
		{
			f_ParseEvalStringString(o_Key, o_pParse);
			return;
		}
		else if (fg_StrStartsWith(pParse, "<<"))
		{
			pParse += 2;
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ':')
			{
				o_Key = "<<";
				o_Key.f_SetUserData(EJSONStringType_NoQuote);
				o_pParse = pParse;
				return;
			}
		}
		else if (fg_StrStartsWith(pParse, "..."))
		{
			pParse += 3;
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ':')
			{
				o_Key = "...";
				o_Key.f_SetUserData(EJSONStringType_NoQuote);
				o_pParse = pParse;
				return;
			}
		}

		return CParseContext::f_ParseKey<CJSONParseContext>(o_Key, o_pParse);
	}

	template <typename tf_CParseContext, typename tf_CStr, typename tf_CSourceStr>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateKeyString(tf_CStr &o_String, tf_CSourceStr const &_Key)
	{
		if (_Key.f_GetUserData() == EJSONStringType_NoQuote && _Key == "<<")
		{
			o_String += "<<";
			return;
		}
		else if (_Key.f_GetUserData() == EJSONStringType_NoQuote && _Key == "...")
		{
			o_String += "...";
			return;
		}
		CParseContext::fs_GenerateKeyString<CJSONParseContext>(o_String, _Key);
	}

	template <typename tf_CParseContext, typename tf_CStr, typename tf_CSourceStr>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateString(tf_CStr &o_String, tf_CSourceStr const &_Value)
	{
		if (_Value.f_GetUserData() == EJSONStringType_Custom)
		{
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext ParseContext;
			ParseContext.m_pStartParse = (uch8 const *)_Value.f_GetStr();
			auto pParse = ParseContext.m_pStartParse;

			auto Tokens = ParseContext.f_ParseEvalStringToken(pParse)["Value"];

			CJSONParseContext::fs_GenerateEvalString(o_String, Tokens, 0);
			return;
		}

		CParseContext::fs_GenerateString<CJSONParseContext>(o_String, _Value);
	}

	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateKeyString
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, CStr
			, CStr
		>
		(CStr &o_String, CStr const &_Key)
	;
	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateKeyString
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, CStrNonTracked
			, CStr
		>
		(CStrNonTracked &o_String, CStr const &_Key)
	;
	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateKeyString
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, CStrAggregate
			, CStr
		>
		(CStrAggregate &o_String, CStr const &_Key)
	;
	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateKeyString
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, CStrAggregateNonTracked
			, CStr
		>
		(CStrAggregateNonTracked &o_String, CStr const &_Key)
	;

	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateString
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, CStrNonTracked
			, CStr
		>
		(CStrNonTracked &o_String, CStr const &_Value)
	;
	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateString
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, CStr
			, CStr
		>
		(CStr &o_String, CStr const &_Value)
	;
	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateString
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, CStrAggregate
			, CStr
		>
		(CStrAggregate &o_String, CStr const &_Value)
	;
	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateString
		<
			TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext
			, CStrAggregateNonTracked
			, CStr
		>
		(CStrAggregateNonTracked &o_String, CStr const &_Value)
	;
}

namespace NMib::NBuildSystem
{
	CBuildSystemRegistryValue::CBuildSystemRegistryValue(NEncoding::CEJSON &&_Other)
		: NEncoding::CEJSON(fg_Move(_Other))
	{
	}

	CBuildSystemRegistryValue::CBuildSystemRegistryValue(NEncoding::CEJSON const &_Other)
		: NEncoding::CEJSON(_Other)
	{
	}

	CBuildSystemRegistryValue &CBuildSystemRegistryValue::operator = (NEncoding::CEJSON &&_Other)
	{
		(NEncoding::CEJSON &)*this = fg_Move(_Other);
		return *this;
	}

	CBuildSystemRegistryValue &CBuildSystemRegistryValue::operator = (NEncoding::CEJSON const &_Other)
	{
		(NEncoding::CEJSON &)*this = _Other;
		return *this;
	}

	aint CBuildSystemRegistryValue::f_Cmp(CBuildSystemRegistryValue const &_Right) const
	{
		if (*this < _Right)
			return -1;
		else if (_Right < *this)
			return 1;
		else
			return 0;
	}
}
