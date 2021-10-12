// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	bool CBuildSystemSyntax::CEvalStringToken::f_IsExpression() const
	{
		return m_Token.f_IsOfType<NStorage::TCIndirection<CExpression>>();
	}

	CBuildSystemSyntax::CExpression const &CBuildSystemSyntax::CEvalStringToken::f_Expression() const
	{
		return m_Token.f_GetAsType<NStorage::TCIndirection<CExpression>>().f_Get();
	}

	bool CBuildSystemSyntax::CEvalStringToken::f_IsString() const
	{
		return m_Token.f_IsOfType<CStr>();
	}

	CStr const &CBuildSystemSyntax::CEvalStringToken::f_String() const
	{
		return m_Token.f_GetAsType<CStr>();
	}

	NEncoding::CEJSON CBuildSystemSyntax::CEvalStringToken::f_ToJSON(bool _bRawString) const
	{
		switch (m_Token.f_GetTypeID())
		{
		case 0:
			{
				if (_bRawString)
					return m_Token.f_Get<0>();
				
				CEJSON Return;
				auto &ReturnObject = Return.f_Object();
				ReturnObject["Type"] = "String";
				ReturnObject["Value"] = m_Token.f_Get<0>();
				return Return;
			}
		case 1: return CEJSON::fs_FromJSON(m_Token.f_Get<1>().f_Get().f_ToJSON().f_UserType().m_Value);
		default: DMibNeverGetHere;
		}

		return {};
	}

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

	NEncoding::CEJSON CBuildSystemSyntax::CEvalString::f_ToJSONArray(bool _bRawString) const
	{
		CEJSON Return;

		auto &Array = Return.f_Array();
		for (auto &Token : m_Tokens)
			Array.f_Insert(Token.f_ToJSON(_bRawString));

		return Return;
	}

	NEncoding::CEJSON CBuildSystemSyntax::CEvalString::f_ToJSON() const
	{
		CEJSON Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = "BuildSystemToken";
		UserType.m_Value["Type"] = "EvalString";
		UserType.m_Value["Value"] = f_ToJSONArray(false).f_ToJSON();

		return Return;
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

	NEncoding::CEJSON CBuildSystemSyntax::CWildcardString::f_ToJSON() const
	{
		CEJSON Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = "BuildSystemToken";
		UserType.m_Value["Type"] = "WildcardString";
		auto &Value = UserType.m_Value["Value"];

		switch (m_String.f_GetTypeID())
		{
		case 0: Value = m_String.f_Get<0>(); break;
		case 1: Value = m_String.f_Get<1>().f_ToJSON().f_UserType().m_Value; break;
		default: DMibNeverGetHere;
		}

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
