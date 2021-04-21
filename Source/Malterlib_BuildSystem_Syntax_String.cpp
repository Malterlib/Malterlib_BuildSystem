// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	auto CBuildSystemSyntax::CEvalStringToken::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position) -> CEvalStringToken
	{
		if (_JSON.f_IsString())
			return CEvalStringToken{_JSON.f_String()};

		auto pType = _JSON.f_GetMember("Type", EJSONType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Eval string token does not have valid Type member");

		if (pType->f_String() == "String")
		{
			auto pValue = _JSON.f_GetMember("Value", EJSONType_String);
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "Eval string string token does not have valid Value member");
			return CEvalStringToken{pValue->f_String()};
		}
		else if (pType->f_String() == "Expression")
		{
			auto pParam = _JSON.f_GetMember("Param");
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "Eval string expression token does not have valid Param member");

			auto pParen = _JSON.f_GetMember("Paren", EJSONType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "Eval string expression does not have valid Paren member");

			return CEvalStringToken{CExpression::fs_FromJSON(*pParam, _Position, pParen->f_Boolean())};
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Eval string token does not have known Type: {}"_f << pType->f_String());

		return {};
	}

	auto CBuildSystemSyntax::CEvalString::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position) -> CEvalString
	{
		if (!_JSON.f_IsArray())
			CBuildSystem::fs_ThrowError(_Position, "Eval string tokens are not an array");

		CEvalString Return;

		for (auto &Token : _JSON.f_Array())
			Return.m_Tokens.f_Insert(CEvalStringToken::fs_FromJSON(Token, _Position));

		return Return;
	}

	auto CBuildSystemSyntax::CWildcardString::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position) -> CWildcardString
	{
		auto &Value = _JSON;
		DMibRequire(Value.f_GetMember("Type", EJSONType_String) && Value.f_GetMember("Type", EJSONType_String)->f_String() == "WildcardString");

		if (auto pValue = Value.f_GetMember("Value", EJSONType_String))
			return CWildcardString{pValue->f_String()};
		else if (auto pValue = Value.f_GetMember("Value", EJSONType_Object))
		{
			auto &Object = pValue->f_Object();
			auto *pType = Object.f_GetMember("Type", EJSONType_String);
			if (!pType || pType->f_String() != "EvalString")
				CBuildSystem::fs_ThrowError(_Position, "Invalid wildcard token type: '{}'"_f << pType->f_String());

			auto pTokenArray = Object.f_GetMember("Value", EJSONType_Array);
			if (!pTokenArray)
				CBuildSystem::fs_ThrowError(_Position, "Invalid Value for wildcard token type");

			return CWildcardString{CEvalString::fs_FromJSON(*pTokenArray, _Position)};
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Wildcard string token does not have valid Value member");

		return {};
	}
}
