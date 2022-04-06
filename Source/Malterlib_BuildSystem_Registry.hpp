// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NContainer
{
	template <>
	struct TCRegistry_CustomKeyValue<NBuildSystem::CBuildSystemSyntax::CRootKey, NBuildSystem::CBuildSystemSyntax::CRootValue>
	{
		static constexpr bool mc_bDefault = false;
		static constexpr bool mc_bDefaultKey = false;
		static constexpr bool mc_bRequireStartScopeOnSeparateLine = true;

		struct CEJSONParseContext : public NEncoding::NJSON::CParseContext
		{
			static constexpr bool mc_bCustomParse = true;
			static constexpr bool mc_bCustomGenerate = true;
			static constexpr bool mc_bAllowSingleQuote = true;
			static constexpr bool mc_bAllowKeyWithoutQuote = true;
			static constexpr NEncoding::NJSON::EParseJSONStringFlag mc_ParseJSONStringFlags = NEncoding::NJSON::EParseJSONStringFlag_AllowMultiLine;
			static constexpr bool mc_bAllowDuplicateKeys = false;
			static inline constexpr ch8 mc_AllowedControlCharacters[] = "\t";
			static inline constexpr ch8 mc_AllowedKeyWithoutQuoteCharacters[] = "$_?.";
			static inline constexpr ch8 mc_ConstantEndCharacters[] = ",}])-";
			static inline constexpr ch8 mc_BinaryOperatorCharacters[] = "+-*/%<>=!&^|";
			static inline constexpr ch8 mc_PrefixOperatorCharacters[] = "+-!~";

			CEJSONParseContext();

			template <typename tf_CParseContext, typename tf_CStr>
			static bool fs_GenerateValue(tf_CStr &o_String, NEncoding::CJSON const &_Value, mint _Depth, ch8 const *_pPrettySeparator);

			NContainer::CByteVector f_ParseBinary(uch8 const * &o_pParse, bool _bWithinParenthesis);
			NTime::CTime f_ParseDate(uch8 const * &o_pParse, bool _bWithinParenthesis);

			bool f_ParseValue(NEncoding::CJSON &o_Value, uch8 const *&o_pParse);
			void f_ParseAfterValue(NEncoding::CJSON &o_Value, uch8 const *&o_pParse);
			static bool fs_IsBinaryOperator(uch8 const *_pParse);
			static bool fs_IsPrefixOperator(uch8 const *_pParse);
		};

		enum EParseExpressionFlag
		{
			EParseExpressionFlag_None = DMibBit(0)
			, EParseExpressionFlag_SupportAppend = DMibBit(1)
			, EParseExpressionFlag_NoParentheses = DMibBit(2)
		};

		struct CJSONParseContext : public CEJSONParseContext
		{
			CJSONParseContext();

			NEncoding::CJSON f_ParseEvalStringToken(uch8 const *&o_pParse);
			NEncoding::CJSON f_ParseWildcardStringToken(uch8 const *&o_pParse);
			NEncoding::CEJSON f_ParseIdentifierTokenEJSON(uch8 const *&o_pParse);
			NEncoding::CJSON f_ParseIdentifierToken(uch8 const *&o_pParse);
			NEncoding::CJSON f_ParseFunctionToken(uch8 const *&o_pParse, NEncoding::CJSON *_pFirstParam, NStr::CStr const &_FunctionName, NStr::CStr const &_PropertyType);
			NEncoding::CJSON f_ParseJSONAccessor(uch8 const *&o_pParse, NEncoding::CJSON &&_Param);
			NEncoding::CJSON f_ParseExpression(uch8 const *&o_pParse, EParseExpressionFlag _Flags, NEncoding::CJSON *_pFirstParam = nullptr);
			NEncoding::CJSON f_ParseDefine(uch8 const *&o_pParse);
			NEncoding::CJSON f_ParseFunctionType(uch8 const *&o_pParse);
			NEncoding::CJSON f_ParseDefaultType(NStr::CStr const &_Identifier);
			NEncoding::CJSON f_ParseType(uch8 const *&o_pParse);
			NEncoding::CJSON f_ParseOneOf(uch8 const *&o_pParse);
			NEncoding::CJSON f_ParseDefaulted(uch8 const *&o_pParse, NEncoding::CJSON &&_Type);
			void f_ParsePostDefine(uch8 const *&o_pParse, NEncoding::CJSON &o_Value);
			NStr::CStr f_ParseIdentifier(uch8 const *&o_pParse);
			NStr::CStr f_ParseIdentifierLax(uch8 const *&o_pParse);

			template <typename tf_CStr>
			static void fs_GenerateExpression(tf_CStr &o_String, NEncoding::CJSON const &_Token, bool _bQuoteStrings, mint _Depth);

			template <typename tf_CStr>
			static void fs_GenerateEvalString(tf_CStr &o_String, NEncoding::CJSON const &_Token, mint _Depth);

			template <typename tf_CParseContext>
			void f_ParseKey(NStr::CStr &o_Key, uch8 const *&o_pParse);

			void f_ParseEvalStringString(NStr::CStr &o_Key, uch8 const *&o_pParse);

			bool f_ParseValue(NEncoding::CJSON &o_Value, uch8 const *&o_pParse);
			void f_ParseAfterValue(NEncoding::CJSON &o_Value, uch8 const *&o_pParse);

			template <typename tf_CParseContext, typename tf_CStr>
			static bool fs_GenerateValue(tf_CStr &o_String, NEncoding::CJSON const &_Value, mint _Depth, ch8 const *_pPrettySeparator);

			template <typename tf_CParseContext, typename tf_CStr, typename tf_CSourceStr>
			static void fs_GenerateKeyString(tf_CStr &o_String, tf_CSourceStr const &_Key);

			template <typename tf_CParseContext, typename tf_CStr, typename tf_CSourceStr>
			static void fs_GenerateString(tf_CStr &o_String, tf_CSourceStr const &_Value);

			auto f_EnableBinaryOperators()
			{
				bool bOldSupportBinaryOperators = m_bSupportBinaryOperators;
				m_bSupportBinaryOperators = true;
				return g_OnScopeExit / [this, bOldSupportBinaryOperators]
					{
						m_bSupportBinaryOperators = bOldSupportBinaryOperators;
					}
				;
			}
			auto f_DisableParseAfterValue()
			{
				bool bOldParseAfterValue = m_bParseAfterValue;
				m_bParseAfterValue = false;
				return g_OnScopeExit / [this, bOldParseAfterValue]
					{
						m_bParseAfterValue = bOldParseAfterValue;
					}
				;
			}

			mint m_ParseDepth = 0;
			bool m_bParsingDefine = false;
			bool m_bSupportBinaryOperators = true;
			bool m_bParseAfterValue = true;
		};

		struct CJSONParseContextCatureStringMap : public CJSONParseContext
		{
			static constexpr bool mc_bRecordStringMap = true;

			void f_MapCharacter(mint _iDestination, mint _iSource, mint _nChars);
			NStr::CParseLocation f_GetLocation(uch8 const *_pParse) const override;

			CJSONParseContext const *m_pOriginalParseContext;
			TCVector<mint> m_StringMap;
			uch8 const *m_pOriginalStartParse = nullptr;
		};

		static NTime::CTime fs_ParseDate(ch8 const * &o_pParse);

		static bool fs_IsReservedWord(ch8 const *_pIdentifier);
		static bool fs_CharIsStartIdentifier(uch8 _Char);
		static bool fs_CharIsIdentifier(uch8 _Char);
		static bool fs_CharIsIdentifierOrExpression(uch8 _Char);
		template <typename tf_CStr, typename tf_CStrIdentifier>
		static void fs_GenerateIdentifier(tf_CStr &o_String, tf_CStrIdentifier const &_Identifier);

		static NContainer::CByteVector fs_ParseBinary(ch8 const * &o_pParse);

		template <typename tf_CParseContext>
		static NBuildSystem::CBuildSystemSyntax::CRootValue fs_Parse(ch8 const * &o_pParse, tf_CParseContext &o_ParseContext, bool &o_bWasEscaped);

		template <typename tf_CParseContext>
		static void fs_ParseKey
			(
				ch8 const * &o_pParse
				, tf_CParseContext &o_ParseContext
				, bool &o_bWasEscaped
				, NBuildSystem::CBuildSystemSyntax::CRootKey &o_Key
				, NBuildSystem::CBuildSystemRegistry::CLocation const &_Location
			)
		;

		template <typename tf_CKey, typename tf_CString>
		static void fs_GenerateKey(tf_CString &o_Output, tf_CKey const &_Value, bool _bForceEscape, mint _Level, tf_CString const &_PreData);

		static bool fs_ValueIsEmpty(NBuildSystem::CBuildSystemSyntax::CRootValue const &_Value, bool _bForceEscape);

		template <typename tf_CString>
		static void fs_Generate(tf_CString &o_Output, NBuildSystem::CBuildSystemSyntax::CRootValue const &_Value, bool _bForceEscape, mint _Level, tf_CString const &_PreData);
	};
}

namespace NMib::NBuildSystem
{
	using CBuildSystemParseContext = NContainer::TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJSONParseContext;
}
