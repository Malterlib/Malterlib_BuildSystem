// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

	NEncoding::CEJsonSorted CBuildSystemSyntax::CEvalStringToken::f_ToJson(bool _bRawString) const
	{
		switch (m_Token.f_GetTypeID())
		{
		case 0:
			{
				if (_bRawString)
					return m_Token.f_Get<0>();

				CEJsonSorted Return;
				auto &ReturnObject = Return.f_Object();
				ReturnObject[gc_ConstString_Type] = gc_ConstString_String;
				ReturnObject[gc_ConstString_Value] = m_Token.f_Get<0>();
				return Return;
			}
		case 1: return CEJsonSorted::fs_FromJson(m_Token.f_Get<1>().f_Get().f_ToJson().f_UserType().m_Value);
		default: DMibNeverGetHere;
		}

		return {};
	}

	auto CBuildSystemSyntax::CEvalStringToken::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position) -> CEvalStringToken
	{
		if (_Json.f_IsString())
			return CEvalStringToken{_Json.f_String()};

		auto pType = _Json.f_GetMember(gc_ConstString_Type, EJsonType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Eval string token does not have valid Type member");

		if (pType->f_String() == gc_ConstString_String.m_String)
		{
			auto pValue = _Json.f_GetMember(gc_ConstString_Value, EJsonType_String);
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "Eval string string token does not have valid Value member");
			return CEvalStringToken{pValue->f_String()};
		}
		else if (pType->f_String() == gc_ConstString_Expression.m_String)
		{
			auto pParam = _Json.f_GetMember(gc_ConstString_Param);
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "Eval string expression token does not have valid Param member");

			auto pParen = _Json.f_GetMember(gc_ConstString_Paren, EJsonType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "Eval string expression does not have valid Paren member");

			return CEvalStringToken{CExpression::fs_FromJson(o_StringCache, *pParam, _Position, pParen->f_Boolean())};
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Eval string token does not have known Type: {}"_f << pType->f_String());

		return {};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CEvalString::f_ToJsonArray(bool _bRawString) const
	{
		CEJsonSorted Return;

		auto &Array = Return.f_Array();
		for (auto &Token : m_Tokens)
			Array.f_Insert(Token.f_ToJson(_bRawString));

		return Return;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CEvalString::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;
		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_EvalString;
		Object[gc_ConstString_Value] = f_ToJsonArray(false).f_ToJson();

		return Return;
	}

	auto CBuildSystemSyntax::CEvalString::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position) -> CEvalString
	{
		if (!_Json.f_IsArray())
			CBuildSystem::fs_ThrowError(_Position, "Eval string tokens are not an array");

		CEvalString Return;

		for (auto &Token : _Json.f_Array())
			Return.m_Tokens.f_Insert(CEvalStringToken::fs_FromJson(o_StringCache, Token, _Position));

		return Return;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CWildcardString::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_WildcardString;
		auto &Value = Object[gc_ConstString_Value];

		switch (m_String.f_GetTypeID())
		{
		case 0: Value = m_String.f_Get<0>(); break;
		case 1: Value = m_String.f_Get<1>().f_ToJson().f_UserType().m_Value; break;
		default: DMibNeverGetHere;
		}

		return Return;
	}

	auto CBuildSystemSyntax::CWildcardString::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position) -> CWildcardString
	{
		auto &Value = _Json;
		DMibRequire
			(
				Value.f_GetMember(gc_ConstString_Type, EJsonType_String) && Value.f_GetMember(gc_ConstString_Type, EJsonType_String)->f_String() == gc_ConstString_WildcardString.m_String
			)
		;

		if (auto pValue = Value.f_GetMember(gc_ConstString_Value, EJsonType_String))
			return CWildcardString{pValue->f_String()};
		else if (auto pValue = Value.f_GetMember(gc_ConstString_Value, EJsonType_Object))
		{
			auto &Object = pValue->f_Object();
			auto *pType = Object.f_GetMember(gc_ConstString_Type, EJsonType_String);
			if (!pType || pType->f_String() != gc_ConstString_EvalString.m_String)
				CBuildSystem::fs_ThrowError(_Position, "Invalid wildcard token type: '{}'"_f << pType->f_String());

			auto pTokenArray = Object.f_GetMember(gc_ConstString_Value, EJsonType_Array);
			if (!pTokenArray)
				CBuildSystem::fs_ThrowError(_Position, "Invalid Value for wildcard token type");

			return CWildcardString{CEvalString::fs_FromJson(o_StringCache, *pTokenArray, _Position)};
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Wildcard string token does not have valid Value member");

		return {};
	}
}
