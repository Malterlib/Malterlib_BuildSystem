// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	void CBuildSystemSyntax::fs_FormatString(NStr::CStrAggregate &o_String, NStr::CStr const &_SourceString)
	{
		CBuildSystemParseContext::fs_GenerateString<CBuildSystemParseContext>(o_String, _SourceString);
	}

	void CBuildSystemSyntax::fs_FormatString(NStr::CStrAggregateNonTracked &o_String, NStr::CStr const &_SourceString)
	{
		CBuildSystemParseContext::fs_GenerateString<CBuildSystemParseContext>(o_String, _SourceString);
	}

	void CBuildSystemSyntax::fs_FormatKeyString(NStr::CStrAggregate &o_String, NStr::CStr const &_SourceString)
	{
		CBuildSystemParseContext::fs_GenerateKeyString<CBuildSystemParseContext>(o_String, _SourceString);
	}

	void CBuildSystemSyntax::fs_FormatKeyString(NStr::CStrAggregateNonTracked &o_String, NStr::CStr const &_SourceString)
	{
		CBuildSystemParseContext::fs_GenerateKeyString<CBuildSystemParseContext>(o_String, _SourceString);
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
		return m_Value.f_IsOfType<NEncoding::CEJSON>() && m_Value.f_GetAsType<NEncoding::CEJSON>().f_IsString();
	}

	NStr::CStr const &CBuildSystemSyntax::CValue::f_ConstantString() const
	{
		return m_Value.f_GetAsType<NEncoding::CEJSON>().f_String();
	}

	bool CBuildSystemSyntax::CValue::f_IsConstant() const
	{
		return m_Value.f_IsOfType<NEncoding::CEJSON>();
	}

	NEncoding::CEJSON const &CBuildSystemSyntax::CValue::f_Constant() const
	{
		return m_Value.f_GetAsType<NEncoding::CEJSON>();
	}

	bool CBuildSystemSyntax::CValue::f_IsValid() const
	{
		return !m_Value.f_IsOfType<NEncoding::CEJSON>() || m_Value.f_GetAsType<NEncoding::CEJSON>().f_IsValid();
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

 	auto CBuildSystemSyntax::CValue::fs_FromJSONToken(NEncoding::CEJSON const &_Token, CStr const &_TokenType, CFilePosition const &_Position, bool _bAppendAllowed) -> CVariant
	{
		if (_TokenType == "Expression" || _TokenType == "AppendExpression")
		{
			auto pParam = _Token.f_GetMember("Param");
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Param member");

			auto pParen = _Token.f_GetMember("Paren", EJSONType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Paren member");

			if (_TokenType == "AppendExpression")
				return CExpressionAppend::fs_FromJSON(*pParam, _Position, pParen->f_Boolean());
			else
				return CExpression::fs_FromJSON(*pParam, _Position, pParen->f_Boolean());
		}
		else if (_TokenType == "EvalString")
		{
			auto pValue = _Token.f_GetMember("Value", EJSONType_Array);
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "Eval string does not have valid Value member");
			return CEvalString::fs_FromJSON(*pValue, _Position);
		}
		else if (_TokenType == "WildcardString")
			return CWildcardString::fs_FromJSON(_Token, _Position);
		else if (_TokenType == "Operator")
			return COperator::fs_FromJSON(_Token, _Position, _bAppendAllowed);
		else if (_TokenType == "TypeDefaulted")
			return CBuildSystemSyntax::CDefine{CBuildSystemSyntax::CType{CBuildSystemSyntax::CTypeDefaulted::fs_FromJSON(_Token, _Position)}};
		else if (_TokenType == "Define")
			return CDefine::fs_FromJSON(_Token, _Position);
		else if (_TokenType == "FunctionType")
			return CBuildSystemSyntax::CDefine{CFunctionType::fs_FromJSON(_Token, _Position)};
		else
			CBuildSystem::fs_ThrowError(_Position, "Unknown build system token type: {}"_f << _TokenType);
	}

	NEncoding::CEJSON CBuildSystemSyntax::CValue::f_ToJSON() const
	{
		switch (m_Value.f_GetTypeID())
		{
		case 0: return m_Value.f_Get<0>();
		case 1: return m_Value.f_Get<1>().f_ToJSON();
		case 2: return m_Value.f_Get<2>().f_ToJSON();
		case 3: return m_Value.f_Get<3>().f_ToJSON();
		case 4: return m_Value.f_Get<4>().f_ToJSON();
		case 5: return m_Value.f_Get<5>().f_ToJSON();
		case 6: return m_Value.f_Get<6>().f_ToJSON();
		case 7: return m_Value.f_Get<7>().f_ToJSON();
		case 8: return m_Value.f_Get<8>().f_ToJSON();
		}

		DMibNeverGetHere;
		return {};
	}

	auto CBuildSystemSyntax::CValue::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position, bool _bAppendAllowed) -> CValue
	{
		CValue Return;

		if (_JSON.f_IsUserType())
		{
			auto &UserType = _JSON.f_UserType();
			if (UserType.m_Type != "BuildSystemToken")
				CBuildSystem::fs_ThrowError(_Position, "Invalid value user type");

			auto Token = CEJSON::fs_FromJSON(UserType.m_Value);

			if (!Token.f_IsObject())
				CBuildSystem::fs_ThrowError(_Position, "Token is not object");

  			auto pType = Token.f_GetMember("Type", EJSONType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Token does not have valid Type member");

			auto &TokenType = pType->f_String();

			Return.m_Value = fs_FromJSONToken(Token, TokenType, _Position, _bAppendAllowed);
		}
		else if (_JSON.f_IsArray())
			Return.m_Value = CArray::fs_FromJSON(_JSON, _Position, _bAppendAllowed);
		else if (_JSON.f_IsObject())
			Return.m_Value = CObject::fs_FromJSON(_JSON, _Position, _bAppendAllowed);
		else
			Return.m_Value = _JSON;

		return Return;
	}

	CEJSON CBuildSystemSyntax::CRootValue::f_ToJSON() const
 	{
		if (!m_Accessors.f_IsEmpty())
		{
			CEJSON Return;
			auto &UserType = Return.f_UserType();
			UserType.m_Type = "BuildSystemToken";
			UserType.m_Value["Type"] = "RootValue";
			UserType.m_Value["Value"] = m_Value.f_ToJSON().f_ToJSON();
			auto &Accessors = UserType.m_Value["Accessors"].f_Array();

			for (auto &Accessor : m_Accessors)
				Accessors.f_Insert(Accessor.f_ToJSON().f_ToJSON());

			return Return;
		}
		else
			return m_Value.f_ToJSON();
	}

	auto CBuildSystemSyntax::CRootValue::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position, bool _bAppendAllowed) -> CRootValue
 	{
		CRootValue Return;

		if (!_JSON.f_IsUserType())
		{
			Return.m_Value = CValue::fs_FromJSON(_JSON, _Position, _bAppendAllowed);
			return Return;
		}

		auto &UserType = _JSON.f_UserType();
		if (UserType.m_Type != "BuildSystemToken")
			CBuildSystem::fs_ThrowError(_Position, "Invalid value user type");

		auto Token = CEJSON::fs_FromJSON(UserType.m_Value);

		if (!Token.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "Token is not object");

		auto pType = Token.f_GetMember("Type", EJSONType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Token does not have valid Type member");

		auto &TokenType = pType->f_String();
		if (TokenType == "RootValue")
		{
			auto pValue = Token.f_GetMember("Value");
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "RootValue token does not have a valid Value member");

			Return.m_Value = CValue::fs_FromJSON(*pValue, _Position, _bAppendAllowed);

			auto pAccessors = Token.f_GetMember("Accessors", EJSONType_Array);
			if (pAccessors)
			{
				for (auto &Accessor : pAccessors->f_Array())
					Return.m_Accessors.f_Insert(CJSONAccessorEntry::fs_FromJSON(Accessor, _Position));
			}
		}
		else
			Return.m_Value.m_Value = CValue::fs_FromJSONToken(Token, TokenType, _Position, _bAppendAllowed);

		return Return;
	}

	CEJSON CBuildSystemSyntax::CRootKey::f_ToJSON() const
	{
		switch (m_Value.f_GetTypeID())
		{
		case 0: return m_Value.f_GetAsType<CValue>().f_ToJSON();
		case 1: return m_Value.f_GetAsType<CKeyPrefixOperator>().f_ToJSON();
		case 2: return m_Value.f_GetAsType<CKeyLogicalOperator>().f_ToJSON();
		}

		DMibNeverGetHere;
		return {};
	}

	auto CBuildSystemSyntax::CRootKey::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> CRootKey
 	{
		CRootKey Return;

		if (!_JSON.f_IsUserType())
		{
			Return.m_Value = CValue::fs_FromJSON(_JSON, _Position, false);
			return Return;
		}

		auto &UserType = _JSON.f_UserType();
		if (UserType.m_Type != "BuildSystemToken")
			CBuildSystem::fs_ThrowError(_Position, "Invalid value user type");

		auto Token = CEJSON::fs_FromJSON(UserType.m_Value);

		if (!Token.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "Token is not object");

		auto pType = Token.f_GetMember("Type", EJSONType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Token does not have valid Type member");

		auto &TokenType = pType->f_String();
		if (TokenType == "KeyLogicalOperator")
			Return.m_Value = CKeyLogicalOperator::fs_FromJSON(_JSON, _Position);
		else if (TokenType == "KeyPrefixOperator")
			Return.m_Value = CKeyPrefixOperator::fs_FromJSON(_JSON, _Position);
		else
			Return.m_Value = CValue::fs_FromJSON(_JSON, _Position, false);

		return Return;
	}
}
