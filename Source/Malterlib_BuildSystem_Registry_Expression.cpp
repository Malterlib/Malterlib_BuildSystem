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

	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContextCatureStringMap::f_MapCharacter(mint _iDestination, mint _iSource, mint _nChars)
	{
		m_StringMap.f_SetAtLeastLen(_iDestination + _nChars, 0);
		for (mint i = 0; i < _nChars; ++i)
			m_StringMap[_iDestination + i] = _iSource + i;
	}

	CParseLocation TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContextCatureStringMap::f_GetLocation(uch8 const *_pParse) const
	{
		if (!m_pOriginalStartParse)
			return m_pOriginalParseContext->f_GetLocation(_pParse);

		auto pParseInOriginal = m_pOriginalStartParse + m_StringMap[_pParse - m_pStartParse];
		using namespace NStr;
		CParseLocation Location;
		Location.m_File = m_FileName;
		Location.m_Character = pParseInOriginal - m_pOriginalStartParse;

		auto *pParse = m_pOriginalStartParse;
		mint Line = 1;
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

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseEvalStringToken(uch8 const *&o_pParse)
	{
		auto *pParse = o_pParse;
		CStr ParsedString;
		auto pParseStart = pParse;

		CJSONParseContextCatureStringMap ParseContext;
		ParseContext.m_pOriginalParseContext = this;
		(CJSONParseContext &)ParseContext = *this;

		CJSON ParsedEvalString;
		auto &TokenArray = ParsedEvalString.f_Array();

		auto fAddStringToken = [&]
			{
				if (ParsedString.f_IsEmpty())
					return;
				TokenArray.f_Insert
					(
						CJSON
						{
							"Type"__= "String"
							, "Value"__= fg_Move(ParsedString)
						}
					)
				;
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
					!fg_ParseJSONString<'`', NEncoding::NJSON::EParseJSONStringFlag_AllowMultiLine>
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
					!fg_ParseJSONString<'`', NEncoding::NJSON::EParseJSONStringFlag_AllowMultiLine | NEncoding::NJSON::EParseJSONStringFlag_NoQuotes>
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

		return
			{
				"Type"__= "EvalString"
				, "Value"__= fg_Move(ParsedEvalString)
			}
		;
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseWildcardStringToken(uch8 const *&o_pParse)
	{
		auto *pParse = o_pParse;
		auto pParseStart = pParse;

		CJSONParseContextCatureStringMap ParseContext;
		ParseContext.m_pOriginalParseContext = this;
		(CJSONParseContext &)ParseContext = *this;

		CJSON Value;
		if (*pParse == '"')
		{
			CStr ParsedString;
			if (!fg_ParseJSONString<'"', NEncoding::NJSON::EParseJSONStringFlag_AllowMultiLine>(ParsedString, pParse, ParseContext))
				f_ThrowError("End of string character \" not found", pParseStart);
			Value = ParsedString;
		}
		else if (*pParse == '\'')
		{
			CStr ParsedString;
			if (!fg_ParseJSONString<'\'', NEncoding::NJSON::EParseJSONStringFlag_AllowMultiLine>(ParsedString, pParse, ParseContext))
				f_ThrowError("End of string character ' not found", pParseStart);
			Value = ParsedString;
		}
		else if (*pParse == '`')
			Value = f_ParseEvalStringToken(pParse);

		o_pParse = pParse;

		return
			{
				"Type"__= "WildcardString"
				, "Value"__= fg_Move(Value)
			}
		;
	}

	namespace
	{
		struct CReservedWords
		{
			TCSet<CStr> m_Words
				{
					""
					, "true"
					, "false"
					, "null"
					, "define"
					, "function"
					, "type"
					, "one_of"
					, "string"
					, "int"
					, "float"
					, "bool"
					, "date"
					, "binary"
					, "any"
					, "void"
				}
			;
		};

		TCAggregate<CReservedWords> g_ReservedWords = {DAggregateInit};
	}

	bool TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::fs_IsReservedWord(ch8 const *_pIdentifier)
	{
		return (*g_ReservedWords).m_Words.f_Exists(_pIdentifier);
	}

	template <typename tf_CStr, typename tf_CStrIdentifier>
	void TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::fs_GenerateIdentifier(tf_CStr &o_String, tf_CStrIdentifier const &_Identifier)
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
				o_String.f_AddChar('\\');

			o_String.f_AddChar(*pParse);
			++pParse;
		}
		
		while (*pParse)
		{
			if (!fs_CharIsIdentifier(*pParse) || *pParse == '\\')
				o_String.f_AddChar('\\');

			o_String.f_AddChar(*pParse);
			++pParse;
		}
	}

	bool TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::fs_CharIsStartIdentifier(uch8 _Char)
	{
		return fg_CharIsAnsiAlphabetical(_Char) || _Char == '_' || _Char == '\\';
	}

	bool TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::fs_CharIsIdentifier(uch8 _Char)
	{
		return fg_CharIsAnsiAlphabetical(_Char) || _Char == '_' || fg_CharIsNumber(_Char) || _Char == '\\';
	}

	bool TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::fs_CharIsIdentifierOrExpression(uch8 _Char)
	{
		return fs_CharIsIdentifier(_Char) || _Char == '@';
	}

	CStr TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseIdentifierLax(uch8 const *&o_pParse)
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

	CStr TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseIdentifier(uch8 const * &o_pParse)
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

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseJSONAccessor(uch8 const *&o_pParse, NEncoding::CJSON &&_Param)
	{
		auto pParse = o_pParse;
		DMibRequire(*pParse == '<');
		++pParse;

		CEJSON Return;
		auto &ReturnUserType = Return.f_UserType();
		ReturnUserType.m_Type = "BuildSystemToken";
		ReturnUserType.m_Value =
			{
				"Type"__= "JSONAccessor"
				, "Param"__= fg_Move(_Param)
			}
		;

		auto &AccessorsArray = ReturnUserType.m_Value["Accessors"].f_Array();

		bool bEndedParen = false;

		while (*pParse)
		{
			if (*pParse == '\'')
			{
				CJSONParseContextCatureStringMap ParseContext;
				ParseContext.m_pOriginalParseContext = this;
				(CJSONParseContext &)ParseContext = *this;

				CStr ParsedString;
				auto pParseStart = pParse;
				if (!fg_ParseJSONString<'\'', NEncoding::NJSON::EParseJSONStringFlag_AllowMultiLine>(ParsedString, pParse, ParseContext))
					f_ThrowError("End of string character ' not found", pParseStart);
				AccessorsArray.f_Insert(fg_Move(ParsedString));
			}
			else if (*pParse == '"')
			{
				CJSONParseContextCatureStringMap ParseContext;
				ParseContext.m_pOriginalParseContext = this;
				(CJSONParseContext &)ParseContext = *this;

				CStr ParsedString;
				auto pParseStart = pParse;
				if (!fg_ParseJSONString<'"', NEncoding::NJSON::EParseJSONStringFlag_AllowMultiLine>(ParsedString, pParse, ParseContext))
					f_ThrowError("End of string character \" not found", pParseStart);
				AccessorsArray.f_Insert(fg_Move(ParsedString));
			}
			else if (fs_CharIsStartIdentifier(*pParse))
				AccessorsArray.f_Insert(f_ParseIdentifier(pParse));
			else if (*pParse == '@')
			{
				++pParse;
				AccessorsArray.f_Insert(f_ParseExpression(pParse, EParseExpressionFlag_None));
			}
			else if (*pParse == '.')
				++pParse;
			else if (*pParse == '[')
			{
				++pParse;

				auto pParseStart = pParse;
				CJSON Param;
				auto Cleanup = f_EnableBinaryOperators();
				NJSON::fg_ParseJSONValue(Param, pParse, *this);

				bool bIsValid = false;
				if (Param.f_IsInteger())
					bIsValid = true;
 				else if (Param.f_IsObject() && Param.f_GetMember("$type", EJSONType_String) && Param.f_GetMember("$type")->f_String() == "BuildSystemToken")
				{
					if (auto pValue = Param.f_GetMember("$value", EJSONType_Object))
					{
						if (auto pType = pValue->f_GetMember("Type", EJSONType_String); pType && pType->f_String() == "Expression")
							bIsValid = true;
					}
				}

				if (!bIsValid)
					f_ThrowError("Subscript needs an integer or @(Identifier) expression", pParseStart);

				AccessorsArray.f_Insert
					(
						CJSON
						{
							"Type"__= "Subscript"
							, "Argument"__= fg_Move(Param)
						}
					)
				;

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

		return fg_Move(Return).f_ToJSON();
	}

	CEJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseIdentifierTokenEJSON(uch8 const *&o_pParse)
	{
		auto *pParse = o_pParse;

		CStr EntityType;
		CStr PropertyType;
		CStr Name;

		if (*pParse != '@')
		{
			Name = f_ParseIdentifier(pParse);

			if (*pParse == ':')
			{
				++pParse;
				EntityType = fg_Move(Name);
				Name = f_ParseIdentifier(pParse);
			}

			if (*pParse == '.' && !fg_StrStartsWith(pParse, "..."))
			{
				++pParse;
				PropertyType = fg_Move(Name);
				Name = f_ParseIdentifier(pParse);
			}
		}

		CJSON NameOrTokenArray;

		if (*pParse == '@')
		{
			auto &TokenArray = NameOrTokenArray.f_Array();
			if (Name)
				TokenArray.f_Insert(fg_Move(Name));

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
		}
		else
			NameOrTokenArray = fg_Move(Name);

		o_pParse = pParse;

		CEJSON Return;
		auto &ReturnUserType = Return.f_UserType();
		ReturnUserType.m_Type = "BuildSystemToken";
		ReturnUserType.m_Value =
			{
				"Type"__= "Identifier"
				, "EntityType"__= fg_Move(EntityType)
				, "PropertyType"__= fg_Move(PropertyType)
				, "Name"__= fg_Move(NameOrTokenArray)
			}
		;

		return Return;
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseIdentifierToken(uch8 const * &o_pParse)
	{
		return f_ParseIdentifierTokenEJSON(o_pParse).f_ToJSON();
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseFunctionToken
		(
			uch8 const *&o_pParse
			, CJSON *_pFirstParam
			, CStr const &_FunctionName
			, CStr const &_PropertyType
		)
	{
		auto *pParse = o_pParse;
		if (*pParse != '(')
			f_ThrowError("Function must be enclosed in ()", pParse);

		++pParse;

		CEJSON Return;
		auto &ReturnUserType = Return.f_UserType();
		ReturnUserType.m_Type = "BuildSystemToken";
		ReturnUserType.m_Value =
			{
				"Type"__= _pFirstParam ? "PostFunction" : "Function"
				, "Name"__= _FunctionName
				, "PropertyType"__= _PropertyType
			}
		;

		auto &FunctionParams = ReturnUserType.m_Value["Params"].f_Array();

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
				CJSON Param;
				auto Cleanup = f_EnableBinaryOperators();
				NJSON::fg_ParseJSONValue(Param, pParse, *this);
				FunctionParams.f_Insert(fg_Move(Param));
				bLastTokenValid = true;
				continue;
			}

			bLastTokenValid = false;
		}

		if (!bEndedParen)
			f_ThrowError("Could not find matching end )", o_pParse);

		o_pParse = pParse;

		return fg_Move(Return).f_ToJSON();
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseExpression(uch8 const *&o_pParse, EParseExpressionFlag _Flags, CJSON *_pFirstParam)
	{
		auto *pParse = o_pParse;
		bool bUseParentheses = !(_Flags & EParseExpressionFlag_NoParentheses);
		if (bUseParentheses)
		{
			if (*pParse != '(')
				f_ThrowError("Expression must be enclosed in ()", pParse);
			++pParse;
		}

		CJSON Return;

		auto &Param = Return["Param"];
		Return["Paren"] = bUseParentheses;

		if (_pFirstParam)
			Param = fg_Move(*_pFirstParam);

		bool bEndedParen = false;

		auto fCheckParam = [&]
			{
				if (Param.f_IsValid())
					f_ThrowError("Expression parameter already specified", pParse);
			}
		;

		mint nParsed = 0;

		while (*pParse && !fg_CharIsNewLine(*pParse))
		{
			++nParsed;

			if (bUseParentheses)
				fg_ParseWhiteSpace(pParse);
			else
			{
				auto *pTestParse = pParse;
				fg_ParseWhiteSpaceNoLines(pTestParse);

				if
					(
						(fg_CharIsWhiteSpace(*pParse) && m_ParseDepth == 1 && nParsed > 1)
						|| *pTestParse == '}'
						|| *pTestParse == ']'
						|| *pTestParse == ','
						|| *pTestParse == ':'
						|| fg_StrStartsWith(pTestParse, "...")
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

				if ((_Flags & EParseExpressionFlag_SupportAppend) && fg_StrStartsWith(pParse, "..."))
				{
					pParse += 3;
					Return["Type"] = "AppendExpression";
				}
				else
					Return["Type"] = "Expression";

				break;
			}
			else if (fs_CharIsStartIdentifier(*pParse) || *pParse == '@')
			{
				fCheckParam();
				if (fg_StrStartsWith(pParse, "true") && !fs_CharIsIdentifierOrExpression(pParse[4]))
				{
					Param = true;
					pParse += 4;
				}
				else if (fg_StrStartsWith(pParse, "false") && !fs_CharIsIdentifierOrExpression(pParse[5]))
				{
					Param = false;
					pParse += 5;
				}
				else if (fg_StrStartsWith(pParse, "null") && !fs_CharIsIdentifierOrExpression(pParse[4]))
				{
					Param = nullptr;
					pParse += 4;
				}
				else
					Param = f_ParseIdentifierToken(pParse);
			}
			else if (*pParse == '(')
			{
				if (!Param.f_IsValid())
				{
					auto Cleanup = f_EnableBinaryOperators();
					auto Expression = f_ParseExpression(pParse, EParseExpressionFlag_None);

					CEJSON Return;
					auto &ReturnUserType = Return.f_UserType();
					ReturnUserType.m_Type = "BuildSystemToken";
					ReturnUserType.m_Value = fg_Move(Expression);

					Param = fg_Move(Return).f_ToJSON();
				}
				else
				{
					if (fg_Const(Param)["$value"]["Type"].f_String() != "Identifier")
						f_ThrowError("Function call needs a name", pParse);

					auto &ParamValue = fg_Const(Param)["$value"];

					if (ParamValue["EntityType"].f_String())
						f_ThrowError("Function call cannot specify entity", pParse);

					Param = f_ParseFunctionToken(pParse, nullptr, ParamValue["Name"].f_String(), ParamValue["PropertyType"].f_String());
				}
			}
			else if (*pParse == '<' && !NStr::fg_CharIsWhiteSpaceNoLines(pParse[1]) && pParse[1] != '<')
			{
				if (!Param.f_IsValid())
					f_ThrowError("JSON accessor needs a param", pParse);

				auto FirstParam = fg_Move(Param);
				Param = f_ParseJSONAccessor(pParse, fg_Move(FirstParam));
			}
			else if (fg_StrStartsWith(pParse, "->"))
			{
				if (!Param.f_IsValid())
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

				auto FirstParam = fg_Move(Param);
				Param = f_ParseFunctionToken(pParse, &FirstParam, FunctionName, PropertyType);
			}
			else if (!m_bParsingDefine && *pParse == '?')
			{
				if (!Param.f_IsValid())
					f_ThrowError("Ternary operator needs a target", pParse);

				++pParse;
				fg_ParseWhiteSpace(pParse);

				CJSON Left;
				auto Cleanup = f_EnableBinaryOperators();
				NJSON::fg_ParseJSONValue(Left, pParse, *this);

				fg_ParseWhiteSpace(pParse);

				if (*pParse != ':')
					f_ThrowError("Expected ':' for ternary operator", pParse);

				++pParse;

				fg_ParseWhiteSpace(pParse);

				CJSON Right;
				NJSON::fg_ParseJSONValue(Right, pParse, *this);

				CEJSON Return;
				auto &ReturnUserType = Return.f_UserType();
				ReturnUserType.m_Type = "BuildSystemToken";
				ReturnUserType.m_Value =
					{
						"Type"__= "Ternary"
						, "Conditional"__= fg_Move(Param)
						, "Left"__= fg_Move(Left)
						, "Right"__= fg_Move(Right)
					}
				;

				Param = fg_Move(Return).f_ToJSON();
			}
			else if (m_bSupportBinaryOperators && !m_bParsingDefine && !Param.f_IsValid() && fs_IsPrefixOperator(pParse))
			{
				CStr Operator(pParse, 1);

				if
					(
						Operator != "+"
						&& Operator != "-"
						&& Operator != "~"
						&& Operator != "!"
					)
				{
					f_ThrowError("Invalid prefix operator: {}"_f << Operator, pParse);
				}

				++pParse;

				fg_ParseWhiteSpace(pParse);

				CJSON Right;
				auto Cleanup = f_DisableParseAfterValue();
				NJSON::fg_ParseJSONValue(Right, pParse, *this);

				CEJSON Return;
				auto &ReturnUserType = Return.f_UserType();
				ReturnUserType.m_Type = "BuildSystemToken";
				ReturnUserType.m_Value =
					{
						"Type"__= "PrefixOperator"
						, "Operator"__= fg_Move(Operator)
						, "Right"__= fg_Move(Right)
					}
				;

				Param = fg_Move(Return).f_ToJSON();
			}
			else if (m_bSupportBinaryOperators && !m_bParsingDefine && fs_IsBinaryOperator(pParse))
			{
				if (!Param.f_IsValid())
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
						Operator != "<"
						&& Operator != ">"
						&& Operator != ">="
						&& Operator != "<="
						&& Operator != "=="
						&& Operator != "!="
						&& Operator != "+"
						&& Operator != "-"
						&& Operator != "/"
						&& Operator != "*"
						&& Operator != "%"
						&& Operator != "<<"
						&& Operator != ">>"
						&& Operator != "&"
						&& Operator != "|"
						&& Operator != "^"
						&& Operator != "&&"
						&& Operator != "||"
					)
				{
					f_ThrowError("Invalid binary operator: {}"_f << Operator, pParseStart);
				}

				++pParse;
				fg_ParseWhiteSpace(pParse);

				CJSON Right;
				NJSON::fg_ParseJSONValue(Right, pParse, *this);

				CEJSON Return;
				auto &ReturnUserType = Return.f_UserType();
				ReturnUserType.m_Type = "BuildSystemToken";
				ReturnUserType.m_Value =
					{
						"Type"__= "BinaryOperator"
						, "Operator"__= fg_Move(Operator)
						, "Left"__= fg_Move(Param)
						, "Right"__= fg_Move(Right)
					}
				;

				Param = fg_Move(Return).f_ToJSON();
			}
			else
			{
				fCheckParam();
				CJSON ParamParse;
				auto Cleanup = f_EnableBinaryOperators();
				NJSON::fg_ParseJSONValue(ParamParse, pParse, *this);
				Param = fg_Move(ParamParse);
			}
		}

		if (bUseParentheses && !bEndedParen)
			f_ThrowError("Could not find matching end )", o_pParse);

		if (!bUseParentheses)
		{
			if ((_Flags & EParseExpressionFlag_SupportAppend) && fg_StrStartsWith(pParse, "..."))
			{
				pParse += 3;
				Return["Type"] = "AppendExpression";
			}
			else
				Return["Type"] = "Expression";
		}

		o_pParse = pParse;
		return Return;
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseDefine(uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;

		bool bPreviousParsingDefine = m_bParsingDefine;
		m_bParsingDefine = true;

		auto Cleanup = g_OnScopeExit > [&]
			{
				m_bParsingDefine = bPreviousParsingDefine;
			}
		;

		CJSON Define;
		auto CleanupBinary = f_EnableBinaryOperators();
		NJSON::fg_ParseJSONValue(Define, pParse, *this);

		o_pParse = pParse;

		return
			{
				"Type"__= "Define"
				, "Define"__= fg_Move(Define)
			}
		;
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseFunctionType(uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;

		bool bPreviousParsingDefine = m_bParsingDefine;
		m_bParsingDefine = true;

		auto Cleanup = g_OnScopeExit > [&]
			{
				m_bParsingDefine = bPreviousParsingDefine;
			}
		;

		fg_ParseWhiteSpace(pParse);
		if (*pParse != '(')
			f_ThrowError("Expected (", pParse);
		++pParse;
		fg_ParseWhiteSpace(pParse);

		TCVector<CJSON> Parameters;
		while (*pParse)
		{
			if (*pParse == ')')
			{
				++pParse;
				break;
			}

			CJSON ParameterType;
			auto Cleanup = f_EnableBinaryOperators();
			NJSON::fg_ParseJSONValue(ParameterType, pParse, *this);

			fg_ParseWhiteSpace(pParse);

			bool bIsEllipsis = false;
			auto pStartEllipsis = pParse;
			if (fg_StrStartsWith(pParse, "..."))
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
				ParameterType = {"$type"__= "BuildSystemToken"};
				ParameterType["$type"] = "BuildSystemToken";
				ParameterType["$value"] = f_ParseDefaulted(pParse, fg_Move(OldParameterType));
			}

			Parameters.f_Insert
				(
					CJSON
					{
						"Type"__= fg_Move(ParameterType)
						, "Name"__= fg_Move(Name)
						, "Ellipsis"__= bIsEllipsis
					}
				)
			;

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

		CJSON ReturnType;
		auto CleanupBinary = f_EnableBinaryOperators();
		NJSON::fg_ParseJSONValue(ReturnType, pParse, *this);

		o_pParse = pParse;

		return
			{
				"Type"__= "FunctionType"
				, "ReturnType"__= fg_Move(ReturnType)
				, "Parameters"__= fg_Move(Parameters)
			}
		;
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseDefaultType( CStr const &_Identifier)
	{
		return
			{
				"Type"__= "DefaultType"
				, "TypeName"__= _Identifier
			}
		;
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseType(uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;

		fg_ParseWhiteSpace(pParse);
		if (*pParse != '(')
			f_ThrowError("Expected (", pParse);
		++pParse;

		CStr TypeIdentifier = f_ParseIdentifier(pParse);

		fg_ParseWhiteSpace(pParse);
		if (*pParse != ')')
			f_ThrowError("Expected )", pParse);
		++pParse;

		o_pParse = pParse;

		return
			{
				"Type"__= "Type"
				, "TypeName"__= fg_Move(TypeIdentifier)
			}
		;
	}

	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParsePostDefine(uch8 const *&o_pParse, NEncoding::CJSON &o_Value)
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
			o_Value["$type"] = "BuildSystemToken";
			o_Value["$value"] =
				{
					"Type"__= "Optional"
					, "Param"__= fg_Move(InnerValue)
				}
			;

			o_pParse = pParse;
		}

		if (*pParse == '=')
		{
			++pParse;

			auto InnerValue = fg_Move(o_Value);
			o_Value["$type"] = "BuildSystemToken";
			o_Value["$value"] = f_ParseDefaulted(pParse, fg_Move(InnerValue));

			o_pParse = pParse;
		}
	}

	NEncoding::CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseDefaulted(uch8 const *&o_pParse, NEncoding::CJSON &&_Type)
	{
		auto pParse = o_pParse;

		bool bPreviousParsingDefine = m_bParsingDefine;
		m_bParsingDefine = false;

		auto Cleanup = g_OnScopeExit > [&]
			{
				m_bParsingDefine = bPreviousParsingDefine;
			}
		;

		fg_ParseWhiteSpace(pParse);

		CJSON DefaultValue;
		auto CleanupBinary = f_EnableBinaryOperators();
		NJSON::fg_ParseJSONValue(DefaultValue, pParse, *this);

		o_pParse = pParse;

		return
			{
				"Type"__= "TypeDefaulted"
				, "InnerType"__= fg_Move(_Type)
				, "DefaultValue"__= fg_Move(DefaultValue)
			}
		;
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseOneOf(uch8 const *&o_pParse)
	{
		auto pParse = o_pParse;

		fg_ParseWhiteSpace(pParse);
		if (*pParse != '(')
			f_ThrowError("Expected (", pParse);
		++pParse;

		bool bEndedParen = false;
		bool bLastTokenValid = true;

		CJSON OneOfList = EJSONType_Array;

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
				CJSON Param;
				auto Cleanup = f_EnableBinaryOperators();
				NJSON::fg_ParseJSONValue(Param, pParse, *this);
				OneOfList.f_Insert(fg_Move(Param));
				bLastTokenValid = true;
				continue;
			}

			bLastTokenValid = false;
		}

		if (!bEndedParen)
			f_ThrowError("Could not find matching end )", o_pParse);

		o_pParse = pParse;

		return
			{
				"Type"__= "OneOf"
				, "OneOfList"__= fg_Move(OneOfList)
			}
		;
	}

	namespace
	{
		template <typename tf_CStr>
		void fg_GenerateAccessors(tf_CStr &o_String, TCVector<CJSON> const &_Accessors, mint _Depth)
		{
			bool bFirst = true;
			for (auto &Ident : _Accessors)
			{
				if (Ident.f_IsString())
				{
					if (!bFirst)
						o_String += ".";
					o_String += Ident.f_String();
				}
				else if (Ident.f_IsObject())
				{
					auto pType = Ident.f_GetMember("Type", EJSONType_String);
					if (!pType)
						DMibError("Accessor does not have valid Type member");

					if (pType->f_String() == "Subscript")
					{
						auto pArgument = Ident.f_GetMember("Argument");
						if (!pArgument)
							DMibError("Accessor does not have valid Argument member");
						o_String += "[";
						NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pArgument, _Depth, "\t", gc_BuildSystemJSONParseFlags);
						o_String += "]";
					}
					else if (pType->f_String() == "Expression")
					{
						auto pParam = Ident.f_GetMember("Param", EJSONType_Object);
						if (!pParam)
							DMibError("Expression token does not have valid Param member");

						o_String += "@(";
						NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pParam, _Depth, "\t", gc_BuildSystemJSONParseFlags);
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
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateEvalString(tf_CStr &o_String, NEncoding::CJSON const &_Token, mint _Depth)
	{
		o_String += "`";
		for (auto &Token : _Token.f_Array())
		{
			auto pType = Token.f_GetMember("Type", EJSONType_String);
			if (!pType)
				DMibError("Token does not have valid Type member");

			auto &TokenType = pType->f_String();
			if (TokenType == "String")
			{
				auto pValue = Token.f_GetMember("Value", EJSONType_String);
				if (!pValue)
					DMibError("String token does not have valid Value member");
				fg_GenerateJSONString<'`', CJSONParseContext, false>(o_String, pValue->f_String());
			}
			else
			{
				o_String += "@";
				CJSONParseContext::fs_GenerateExpression(o_String, Token, false, _Depth);
			}
		}
		o_String += "`";
	}

	template <typename tf_CStr>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateExpression
		(
		 	tf_CStr &o_String
		 	, CJSON const &_Token
		 	, bool _bQuoteStrings
			, mint _Depth
		)
	{
		if (!_Token.f_IsObject())
			DMibError("Token is not object");

		auto pType = _Token.f_GetMember("Type", EJSONType_String);
		if (!pType)
			DMibError("Token does not have valid Type member");

		auto &TokenType = pType->f_String();

		if (TokenType == "String")
		{
			auto pValue = _Token.f_GetMember("Value", EJSONType_String);
			if (!pValue)
				DMibError("String token does not have valid Value member");
			if (_bQuoteStrings)
				fs_GenerateString<CJSONParseContext>(o_String, pValue->f_String());
			else
				o_String += pValue->f_String();
		}
		else if (TokenType == "EvalString")
		{
			auto pValue = _Token.f_GetMember("Value", EJSONType_Array);
			if (!pValue)
				DMibError("Eval string token does not have valid Value member");

			CJSONParseContext::fs_GenerateEvalString(o_String, *pValue, _Depth);
		}
		else if (TokenType == "WildcardString")
		{
			if (auto pValue = _Token.f_GetMember("Value", EJSONType_String))
			{
				o_String += "~";
				if (pValue->f_String().f_GetUserData() == EJSONStringType_SingleQuote)
					fg_GenerateJSONString<'\'', CJSONParseContext>(o_String, pValue->f_String());
				else
					fg_GenerateJSONString<'"', CJSONParseContext>(o_String, pValue->f_String());
			}
			else if (auto pValue = _Token.f_GetMember("Value", EJSONType_Object))
			{
				o_String += "~";
				fs_GenerateExpression(o_String, *pValue, _bQuoteStrings, _Depth);
			}
			else
				DMibError("Wildcard string token does not have valid Value member");
		}
		else if (TokenType == "Operator")
		{
			auto pOperator = _Token.f_GetMember("Operator", EJSONType_String);
			if (!pOperator)
				DMibError("Operator token does not have valid Operator member");

			auto pRight = _Token.f_GetMember("Right");
			if (!pRight)
				DMibError("Operator token does not have valid Right member");

			o_String += pOperator->f_String();
			o_String += " ";
			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else if (TokenType == "RootValue")
		{
			auto pRight = _Token.f_GetMember("Value");
			if (!pRight)
				DMibError("RootValue token does not have valid Value member");

			auto pAccessors = _Token.f_GetMember("Accessors", EJSONType_Array);
			if (!pAccessors)
				DMibError("RootValue token does not have valid Accessors member");

			o_String += "#<";
			fg_GenerateAccessors(o_String, pAccessors->f_Array(), _Depth);
			o_String += "> ";

			 NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else if (TokenType == "Define")
		{
			auto pDefine = _Token.f_GetMember("Define");
			if (!pDefine)
				DMibError("Define token does not have valid Define member");

			o_String += "define ";
			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pDefine, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else if (TokenType == "DefaultType")
		{
			auto pTypeName = _Token.f_GetMember("TypeName", EJSONType_String);
			if (!pTypeName)
				DMibError("DefaultType token does not have valid TypeName member");

			o_String += pTypeName->f_String();
		}
		else if (TokenType == "Type")
		{
			auto pTypeName = _Token.f_GetMember("TypeName", EJSONType_String);
			if (!pTypeName)
				DMibError("Type token does not have valid TypeName member");

			o_String += "type(";
			o_String += pTypeName->f_String();
			o_String += ")";
		}
		else if (TokenType == "Optional")
		{
			auto pParam = _Token.f_GetMember("Param");
			if (!pParam)
				DMibError("Optional token does not have valid Param member");

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pParam, _Depth, "\t", gc_BuildSystemJSONParseFlags);
			o_String += "?";
		}
		else if (TokenType == "FunctionType")
		{
			auto pReturnType = _Token.f_GetMember("ReturnType");
			if (!pReturnType)
				DMibError("FunctionType token does not have valid ReturnType member");

			auto pParameters = _Token.f_GetMember("Parameters", EJSONType_Array);
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

				auto pType = Parameter.f_GetMember("Type");
				if (!pType)
					DMibError("FunctionType parameter does not have valid Type member");

				auto pName = Parameter.f_GetMember("Name", EJSONType_String);
				if (!pName)
					DMibError("FunctionType parameter does not have valid Name member");

				auto pEllipsis = Parameter.f_GetMember("Ellipsis", EJSONType_Boolean);
				if (!pEllipsis)
					DMibError("FunctionType parameter does not have valid Ellipsis member");

				CJSON const *pDefaultValue = nullptr;
				if (pType->f_IsObject())
				{
					if (auto pUserType = pType->f_GetMember("$type", EJSONType_String))
					{
						if (pUserType->f_String() == "BuildSystemToken")
						{
							if (auto pValue = pType->f_GetMember("$value", EJSONType_Object))
							{
								if (auto pTokenType = pValue->f_GetMember("Type", EJSONType_String))
								{
									if (pTokenType->f_String() == "TypeDefaulted")
									{
										if (auto pInnerType = pValue->f_GetMember("InnerType"))
											pType = pInnerType;
										else
											DMibError("TypeDefaulted token does not have valid InnerType member");

										pDefaultValue = pValue->f_GetMember("DefaultValue");
										if (!pDefaultValue)
											DMibError("TypeDefaulted token does not have valid InnerType member");
									}
								}
							}
						}
					}
				}

				NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pType, _Depth, "\t", gc_BuildSystemJSONParseFlags);

				if (pEllipsis->f_Boolean())
					o_String += "...";

				o_String += typename tf_CStr::CFormat(" {}") << pName->f_String();

				if (pDefaultValue)
				{
					o_String += " = ";
					NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pDefaultValue, _Depth, "\t", gc_BuildSystemJSONParseFlags);
				}
			}

			o_String += ") ";

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pReturnType, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else if (TokenType == "OneOf")
		{
			auto pOneOfList = _Token.f_GetMember("OneOfList", EJSONType_Array);
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

				NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *iOneOf, _Depth, "\t", gc_BuildSystemJSONParseFlags);
				o_String += "";
			}

			o_String += ")";
		}
		else if (TokenType == "Expression" || TokenType == "AppendExpression")
		{
			auto pParam = _Token.f_GetMember("Param");
			if (!pParam)
				DMibError("Expression token does not have valid Param member");

			auto pParen = _Token.f_GetMember("Paren");
			if (!pParen)
				DMibError("Expression token does not have valid Paren member");

			if (pParen->f_Boolean())
				o_String += "(";

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pParam, _Depth, "\t", gc_BuildSystemJSONParseFlags);

			if (pParen->f_Boolean())
				o_String += ")";

			if (TokenType == "AppendExpression")
				o_String += "...";
		}
		else if (TokenType == "Ternary")
		{
			auto pConditional = _Token.f_GetMember("Conditional");
			if (!pConditional)
				DMibError("Ternary token does not have valid Conditional member");

			auto pLeft = _Token.f_GetMember("Left");
			if (!pLeft)
				DMibError("Ternary token does not have valid Left member");

			auto pRight = _Token.f_GetMember("Right");
			if (!pRight)
				DMibError("Ternary token does not have valid Right member");

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pConditional, _Depth, "\t", gc_BuildSystemJSONParseFlags);

			o_String += " ? ";

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pLeft, _Depth, "\t", gc_BuildSystemJSONParseFlags);

			o_String += " : ";

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else if (TokenType == "Identifier")
		{
			auto pEntityType = _Token.f_GetMember("EntityType", EJSONType_String);
			if (!pEntityType)
				DMibError("Identifier token does not have valid EntityType member");

			auto pPropertyType = _Token.f_GetMember("PropertyType", EJSONType_String);
			if (!pPropertyType)
				DMibError("Identifier token does not have valid PropertyType member");

			auto pName = _Token.f_GetMember("Name");
			if (!pName)
				DMibError("Identifier token does not have valid Name member");

			if (pEntityType->f_String())
			{
				o_String += pEntityType->f_String();
				o_String += ":";
			}

			if (pPropertyType->f_String())
			{
				o_String += pPropertyType->f_String();
				o_String += ".";
			}

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
						CJSONParseContext::fs_GenerateExpression(o_String, Token, false, _Depth);
					}
				}
			}
			else
				DMibError("Identifier token does not have valid Name member");

		}
		else if (TokenType == "JSONAccessor")
		{
			auto pParam = _Token.f_GetMember("Param");
			if (!pParam)
				DMibError("JSONAccessor token does not have valid Param member");

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pParam, _Depth, "\t", gc_BuildSystemJSONParseFlags);

			auto pAccessors = _Token.f_GetMember("Accessors", EJSONType_Array);
			if (!pAccessors)
				DMibError("JSONAccessor token does not have valid Accessors member");

			o_String += "<";
			fg_GenerateAccessors(o_String, pAccessors->f_Array(), _Depth);
			o_String += ">";
		}
		else if (TokenType == "TypeDefaulted")
		{
			auto pInnerType = _Token.f_GetMember("InnerType");
			if (!pInnerType)
				DMibError("TypeDefaulted token does not have valid InnerType member");

			auto pDefaultValue = _Token.f_GetMember("DefaultValue");
			if (!pDefaultValue)
				DMibError("TypeDefaulted token does not have valid DefaultValue member");

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pInnerType, _Depth, "\t", gc_BuildSystemJSONParseFlags);

			o_String += " = ";
			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pDefaultValue, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else if (TokenType == "PostFunction" || TokenType == "Function")
		{
			auto pName = _Token.f_GetMember("Name", EJSONType_String);
			if (!pName)
				DMibError("Function token does not have valid Name member");

			auto pPropertyType = _Token.f_GetMember("PropertyType", EJSONType_String);
			if (!pPropertyType)
				DMibError("Function token does not have valid PropertyType member");

			auto pParams = _Token.f_GetMember("Params", EJSONType_Array);
			if (!pParams)
				DMibError("Function token does not have valid Params member");

			auto &Params = pParams->f_Array();

			auto iParam = Params.f_GetIterator();

			if (TokenType == "PostFunction")
			{
				if (!iParam)
					DMibError("Invalid number of parmeters in post function call");

				NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *iParam, _Depth, "\t", gc_BuildSystemJSONParseFlags);
				++iParam;
				o_String += "->";
			}

			if (pPropertyType->f_String())
			{
				o_String += pName->f_String();
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

				NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *iParam, _Depth, "\t", gc_BuildSystemJSONParseFlags);
			}

			o_String += ")";
		}
		else if (TokenType == "KeyPrefixOperator")
		{
			auto pOperator = _Token.f_GetMember("Operator");
			if (!pOperator)
				DMibError("PrefixOperator token does not have valid Operator member");

			auto pRight = _Token.f_GetMember("Right");
			if (!pRight)
				DMibError("PrefixOperator token does not have valid Right member");

			o_String += pOperator->f_String();

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else if (TokenType == "KeyLogicalOperator")
		{
			auto pOperator = _Token.f_GetMember("Operator");
			if (!pOperator)
				DMibError("KeyLogicalOperator token does not have valid Operator member");

			o_String += pOperator->f_String();
		}
		else if (TokenType == "BinaryOperator")
		{
			auto pOperator = _Token.f_GetMember("Operator");
			if (!pOperator)
				DMibError("BinaryOperator token does not have valid Operator member");

			auto pLeft = _Token.f_GetMember("Left");
			if (!pLeft)
				DMibError("BinaryOperator token does not have valid Left member");

			auto pRight = _Token.f_GetMember("Right");
			if (!pRight)
				DMibError("BinaryOperator token does not have valid Right member");

			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pLeft, _Depth, "\t", gc_BuildSystemJSONParseFlags);
			o_String += " ";
			o_String += pOperator->f_String();
			o_String += " ";
			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else if (TokenType == "PrefixOperator")
		{
			auto pOperator = _Token.f_GetMember("Operator");
			if (!pOperator)
				DMibError("PrefixOperator token does not have valid Operator member");

			auto pRight = _Token.f_GetMember("Right");
			if (!pRight)
				DMibError("PrefixOperator token does not have valid Right member");

			o_String += pOperator->f_String();
			NJSON::fg_GenerateJSONValue<CBuildSystemParseContext, tf_CStr>(o_String, *pRight, _Depth, "\t", gc_BuildSystemJSONParseFlags);
		}
		else
			DMibError("Token Type '{}' not supported"_f << TokenType);
	}

	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateExpression
		(
		 	CStr &o_String
		 	, CJSON const &_Token
		 	, bool _bQuoteStrings
			, mint _Depth
		)
	;

	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateExpression
		(
		 	CStrNonTracked &o_String
		 	, CJSON const &_Token
		 	, bool _bQuoteStrings
			, mint _Depth
		)
	;

	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateExpression
		(
		 	CStrAggregate &o_String
		 	, CJSON const &_Token
		 	, bool _bQuoteStrings
			, mint _Depth
		)
	;

	template void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateExpression
		(
			CStrAggregateNonTracked &o_String
		 	, CJSON const &_Token
		 	, bool _bQuoteStrings
			, mint _Depth
		)
	;
}
