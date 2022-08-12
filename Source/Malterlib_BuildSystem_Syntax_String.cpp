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

	NEncoding::CEJSONSorted CBuildSystemSyntax::CEvalStringToken::f_ToJSON(bool _bRawString) const
	{
		switch (m_Token.f_GetTypeID())
		{
		case 0:
			{
				if (_bRawString)
					return m_Token.f_Get<0>();
				
				CEJSONSorted Return;
				auto &ReturnObject = Return.f_Object();
				ReturnObject[gc_ConstString_Type] = gc_ConstString_String;
				ReturnObject[gc_ConstString_Value] = m_Token.f_Get<0>();
				return Return;
			}
		case 1: return CEJSONSorted::fs_FromJSON(m_Token.f_Get<1>().f_Get().f_ToJSON().f_UserType().m_Value);
		default: DMibNeverGetHere;
		}

		return {};
	}

	auto CBuildSystemSyntax::CEvalStringToken::fs_FromJSON(CStringCache &o_StringCache, CEJSONSorted const &_JSON, CFilePosition const &_Position) -> CEvalStringToken
	{
		if (_JSON.f_IsString())
			return CEvalStringToken{_JSON.f_String()};

		auto pType = _JSON.f_GetMember(gc_ConstString_Type, EJSONType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Eval string token does not have valid Type member");

		if (pType->f_String() == gc_ConstString_String.m_String)
		{
			auto pValue = _JSON.f_GetMember(gc_ConstString_Value, EJSONType_String);
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "Eval string string token does not have valid Value member");
			return CEvalStringToken{pValue->f_String()};
		}
		else if (pType->f_String() == gc_ConstString_Expression.m_String)
		{
			auto pParam = _JSON.f_GetMember(gc_ConstString_Param);
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "Eval string expression token does not have valid Param member");

			auto pParen = _JSON.f_GetMember(gc_ConstString_Paren, EJSONType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "Eval string expression does not have valid Paren member");

			return CEvalStringToken{CExpression::fs_FromJSON(o_StringCache, *pParam, _Position, pParen->f_Boolean())};
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Eval string token does not have known Type: {}"_f << pType->f_String());

		return {};
	}

	NEncoding::CEJSONSorted CBuildSystemSyntax::CEvalString::f_ToJSONArray(bool _bRawString) const
	{
		CEJSONSorted Return;

		auto &Array = Return.f_Array();
		for (auto &Token : m_Tokens)
			Array.f_Insert(Token.f_ToJSON(_bRawString));

		return Return;
	}

	NEncoding::CEJSONSorted CBuildSystemSyntax::CEvalString::f_ToJSON() const
	{
		CEJSONSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;
		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_EvalString;
		Object[gc_ConstString_Value] = f_ToJSONArray(false).f_ToJSON();

		return Return;
	}

	auto CBuildSystemSyntax::CEvalString::fs_FromJSON(CStringCache &o_StringCache, CEJSONSorted const &_JSON, CFilePosition const &_Position) -> CEvalString
	{
		if (!_JSON.f_IsArray())
			CBuildSystem::fs_ThrowError(_Position, "Eval string tokens are not an array");

		CEvalString Return;

		for (auto &Token : _JSON.f_Array())
			Return.m_Tokens.f_Insert(CEvalStringToken::fs_FromJSON(o_StringCache, Token, _Position));

		return Return;
	}

	NEncoding::CEJSONSorted CBuildSystemSyntax::CWildcardString::f_ToJSON() const
	{
		CEJSONSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_WildcardString;
		auto &Value = Object[gc_ConstString_Value];

		switch (m_String.f_GetTypeID())
		{
		case 0: Value = m_String.f_Get<0>(); break;
		case 1: Value = m_String.f_Get<1>().f_ToJSON().f_UserType().m_Value; break;
		default: DMibNeverGetHere;
		}

		return Return;
	}

	auto CBuildSystemSyntax::CWildcardString::fs_FromJSON(CStringCache &o_StringCache, CEJSONSorted const &_JSON, CFilePosition const &_Position) -> CWildcardString
	{
		auto &Value = _JSON;
		DMibRequire
			(
				Value.f_GetMember(gc_ConstString_Type, EJSONType_String) && Value.f_GetMember(gc_ConstString_Type, EJSONType_String)->f_String() == gc_ConstString_WildcardString.m_String
			)
		;

		if (auto pValue = Value.f_GetMember(gc_ConstString_Value, EJSONType_String))
			return CWildcardString{pValue->f_String()};
		else if (auto pValue = Value.f_GetMember(gc_ConstString_Value, EJSONType_Object))
		{
			auto &Object = pValue->f_Object();
			auto *pType = Object.f_GetMember(gc_ConstString_Type, EJSONType_String);
			if (!pType || pType->f_String() != gc_ConstString_EvalString.m_String)
				CBuildSystem::fs_ThrowError(_Position, "Invalid wildcard token type: '{}'"_f << pType->f_String());

			auto pTokenArray = Object.f_GetMember(gc_ConstString_Value, EJSONType_Array);
			if (!pTokenArray)
				CBuildSystem::fs_ThrowError(_Position, "Invalid Value for wildcard token type");

			return CWildcardString{CEvalString::fs_FromJSON(o_StringCache, *pTokenArray, _Position)};
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Wildcard string token does not have valid Value member");

		return {};
	}
}
