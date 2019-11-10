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

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseEvalString(uch8 const *&o_pParse)
	{
		CJSON Value;

		uch8 const *pParse = o_pParse;
		uch8 const *pParseStart = pParse;

		auto &TokenArray = Value.f_Array();
		auto fAddStringToken = [&]
			{
				if (pParse == pParseStart)
					return;

				TokenArray.f_Insert
					(
						CJSON
						{
							"Type"__= "String"
							, "Value"__= CStr(pParseStart, pParse - pParseStart)
						}
					)
				;
			}
		;

		while (*pParse)
		{
			if (*pParse == '@')
			{
				fAddStringToken();
				++pParse;
				fg_ParseWhiteSpace(pParse);
				TokenArray.f_Insert(f_ParseExpression(pParse, "Expression"));
				pParseStart = pParse;
			}
			else
				++pParse;
		}

		fAddStringToken();

		o_pParse = pParse;
		return Value;
	}

	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContextCatureStringMap::f_MapCharacter(mint _iDestination, mint _iSource, mint _nChars) const
	{
		m_StringMap.f_SetAtLeastLen(_iDestination + _nChars, 0);
		for (mint i = 0; i < _nChars; ++i)
			m_StringMap[_iDestination + i] = _iSource + i;
	}

	NStr::CParseLocation TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContextCatureStringMap::f_GetLocation(uch8 const *_pParse) const
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

			if (pParse > pParseInOriginal)
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

	NEncoding::CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseEvalStringToken(uch8 const *&o_pParse) const
	{
		auto *pParse = o_pParse;
		CStr ParsedString;
		auto pParseStart = pParse;

		CJSONParseContextCatureStringMap ParseContext;
		ParseContext.m_pOriginalParseContext = this;
		(CJSONParseContext &)ParseContext = *this;

		if (!fg_ParseJSONString<'`', true>(ParsedString, pParse, ParseContext))
			f_ThrowError("End of eval string character ` not found for string", pParseStart);

		uch8 const *pParseEval = (uch8 const *)ParsedString.f_GetStr();
		ParseContext.m_pOriginalStartParse = ParseContext.m_pStartParse;
		ParseContext.m_pStartParse = pParseEval;

		CJSON ParsedEvalString;
		ParsedEvalString = ParseContext.f_ParseEvalString(pParseEval);

		o_pParse = pParse;

		return
			{
				"Type"__= "EvalString"
				, "Value"__= fg_Move(ParsedEvalString)
			}
		;
	}

	namespace
	{
		bool fg_CharIsStartIdentifier(uch8 _Char)
		{
			return fg_CharIsAnsiAlphabetical(_Char) || _Char == '_';
		}

		bool fg_CharIsIdentifier(uch8 _Char)
		{
			return fg_CharIsAnsiAlphabetical(_Char) || _Char == '_' || fg_CharIsNumber(_Char);
		}
	}

	CStr TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseIdentifier(uch8 const * &o_pParse) const
	{
		auto *pParse = o_pParse;

		if (!fg_CharIsStartIdentifier(*pParse))
			f_ThrowError("Expected start of identifier (A-Z a-z _)", o_pParse);

		auto pParseStart = pParse;
		while (fg_CharIsIdentifier(*pParse))
			++pParse;

		o_pParse = pParse;

		return CStr(pParseStart, pParse - pParseStart);
	}

	NEncoding::CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseIdentifierToken(uch8 const * &o_pParse) const
	{
		auto *pParse = o_pParse;

		CStr EntityScope;
		CStr EntityType;
		CStr Name;

		Name = f_ParseIdentifier(pParse);

		if (*pParse == ':')
		{
			++pParse;
			EntityScope = fg_Move(Name);
			Name = f_ParseIdentifier(pParse);
		}

		if (*pParse == '.')
		{
			++pParse;
			EntityType = fg_Move(Name);
			Name = f_ParseIdentifier(pParse);
		}

		o_pParse = pParse;
		CEJSON Return;
		auto &ReturnUserType = Return.f_UserType();
		ReturnUserType.m_Type = "BuildSystemToken";
		ReturnUserType.m_Value =
			{
				"Type"__= "Identifier"
				, "EntityScope"__= fg_Move(EntityScope)
				, "EntityType"__= fg_Move(EntityType)
				, "Name"__= fg_Move(Name)
			}
		;

		return Return.f_ToJSON();
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseFunctionToken(uch8 const *&o_pParse, CJSON *_pFirstParam, NStr::CStr const &_FunctionName)
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

		return Return.f_ToJSON();
	}

	CJSON TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseExpression(uch8 const *&o_pParse, NStr::CStr const &_TokenType)
	{
		auto *pParse = o_pParse;
		if (*pParse != '(')
			f_ThrowError("Expression must be enclosed in ()", pParse);

		++pParse;

		CJSON Return
			{
				"Type"__= _TokenType
			}
		;

		auto &Param = Return["Param"];
		bool bEndedParen = false;

		auto fCheckParam = [&]
			{
				if (Param.f_IsValid())
					f_ThrowError("Expression parameter already specified", pParse);
			}
		;

		while (*pParse)
		{
			fg_ParseWhiteSpace(pParse);
			if (*pParse == ')')
			{
				++pParse;
				bEndedParen = true;
				break;
			}
			else if (fg_CharIsStartIdentifier(*pParse))
			{
				fCheckParam();
				Param = f_ParseIdentifierToken(pParse);
			}
			else if (*pParse == '(')
			{
				if (!Param.f_IsValid() || fg_Const(Param)["$value"]["Type"].f_String() != "Identifier")
					f_ThrowError("Function call needs a name", pParse);

				auto &ParamValue = fg_Const(Param)["$value"];

				if (ParamValue["EntityScope"].f_String() || ParamValue["EntityType"].f_String())
					f_ThrowError("Function call cannot specify entity", pParse);

				CStr FunctionName = ParamValue["Name"].f_String();
				Param = f_ParseFunctionToken(pParse, nullptr, FunctionName);
			}
			else if (fg_StrStartsWith(pParse, "->"))
			{
				if (!Param.f_IsValid())
					f_ThrowError("Function call needs a target", pParse);

				pParse += 2;
				fg_ParseWhiteSpace(pParse);

				CStr FunctionName = f_ParseIdentifier(pParse);
				fg_ParseWhiteSpace(pParse);

				auto FirstParam = fg_Move(Param);
				Param = f_ParseFunctionToken(pParse, &FirstParam, FunctionName);
			}
			else
			{
				fCheckParam();
				CJSON ParamParse;
				NJSON::fg_ParseJSONValue(ParamParse, pParse, *this);
				Param = fg_Move(ParamParse);
			}
		}

		if (!bEndedParen)
			f_ThrowError("Could not find matching end )", o_pParse);

		o_pParse = pParse;
		return Return;
	}

	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateExpression
		(
		 	NStr::CStr &o_String
		 	, NEncoding::CJSON const &_Token
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

			CStr Value;
			for (auto &Token : pValue->f_Array())
				CJSONParseContext::fs_GenerateExpression(Value, Token, false, _Depth);
			fg_GenerateJSONString<'`', CJSONParseContext>(o_String, Value);
		}
		else if (TokenType == "Expression" || TokenType == "AppendExpression")
		{
			auto pParam = _Token.f_GetMember("Param", EJSONType_Object);
			if (!pParam)
				DMibError("Expression token does not have valid Param member");

			if (TokenType == "AppendExpression")
				o_String += "@@(";
			else
				o_String += "@(";

			NJSON::fg_GenerateJSONValue<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext, CStr>(o_String, *pParam, _Depth, "\t", true);

			o_String += ")";
		}
		else if (TokenType == "Identifier")
		{
			auto pEntityScope = _Token.f_GetMember("EntityScope", EJSONType_String);
			if (!pEntityScope)
				DMibError("Identifier token does not have valid EntityScope member");

			auto pEntityType = _Token.f_GetMember("EntityType", EJSONType_String);
			if (!pEntityType)
				DMibError("Identifier token does not have valid EntityType member");

			auto pName = _Token.f_GetMember("Name", EJSONType_String);
			if (!pName)
				DMibError("Identifier token does not have valid Name member");

			if (pEntityScope->f_String())
			{
				o_String += pEntityScope->f_String();
				o_String += ":";
			}

			if (pEntityType->f_String())
			{
				o_String += pEntityType->f_String();
				o_String += ".";
			}

			o_String += pName->f_String();
		}
		else if (TokenType == "PostFunction" || TokenType == "Function")
		{
			auto pName = _Token.f_GetMember("Name", EJSONType_String);
			if (!pName)
				DMibError("Function token does not have valid Name member");

			auto pParams = _Token.f_GetMember("Params", EJSONType_Array);
			if (!pParams)
				DMibError("Function token does not have valid Params member");

			auto &Params = pParams->f_Array();

			auto iParam = Params.f_GetIterator();

			if (TokenType == "PostFunction")
			{
				if (!iParam)
					DMibError("Invalid number of parmas in post function call");

				NJSON::fg_GenerateJSONValue<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext, CStr>(o_String, *iParam, _Depth, "\t", true);
				++iParam;
				o_String += "->";
			}

			o_String += pName->f_String();
			o_String += "(";

			bool bFirst = true;
			for (; iParam; ++iParam, bFirst = false)
			{
				if (!bFirst)
					o_String += ", ";
				NJSON::fg_GenerateJSONValue<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext, CStr>(o_String, *iParam, _Depth, "\t", true);
				o_String += "";
			}

			o_String += ")";
		}
		else
			DMibError("Token Type '{}' not supported"_f << TokenType);
	}
}
