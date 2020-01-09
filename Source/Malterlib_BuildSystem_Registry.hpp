// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NContainer
{
	template <>
	struct TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>
	{
		static constexpr bool mc_bDefault = false;
		static constexpr bool mc_bRequireStartScopeOnSeparateLine = true;

		struct CJSONParseContext : public NEncoding::NJSON::CParseContext
		{
			static constexpr bool mc_bCustomParse = true;
			static constexpr bool mc_bCustomGenerate = true;
			static constexpr bool mc_bAllowSingleQuote = true;
			static constexpr bool mc_bAllowKeyWithoutQuote = true;
			static constexpr bool mc_bAllowMultilineString = true;
			static constexpr bool mc_bAllowDuplicateKeys = false;

			NEncoding::CJSON f_ParseEvalStringToken(uch8 const *&o_pParse) const;
			NEncoding::CJSON f_ParseIdentifierToken(uch8 const *&o_pParse) const;
			NEncoding::CJSON f_ParseFunctionToken(uch8 const *&o_pParse, NEncoding::CJSON *_pFirstParam, NStr::CStr const &_FunctionName);
			NEncoding::CJSON f_ParseEvalString(uch8 const *&o_pParse);
			NEncoding::CJSON f_ParseExpression(uch8 const *&o_pParse, NStr::CStr const &_TokenType);
			NStr::CStr f_ParseIdentifier(uch8 const *&o_pParse) const;

			static void fs_GenerateExpression(NStr::CStr &o_String, NEncoding::CJSON const &_Token, bool _bQuoteStrings, mint _Depth);

			template <typename tf_CParseContext>
			void f_ParseKey(NStr::CStr &o_Key, uch8 const *&o_pParse);

			bool f_ParseValue(NEncoding::CJSON &o_Value, uch8 const *&o_pParse);
			bool f_ParseAfterValue(NEncoding::CJSON &o_Value, uch8 const *&o_pParse);

			template <typename tf_CParseContext, typename tf_CStr>
			static bool fs_GenerateValue(tf_CStr &o_String, NEncoding::CJSON const &_Value, mint _Depth, ch8 const *_pPrettySeparator);

			template <typename tf_CParseContext, typename tf_CStr>
			static void fs_GenerateKeyString(tf_CStr &o_String, tf_CStr const &_Key);

			template <typename tf_CParseContext, typename tf_CStr>
			static void fs_GenerateString(tf_CStr &o_String, tf_CStr const &_Value);
		};

		struct CJSONParseContextCatureStringMap : public CJSONParseContext
		{
			static constexpr bool mc_bRecordStringMap = true;

			void f_MapCharacter(mint _iDestination, mint _iSource, mint _nChars) const;
			NStr::CParseLocation f_GetLocation(uch8 const *_pParse) const override;

			CJSONParseContext const *m_pOriginalParseContext;
			mutable TCVector<mint> m_StringMap;
			uch8 const *m_pOriginalStartParse = nullptr;
		};

		static NTime::CTime fs_ParseDate(ch8 const * &o_pParse);

		static NContainer::CByteVector fs_ParseBinary(ch8 const * &o_pParse);

		template <typename tf_CParseContext>
		static NBuildSystem::CBuildSystemRegistryValue fs_Parse(ch8 const * &o_pParse, tf_CParseContext &o_ParseContext, bool &o_bWasEscaped);

		static bool fs_ValueIsEmpty(NBuildSystem::CBuildSystemRegistryValue const &_Value, bool _bForceEscape);

		template <typename tf_CString>
		static void fs_Generate(tf_CString &o_Output, NBuildSystem::CBuildSystemRegistryValue const &_Value, bool _bForceEscape, mint _Level, tf_CString const &_PreData);
	};
}
