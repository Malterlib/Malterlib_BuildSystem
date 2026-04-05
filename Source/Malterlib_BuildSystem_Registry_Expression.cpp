// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Registry.h"

#include <Mib/Encoding/EJsonParse>

namespace NMib::NContainer
{
#ifndef DDocumentation_Doxygen
	using namespace NEncoding;
	using namespace NEncoding::NJson;
	using namespace NBuildSystem;
	using namespace NStr;

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContextCatureStringMap::f_MapCharacter(umint _iDestination, umint _iSource, umint _nChars)
	{
		m_StringMap.f_SetAtLeastLen(_iDestination + _nChars, 0);
		for (umint i = 0; i < _nChars; ++i)
			m_StringMap[_iDestination + i] = _iSource + i;
	}

	CParseLocation TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContextCatureStringMap::f_GetLocation(uch8 const *_pParse) const
	{
		if (!m_pOriginalStartParse)
			return m_pOriginalParseContext->f_GetLocation(_pParse);

		auto pParseInOriginal = m_pOriginalStartParse + m_StringMap[_pParse - m_pStartParse];
		using namespace NStr;
		CParseLocation Location;
		Location.m_File = m_FileName;
		Location.m_Character = pParseInOriginal - m_pOriginalStartParse;

		auto *pParse = m_pOriginalStartParse;
		umint Line = 1;
		auto *pLastLine = pParse;
		while (*pParse)
		{
			fg_ParseToEndOfLine(pParse);

			if (pParse >= pParseInOriginal)
				break;

			if (fg_ParseEndOfLine(pParse))
			{
				++Line;
				pLastLine = pParse;
			}
		}

		Location.m_Line = Line;
		if (pParseInOriginal >= pLastLine)
			Location.m_Column = (pParseInOriginal - pLastLine) + 1;
		else
			Location.m_Column = 1;

		return Location;
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseEvalStringToken(uch8 const *&o_pParse)
	{
		auto *pParse = o_pParse;
		CStr ParsedString;
		auto pParseStart = pParse;

		CJsonParseContextCatureStringMap ParseContext;
		ParseContext.m_pOriginalParseContext = this;
		(CJsonParseContext &)ParseContext = *this;

		CJsonSorted ParsedEvalString;
		auto &TokenArray = ParsedEvalString.f_Array();

		auto fAddStringToken = [&]
			{
				if (ParsedString.f_IsEmpty())
					return;
				auto &Token = TokenArray.f_Insert();
				auto &Object = Token.f_Object();
				Object[gc_ConstString_Type] = gc_ConstString_String;
				Object[gc_ConstString_Value] = fg_Move(ParsedString);
				DMibMovedFromValid(ParsedString);
			}
		;

		auto fParseExpression = [&](uch8 const *&o_pParse) -> bool
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

				fAddStringToken();
				fg_ParseWhiteSpace(pParse);
				TokenArray.f_Insert(f_ParseExpression(pParse, EParseExpressionFlag_None));

				o_pParse = pParse;
				return true;
			}
		;

		if (*pParse == '`')
		{
			if
				(
					!fg_ParseJsonString<'`', NEncoding::NJson::EParseJsonStringFlag_AllowMultiLine>
					(
						ParsedString
						, pParse
						, ParseContext
						, fParseExpression
					)
				)
			{
				f_ThrowError("End of eval string character ` not found for string", pParseStart);
			}
		}
		else
		{
			if
				(
					!fg_ParseJsonString<'`', NEncoding::NJson::EParseJsonStringFlag_AllowMultiLine | NEncoding::NJson::EParseJsonStringFlag_NoQuotes>
					(
						ParsedString
						, pParse
						, ParseContext
						, fParseExpression
					)
				)
			{
				f_ThrowError("End of eval string character ` not found for string", pParseStart);
			}
		}

		fAddStringToken();

		o_pParse = pParse;

		CJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_EvalString;
		Object[gc_ConstString_Value] = fg_Move(ParsedEvalString);
		return Return;
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseWildcardStringToken(uch8 const *&o_pParse)
	{
		auto *pParse = o_pParse;
		auto pParseStart = pParse;

		CJsonParseContextCatureStringMap ParseContext;
		ParseContext.m_pOriginalParseContext = this;
		(CJsonParseContext &)ParseContext = *this;

		CJsonSorted Value;
		if (*pParse == '"')
		{
			CStr ParsedString;
			if (!fg_ParseJsonString<'"', NEncoding::NJson::EParseJsonStringFlag_AllowMultiLine>(ParsedString, pParse, ParseContext))
				f_ThrowError("End of string character \" not found", pParseStart);
			Value = ParsedString;
		}
		else if (*pParse == '\'')
		{
			CStr ParsedString;
			if (!fg_ParseJsonString<'\'', NEncoding::NJson::EParseJsonStringFlag_AllowMultiLine>(ParsedString, pParse, ParseContext))
				f_ThrowError("End of string character ' not found", pParseStart);
			Value = ParsedString;
		}
		else if (*pParse == '`')
			Value = f_ParseEvalStringToken(pParse);

		o_pParse = pParse;

		CJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_WildcardString;
		Object[gc_ConstString_Value] = fg_Move(Value);
		return Return;
	}

	namespace
	{
		struct CReservedWords
		{
			TCSet<CStr> m_Words
				{
					gc_ConstString_Empty
					, gc_ConstString_true
					, gc_ConstString_false
					, gc_ConstString_null
					, gc_ConstString_undefined
					, gc_ConstString_define
					, gc_ConstString_function
					, gc_ConstString_type
					, gc_ConstString_one_of
					, gc_ConstString_string
					, gc_ConstString_int
					, gc_ConstString_float
					, gc_ConstString_bool
					, gc_ConstString_date
					, gc_ConstString_binary
					, gc_ConstString_any
					, gc_ConstString_void
					, gc_ConstString_identifier
				}
			;
		};

		TCAggregate<CReservedWords> g_ReservedWords = {DAggregateInit};
	}

	bool NBuildSystem::CCustomRegistryKeyValue::fs_IsReservedWord(ch8 const *_pIdentifier)
	{
		return (*g_ReservedWords).m_Words.f_Exists(_pIdentifier);
	}

	template <typename tf_CStr, typename tf_CStrIdentifier>
	void NBuildSystem::CCustomRegistryKeyValue::fs_GenerateIdentifier(tf_CStr &o_String, tf_CStrIdentifier const &_Identifier)
	{
		if (fs_IsReservedWord(_Identifier.f_GetStr()))
		{
			o_String += "\\";
			o_String += _Identifier;
			return;
		}

		auto *pParse = _Identifier.f_GetStr();
		if (*pParse)
		{
			if (!fs_CharIsStartIdentifier(*pParse) || *pParse == '\\')
				o_String += '\\';

			o_String += *pParse;
			++pParse;
		}

		while (*pParse)
		{
			if (!fs_CharIsIdentifier(*pParse) || *pParse == '\\')
				o_String += '\\';

			o_String += *pParse;
			++pParse;
		}
	}

	bool NBuildSystem::CCustomRegistryKeyValue::fs_CharIsStartIdentifier(uch8 _Char)
	{
		return fg_CharIsAnsiAlphabetical(_Char) || _Char == '_' || _Char == '\\';
	}

	bool NBuildSystem::CCustomRegistryKeyValue::fs_CharIsIdentifier(uch8 _Char)
	{
		return fg_CharIsAnsiAlphabetical(_Char) || _Char == '_' || fg_CharIsNumber(_Char) || _Char == '\\';
	}

	bool NBuildSystem::CCustomRegistryKeyValue::fs_CharIsIdentifierOrExpression(uch8 _Char)
	{
		return fs_CharIsIdentifier(_Char) || _Char == '@';
	}

	CStr TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseIdentifierLax(uch8 const *&o_pParse)
	{
		auto *pParse = o_pParse;

		if (!fs_CharIsIdentifier(*pParse))
			f_ThrowError("Expected start of identifier (A-Z a-z 0-9 _)", o_pParse);

		CStr Return;
		while (fs_CharIsIdentifier(*pParse))
		{
			if (*pParse == '\\')
			{
				++pParse;
				if (*pParse)
				{
					Return.f_AddChar(*pParse);
					++pParse;
				}
				continue;
			}
			Return.f_AddChar(*pParse);
			++pParse;
		}

		o_pParse = pParse;

		return Return;
	}

	CStr TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseIdentifier(uch8 const * &o_pParse)
	{
		auto *pParse = o_pParse;

		if (!fs_CharIsStartIdentifier(*pParse))
			f_ThrowError("Expected start of identifier (A-Z a-z _ \\)", o_pParse);

		CStr Return;
		while (fs_CharIsIdentifier(*pParse))
		{
			if (*pParse == '\\')
			{
				++pParse;
				if (fg_CharIsWhiteSpace(*pParse) && Return.f_IsEmpty())
					break;

				if (*pParse)
				{
					Return.f_AddChar(*pParse);
					++pParse;
				}
				continue;
			}
			Return.f_AddChar(*pParse);
			++pParse;
		}

		o_pParse = pParse;

		return Return;
	}

	NStr::CStr TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseIdentifierWithNamespace(uch8 const *&o_pParse)
	{
		auto *pParse = o_pParse;

		if (!fs_CharIsStartIdentifier(*pParse))
			f_ThrowError("Expected start of identifier (A-Z a-z _ \\)", o_pParse);

		TCVector<CStr> Identifiers;

		while (auto Identifier = f_ParseIdentifier(pParse))
		{
			Identifiers.f_Insert(fg_Move(Identifier));
			if (fg_StrStartsWith(pParse, "::"))
				pParse += 2;
			else
				break;
		}

		o_pParse = pParse;

		return CStr::fs_Join(Identifiers, "::");
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseJsonAccessor(uch8 const *&o_pParse, NEncoding::CJsonSorted &&_Param)
	{
		auto pParse = o_pParse;
		DMibRequire(*pParse == '<');
		++pParse;

		CEJsonSorted Return;
		auto &ReturnUserType = Return.f_UserType();
		ReturnUserType.m_Type = gc_ConstString_BuildSystemToken;
		auto &Object = ReturnUserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_JsonAccessor;
		Object[gc_ConstString_Param] = fg_Move(_Param);

		auto &AccessorsArray = Object[gc_ConstString_Accessors].f_Array();

		bool bOptionalChaining = false;
		auto fAddAccessor = [&](CJsonSorted &&_Accessor)
			{
				CJsonSorted Accessor;
				Accessor[gc_ConstString_Accessor] = fg_Move(_Accessor);
				Accessor[gc_ConstString_OptionalChaining] = fg_Exchange(bOptionalChaining, false);

				AccessorsArray.f_Insert(fg_Move(Accessor));
			}
		;

		bool bEndedParen = false;

		while (*pParse)
		{
			if (pParse[0] == '?' && pParse[1] == '.')
			{
				if (bOptionalChaining)
					f_ThrowError("Optional chaining already specified", pParse);
				bOptionalChaining = true;

				pParse += 2;
			}
			else if (*pParse == '\'')
			{
				CJsonParseContextCatureStringMap ParseContext;
				ParseContext.m_pOriginalParseContext = this;
				(CJsonParseContext &)ParseContext = *this;

				CStr ParsedString;
				auto pParseStart = pParse;
				if (!fg_ParseJsonString<'\'', NEncoding::NJson::EParseJsonStringFlag_AllowMultiLine>(ParsedString, pParse, ParseContext))
					f_ThrowError("End of string character ' not found", pParseStart);
				fAddAccessor(fg_Move(ParsedString));
				DMibMovedFromValid(ParsedString);
			}
			else if (*pParse == '"')
			{
				CJsonParseContextCatureStringMap ParseContext;
				ParseContext.m_pOriginalParseContext = this;
				(CJsonParseContext &)ParseContext = *this;

				CStr ParsedString;
				auto pParseStart = pParse;
				if (!fg_ParseJsonString<'"', NEncoding::NJson::EParseJsonStringFlag_AllowMultiLine>(ParsedString, pParse, ParseContext))
					f_ThrowError("End of string character \" not found", pParseStart);
				fAddAccessor(fg_Move(ParsedString));
				DMibMovedFromValid(ParsedString);
			}
			else if (fs_CharIsStartIdentifier(*pParse))
				fAddAccessor(f_ParseIdentifier(pParse));
			else if (*pParse == '@')
			{
				++pParse;
				fAddAccessor(f_ParseExpression(pParse, EParseExpressionFlag_None));
			}
			else if (*pParse == '.')
				++pParse;
			else if (*pParse == '[')
			{
				++pParse;

				auto pParseStart = pParse;
				CJsonSorted Param;
				auto Cleanup = f_EnableBinaryOperators();
				NJson::fg_ParseJsonValue(Param, pParse, *this);

				bool bIsValid = false;
				if (Param.f_IsInteger())
					bIsValid = true;
				else if
					(
						Param.f_IsObject()
						&& Param.f_GetMember(CEJsonConstStrings::mc_Type, EJsonType_String)
						&& Param.f_GetMember(CEJsonConstStrings::mc_Type)->f_String() == gc_ConstString_BuildSystemToken.m_String
					)
				{
					if (auto pValue = Param.f_GetMember(CEJsonConstStrings::mc_Value, EJsonType_Object))
					{
						if (auto pType = pValue->f_GetMember(gc_ConstString_Type, EJsonType_String); pType && pType->f_String() == gc_ConstString_Expression.m_String)
							bIsValid = true;
					}
				}

				if (!bIsValid)
					f_ThrowError("Subscript needs an integer or @(Identifier) expression", pParseStart);

				CJsonSorted Object;
				Object[gc_ConstString_Type] = gc_ConstString_Subscript;
				Object[gc_ConstString_Argument] = fg_Move(Param);

				fAddAccessor(fg_Move(Object));

				if (*pParse != ']')
					f_ThrowError("Expected ]", pParse);
				++pParse;
			}
			else if (*pParse == '>')
			{
				++pParse;
				bEndedParen = true;
				break;
			}
			else
				f_ThrowError("Unexpected character", pParse);
		}

		if (!bEndedParen)
			f_ThrowError("Could not find matching end >", o_pParse);

		o_pParse = pParse;

		return fg_Move(Return).f_ToJson();
	}

	CEJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseIdentifierTokenEJson(uch8 const *&o_pParse)
	{
		auto *pParse = o_pParse;

		auto fParseConstantOrEvalString = [&]() -> CJsonSorted
			{
				CStr Constant;
				if (*pParse != '@')
					Constant = f_ParseIdentifier(pParse);

				if (*pParse == '@')
				{
					CJsonSorted Return;

					auto &TokenArray = Return.f_Array();
					if (Constant)
						TokenArray.f_Insert(fg_Move(Constant));

					while (*pParse)
					{
						if (*pParse == '@')
						{
							++pParse;
							TokenArray.f_Insert(f_ParseExpression(pParse, EParseExpressionFlag_None));
						}
						else if (fs_CharIsIdentifier(*pParse))
							TokenArray.f_Insert(f_ParseIdentifierLax(pParse));
						else
							break;
					}

					return Return;
				}
				else
					return Constant;
			}
		;

		CStr EntityType;
		CJsonSorted PropertyType;
		CJsonSorted Name;

		Name = fParseConstantOrEvalString();

		if (*pParse == ':' && !fg_CharIsWhiteSpace(pParse[1]))
		{
			++pParse;
			if (!Name.f_IsString())
				f_ThrowError("Entity type must be constant", o_pParse);

			EntityType = fg_Move(Name.f_String());
			Name = fParseConstantOrEvalString();
		}

		if (*pParse == '.' && !fg_StrStartsWith(pParse, gc_ConstString_Symbol_Ellipsis.m_String))
		{
			++pParse;
			PropertyType = fg_Move(Name);
			Name = fParseConstantOrEvalString();
		}

		o_pParse = pParse;

		if (!PropertyType.f_IsValid())
			PropertyType = gc_ConstString_Empty;

		CEJsonSorted Return;
		auto &ReturnUserType = Return.f_UserType();
		ReturnUserType.m_Type = gc_ConstString_BuildSystemToken;
		auto &Object = ReturnUserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_Identifier;
		Object[gc_ConstString_EntityType] = fg_Move(EntityType);
		Object[gc_ConstString_PropertyType] = fg_Move(PropertyType);
		Object[gc_ConstString_Name] = fg_Move(Name);

		return Return;
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseIdentifierToken(uch8 const * &o_pParse)
	{
		return f_ParseIdentifierTokenEJson(o_pParse).f_ToJson();
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseFunctionToken
		(
			uch8 const *&o_pParse
			, CJsonSorted *_pFirstParam
			, CStr const &_FunctionName
			, CStr const &_PropertyType
		)
	{
		auto *pParse = o_pParse;
		if (*pParse != '(')
			f_ThrowError("Function must be enclosed in ()", pParse);

		++pParse;

		CEJsonSorted Return;
		auto &ReturnUserType = Return.f_UserType();
		ReturnUserType.m_Type = gc_ConstString_BuildSystemToken;
		auto &Object = ReturnUserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = _pFirstParam ? gc_ConstString_PostFunction : gc_ConstString_Function;
		Object[gc_ConstString_Name] = _FunctionName;
		Object[gc_ConstString_PropertyType] = _PropertyType;

		auto &FunctionParams = Object[gc_ConstString_Params].f_Array();

		if (_pFirstParam)
			FunctionParams.f_Insert(fg_Move(*_pFirstParam));

		bool bEndedParen = false;
		bool bLastTokenValid = true;

		while (*pParse)
		{
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ')')
			{
				if (!bLastTokenValid)
					f_ThrowError("Missing param", pParse);
				++pParse;
				bEndedParen = true;
				break;
			}
			else if (*pParse == ',')
			{
				if (!bLastTokenValid)
					f_ThrowError(", is not valid here", pParse);
				++pParse;
			}
			else
			{
				CJsonSorted Param;
				auto Cleanup = f_EnableBinaryOperators();
				auto Cleanup2 = f_ParsingFunctionParams();
				NJson::fg_ParseJsonValue(Param, pParse, *this);
				FunctionParams.f_Insert(fg_Move(Param));
				bLastTokenValid = true;
				continue;
			}

			bLastTokenValid = false;
		}

		if (!bEndedParen)
			f_ThrowError("Could not find matching end )", o_pParse);

		o_pParse = pParse;

		return fg_Move(Return).f_ToJson();
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseExpression
		(
			uch8 const *&o_pParse
			, EParseExpressionFlag _Flags
			, CJsonSorted *_pFirstParam
		)
	{
		auto *pParse = o_pParse;
		bool bUseParentheses = !(_Flags & EParseExpressionFlag_NoParentheses);
		if (bUseParentheses)
		{
			if (*pParse != '(')
				f_ThrowError("Expression must be enclosed in ()", pParse);
			++pParse;
			fg_ParseWhiteSpace(pParse);
		}

		CJsonSorted Return;

		auto &ReturnObject = Return.f_Object();

		TCOptional<CJsonSorted> Param;
		auto SetParam = g_OnScopeExit / [&]
			{
				if (Param)
					ReturnObject[gc_ConstString_Param] = fg_Move(*Param);
			}
		;
		ReturnObject[gc_ConstString_Paren] = bUseParentheses;

		if (_pFirstParam)
			Param = fg_Move(*_pFirstParam);

		bool bEndedParen = false;

		auto fCheckParam = [&]
			{
				if (Param)
					f_ThrowError("Expression parameter already specified", pParse);
			}
		;

		umint nParsed = 0;

		while (*pParse && !fg_CharIsNewLine(*pParse))
		{
			++nParsed;

			auto Cleanup = g_OnScopeExit / [&]
				{
					if (bUseParentheses && !bEndedParen)
						fg_ParseWhiteSpace(pParse);
				}
			;

			if (_Flags & EParseExpressionFlag_ParsingFunctionParams)
			{
				fg_ParseWhiteSpace(pParse);
				if (*pParse == ',' || *pParse == ')' || fg_StrStartsWith(pParse, gc_ConstString_Symbol_Ellipsis.m_String))
					break;
			}
			else if (bUseParentheses)
				fg_ParseWhiteSpace(pParse);
			else
			{
				auto *pTestParse = pParse;
				fg_ParseWhiteSpaceNoLines(pTestParse);

				if
					(
						(fg_CharIsWhiteSpace(*pParse) && m_ParseDepth == 1 && nParsed > 1 && m_ObjectArrayParseDepth == 0)
						|| *pTestParse == '}'
						|| *pTestParse == ']'
						|| *pTestParse == ','
						|| *pTestParse == ':'
						|| fg_StrStartsWith(pTestParse, gc_ConstString_Symbol_Ellipsis.m_String)
						|| fg_StrStartsWith(pTestParse, "//")
					)
				{
					break;
				}
				pParse = pTestParse;
			}

			if (*pParse == ')')
			{
				if (!bUseParentheses)
					break;
				++pParse;
				bEndedParen = true;

				if ((_Flags & EParseExpressionFlag_SupportAppend) && fg_StrStartsWith(pParse, gc_ConstString_Symbol_Ellipsis.m_String))
				{
					pParse += 3;
					ReturnObject[gc_ConstString_Type] = gc_ConstString_AppendExpression;
				}
				else
					ReturnObject[gc_ConstString_Type] = gc_ConstString_Expression;

				break;
			}
			else if (fs_CharIsStartIdentifier(*pParse) || *pParse == '@')
			{
				fCheckParam();
				if (fg_StrStartsWith(pParse, gc_ConstString_true.m_String) && !fs_CharIsIdentifierOrExpression(pParse[4]))
				{
					Param = true;
					pParse += 4;
				}
				else if (fg_StrStartsWith(pParse, gc_ConstString_false.m_String) && !fs_CharIsIdentifierOrExpression(pParse[5]))
				{
					Param = false;
					pParse += 5;
				}
				else if (fg_StrStartsWith(pParse, gc_ConstString_null.m_String) && !fs_CharIsIdentifierOrExpression(pParse[4]))
				{
					Param = nullptr;
					pParse += 4;
				}
				else if (fg_StrStartsWith(pParse, gc_ConstString_undefined.m_String) && !fs_CharIsIdentifierOrExpression(pParse[9]))
				{
					Param = CJsonSorted();
					pParse += 9;
				}
				else
					Param = f_ParseIdentifierToken(pParse);
			}
			else if (*pParse == '(')
			{
				if (!Param)
				{
					auto Cleanup = f_EnableBinaryOperators();
					auto Expression = f_ParseExpression(pParse, EParseExpressionFlag_None);

					CEJsonSorted Return;
					auto &ReturnUserType = Return.f_UserType();
					ReturnUserType.m_Type = gc_ConstString_BuildSystemToken;
					ReturnUserType.m_Value = fg_Move(Expression);

					Param = fg_Move(Return).f_ToJson();
				}
				else
				{
					if (fg_Const(*Param)[CEJsonConstStrings::mc_Value][gc_ConstString_Type.m_String].f_String() != gc_ConstString_Identifier.m_String)
						f_ThrowError("Function call needs a name", pParse);

					auto &ParamValue = fg_Const(*Param)[CEJsonConstStrings::mc_Value].f_Object();

					if (ParamValue[gc_ConstString_EntityType].f_String())
						f_ThrowError("Function call cannot specify entity", pParse);

					Param = f_ParseFunctionToken(pParse, nullptr, ParamValue[gc_ConstString_Name].f_String(), ParamValue[gc_ConstString_PropertyType].f_String());
				}
			}
			else if (*pParse == '<' && !NStr::fg_CharIsWhiteSpaceNoLines(pParse[1]) && pParse[1] != '<' && pParse[1] != '=' && pParse[1] != '!')
			{
				if (!Param)
					f_ThrowError("JSON accessor needs a param", pParse);

				auto FirstParam = fg_Move(*Param);
				Param = f_ParseJsonAccessor(pParse, fg_Move(FirstParam));
			}
			else if (fg_StrStartsWith(pParse, gc_ConstString_Symbol_AccessObject.m_String))
			{
				if (!Param)
					f_ThrowError("Function call needs a target", pParse);

				pParse += 2;
				fg_ParseWhiteSpace(pParse);

				CStr PropertyType;
				CStr FunctionName = f_ParseIdentifier(pParse);
				fg_ParseWhiteSpace(pParse);

				if (*pParse == '.')
				{
					++pParse;
					PropertyType = f_ParseIdentifier(pParse);
					fg_ParseWhiteSpace(pParse);
					fg_Swap(PropertyType, FunctionName);
				}

				auto FirstParam = fg_Move(*Param);
				Param = f_ParseFunctionToken(pParse, &FirstParam, FunctionName, PropertyType);
			}
			else if (!m_bParsingDefine && *pParse == '?' && !fs_IsBinaryOperator(pParse+1))
			{
				if (!Param)
					f_ThrowError("Ternary operator needs a target", pParse);

				++pParse;
				fg_ParseWhiteSpace(pParse);

				CJsonSorted Left;
				auto Cleanup = f_EnableBinaryOperators();
				NJson::fg_ParseJsonValue(Left, pParse, *this);

				fg_ParseWhiteSpace(pParse);

				if (*pParse != ':')
					f_ThrowError("Expected ':' for ternary operator", pParse);

				++pParse;

				fg_ParseWhiteSpace(pParse);

				CJsonSorted Right;
				NJson::fg_ParseJsonValue(Right, pParse, *this);

				CEJsonSorted Return;
				auto &ReturnUserType = Return.f_UserType();
				ReturnUserType.m_Type = gc_ConstString_BuildSystemToken;
				auto &Object = ReturnUserType.m_Value.f_Object();
				Object[gc_ConstString_Type] = gc_ConstString_Ternary;
				Object[gc_ConstString_Conditional] = fg_Move(*Param);
				Object[gc_ConstString_Left] = fg_Move(Left);
				Object[gc_ConstString_Right] = fg_Move(Right);

				Param = fg_Move(Return).f_ToJson();
			}
			else if (m_bSupportBinaryOperators && !m_bParsingDefine && !Param && fs_IsPrefixOperator(pParse))
			{
				CStr Operator(pParse, 1);

				if
					(
						Operator != gc_ConstString_Symbol_OperatorAdd.m_String
						&& Operator != gc_ConstString_Symbol_OperatorSubtract.m_String
						&& Operator != gc_ConstString_Symbol_BitwiseNot.m_String
						&& Operator != gc_ConstString_Symbol_LogicalNot.m_String
					)
				{
					f_ThrowError("Invalid prefix operator: {}"_f << Operator, pParse);
				}

				++pParse;

				fg_ParseWhiteSpace(pParse);

				CJsonSorted Right;
				auto Cleanup = f_DisableParseAfterValue();
				NJson::fg_ParseJsonValue(Right, pParse, *this);

				CEJsonSorted Return;
				auto &ReturnUserType = Return.f_UserType();
				ReturnUserType.m_Type = gc_ConstString_BuildSystemToken;
				auto &Object = ReturnUserType.m_Value.f_Object();
				Object[gc_ConstString_Type] = gc_ConstString_PrefixOperator;
				Object[gc_ConstString_Operator] = fg_Move(Operator);
				Object[gc_ConstString_Right] = fg_Move(Right);

				Param = fg_Move(Return).f_ToJson();
			}
			else if (m_bSupportBinaryOperators && !m_bParsingDefine && fs_IsBinaryOperator(pParse))
			{
				if (!Param)
					f_ThrowError("Binary operator needs a left operand", pParse);

				auto pParseStart = pParse;
				++pParse;

				while (NStr::fg_StrFindChar(mc_BinaryOperatorCharacters, *pParse) >= 0)
					++pParse;

				CStr Operator(pParseStart, pParse - pParseStart);
				if (!fg_CharIsWhiteSpaceNoLines(*pParse))
					f_ThrowError("Operator needs to be followed by whitespace", pParse);

				if
					(
						Operator != gc_ConstString_Symbol_OperatorLessThan.m_String
						&& Operator != gc_ConstString_Symbol_OperatorGreaterThan.m_String
						&& Operator != gc_ConstString_Symbol_OperatorGreaterThanEqual.m_String
						&& Operator != gc_ConstString_Symbol_OperatorLessThanEqual.m_String
						&& Operator != gc_ConstString_Symbol_OperatorEqual.m_String
						&& Operator != gc_ConstString_Symbol_OperatorNotEqual.m_String
						&& Operator != gc_ConstString_Symbol_OperatorAdd.m_String
						&& Operator != gc_ConstString_Symbol_OperatorSubtract.m_String
						&& Operator != gc_ConstString_Symbol_OperatorDivide.m_String
						&& Operator != gc_ConstString_Symbol_OperatorMultiply.m_String
						&& Operator != gc_ConstString_Symbol_OperatorModulus.m_String
						&& Operator != gc_ConstString_Symbol_OperatorBitwiseLeftShift.m_String
						&& Operator != gc_ConstString_Symbol_OperatorBitwiseRightShift.m_String
						&& Operator != gc_ConstString_Symbol_OperatorBitwiseAnd.m_String
						&& Operator != gc_ConstString_Symbol_OperatorBitwiseOr.m_String
						&& Operator != gc_ConstString_Symbol_OperatorBitwiseXor.m_String
						&& Operator != gc_ConstString_Symbol_OperatorAnd.m_String
						&& Operator != gc_ConstString_Symbol_OperatorOr.m_String
						&& Operator != gc_ConstString_Symbol_OperatorNullishCoalescing.m_String
						&& Operator != gc_ConstString_Symbol_OperatorMatchEqual.m_String
						&& Operator != gc_ConstString_Symbol_OperatorMatchNotEqual.m_String
					)
				{
					f_ThrowError("Invalid binary operator: {}"_f << Operator, pParseStart);
				}

				++pParse;
				fg_ParseWhiteSpace(pParse);

				CJsonSorted Right;
				NJson::fg_ParseJsonValue(Right, pParse, *this);

				CEJsonSorted Return;
				auto &ReturnUserType = Return.f_UserType();
				ReturnUserType.m_Type = gc_ConstString_BuildSystemToken;
				auto &Object = ReturnUserType.m_Value.f_Object();
				Object[gc_ConstString_Type] = gc_ConstString_BinaryOperator;
				Object[gc_ConstString_Operator] = fg_Move(Operator);
				Object[gc_ConstString_Left] = fg_Move(*Param);
				Object[gc_ConstString_Right] = fg_Move(Right);

				Param = fg_Move(Return).f_ToJson();
			}
			else
			{
				fCheckParam();
				CJsonSorted ParamParse;
				auto Cleanup = f_EnableBinaryOperators();
				NJson::fg_ParseJsonValue(ParamParse, pParse, *this);
				Param = fg_Move(ParamParse);
			}
		}

		if (bUseParentheses && !bEndedParen)
			f_ThrowError("Could not find matching end )", o_pParse);

		if (!bUseParentheses)
		{
			if ((_Flags & EParseExpressionFlag_SupportAppend) && fg_StrStartsWith(pParse, gc_ConstString_Symbol_Ellipsis.m_String))
			{
				pParse += 3;
				ReturnObject[gc_ConstString_Type] = gc_ConstString_AppendExpression;
			}
			else
				ReturnObject[gc_ConstString_Type] = gc_ConstString_Expression;
		}

		o_pParse = pParse;
		return Return;
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseDefine(uch8 const *&o_pParse, bool _bLegacy)
	{
		auto pParse = o_pParse;

		bool bPreviousParsingDefine = m_bParsingDefine;
		m_bParsingDefine = true;

		auto Cleanup = g_OnScopeExit / [&]
			{
				m_bParsingDefine = bPreviousParsingDefine;
			}
		;

		CJsonSorted Define;
		auto CleanupBinary = f_EnableBinaryOperators();
		NJson::fg_ParseJsonValue(Define, pParse, *this);

		o_pParse = pParse;

		CJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_Define;
		Object[gc_ConstString_Define] = fg_Move(Define);
		if (_bLegacy)
			Object[gc_ConstString_Legacy] = _bLegacy;

		return Return;
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseFunctionType(uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;

		bool bPreviousParsingDefine = m_bParsingDefine;
		m_bParsingDefine = true;

		auto Cleanup = g_OnScopeExit / [&]
			{
				m_bParsingDefine = bPreviousParsingDefine;
			}
		;

		fg_ParseWhiteSpace(pParse);
		if (*pParse != '(')
			f_ThrowError("Expected (", pParse);
		++pParse;
		fg_ParseWhiteSpace(pParse);

		TCVector<CJsonSorted> Parameters;
		while (*pParse)
		{
			if (*pParse == ')')
			{
				++pParse;
				break;
			}

			CJsonSorted ParameterType;
			auto Cleanup = f_EnableBinaryOperators();
			NJson::fg_ParseJsonValue(ParameterType, pParse, *this);

			fg_ParseWhiteSpace(pParse);

			bool bIsEllipsis = false;
			auto pStartEllipsis = pParse;
			if (fg_StrStartsWith(pParse, gc_ConstString_Symbol_Ellipsis.m_String))
			{
				bIsEllipsis = true;
				pParse += 3;
			}

			fg_ParseWhiteSpace(pParse);

			CStr Name = f_ParseIdentifier(pParse);
			fg_ParseWhiteSpace(pParse);

			if (*pParse == '=')
			{
				++pParse;
				fg_ParseWhiteSpace(pParse);

				auto OldParameterType = fg_Move(ParameterType);
				DMibMovedFromValid(ParameterType);

				auto &Object = ParameterType.f_Object();
				Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
				Object[CEJsonConstStrings::mc_Value] = f_ParseDefaulted(pParse, fg_Move(OldParameterType));
			}

			auto &Object = Parameters.f_Insert().f_Object();
			Object[gc_ConstString_Type] = fg_Move(ParameterType);
			Object[gc_ConstString_Name] = fg_Move(Name);
			Object[gc_ConstString_Ellipsis] = bIsEllipsis;

			if (*pParse == ',')
			{
				if (bIsEllipsis)
					f_ThrowError("Ellipsis needs to be last parameter", pStartEllipsis);
				++pParse;
				fg_ParseWhiteSpace(pParse);
				continue;
			}
			else if (*pParse != ')')
				f_ThrowError("Expected , or )", pParse);
		}

		fg_ParseWhiteSpaceNoLines(pParse);

		if (!*pParse || fg_CharIsNewLine(*pParse))
			f_ThrowError("Missing return type", pParse);

		CJsonSorted ReturnType;
		auto CleanupBinary = f_EnableBinaryOperators();
		NJson::fg_ParseJsonValue(ReturnType, pParse, *this);

		o_pParse = pParse;

		CJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_FunctionType;
		Object[gc_ConstString_ReturnType] = fg_Move(ReturnType);
		Object[gc_ConstString_Parameters] = fg_Move(Parameters);
		return Return;
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseDefaultType(CStr const &_Identifier)
	{
		CJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_DefaultType;
		Object[gc_ConstString_TypeName] = _Identifier;
		return Return;
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseType(uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;

		fg_ParseWhiteSpace(pParse);
		if (*pParse != '(')
			f_ThrowError("Expected (", pParse);
		++pParse;

		CStr TypeIdentifier = f_ParseIdentifierWithNamespace(pParse);

		fg_ParseWhiteSpace(pParse);
		if (*pParse != ')')
			f_ThrowError("Expected )", pParse);
		++pParse;

		o_pParse = pParse;

		CJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_Type;
		Object[gc_ConstString_TypeName] = fg_Move(TypeIdentifier);
		return Return;
	}

	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParsePostDefine(uch8 const *&o_pParse, NEncoding::CJsonSorted &o_Value)
	{
		auto pParse = o_pParse;
		fg_ParseWhiteSpaceNoLines(pParse);

		if (fg_StrStartsWith(o_pParse, "//") || fg_StrStartsWith(o_pParse, "/*"))
			return;

		if (*pParse == '?')
		{
			++pParse;
			fg_ParseWhiteSpaceNoLines(pParse);

			auto InnerValue = fg_Move(o_Value);

			auto &Object = o_Value.f_Object();
			Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;

			auto &ValueObject = Object[CEJsonConstStrings::mc_Value].f_Object();
			ValueObject[gc_ConstString_Type] = gc_ConstString_Optional;
			ValueObject[gc_ConstString_Param] = fg_Move(InnerValue);

			o_pParse = pParse;
		}

		if (*pParse == '=')
		{
			++pParse;

			auto InnerValue = fg_Move(o_Value);
			auto &Object = o_Value.f_Object();
			Object[CEJsonConstStrings::mc_Type] = gc_ConstString_BuildSystemToken;
			Object[CEJsonConstStrings::mc_Value] = f_ParseDefaulted(pParse, fg_Move(InnerValue));

			o_pParse = pParse;
		}
	}

	NEncoding::CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseDefaulted(uch8 const *&o_pParse, NEncoding::CJsonSorted &&_Type)
	{
		auto pParse = o_pParse;

		bool bPreviousParsingDefine = m_bParsingDefine;
		m_bParsingDefine = false;

		auto Cleanup = g_OnScopeExit / [&]
			{
				m_bParsingDefine = bPreviousParsingDefine;
			}
		;

		fg_ParseWhiteSpace(pParse);

		CJsonSorted DefaultValue;
		auto CleanupBinary = f_EnableBinaryOperators();
		NJson::fg_ParseJsonValue(DefaultValue, pParse, *this);

		o_pParse = pParse;

		CJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_TypeDefaulted;
		Object[gc_ConstString_InnerType] = fg_Move(_Type);
		Object[gc_ConstString_DefaultValue] = fg_Move(DefaultValue);
		return Return;
	}

	CJsonSorted TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::f_ParseOneOf(uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;

		fg_ParseWhiteSpace(pParse);
		if (*pParse != '(')
			f_ThrowError("Expected (", pParse);
		++pParse;

		bool bEndedParen = false;
		bool bLastTokenValid = true;

		CJsonSorted OneOfList = EJsonType_Array;

		while (*pParse)
		{
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ')')
			{
				if (!bLastTokenValid)
					f_ThrowError("Missing param", pParse);
				++pParse;
				bEndedParen = true;
				break;
			}
			else if (*pParse == ',')
			{
				if (!bLastTokenValid)
					f_ThrowError(", is not valid here", pParse);
				++pParse;
			}
			else
			{
				CJsonSorted Param;
				auto Cleanup = f_EnableBinaryOperators();
				NJson::fg_ParseJsonValue(Param, pParse, *this);
				OneOfList.f_Insert(fg_Move(Param));
				bLastTokenValid = true;
				continue;
			}

			bLastTokenValid = false;
		}

		if (!bEndedParen)
			f_ThrowError("Could not find matching end )", o_pParse);

		o_pParse = pParse;

		CJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_OneOf;
		Object[gc_ConstString_OneOfList] = fg_Move(OneOfList);
		return Return;
	}

	namespace
	{
		template <typename tf_CStr>
		void fg_GenerateAccessors(tf_CStr &o_String, TCVector<CJsonSorted> const &_Accessors, umint _Depth)
		{
			bool bFirst = true;
			for (auto &Accessor : _Accessors)
			{
				if (!Accessor.f_IsObject())
					DMibError("Invalid accessor");

				auto pOptionalChaining = Accessor.f_GetMember(gc_ConstString_OptionalChaining, EJsonType_Boolean);
				if (!pOptionalChaining)
					DMibError("Accessor does not have valid OptionalChaining member");

				auto pAccessor = Accessor.f_GetMember(gc_ConstString_Accessor);
				if (!pAccessor)
					DMibError("Accessor does not have valid Accessor member");

				bool bAddedChainging = false;
				if (pOptionalChaining->f_Boolean())
				{
					bAddedChainging = true;
					o_String += "?.";
				}

				auto &Ident = *pAccessor;

				if (Ident.f_IsString())
				{
					if (!bFirst && !bAddedChainging)
						o_String += ".";
					o_String += Ident.f_String();
				}
				else if (Ident.f_IsObject())
				{
					auto pType = Ident.f_GetMember(gc_ConstString_Type, EJsonType_String);
					if (!pType)
						DMibError("Accessor does not have valid Type member");

					if (pType->f_String() == gc_ConstString_Subscript.m_String)
					{
						auto pArgument = Ident.f_GetMember(gc_ConstString_Argument);
						if (!pArgument)
							DMibError("Accessor does not have valid Argument member");
						o_String += "[";
						NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pArgument, _Depth, "\t", gc_BuildSystemJsonParseFlags);
						o_String += "]";
					}
					else if (pType->f_String() == gc_ConstString_Expression.m_String)
					{
						auto pParam = Ident.f_GetMember(gc_ConstString_Param, EJsonType_Object);
						if (!pParam)
							DMibError("Expression token does not have valid Param member");

						if (!bFirst && !bAddedChainging)
							o_String += ".";
						o_String += "@(";
						NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pParam, _Depth, "\t", gc_BuildSystemJsonParseFlags);
						o_String += ")";
					}
					else
						DMibError("Unknown type for accessor: {}"_f << pType->f_String());
				}
				else
					DMibError("Invalid accessor type");
				bFirst = false;
			}
		}
	}

	template <typename tf_CStr>
	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateEvalString
		(
			tf_CStr &o_String
			, NEncoding::CJsonSorted const &_Token
			, umint _Depth
		)
	{
		o_String += "`";
		for (auto &Token : _Token.f_Array())
		{
			auto pType = Token.f_GetMember(gc_ConstString_Type, EJsonType_String);
			if (!pType)
				DMibError("Token does not have valid Type member");

			auto &TokenType = pType->f_String();
			if (TokenType == gc_ConstString_String.m_String)
			{
				auto pValue = Token.f_GetMember(gc_ConstString_Value, EJsonType_String);
				if (!pValue)
					DMibError("String token does not have valid Value member");
				fg_GenerateJsonString<'`', CJsonParseContext, false>(o_String, pValue->f_String().f_Replace("@", "@@"));
			}
			else
			{
				o_String += "@";
				CJsonParseContext::fs_GenerateExpression(o_String, Token, false, _Depth);
			}
		}
		o_String += "`";
	}

	template <typename tf_CStr>
	void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateExpression
		(
			tf_CStr &o_String
			, CJsonSorted const &_Token
			, bool _bQuoteStrings
			, umint _Depth
		)
	{
		if (!_Token.f_IsObject())
			DMibError("Token is not object");

		auto pType = _Token.f_GetMember(gc_ConstString_Type, EJsonType_String);
		if (!pType)
			DMibError("Token does not have valid Type member");

		auto &TokenType = pType->f_String();

		if (TokenType == gc_ConstString_String.m_String)
		{
			auto pValue = _Token.f_GetMember(gc_ConstString_Value, EJsonType_String);
			if (!pValue)
				DMibError("String token does not have valid Value member");
			if (_bQuoteStrings)
				fs_GenerateString<CJsonParseContext>(o_String, pValue->f_String());
			else
				o_String += pValue->f_String();
		}
		else if (TokenType == gc_ConstString_EvalString.m_String)
		{
			auto pValue = _Token.f_GetMember(gc_ConstString_Value, EJsonType_Array);
			if (!pValue)
				DMibError("Eval string token does not have valid Value member");

			CJsonParseContext::fs_GenerateEvalString(o_String, *pValue, _Depth);
		}
		else if (TokenType == gc_ConstString_WildcardString.m_String)
		{
			if (auto pValue = _Token.f_GetMember(gc_ConstString_Value, EJsonType_String))
			{
				o_String += "~";
				if (pValue->f_String().f_GetUserData() == EJsonStringType_SingleQuote)
					fg_GenerateJsonString<'\'', CJsonParseContext>(o_String, pValue->f_String());
				else
					fg_GenerateJsonString<'"', CJsonParseContext>(o_String, pValue->f_String());
			}
			else if (auto pValue = _Token.f_GetMember(gc_ConstString_Value, EJsonType_Object))
			{
				o_String += "~";
				fs_GenerateExpression(o_String, *pValue, _bQuoteStrings, _Depth);
			}
			else
				DMibError("Wildcard string token does not have valid Value member");
		}
		else if (TokenType == gc_ConstString_Operator.m_String)
		{
			auto pOperator = _Token.f_GetMember(gc_ConstString_Operator, EJsonType_String);
			if (!pOperator)
				DMibError("Operator token does not have valid Operator member");

			auto pRight = _Token.f_GetMember(gc_ConstString_Right);
			if (!pRight)
				DMibError("Operator token does not have valid Right member");

			o_String += pOperator->f_String();
			o_String += " ";
			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_RootValue.m_String)
		{
			auto pRight = _Token.f_GetMember(gc_ConstString_Value);
			if (!pRight)
				DMibError("RootValue token does not have valid Value member");

			auto pAccessors = _Token.f_GetMember(gc_ConstString_Accessors, EJsonType_Array);
			if (!pAccessors)
				DMibError("RootValue token does not have valid Accessors member");

			o_String += "#<";
			fg_GenerateAccessors(o_String, pAccessors->f_Array(), _Depth);
			o_String += "> ";

			 NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_Define.m_String)
		{
			auto pDefine = _Token.f_GetMember(gc_ConstString_Define);
			if (!pDefine)
				DMibError("Define token does not have valid Define member");

			bool bLegacy = false;
			if (auto *pValue = _Token.f_GetMember(gc_ConstString_Legacy))
				bLegacy = pValue->f_Boolean();

			if (bLegacy)
				o_String += "define ";
			else
				o_String += ": ";

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pDefine, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_DefaultType.m_String)
		{
			auto pTypeName = _Token.f_GetMember(gc_ConstString_TypeName, EJsonType_String);
			if (!pTypeName)
				DMibError("DefaultType token does not have valid TypeName member");

			o_String += pTypeName->f_String();
		}
		else if (TokenType == gc_ConstString_Type.m_String)
		{
			auto pTypeName = _Token.f_GetMember(gc_ConstString_TypeName, EJsonType_String);
			if (!pTypeName)
				DMibError("Type token does not have valid TypeName member");

			o_String += "type(";
			o_String += pTypeName->f_String();
			o_String += ")";
		}
		else if (TokenType == gc_ConstString_Optional.m_String)
		{
			auto pParam = _Token.f_GetMember(gc_ConstString_Param);
			if (!pParam)
				DMibError("Optional token does not have valid Param member");

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pParam, _Depth, "\t", gc_BuildSystemJsonParseFlags);
			o_String += gc_ConstString_Symbol_Optional.m_String;
		}
		else if (TokenType == gc_ConstString_FunctionType.m_String)
		{
			auto pReturnType = _Token.f_GetMember(gc_ConstString_ReturnType);
			if (!pReturnType)
				DMibError("FunctionType token does not have valid ReturnType member");

			auto pParameters = _Token.f_GetMember(gc_ConstString_Parameters, EJsonType_Array);
			if (!pParameters)
				DMibError("FunctionType token does not have valid Parameters member");

			o_String += "function(";

			bool bFirst = true;
			for (auto &Parameter : pParameters->f_Array())
			{
				if (bFirst)
					bFirst = false;
				else
					o_String += ", ";

				auto pType = Parameter.f_GetMember(gc_ConstString_Type);
				if (!pType)
					DMibError("FunctionType parameter does not have valid Type member");

				auto pName = Parameter.f_GetMember(gc_ConstString_Name, EJsonType_String);
				if (!pName)
					DMibError("FunctionType parameter does not have valid Name member");

				auto pEllipsis = Parameter.f_GetMember(gc_ConstString_Ellipsis, EJsonType_Boolean);
				if (!pEllipsis)
					DMibError("FunctionType parameter does not have valid Ellipsis member");

				CJsonSorted const *pDefaultValue = nullptr;
				if (pType->f_IsObject())
				{
					if (auto pUserType = pType->f_GetMember(CEJsonConstStrings::mc_Type, EJsonType_String))
					{
						if (pUserType->f_String() == gc_ConstString_BuildSystemToken.m_String)
						{
							if (auto pValue = pType->f_GetMember(CEJsonConstStrings::mc_Value, EJsonType_Object))
							{
								if (auto pTokenType = pValue->f_GetMember(gc_ConstString_Type, EJsonType_String))
								{
									if (pTokenType->f_String() == gc_ConstString_TypeDefaulted.m_String)
									{
										if (auto pInnerType = pValue->f_GetMember(gc_ConstString_InnerType))
											pType = pInnerType;
										else
											DMibError("TypeDefaulted token does not have valid InnerType member");

										pDefaultValue = pValue->f_GetMember(gc_ConstString_DefaultValue);
										if (!pDefaultValue)
											DMibError("TypeDefaulted token does not have valid InnerType member");
									}
								}
							}
						}
					}
				}

				NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pType, _Depth, "\t", gc_BuildSystemJsonParseFlags);

				if (pEllipsis->f_Boolean())
					o_String += gc_ConstString_Symbol_Ellipsis.m_String;

				{
					auto Committed = o_String.f_Commit();
					Committed.m_String += typename tf_CStr::CString::CFormat(" {}") << pName->f_String();
				}

				if (pDefaultValue)
				{
					o_String += " = ";
					NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pDefaultValue, _Depth, "\t", gc_BuildSystemJsonParseFlags);
				}
			}

			o_String += ") ";

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pReturnType, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_OneOf.m_String)
		{
			auto pOneOfList = _Token.f_GetMember(gc_ConstString_OneOfList, EJsonType_Array);
			if (!pOneOfList)
				DMibError("OneOf token does not have valid OneOfList member");

			o_String += "one_of(";

			bool bFirst = true;
			for (auto iOneOf = pOneOfList->f_Array().f_GetIterator(); iOneOf; ++iOneOf)
			{
				if (bFirst)
					bFirst = false;
				else
					o_String += ", ";

				NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *iOneOf, _Depth, "\t", gc_BuildSystemJsonParseFlags);
				o_String += "";
			}

			o_String += ")";
		}
		else if (TokenType == gc_ConstString_Expression.m_String || TokenType == gc_ConstString_AppendExpression.m_String)
		{
			auto pParam = _Token.f_GetMember(gc_ConstString_Param);
			if (!pParam)
				DMibError("Expression token does not have valid Param member");

			auto pParen = _Token.f_GetMember(gc_ConstString_Paren);
			if (!pParen)
				DMibError("Expression token does not have valid Paren member");

			if (pParen->f_Boolean())
				o_String += "(";

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pParam, _Depth, "\t", gc_BuildSystemJsonParseFlags);

			if (pParen->f_Boolean())
				o_String += ")";

			if (TokenType == gc_ConstString_AppendExpression.m_String)
				o_String += gc_ConstString_Symbol_Ellipsis.m_String;
		}
		else if (TokenType == gc_ConstString_Ternary.m_String)
		{
			auto pConditional = _Token.f_GetMember(gc_ConstString_Conditional);
			if (!pConditional)
				DMibError("Ternary token does not have valid Conditional member");

			auto pLeft = _Token.f_GetMember(gc_ConstString_Left);
			if (!pLeft)
				DMibError("Ternary token does not have valid Left member");

			auto pRight = _Token.f_GetMember(gc_ConstString_Right);
			if (!pRight)
				DMibError("Ternary token does not have valid Right member");

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pConditional, _Depth, "\t", gc_BuildSystemJsonParseFlags);

			o_String += " ? ";

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pLeft, _Depth, "\t", gc_BuildSystemJsonParseFlags);

			o_String += " : ";

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_IdentifierReference.m_String)
		{
			auto pIdentifier = _Token.f_GetMember(gc_ConstString_Identifier);
			if (!pIdentifier)
				DMibError("IdentifierReference token does not have valid Identifier member");

			o_String += gc_ConstString_Symbol_MakeReference.m_String;

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pIdentifier, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_Identifier.m_String)
		{
			auto pEntityType = _Token.f_GetMember(gc_ConstString_EntityType, EJsonType_String);
			if (!pEntityType)
				DMibError("Identifier token does not have valid EntityType member");

			auto pPropertyType = _Token.f_GetMember(gc_ConstString_PropertyType);
			if (!pPropertyType)
				DMibError("Identifier token does not have valid PropertyType member");

			auto pName = _Token.f_GetMember(gc_ConstString_Name);
			if (!pName)
				DMibError("Identifier token does not have valid Name member");

			if (pEntityType->f_String())
			{
				o_String += pEntityType->f_String();
				o_String += ":";
			}

			if (pPropertyType->f_IsString())
			{
				if (pPropertyType->f_String())
				{
					o_String += pPropertyType->f_String();
					o_String += ".";
				}
			}
			else if (pPropertyType->f_IsArray())
			{
				for (auto &Token : pPropertyType->f_Array())
				{
					if (Token.f_IsString())
						fs_GenerateIdentifier(o_String, Token.f_String());
					else
					{
						o_String += "@";
						CJsonParseContext::fs_GenerateExpression(o_String, Token, false, _Depth);
					}
				}
				o_String += ".";
			}
			else
				DMibError("Identifier token does not have valid PropertyType member");

			if (pName->f_IsString())
				fs_GenerateIdentifier(o_String, pName->f_String());
			else if (pName->f_IsArray())
			{
				for (auto &Token : pName->f_Array())
				{
					if (Token.f_IsString())
						fs_GenerateIdentifier(o_String, Token.f_String());
					else
					{
						o_String += "@";
						CJsonParseContext::fs_GenerateExpression(o_String, Token, false, _Depth);
					}
				}
			}
			else
				DMibError("Identifier token does not have valid Name member");

		}
		else if (TokenType == gc_ConstString_JsonAccessor.m_String)
		{
			auto pParam = _Token.f_GetMember(gc_ConstString_Param);
			if (!pParam)
				DMibError("JsonAccessor token does not have valid Param member");

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pParam, _Depth, "\t", gc_BuildSystemJsonParseFlags);

			auto pAccessors = _Token.f_GetMember(gc_ConstString_Accessors, EJsonType_Array);
			if (!pAccessors)
				DMibError("JsonAccessor token does not have valid Accessors member");

			o_String += "<";
			fg_GenerateAccessors(o_String, pAccessors->f_Array(), _Depth);
			o_String += ">";
		}
		else if (TokenType == gc_ConstString_TypeDefaulted.m_String)
		{
			auto pInnerType = _Token.f_GetMember(gc_ConstString_InnerType);
			if (!pInnerType)
				DMibError("TypeDefaulted token does not have valid InnerType member");

			auto pDefaultValue = _Token.f_GetMember(gc_ConstString_DefaultValue);
			if (!pDefaultValue)
				DMibError("TypeDefaulted token does not have valid DefaultValue member");

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pInnerType, _Depth, "\t", gc_BuildSystemJsonParseFlags);

			o_String += " = ";
			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pDefaultValue, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_PostFunction.m_String || TokenType == gc_ConstString_Function.m_String)
		{
			auto pName = _Token.f_GetMember(gc_ConstString_Name, EJsonType_String);
			if (!pName)
				DMibError("Function token does not have valid Name member");

			auto pPropertyType = _Token.f_GetMember(gc_ConstString_PropertyType, EJsonType_String);
			if (!pPropertyType)
				DMibError("Function token does not have valid PropertyType member");

			auto pParams = _Token.f_GetMember(gc_ConstString_Params, EJsonType_Array);
			if (!pParams)
				DMibError("Function token does not have valid Params member");

			auto &Params = pParams->f_Array();

			auto iParam = Params.f_GetIterator();

			if (TokenType == gc_ConstString_PostFunction.m_String)
			{
				if (!iParam)
					DMibError("Invalid number of parmeters in post function call");

				NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *iParam, _Depth, "\t", gc_BuildSystemJsonParseFlags);
				++iParam;
				o_String += gc_ConstString_Symbol_AccessObject.m_String;
			}

			if (pPropertyType->f_String())
			{
				o_String += pPropertyType->f_String();
				o_String += ".";
			}

			o_String += pName->f_String();
			o_String += "(";

			bool bFirst = true;
			for (; iParam; ++iParam)
			{
				if (bFirst)
					bFirst = false;
				else
					o_String += ", ";

				NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *iParam, _Depth, "\t", gc_BuildSystemJsonParseFlags);
			}

			o_String += ")";
		}
		else if (TokenType == gc_ConstString_KeyPrefixOperator.m_String)
		{
			auto pOperator = _Token.f_GetMember(gc_ConstString_Operator);
			if (!pOperator)
				DMibError("PrefixOperator token does not have valid Operator member");

			auto pRight = _Token.f_GetMember(gc_ConstString_Right);
			if (!pRight)
				DMibError("PrefixOperator token does not have valid Right member");

			o_String += pOperator->f_String();

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_Namespace.m_String)
			o_String += gc_ConstString_namespace.m_String;
		else if (TokenType == gc_ConstString_KeyLogicalOperator.m_String)
		{
			auto pOperator = _Token.f_GetMember(gc_ConstString_Operator);
			if (!pOperator)
				DMibError("KeyLogicalOperator token does not have valid Operator member");

			o_String += pOperator->f_String();
		}
		else if (TokenType == gc_ConstString_BinaryOperator.m_String)
		{
			auto pOperator = _Token.f_GetMember(gc_ConstString_Operator);
			if (!pOperator)
				DMibError("BinaryOperator token does not have valid Operator member");

			auto pLeft = _Token.f_GetMember(gc_ConstString_Left);
			if (!pLeft)
				DMibError("BinaryOperator token does not have valid Left member");

			auto pRight = _Token.f_GetMember(gc_ConstString_Right);
			if (!pRight)
				DMibError("BinaryOperator token does not have valid Right member");

			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pLeft, _Depth, "\t", gc_BuildSystemJsonParseFlags);
			o_String += " ";
			o_String += pOperator->f_String();
			o_String += " ";
			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else if (TokenType == gc_ConstString_PrefixOperator.m_String)
		{
			auto pOperator = _Token.f_GetMember(gc_ConstString_Operator);
			if (!pOperator)
				DMibError("PrefixOperator token does not have valid Operator member");

			auto pRight = _Token.f_GetMember(gc_ConstString_Right);
			if (!pRight)
				DMibError("PrefixOperator token does not have valid Right member");

			o_String += pOperator->f_String();
			NJson::fg_GenerateJsonValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJsonParseFlags);
		}
		else
			DMibError("Token Type '{}' not supported"_f << TokenType);
	}

	template void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateExpression
		(
			CStr::CAppender &o_String
			, CJsonSorted const &_Token
			, bool _bQuoteStrings
			, umint _Depth
		)
	;

	template void TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext::fs_GenerateExpression
		(
			CStrNonTracked::CAppender &o_String
			, CJsonSorted const &_Token
			, bool _bQuoteStrings
			, umint _Depth
		)
	;
#endif
}
