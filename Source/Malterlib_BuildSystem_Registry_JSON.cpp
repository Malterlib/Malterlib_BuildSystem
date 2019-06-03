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

	template <>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseKey<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext>
		(
		 	CStr &o_Key
		 	, uch8 const *&o_pParse
		) const
	{
		auto pParse = o_pParse;
		if (*pParse == '`')
		{
			auto pParseStart = pParse;
			if (!fg_ParseJSONString<'`', true>(o_Key, pParse, *this))
				f_ThrowError("End of eval string character ` not found for string", pParseStart);

			o_pParse = pParse;
			return;
		}
		else if (fg_StrStartsWith(pParse, "<<"))
		{
			pParse += 2;
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ':')
			{
				o_Key = "<<";
				o_Key.f_SetUserData(ERegistryStringType_NoQuote);
				o_pParse = pParse;
				return;
			}
		}

		return CParseContext::f_ParseKey<CJSONParseContext>(o_Key, o_pParse);
	}

	template <>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateKeyString<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext, CStr>
		(
		 	CStr &o_String
		 	, CStr const &_Key
		)
	{
		if (_Key.f_GetUserData() == ERegistryStringType_NoQuote && _Key == "<<")
		{
			o_String += "<<";
			return;
		}
		CParseContext::fs_GenerateKeyString<CJSONParseContext>(o_String, _Key);
	}

	template <>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateString<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext, CStr>
		(
		 	CStr &o_String
		 	, CStr const &_Value
		)
	{
		if (_Value.f_GetUserData() == ERegistryStringType_Custom)
		{
			fg_GenerateJSONString<'`', CJSONParseContext>(o_String, _Value);
			return;
		}
		CParseContext::fs_GenerateString<CJSONParseContext>(o_String, _Value);
	}
}
