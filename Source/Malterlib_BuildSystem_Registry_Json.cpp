// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Registry.h"

namespace NMib::NContainer
{
#ifndef DDocumentation_Doxygen
	using namespace NEncoding;
	using namespace NEncoding::NJson;
	using namespace NBuildSystem;
	using namespace NStr;

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseEvalStringString(NStr::CStr &o_Key, uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;
		auto pParseStart = pParse;
		CStr TempKey;
		if
			(
				!fg_ParseJsonString<'`', NEncoding::NJson::EParseJsonStringFlag_AllowMultiLine>
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
		o_Key.f_SetUserData(EJsonStringType_Custom);
	}

	template <>
	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseKey
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey
			, CBuildSystemSyntax::CRootValue>::CJsonParseContext
		>
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
		else if (fg_StrStartsWith(pParse, gc_ConstString_Symbol_AppendObjectWithoutUndefined.m_String))
		{
			pParse += 3;
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ':')
			{
				o_Key = gc_ConstString_Symbol_AppendObjectWithoutUndefinedNoQuote.m_String;
				o_pParse = pParse;
				return;
			}
		}
		else if (fg_StrStartsWith(pParse, gc_ConstString_Symbol_AppendObject.m_String))
		{
			pParse += 2;
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ':')
			{
				o_Key = gc_ConstString_Symbol_AppendObjectNoQuote.m_String;
				o_pParse = pParse;
				return;
			}
		}
		else if (fg_StrStartsWith(pParse, gc_ConstString_Symbol_Ellipsis.m_String))
		{
			pParse += 3;
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ':')
			{
				o_Key = gc_ConstString_Symbol_EllipsisNoQuote.m_String;
				o_pParse = pParse;
				return;
			}
		}

		return CParseContext::f_ParseKey<CJsonParseContext>(o_Key, o_pParse);
	}

	template <typename tf_CParseContext, typename tf_CStr, typename tf_CSourceStr>
	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateKeyString(tf_CStr &o_String, tf_CSourceStr const &_Key)
	{
		if (_Key.f_GetUserData() == EJsonStringType_NoQuote && _Key == gc_ConstString_Symbol_AppendObject.m_String)
		{
			o_String += gc_ConstString_Symbol_AppendObject.m_String;
			return;
		}
		else if (_Key.f_GetUserData() == EJsonStringType_NoQuote && _Key == gc_ConstString_Symbol_Ellipsis.m_String)
		{
			o_String += gc_ConstString_Symbol_Ellipsis.m_String;
			return;
		}
		CParseContext::fs_GenerateKeyString<CJsonParseContext>(o_String, _Key);
	}

	template <typename tf_CParseContext, typename tf_CStr, typename tf_CSourceStr>
	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateString(tf_CStr &o_String, tf_CSourceStr const &_Value)
	{
		if (_Value.f_GetUserData() == EJsonStringType_Custom)
		{
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext ParseContext;
			ParseContext.m_pStartParse = (uch8 const *)_Value.f_GetStr();
			auto pParse = ParseContext.m_pStartParse;

			auto Tokens = ParseContext.f_ParseEvalStringToken(pParse)[gc_ConstString_Value];

			CJsonParseContext::fs_GenerateEvalString(o_String, Tokens, 0);
			return;
		}

		CParseContext::fs_GenerateString<CJsonParseContext>(o_String, _Value);
	}

	template void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateKeyString
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext
			, CStr::CAppender
			, CStr
		>
		(CStr::CAppender &o_String, CStr const &_Key)
	;
	template void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateKeyString
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext
			, CStrNonTracked::CAppender
			, CStr
		>
		(CStrNonTracked::CAppender &o_String, CStr const &_Key)
	;

	template void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateString
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext
			, CStrNonTracked::CAppender
			, CStr
		>
		(CStrNonTracked::CAppender &o_String, CStr const &_Value)
	;
	template void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateString
		<
			TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext
			, CStr::CAppender
			, CStr
		>
		(CStr::CAppender &o_String, CStr const &_Value)
	;
#endif
}
