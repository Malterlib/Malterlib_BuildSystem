// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	COrdering_Partial CBuildSystemSyntax::CObject::CObjectValue::operator <=> (CObjectValue const &_Right) const
	{
		return m_Value <=> _Right.m_Value;
	}

	bool CBuildSystemSyntax::CObject::CObjectValue::operator == (CObjectValue const &_Right) const
	{
		return m_Value == _Right.m_Value;
	}

	COrdering_Partial CBuildSystemSyntax::CObject::operator <=> (CObject const &_Right) const
	{
		return m_Object <=> _Right.m_Object;
	}

	bool CBuildSystemSyntax::CObject::operator == (CObject const &_Right) const
	{
		return m_Object == _Right.m_Object;
	}

	COrdering_Partial CBuildSystemSyntax::CClassType::operator <=> (CClassType const &_Right) const
	{
		return m_Members <=> _Right.m_Members;
	}

	bool CBuildSystemSyntax::CClassType::operator == (CClassType const &_Right) const
	{
		return m_Members == _Right.m_Members;
	}

	COrdering_Partial CBuildSystemSyntax::CClassType::CMember::operator <=> (CMember const &_Right) const
	{
		return fg_TupleReferences(m_Type, m_bOptional) <=> fg_TupleReferences(_Right.m_Type, _Right.m_bOptional);
	}

	bool CBuildSystemSyntax::CClassType::CMember::operator == (CMember const &_Right) const
	{
		return fg_TupleReferences(m_Type, m_bOptional) == fg_TupleReferences(_Right.m_Type, _Right.m_bOptional);
	}


	COrdering_Partial CBuildSystemSyntax::CEvalStringToken::operator <=> (CEvalStringToken const &_Right) const
	{
		return m_Token <=> _Right.m_Token;
	}

	bool CBuildSystemSyntax::CEvalStringToken::operator == (CEvalStringToken const &_Right) const
	{
		return m_Token == _Right.m_Token;
	}

	COrdering_Partial CBuildSystemSyntax::CEvalString::operator <=> (CEvalString const &_Right) const
	{
		return m_Tokens <=> _Right.m_Tokens;
	}

	bool CBuildSystemSyntax::CEvalString::operator == (CEvalString const &_Right) const
	{
		return m_Tokens == _Right.m_Tokens;
	}

	COrdering_Partial CBuildSystemSyntax::CWildcardString::operator <=> (CWildcardString const &_Right) const
	{
		return m_String <=> _Right.m_String;
	}

	bool CBuildSystemSyntax::CWildcardString::operator == (CWildcardString const &_Right) const
	{
		return m_String == _Right.m_String;
	}

	COrdering_Partial CBuildSystemSyntax::CParam::operator <=> (CParam const &_Right) const
	{
		return m_Param <=> _Right.m_Param;
	}

	bool CBuildSystemSyntax::CParam::operator == (CParam const &_Right) const
	{
		return m_Param == _Right.m_Param;
	}

	COrdering_Partial CBuildSystemSyntax::CFunctionCall::operator <=> (CFunctionCall const &_Right) const
	{
		return fg_TupleReferences(m_Name, m_PropertyType, m_bEmptyPropertyType, m_Params, m_bPostFunction)
			<=> fg_TupleReferences(_Right.m_Name, _Right.m_PropertyType, _Right.m_bEmptyPropertyType, _Right.m_Params, _Right.m_bPostFunction)
		;
	}

	bool CBuildSystemSyntax::CFunctionCall::operator == (CFunctionCall const &_Right) const
	{
		return fg_TupleReferences(m_Name, m_PropertyType, m_bEmptyPropertyType, m_Params, m_bPostFunction)
			== fg_TupleReferences(_Right.m_Name, _Right.m_PropertyType, _Right.m_bEmptyPropertyType, _Right.m_Params, _Right.m_bPostFunction)
		;
	}

	COrdering_Partial CBuildSystemSyntax::CTernary::operator <=> (CTernary const &_Right) const
	{
		return fg_TupleReferences(m_Conditional, m_Left, m_Right) <=> fg_TupleReferences(_Right.m_Conditional, _Right.m_Left, _Right.m_Right);
	}

	bool CBuildSystemSyntax::CTernary::operator == (CTernary const &_Right) const
	{
		return fg_TupleReferences(m_Conditional, m_Left, m_Right) == fg_TupleReferences(_Right.m_Conditional, _Right.m_Left, _Right.m_Right);
	}

	COrdering_Partial CBuildSystemSyntax::CBinaryOperator::operator <=> (CBinaryOperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator, m_Left, m_Right) <=> fg_TupleReferences(_Right.m_Operator, _Right.m_Left, _Right.m_Right);
	}

	bool CBuildSystemSyntax::CBinaryOperator::operator == (CBinaryOperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator, m_Left, m_Right) == fg_TupleReferences(_Right.m_Operator, _Right.m_Left, _Right.m_Right);
	}

	COrdering_Partial CBuildSystemSyntax::CPrefixOperator::operator <=> (CPrefixOperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator, m_Right) <=> fg_TupleReferences(_Right.m_Operator, _Right.m_Right);
	}

	bool CBuildSystemSyntax::CPrefixOperator::operator == (CPrefixOperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator, m_Right) == fg_TupleReferences(_Right.m_Operator, _Right.m_Right);
	}

	COrdering_Partial CBuildSystemSyntax::CExpression::operator <=> (CExpression const &_Right) const
	{
		return m_Expression <=> _Right.m_Expression;
	}

	bool CBuildSystemSyntax::CExpression::operator == (CExpression const &_Right) const
	{
		return m_Expression == _Right.m_Expression;
	}

	COrdering_Partial CBuildSystemSyntax::CExpressionAppend::operator <=> (CExpressionAppend const &_Right) const
	{
		return COrdering_Partial::equivalent;
	}

	bool CBuildSystemSyntax::CExpressionAppend::operator == (CExpressionAppend const &_Right) const
	{
		return true;
	}

	COrdering_Partial CBuildSystemSyntax::CJSONSubscript::operator <=> (CJSONSubscript const &_Right) const
	{
		return m_Index <=> _Right.m_Index;
	}

	bool CBuildSystemSyntax::CJSONSubscript::operator == (CJSONSubscript const &_Right) const
	{
		return m_Index == _Right.m_Index;
	}

	COrdering_Partial CBuildSystemSyntax::CJSONAccessorEntry::operator <=> (CJSONAccessorEntry const &_Right) const
	{
		return m_Accessor <=> _Right.m_Accessor;
	}

	bool CBuildSystemSyntax::CJSONAccessorEntry::operator == (CJSONAccessorEntry const &_Right) const
	{
		return m_Accessor == _Right.m_Accessor;
	}

	COrdering_Partial CBuildSystemSyntax::CJSONAccessor::operator <=> (CJSONAccessor const &_Right) const
	{
		return fg_TupleReferences(m_Param, m_Accessors) <=> fg_TupleReferences(_Right.m_Param, _Right.m_Accessors);
	}

	bool CBuildSystemSyntax::CJSONAccessor::operator == (CJSONAccessor const &_Right) const
	{
		return fg_TupleReferences(m_Param, m_Accessors) == fg_TupleReferences(_Right.m_Param, _Right.m_Accessors);
	}

	COrdering_Partial CBuildSystemSyntax::CIdentifier::operator <=> (CIdentifier const &_Right) const
	{
		return fg_TupleReferences(m_Name, m_EntityType, m_PropertyType) <=> fg_TupleReferences(_Right.m_Name, _Right.m_EntityType, _Right.m_PropertyType);
	}

	bool CBuildSystemSyntax::CIdentifier::operator == (CIdentifier const &_Right) const
	{
		return fg_TupleReferences(m_Name, m_EntityType, m_PropertyType) == fg_TupleReferences(_Right.m_Name, _Right.m_EntityType, _Right.m_PropertyType);
	}

	COrdering_Partial CBuildSystemSyntax::CDefaultType::operator <=> (CDefaultType const &_Right) const
	{
		return m_Type <=> _Right.m_Type;
	}

	bool CBuildSystemSyntax::CDefaultType::operator == (CDefaultType const &_Right) const
	{
		return m_Type == _Right.m_Type;
	}

	COrdering_Partial CBuildSystemSyntax::CTypeDefaulted::operator <=> (CTypeDefaulted const &_Right) const
	{
		return fg_TupleReferences(m_Type, m_DefaultValue) <=> fg_TupleReferences(_Right.m_Type, _Right.m_DefaultValue);
	}

	bool CBuildSystemSyntax::CTypeDefaulted::operator == (CTypeDefaulted const &_Right) const
	{
		return fg_TupleReferences(m_Type, m_DefaultValue) == fg_TupleReferences(_Right.m_Type, _Right.m_DefaultValue);
	}

	COrdering_Partial CBuildSystemSyntax::CUserType::operator <=> (CUserType const &_Right) const
	{
		return m_Name <=> _Right.m_Name;
	}

	bool CBuildSystemSyntax::CUserType::operator == (CUserType const &_Right) const
	{
		return m_Name == _Right.m_Name;
	}

	COrdering_Partial CBuildSystemSyntax::CArrayType::operator <=> (CArrayType const &_Right) const
	{
		return m_Type <=> _Right.m_Type;
	}

	bool CBuildSystemSyntax::CArrayType::operator == (CArrayType const &_Right) const
	{
		return m_Type == _Right.m_Type;
	}

	COrdering_Partial CBuildSystemSyntax::CType::operator <=> (CType const &_Right) const
	{
		return m_Type <=> _Right.m_Type;
	}

	bool CBuildSystemSyntax::CType::operator == (CType const &_Right) const
	{
		return m_Type == _Right.m_Type;
	}

	COrdering_Partial CBuildSystemSyntax::COneOf::operator <=> (COneOf const &_Right) const
	{
		return m_OneOf <=> _Right.m_OneOf;
	}

	bool CBuildSystemSyntax::COneOf::operator == (COneOf const &_Right) const
	{
		return m_OneOf == _Right.m_OneOf;
	}

	COrdering_Partial CBuildSystemSyntax::CDefine::operator <=> (CDefine const &_Right) const
	{
		return m_Type <=> _Right.m_Type;
	}

	bool CBuildSystemSyntax::CDefine::operator == (CDefine const &_Right) const
	{
		return m_Type == _Right.m_Type;
	}

	COrdering_Partial CBuildSystemSyntax::CFunctionParameter::operator <=> (CFunctionParameter const &_Right) const
	{
		return fg_TupleReferences(m_Type, m_ParamType, m_Name) <=> fg_TupleReferences(_Right.m_Type, _Right.m_ParamType, _Right.m_Name);
	}

	bool CBuildSystemSyntax::CFunctionParameter::operator == (CFunctionParameter const &_Right) const
	{
		return fg_TupleReferences(m_Type, m_ParamType, m_Name) == fg_TupleReferences(_Right.m_Type, _Right.m_ParamType, _Right.m_Name);
	}

	COrdering_Partial CBuildSystemSyntax::CFunctionType::operator <=> (CFunctionType const &_Right) const
	{
		return fg_TupleReferences(m_Return, m_Parameters) <=> fg_TupleReferences(_Right.m_Return, _Right.m_Parameters);
	}

	bool CBuildSystemSyntax::CFunctionType::operator == (CFunctionType const &_Right) const
	{
		return fg_TupleReferences(m_Return, m_Parameters) == fg_TupleReferences(_Right.m_Return, _Right.m_Parameters);
	}

	COrdering_Partial CBuildSystemSyntax::COperator::operator <=> (COperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator, m_Right) <=> fg_TupleReferences(_Right.m_Operator, _Right.m_Right);
	}

	bool CBuildSystemSyntax::COperator::operator == (COperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator, m_Right) == fg_TupleReferences(_Right.m_Operator, _Right.m_Right);
	}

	COrdering_Partial CBuildSystemSyntax::CArray::operator <=> (CArray const &_Right) const
	{
		return m_Array <=> _Right.m_Array;
	}

	bool CBuildSystemSyntax::CArray::operator == (CArray const &_Right) const
	{
		return m_Array == _Right.m_Array;
	}

	COrdering_Partial CBuildSystemSyntax::CAppendObject::operator <=> (CAppendObject const &_Right) const
	{
		return COrdering_Partial::equivalent;
	}

	bool CBuildSystemSyntax::CAppendObject::operator == (CAppendObject const &_Right) const
	{
		return true;
	}

	COrdering_Partial CBuildSystemSyntax::CObjectKey::operator <=> (CObjectKey const &_Right) const
	{
		return m_Key <=> _Right.m_Key;
	}

	bool CBuildSystemSyntax::CObjectKey::operator == (CObjectKey const &_Right) const
	{
		return m_Key == _Right.m_Key;
	}

	COrdering_Partial CBuildSystemSyntax::CValue::operator <=> (CValue const &_Right) const
	{
		return m_Value <=> _Right.m_Value;
	}

	bool CBuildSystemSyntax::CValue::operator == (CValue const &_Right) const
	{
		return m_Value == _Right.m_Value;
	}

	COrdering_Partial CBuildSystemSyntax::CRootValue::operator <=> (CRootValue const &_Right) const
	{
		return fg_TupleReferences(m_Value, m_Accessors) <=> fg_TupleReferences(_Right.m_Value, _Right.m_Accessors);
	}

	bool CBuildSystemSyntax::CRootValue::operator == (CRootValue const &_Right) const
	{
		return fg_TupleReferences(m_Value, m_Accessors) == fg_TupleReferences(_Right.m_Value, _Right.m_Accessors);
	}

	COrdering_Partial CBuildSystemSyntax::CRootKey::operator <=> (CRootKey const &_Right) const
	{
		return fg_TupleReferences(m_Value) <=> fg_TupleReferences(_Right.m_Value);
	}

	bool CBuildSystemSyntax::CRootKey::operator == (CRootKey const &_Right) const
	{
		return fg_TupleReferences(m_Value) == fg_TupleReferences(_Right.m_Value);
	}

	COrdering_Partial CBuildSystemSyntax::CKeyPrefixOperator::operator <=> (CKeyPrefixOperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator, m_Right) <=> fg_TupleReferences(_Right.m_Operator, _Right.m_Right);
	}

	bool CBuildSystemSyntax::CKeyPrefixOperator::operator == (CKeyPrefixOperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator, m_Right) == fg_TupleReferences(_Right.m_Operator, _Right.m_Right);
	}

	COrdering_Partial CBuildSystemSyntax::CKeyLogicalOperator::operator <=> (CKeyLogicalOperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator) <=> fg_TupleReferences(_Right.m_Operator);
	}

	bool CBuildSystemSyntax::CKeyLogicalOperator::operator == (CKeyLogicalOperator const &_Right) const
	{
		return fg_TupleReferences(m_Operator) == fg_TupleReferences(_Right.m_Operator);
	}

	aint CBuildSystemSyntax::CRootKey::f_Cmp(CRootKey const &_Right) const
	{
		auto Result = *this <=> _Right;
		if (Result < 0)
			return -1;
		else if (Result > 0)
			return 1;
		return 0;
	}

	bool CBuildSystemSyntax::CRootKey::f_IsValid() const
	{
		if (f_IsValue() && !f_Value().f_IsValid())
			return false;
		return true;
	}

	bool CBuildSystemSyntax::CRootKey::f_IsValue() const
	{
		return m_Value.f_IsOfType<CValue>();
	}

	CBuildSystemSyntax::CValue const &CBuildSystemSyntax::CRootKey::f_Value() const
	{
		return m_Value.f_GetAsType<CValue>();
	}

	bool CBuildSystemSyntax::CRootKey::f_IsKeyPrefixOperator() const
	{
		return m_Value.f_IsOfType<CKeyPrefixOperator>();
	}

	CBuildSystemSyntax::CKeyPrefixOperator const &CBuildSystemSyntax::CRootKey::f_KeyPrefixOperator() const
	{
		return m_Value.f_GetAsType<CKeyPrefixOperator>();
	}

	bool CBuildSystemSyntax::CRootKey::f_IsKeyLogicalOperator() const
	{
		return m_Value.f_IsOfType<CKeyLogicalOperator>();
	}

	CBuildSystemSyntax::CKeyLogicalOperator const &CBuildSystemSyntax::CRootKey::f_KeyLogicalOperator() const
	{
		return m_Value.f_GetAsType<CKeyLogicalOperator>();
	}

}
