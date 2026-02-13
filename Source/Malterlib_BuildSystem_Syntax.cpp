// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem_Registry.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
#if DMibPPtrBits == 64
	static_assert(sizeof(CEJsonSorted) == sizeof(void *) * 2);
	static_assert(sizeof(CJsonSorted) == sizeof(void *) * 2);
#endif

	void CBuildSystemSyntax::fs_FormatString(NStr::CStr &o_String, NStr::CStr const &_SourceString)
	{
		NStr::CStr::CAppender Appender(o_String);
		CBuildSystemParseContext::fs_GenerateString<CBuildSystemParseContext>(Appender, _SourceString);
	}

	void CBuildSystemSyntax::fs_FormatString(NStr::CStrNonTracked &o_String, NStr::CStr const &_SourceString)
	{
		NStr::CStrNonTracked::CAppender Appender(o_String);
		CBuildSystemParseContext::fs_GenerateString<CBuildSystemParseContext>(Appender, _SourceString);
	}

	void CBuildSystemSyntax::fs_FormatKeyString(NStr::CStr &o_String, NStr::CStr const &_SourceString)
	{
		NStr::CStr::CAppender Appender(o_String);
		CBuildSystemParseContext::fs_GenerateKeyString<CBuildSystemParseContext>(Appender, _SourceString);
	}

	void CBuildSystemSyntax::fs_FormatKeyString(NStr::CStrNonTracked &o_String, NStr::CStr const &_SourceString)
	{
		NStr::CStrNonTracked::CAppender Appender(o_String);
		CBuildSystemParseContext::fs_GenerateKeyString<CBuildSystemParseContext>(Appender, _SourceString);
	}

	bool CBuildSystemSyntax::CValue::f_IsIdentifier() const
	{
		if (!m_Value.f_IsOfType<CBuildSystemSyntax::CExpression>())
			return false;

		auto &Expression = m_Value.f_GetAsType<CBuildSystemSyntax::CExpression>();
		if (!Expression.m_Expression.f_IsOfType<CBuildSystemSyntax::CParam>())
			return false;

		auto &Param = Expression.m_Expression.f_GetAsType<CBuildSystemSyntax::CParam>();

		if (!Param.m_Param.f_IsOfType<NStorage::TCIndirection<CIdentifier>>())
			return false;

		return true;
	}

	CBuildSystemSyntax::CIdentifier const &CBuildSystemSyntax::CValue::f_Identifier() const
	{
		auto &Expression = m_Value.f_GetAsType<CBuildSystemSyntax::CExpression>();
		auto &Param = Expression.m_Expression.f_GetAsType<CBuildSystemSyntax::CParam>();
		return Param.m_Param.f_GetAsType<NStorage::TCIndirection<CIdentifier>>().f_Get();
	}

	bool CBuildSystemSyntax::CValue::f_IsConstantString() const
	{
		return m_Value.f_IsOfType<NEncoding::CEJsonSorted>() && m_Value.f_GetAsType<NEncoding::CEJsonSorted>().f_IsString();
	}

	NStr::CStr const &CBuildSystemSyntax::CValue::f_ConstantString() const
	{
		return m_Value.f_GetAsType<NEncoding::CEJsonSorted>().f_String();
	}

	bool CBuildSystemSyntax::CValue::f_IsConstant() const
	{
		return m_Value.f_IsOfType<NEncoding::CEJsonSorted>();
	}

	NEncoding::CEJsonSorted const &CBuildSystemSyntax::CValue::f_Constant() const
	{
		return m_Value.f_GetAsType<NEncoding::CEJsonSorted>();
	}

	bool CBuildSystemSyntax::CValue::f_IsValid() const
	{
		return !m_Value.f_IsOfType<NEncoding::CEJsonSorted>() || m_Value.f_GetAsType<NEncoding::CEJsonSorted>().f_IsValid();
	}

	bool CBuildSystemSyntax::CValue::f_IsArray() const
	{
		return m_Value.f_IsOfType<CArray>();
	}

	CBuildSystemSyntax::CArray const &CBuildSystemSyntax::CValue::f_Array() const
	{
		return m_Value.f_GetAsType<CArray>();
	}

	bool CBuildSystemSyntax::CValue::f_IsEvalString() const
	{
		return m_Value.f_IsOfType<CEvalString>();
	}

	CBuildSystemSyntax::CEvalString const &CBuildSystemSyntax::CValue::f_EvalString() const
	{
		return m_Value.f_GetAsType<CEvalString>();
	}

	bool CBuildSystemSyntax::CValue::f_IsExpression() const
	{
		return m_Value.f_IsOfType<CExpression>();
	}

	CBuildSystemSyntax::CExpression const &CBuildSystemSyntax::CValue::f_Expression() const
	{
		return m_Value.f_GetAsType<CExpression>();
	}

	auto CBuildSystemSyntax::CValue::fs_FromJsonToken
		(
			CStringCache &o_StringCache
			, NEncoding::CEJsonSorted const &_Token
			, CStr const &_TokenType
			, CFilePosition const &_Position
			, bool _bAppendAllowed
		) -> CVariant
	{
		if (_TokenType == gc_ConstString_Expression.m_String || _TokenType == gc_ConstString_AppendExpression.m_String)
		{
			auto pParam = _Token.f_GetMember(gc_ConstString_Param);
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Param member");

			auto pParen = _Token.f_GetMember(gc_ConstString_Paren, EJsonType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Paren member");

			if (_TokenType == gc_ConstString_AppendExpression.m_String)
				return CExpressionAppend::fs_FromJson(o_StringCache, *pParam, _Position, pParen->f_Boolean());
			else
				return CExpression::fs_FromJson(o_StringCache, *pParam, _Position, pParen->f_Boolean());
		}
		else if (_TokenType == gc_ConstString_EvalString.m_String)
		{
			auto pValue = _Token.f_GetMember(gc_ConstString_Value, EJsonType_Array);
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "Eval string does not have valid Value member");
			return CEvalString::fs_FromJson(o_StringCache, *pValue, _Position);
		}
		else if (_TokenType == gc_ConstString_WildcardString.m_String)
			return CWildcardString::fs_FromJson(o_StringCache, _Token, _Position);
		else if (_TokenType == gc_ConstString_Operator.m_String)
			return COperator::fs_FromJson(o_StringCache, _Token, _Position, _bAppendAllowed);
		else if (_TokenType == gc_ConstString_TypeDefaulted.m_String)
			return CBuildSystemSyntax::CDefine{CBuildSystemSyntax::CType{CBuildSystemSyntax::CTypeDefaulted::fs_FromJson(o_StringCache, _Token, _Position)}};
		else if (_TokenType == gc_ConstString_Define.m_String)
			return CDefine::fs_FromJson(o_StringCache, _Token, _Position);
		else if (_TokenType == gc_ConstString_FunctionType.m_String)
			return CBuildSystemSyntax::CDefine{.m_Type = {CFunctionType::fs_FromJson(o_StringCache, _Token, _Position)}};
		else
			CBuildSystem::fs_ThrowError(_Position, "Unknown build system token type: {}"_f << _TokenType);
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CValue::f_ToJson() const
	{
		switch (m_Value.f_GetTypeID())
		{
		case 0: return m_Value.f_Get<0>();
		case 1: return m_Value.f_Get<1>().f_ToJson();
		case 2: return m_Value.f_Get<2>().f_ToJson();
		case 3: return m_Value.f_Get<3>().f_ToJson();
		case 4: return m_Value.f_Get<4>().f_ToJson();
		case 5: return m_Value.f_Get<5>().f_ToJson();
		case 6: return m_Value.f_Get<6>().f_ToJson();
		case 7: return m_Value.f_Get<7>().f_ToJson();
		case 8: return m_Value.f_Get<8>().f_ToJson();
		}

		DMibNeverGetHere;
		return {};
	}

	auto CBuildSystemSyntax::CValue::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed) -> CValue
	{
		CValue Return;

		if (_Json.f_IsUserType())
		{
			auto &UserType = _Json.f_UserType();
			if (UserType.m_Type != gc_ConstString_BuildSystemToken.m_String)
				CBuildSystem::fs_ThrowError(_Position, "Invalid value user type");

			auto Token = CEJsonSorted::fs_FromJson(UserType.m_Value);

			if (!Token.f_IsObject())
				CBuildSystem::fs_ThrowError(_Position, "Token is not object");

			auto pType = Token.f_GetMember(gc_ConstString_Type, EJsonType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Token does not have valid Type member");

			auto &TokenType = pType->f_String();

			Return.m_Value = fs_FromJsonToken(o_StringCache, Token, TokenType, _Position, _bAppendAllowed);
		}
		else if (_Json.f_IsArray())
			Return.m_Value = CArray::fs_FromJson(o_StringCache, _Json, _Position, _bAppendAllowed);
		else if (_Json.f_IsObject())
			Return.m_Value = CObject::fs_FromJson(o_StringCache, _Json, _Position, _bAppendAllowed);
		else
			Return.m_Value = _Json;

		return Return;
	}

	CEJsonSorted CBuildSystemSyntax::CRootValue::f_ToJson() const
	{
		if (!m_Accessors.f_IsEmpty())
		{
			CEJsonSorted Return;
			auto &UserType = Return.f_UserType();
			UserType.m_Type = gc_ConstString_BuildSystemToken;

			auto &Object = UserType.m_Value.f_Object();
			Object[gc_ConstString_Type] = gc_ConstString_RootValue;
			Object[gc_ConstString_Value] = m_Value.f_ToJson().f_ToJson();
			auto &Accessors = Object[gc_ConstString_Accessors].f_Array();

			for (auto &Accessor : m_Accessors)
				Accessors.f_Insert(Accessor.f_ToJson().f_ToJson());

			return Return;
		}
		else
			return m_Value.f_ToJson();
	}

	auto CBuildSystemSyntax::CRootValue::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed) -> CRootValue
	{
		CRootValue Return;

		if (!_Json.f_IsUserType())
		{
			Return.m_Value = CValue::fs_FromJson(o_StringCache, _Json, _Position, _bAppendAllowed);
			return Return;
		}

		auto &UserType = _Json.f_UserType();
		if (UserType.m_Type != gc_ConstString_BuildSystemToken.m_String)
			CBuildSystem::fs_ThrowError(_Position, "Invalid value user type");

		auto Token = CEJsonSorted::fs_FromJson(UserType.m_Value);

		if (!Token.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "Token is not object");

		auto pType = Token.f_GetMember(gc_ConstString_Type, EJsonType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Token does not have valid Type member");

		auto &TokenType = pType->f_String();
		if (TokenType == gc_ConstString_RootValue.m_String)
		{
			auto pValue = Token.f_GetMember(gc_ConstString_Value);
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "RootValue token does not have a valid Value member");

			Return.m_Value = CValue::fs_FromJson(o_StringCache, *pValue, _Position, _bAppendAllowed);

			auto pAccessors = Token.f_GetMember(gc_ConstString_Accessors, EJsonType_Array);
			if (pAccessors)
			{
				for (auto &Accessor : pAccessors->f_Array())
					Return.m_Accessors.f_Insert(CJsonAccessorEntry::fs_FromJson(o_StringCache, Accessor, _Position));
			}
		}
		else
			Return.m_Value.m_Value = CValue::fs_FromJsonToken(o_StringCache, Token, TokenType, _Position, _bAppendAllowed);

		return Return;
	}

	CEJsonSorted CBuildSystemSyntax::CRootKey::f_ToJson() const
	{
		switch (m_Value.f_GetTypeID())
		{
		case 0: return m_Value.f_GetAsType<CValue>().f_ToJson();
		case 1: return m_Value.f_GetAsType<CKeyPrefixOperator>().f_ToJson();
		case 2: return m_Value.f_GetAsType<CKeyLogicalOperator>().f_ToJson();
		case 3: return m_Value.f_GetAsType<CNamespace>().f_ToJson();
		}

		DMibNeverGetHere;
		return {};
	}

	auto CBuildSystemSyntax::CRootKey::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> CRootKey
	{
		CRootKey Return;

		if (!_Json.f_IsUserType())
		{
			Return.m_Value = CValue::fs_FromJson(o_StringCache, _Json, _Position, false);
			return Return;
		}

		auto &UserType = _Json.f_UserType();
		if (UserType.m_Type != gc_ConstString_BuildSystemToken.m_String)
			CBuildSystem::fs_ThrowError(_Position, "Invalid value user type");

		auto Token = CEJsonSorted::fs_FromJson(UserType.m_Value);

		if (!Token.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "Token is not object");

		auto pType = Token.f_GetMember(gc_ConstString_Type, EJsonType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Token does not have valid Type member");

		auto &TokenType = pType->f_String();
		if (TokenType == gc_ConstString_KeyLogicalOperator.m_String)
			Return.m_Value = CKeyLogicalOperator::fs_FromJson(_Json, _Position);
		else if (TokenType == gc_ConstString_KeyPrefixOperator.m_String)
			Return.m_Value = CKeyPrefixOperator::fs_FromJson(o_StringCache, _Json, _Position);
		else if (TokenType == gc_ConstString_Namespace.m_String)
			Return.m_Value = CNamespace::fs_FromJson(o_StringCache, _Json, _Position);
		else
			Return.m_Value = CValue::fs_FromJson(o_StringCache, _Json, _Position, false);

		return Return;
	}
}
