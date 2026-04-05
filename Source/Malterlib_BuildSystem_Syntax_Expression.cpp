// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	NEncoding::CEJsonSorted CBuildSystemSyntax::CKeyLogicalOperator::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();

		Object[gc_ConstString_Type] = gc_ConstString_KeyLogicalOperator;
		auto &Operator = Object[gc_ConstString_Operator];

		switch (m_Operator)
		{
		case EOperator_And: Operator = gc_ConstString_Symbol_OperatorBitwiseAnd; break;
		case EOperator_Or: Operator = gc_ConstString_Symbol_OperatorBitwiseOr; break;
		case EOperator_Not: Operator = gc_ConstString_Symbol_LogicalNot; break;
		}

		return Return;
	}

	CBuildSystemSyntax::CKeyLogicalOperator CBuildSystemSyntax::CKeyLogicalOperator::fs_FromJson(NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position)
	{
		auto &UserType = _Json.f_UserType();
		DMibRequire(UserType.m_Type == gc_ConstString_BuildSystemToken.m_String);

		auto &Value = UserType.m_Value;
		auto *pType = UserType.m_Value.f_GetMember(gc_ConstString_Type);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Missing Type member for KeyLogicalOperator token");

		auto &Type = pType->f_String();

		if (Type != gc_ConstString_KeyLogicalOperator.m_String)
			CBuildSystem::fs_ThrowError(_Position, "'{}' is not a valid type for KeyLogicalOperator"_f << Type);

		auto pOperator = Value.f_GetMember(gc_ConstString_Operator);
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "Missing Operator member for KeyLogicalOperator token");

		CBuildSystemSyntax::CKeyLogicalOperator OutputOperator;

		auto Operator = pOperator->f_String();
		if (Operator == gc_ConstString_Symbol_OperatorBitwiseAnd.m_String)
			OutputOperator.m_Operator = EOperator_And;
		else if (Operator == gc_ConstString_Symbol_OperatorBitwiseOr.m_String)
			OutputOperator.m_Operator = EOperator_Or;
		else if (Operator == gc_ConstString_Symbol_LogicalNot.m_String)
			OutputOperator.m_Operator = EOperator_Not;
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid operator '{}' for KeyLogicalOperator token"_f << Operator);

		return OutputOperator;
	}

	auto CBuildSystemSyntax::CKeyPrefixOperator::fs_TypeFromJson(NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> EOperator
	{
		auto &UserType = _Json.f_UserType();
		DMibRequire(UserType.m_Type == gc_ConstString_BuildSystemToken.m_String);

		auto &Value = UserType.m_Value;
		auto *pType = UserType.m_Value.f_GetMember(gc_ConstString_Type);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Missing Type member for KeyPrefixOperator token");

		auto &Type = pType->f_String();

		if (Type != gc_ConstString_KeyPrefixOperator.m_String)
			CBuildSystem::fs_ThrowError(_Position, "'{}' is not a valid type for KeyPrefixOperator"_f << Type);

		auto pOperator = Value.f_GetMember(gc_ConstString_Operator);
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "Missing Operator member for KeyPrefixOperator token");

		CBuildSystemSyntax::CKeyPrefixOperator OutputOperator;

		auto Operator = pOperator->f_String();
		if (Operator == gc_ConstString_Symbol_LogicalNotNot.m_String)
			return EOperator_Equal;
		else if (Operator == gc_ConstString_Symbol_LogicalNot.m_String)
			return EOperator_NotEqual;
		else if (Operator == gc_ConstString_Symbol_EntityPrefix.m_String)
			return EOperator_Entity;
		else if (Operator == gc_ConstString_Symbol_ConfigurationPrefix.m_String)
			return EOperator_ConfigurationTuple;
		else if (Operator == gc_ConstString_Symbol_PragmaPrefix.m_String)
			return EOperator_Pragma;
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid operator '{}' for KeyPrefixOperator token"_f << Operator);
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CKeyPrefixOperator::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_KeyPrefixOperator;
		auto &Operator = Object[gc_ConstString_Operator];

		switch (m_Operator)
		{
		case EOperator_Equal: Operator = gc_ConstString_Symbol_LogicalNotNot; break;
		case EOperator_NotEqual: Operator = gc_ConstString_Symbol_LogicalNot; break;
		case EOperator_Entity: Operator = gc_ConstString_Symbol_EntityPrefix; break;
		case EOperator_ConfigurationTuple: Operator = gc_ConstString_Symbol_ConfigurationPrefix; break;
		case EOperator_Pragma: Operator = gc_ConstString_Symbol_PragmaPrefix; break;
		}

		Object[gc_ConstString_Right] = m_Right.f_ToJson().f_ToJson();

		return Return;
	}

	CBuildSystemSyntax::CValue CBuildSystemSyntax::CValue::fs_Identifier(CPropertyKeyReference const &_KeyReference)
	{
		return
			{
				CBuildSystemSyntax::CExpression
				{
					CBuildSystemSyntax::CParam
					{
						NStorage::TCIndirection<CBuildSystemSyntax::CIdentifier>
						{
							CBuildSystemSyntax::CIdentifier
							{
								CStringAndHash
								{
									CAssertAddedToStringCache()
									, _KeyReference.m_Name
									, _KeyReference.f_GetNameHash()
								}
								, EEntityType_Invalid
								, _KeyReference.f_GetType()
								, _KeyReference.f_GetType() == EPropertyType_Property
							}
						}
					}
				}
			}
		;
	}

	CBuildSystemSyntax::CValue CBuildSystemSyntax::CValue::fs_Identifier(CStringCache &o_StringCache, CStr const &_Identifier, EPropertyType _PropertyType)
	{
		return
			{
				CBuildSystemSyntax::CExpression
				{
					CBuildSystemSyntax::CParam
					{
						NStorage::TCIndirection<CBuildSystemSyntax::CIdentifier>
						{
							CBuildSystemSyntax::CIdentifier
							{
								CStringAndHash
								{
									o_StringCache
									, _Identifier
									, _Identifier.f_Hash()
								}
								, EEntityType_Invalid
								, _PropertyType
								, _PropertyType == EPropertyType_Property
							}
						}
					}
				}
			}
		;
	}

	CBuildSystemSyntax::CKeyPrefixOperator CBuildSystemSyntax::CKeyPrefixOperator::fs_Entity(CStringCache &o_StringCache, CStr const &_Name)
	{
		return {EOperator_Entity, CBuildSystemSyntax::CValue::fs_Identifier(o_StringCache, _Name)};
	}

	CBuildSystemSyntax::CKeyPrefixOperator CBuildSystemSyntax::CKeyPrefixOperator::fs_FromJson
		(
			CStringCache &o_StringCache
			, EOperator _Operator
			, NEncoding::CEJsonSorted const &_Json
			, CFilePosition const &_Position
		)
	{
		auto &UserType = _Json.f_UserType();
		DMibRequire(UserType.m_Type == gc_ConstString_BuildSystemToken.m_String);

		CBuildSystemSyntax::CKeyPrefixOperator OutputOperator;
		OutputOperator.m_Operator = _Operator;

		auto &Value = UserType.m_Value;
		auto pRight = Value.f_GetMember(gc_ConstString_Right);
		if (!pRight)
			CBuildSystem::fs_ThrowError(_Position, "Missing Right member for PrefixOperator token");

		auto Right = CEJsonSorted::fs_FromJson(*pRight);
		OutputOperator.m_Right = CBuildSystemSyntax::CValue::fs_FromJson(o_StringCache, Right, _Position, false);

		return OutputOperator;
	}

	CBuildSystemSyntax::CKeyPrefixOperator CBuildSystemSyntax::CKeyPrefixOperator::fs_FromJson
		(
			CStringCache &o_StringCache
			, NEncoding::CEJsonSorted const &_Json
			, CFilePosition const &_Position
		)
	{
		return fs_FromJson(o_StringCache, CKeyPrefixOperator::fs_TypeFromJson(_Json, _Position), _Json, _Position);
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CNamespace::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_Namespace;

		return Return;
	}

	CBuildSystemSyntax::CNamespace CBuildSystemSyntax::CNamespace::fs_FromJson
		(
			CStringCache &o_StringCache
			, NEncoding::CEJsonSorted const &_Json
			, CFilePosition const &_Position
		)
	{
		auto &UserType = _Json.f_UserType();
		DMibRequire(UserType.m_Type == gc_ConstString_BuildSystemToken.m_String);

		auto &Value = UserType.m_Value;
		auto *pType = Value.f_GetMember(gc_ConstString_Type);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Missing Type member for Namespace token");

		auto &Type = pType->f_String();

		if (Type != gc_ConstString_Namespace.m_String)
			CBuildSystem::fs_ThrowError(_Position, "'{}' is not a valid type for Namespace"_f << Type);

		return {};
	}

	bool CBuildSystemSyntax::CParam::f_IsBinaryOperator() const
	{
		if (m_Param.f_IsOfType<NStorage::TCIndirection<CBinaryOperator>>())
			return true;

		if (m_Param.f_IsOfType<NStorage::TCIndirection<CExpression>>())
		{
			auto &Expression = m_Param.f_GetAsType<NStorage::TCIndirection<CExpression>>().f_Get();
			if (!Expression.m_bParen && Expression.m_Expression.f_IsOfType<CParam>())
				return Expression.m_Expression.f_GetAsType<CParam>().f_IsBinaryOperator();
		}

		return false;
	}

	CBuildSystemSyntax::CBinaryOperator &CBuildSystemSyntax::CParam::f_BinaryOperator()
	{
		if (m_Param.f_IsOfType<NStorage::TCIndirection<CExpression>>())
		{
			auto &Expression = m_Param.f_GetAsType<NStorage::TCIndirection<CExpression>>().f_Get();
			if (!Expression.m_bParen && Expression.m_Expression.f_IsOfType<CParam>())
				return Expression.m_Expression.f_GetAsType<CParam>().f_BinaryOperator();
		}

		return m_Param.f_GetAsType<NStorage::TCIndirection<CBinaryOperator>>();
	}

	CBuildSystemSyntax::CBinaryOperator const &CBuildSystemSyntax::CParam::f_BinaryOperator() const
	{
		if (m_Param.f_IsOfType<NStorage::TCIndirection<CExpression>>())
		{
			auto &Expression = m_Param.f_GetAsType<NStorage::TCIndirection<CExpression>>().f_Get();
			if (!Expression.m_bParen && Expression.m_Expression.f_IsOfType<CParam>())
				return Expression.m_Expression.f_GetAsType<CParam>().f_BinaryOperator();
		}

		return m_Param.f_GetAsType<NStorage::TCIndirection<CBinaryOperator>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsJson() const
	{
		return m_Param.f_IsOfType<NEncoding::CEJsonSorted>();
	}

	NEncoding::CEJsonSorted const &CBuildSystemSyntax::CParam::f_Json() const
	{
		return m_Param.f_GetAsType<NEncoding::CEJsonSorted>();
	}

	bool CBuildSystemSyntax::CParam::f_IsObject() const
	{
		return m_Param.f_IsOfType<CObject>();
	}

	CBuildSystemSyntax::CObject const &CBuildSystemSyntax::CParam::f_Object() const
	{
		return m_Param.f_GetAsType<CObject>();
	}

	bool CBuildSystemSyntax::CParam::f_IsArray() const
	{
		return m_Param.f_IsOfType<CArray>();
	}

	CBuildSystemSyntax::CArray const &CBuildSystemSyntax::CParam::f_Array() const
	{
		return m_Param.f_GetAsType<CArray>();
	}

	bool CBuildSystemSyntax::CParam::f_IsIdentifier() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CIdentifier>>();
	}

	CBuildSystemSyntax::CIdentifier const &CBuildSystemSyntax::CParam::f_Identifier() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CIdentifier>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsEvalString() const
	{
		return m_Param.f_IsOfType<CEvalString>();
	}

	CBuildSystemSyntax::CEvalString const &CBuildSystemSyntax::CParam::f_EvalString() const
	{
		return m_Param.f_GetAsType<CEvalString>();
	}

	bool CBuildSystemSyntax::CParam::f_IsWildcardString() const
	{
		return m_Param.f_IsOfType<CWildcardString>();
	}

	CBuildSystemSyntax::CWildcardString const &CBuildSystemSyntax::CParam::f_WildcardString() const
	{
		return m_Param.f_GetAsType<CWildcardString>();
	}

	bool CBuildSystemSyntax::CParam::f_IsExpression() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CExpression>>();
	}

	CBuildSystemSyntax::CExpression const &CBuildSystemSyntax::CParam::f_Expression() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CExpression>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsExpressionAppend() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CExpressionAppend>>();
	}

	CBuildSystemSyntax::CExpressionAppend const &CBuildSystemSyntax::CParam::f_ExpressionAppend() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CExpressionAppend>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsTernary() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CTernary>>();
	}

	CBuildSystemSyntax::CTernary const &CBuildSystemSyntax::CParam::f_Ternary() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CTernary>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsPrefixOperator() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CPrefixOperator>>();
	}

	CBuildSystemSyntax::CPrefixOperator const &CBuildSystemSyntax::CParam::f_PrefixOperator() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CPrefixOperator>>();
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CParam::f_ToJson() const
	{
		switch (m_Param.f_GetTypeID())
		{
		case 0: return m_Param.f_Get<0>();
		case 1: return m_Param.f_Get<1>().f_ToJson();
		case 2: return m_Param.f_Get<2>().f_ToJson();
		case 3: return m_Param.f_Get<3>().f_Get().f_ToJson();
		case 4: return m_Param.f_Get<4>().f_Get().f_ToJson();
		case 5: return m_Param.f_Get<5>().f_ToJson();
		case 6: return m_Param.f_Get<6>().f_ToJson();
		case 7: return m_Param.f_Get<7>().f_Get().f_ToJson();
		case 8: return m_Param.f_Get<8>().f_Get().f_ToJson();
		case 9: return m_Param.f_Get<9>().f_Get().f_ToJson();
		case 10: return m_Param.f_Get<10>().f_Get().f_ToJson();
		case 11: return m_Param.f_Get<11>().f_Get().f_ToJson();
		}

		DNeverGetHere;
		return {};
	}

	auto CBuildSystemSyntax::CParam::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position, NStr::CStr const &_Type, bool _bAppendAllowed)
		-> CParam
	{
		if (!_Json.f_IsUserType())
		{
			if (_Json.f_IsArray())
				return CParam{CArray::fs_FromJson(o_StringCache, _Json, _Position, false)};
			else if (_Json.f_IsObject())
				return CParam{CObject::fs_FromJson(o_StringCache, _Json, _Position, false)};
			else
				return CParam{_Json};
		}

		auto &UserType = _Json.f_UserType();
		DMibRequire(UserType.m_Type == gc_ConstString_BuildSystemToken.m_String);

		auto &Value = UserType.m_Value;

		if (_Type == gc_ConstString_Identifier.m_String)
			return CParam{CIdentifier::fs_FromJson(o_StringCache, _Json, _Position)};
		else if (_Type == gc_ConstString_EvalString.m_String)
		{
			auto pValue = Value.f_GetMember(gc_ConstString_Value);
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "Missing Value member for EvalString token"_f << _Type);
			return CParam{CEvalString::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(*pValue), _Position)};
		}
		else if (_Type == gc_ConstString_WildcardString.m_String)
			return CParam{CWildcardString::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(Value), _Position)};
		else if (_Type == gc_ConstString_JsonAccessor.m_String)
		{
			return CParam
				{
					NStorage::TCIndirection<CExpression>
					(
						CExpression{NStorage::TCIndirection<CJsonAccessor>(CJsonAccessor::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(Value), _Position))}
					)
				}
			;
		}
		else if (_Type == gc_ConstString_PostFunction.m_String || _Type == gc_ConstString_Function.m_String)
			return CParam{NStorage::TCIndirection<CExpression>(CExpression{CFunctionCall::fs_FromJson(o_StringCache, Value, _Position, _Type)})};
		else if (_Type == gc_ConstString_Expression.m_String)
		{
			auto pParam = Value.f_GetMember(gc_ConstString_Param);
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Param member");

			auto pParen = Value.f_GetMember(gc_ConstString_Paren, EJsonType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Paren member");

			return CParam{NStorage::TCIndirection<CExpression>(CExpression::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(*pParam), _Position, pParen->f_Boolean()))};
		}
		else if (_Type == gc_ConstString_AppendExpression.m_String)
		{
			if (!_bAppendAllowed)
				CBuildSystem::fs_ThrowError(_Position, "AppendExpression is only allowed as function parameter");

			auto pParam = Value.f_GetMember(gc_ConstString_Param);
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "AppendExpression token does not have valid Param member");

			auto pParen = Value.f_GetMember(gc_ConstString_Paren, EJsonType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "AppendExpression does not have valid Paren member");

			return CParam{NStorage::TCIndirection<CExpressionAppend>(CExpressionAppend::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(*pParam), _Position, pParen->f_Boolean()))};
		}
		else if (_Type == gc_ConstString_IdentifierReference.m_String)
		{
			auto pIdentifier = Value.f_GetMember(gc_ConstString_Identifier);
			if (!pIdentifier)
				CBuildSystem::fs_ThrowError(_Position, "Missing Identifier member for IdentifierReference token"_f << _Type);

			return CParam{NStorage::TCIndirection<CIdentifierReference>{CIdentifierReference{CIdentifier::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(*pIdentifier), _Position)}}};
		}
		else if (_Type == gc_ConstString_Ternary.m_String)
			return CParam{NStorage::TCIndirection<CTernary>(CTernary::fs_FromJson(o_StringCache, Value, _Position))};
		else if (_Type == gc_ConstString_PrefixOperator.m_String)
			return CParam{NStorage::TCIndirection<CPrefixOperator>(CPrefixOperator::fs_FromJson(o_StringCache, Value, _Position))};
		else if (_Type == gc_ConstString_BinaryOperator.m_String)
		{
			auto Operator = CBinaryOperator::fs_FromJson(o_StringCache, Value, _Position);

			if (Operator.m_Right.f_IsBinaryOperator())
			{
				auto fOperatorPrecedence = [](CBinaryOperator::EOperator _Operator) -> uint32
					{
						switch (_Operator)
						{
						case CBinaryOperator::EOperator_LessThan:
						case CBinaryOperator::EOperator_LessThanEqual:
						case CBinaryOperator::EOperator_GreaterThan:
						case CBinaryOperator::EOperator_GreaterThanEqual:
							return 9;

						case CBinaryOperator::EOperator_Equal:
						case CBinaryOperator::EOperator_NotEqual:
						case CBinaryOperator::EOperator_MatchEqual:
						case CBinaryOperator::EOperator_MatchNotEqual:
							return 10;

						case CBinaryOperator::EOperator_Add:
						case CBinaryOperator::EOperator_Subtract:
							return 6;

						case CBinaryOperator::EOperator_Divide:
						case CBinaryOperator::EOperator_Multiply:
						case CBinaryOperator::EOperator_Modulus:
							return 5;

						case CBinaryOperator::EOperator_BitwiseLeftShift:
						case CBinaryOperator::EOperator_BitwiseRightShift:
							return 7;

						case CBinaryOperator::EOperator_BitwiseAnd: return 11;
						case CBinaryOperator::EOperator_BitwiseXor: return 12;
						case CBinaryOperator::EOperator_BitwiseOr: return 13;
						case CBinaryOperator::EOperator_And: return 14;
						case CBinaryOperator::EOperator_Or: return 15;
						case CBinaryOperator::EOperator_NullishCoalescing: return 15;
						}
						return 0;
					}
				;

				auto RightBinary = fg_Move(Operator.m_Right.f_BinaryOperator());

				auto Prededence = fOperatorPrecedence(Operator.m_Operator);
				auto PrededenceRightBinary = fOperatorPrecedence(RightBinary.m_Operator);
				if (PrededenceRightBinary >= Prededence)
				{
					auto Left1 = fg_Move(Operator.m_Left);

					auto OperatorType = Operator.m_Operator;

					Operator = fg_Move(RightBinary);
					auto *pDestination = &Operator;
					while (pDestination->m_Left.f_IsBinaryOperator())
					{
						auto &Next = pDestination->m_Left.f_BinaryOperator();
						auto PrededenceRightBinary = fOperatorPrecedence(Next.m_Operator);
						if (PrededenceRightBinary < Prededence)
							break;
						pDestination = &pDestination->m_Left.f_BinaryOperator();
					}

					auto Left2 = fg_Move(pDestination->m_Left);

					pDestination->m_Left.m_Param = NStorage::TCIndirection<CBinaryOperator>
						(
							CBinaryOperator
							{
								OperatorType
								, fg_Move(Left1)
								, fg_Move(Left2)
							}
						)
					;
				}
				else
					Operator.m_Right.m_Param = NStorage::TCIndirection<CBinaryOperator>(fg_Move(RightBinary));
			}

			return CParam{NStorage::TCIndirection<CBinaryOperator>(fg_Move(Operator))};
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "'{}' is not a valid type for Param"_f << _Type);

		return {};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CTernary::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_Ternary;
		Object[gc_ConstString_Conditional] = m_Conditional.f_ToJson().f_ToJson();
		Object[gc_ConstString_Left] = m_Left.f_ToJson().f_ToJson();
		Object[gc_ConstString_Right] = m_Right.f_ToJson().f_ToJson();

		return Return;
	}

	auto CBuildSystemSyntax::CTernary::fs_FromJson(CStringCache &o_StringCache, CJsonSorted const &_Json, CFilePosition const &_Position) -> CTernary
	{
		CTernary Ternary;

		auto pConditional = _Json.f_GetMember(gc_ConstString_Conditional);
		if (!pConditional)
			CBuildSystem::fs_ThrowError(_Position, "Ternary token does not have valid Conditional member");

		auto pLeft = _Json.f_GetMember(gc_ConstString_Left);
		if (!pLeft)
			CBuildSystem::fs_ThrowError(_Position, "Ternary token does not have valid Left member");

		auto pRight = _Json.f_GetMember(gc_ConstString_Right);
		if (!pRight)
			CBuildSystem::fs_ThrowError(_Position, "Ternary token does not have valid Right member");

		auto fParseParam = [&](CEJsonSorted const &_Json) -> CParam
			{
				if (!_Json.f_IsUserType())
					return CParam::fs_FromJson(o_StringCache, _Json, _Position, {}, true);

				auto &UserType = _Json.f_UserType();
				auto &Value = UserType.m_Value;

				auto pType = Value.f_GetMember(gc_ConstString_Type, EJsonType_String);
				if (!pType)
					CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

				return CParam::fs_FromJson(o_StringCache, _Json, _Position, pType->f_String(), true);
			}
		;

		Ternary.m_Conditional = fParseParam(CEJsonSorted::fs_FromJson(*pConditional));
		Ternary.m_Left = fParseParam(CEJsonSorted::fs_FromJson(*pLeft));
		Ternary.m_Right = fParseParam(CEJsonSorted::fs_FromJson(*pRight));

		return Ternary;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CFunctionCall::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;
		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = m_bPostFunction ? gc_ConstString_PostFunction : gc_ConstString_Function;
		Object[gc_ConstString_Name] = m_PropertyKey.m_Name;
		Object[gc_ConstString_PropertyType] = m_bEmptyPropertyType ? CStr("") : fg_PropertyTypeToStr(m_PropertyKey.f_GetType());

		auto &Params = Object[gc_ConstString_Params].f_Array();
		for (auto &Param : m_Params)
			Params.f_Insert(Param.f_ToJson().f_ToJson());

		return Return;
	}

	auto CBuildSystemSyntax::CFunctionCall::fs_FromJson(CStringCache &o_StringCache, CJsonSorted const &_Json, CFilePosition const &_Position, NStr::CStr const &_Type) -> CFunctionCall
	{
		CFunctionCall FunctionCall;

		FunctionCall.m_bPostFunction = _Type == gc_ConstString_PostFunction.m_String;

		auto pName = _Json.f_GetMember(gc_ConstString_Name, EJsonType_String);
		if (!pName)
			CBuildSystem::fs_ThrowError(_Position, "Function token does not have valid Name member");

		auto pPropertyType = _Json.f_GetMember(gc_ConstString_PropertyType, EJsonType_String);
		if (!pPropertyType)
			CBuildSystem::fs_ThrowError(_Position, "Function token does not have valid PropertyType member");

		FunctionCall.m_PropertyKey = CPropertyKey(o_StringCache, fg_PropertyTypeFromStr(pPropertyType->f_String()), pName->f_String());
		FunctionCall.m_bEmptyPropertyType = pPropertyType->f_String().f_IsEmpty();
		if (FunctionCall.m_PropertyKey.f_GetType() == EPropertyType_Invalid)
			CBuildSystem::fs_ThrowError(_Position, "Unknown property type '{}'"_f << pPropertyType->f_String());

		auto pParams = _Json.f_GetMember(gc_ConstString_Params, EJsonType_Array);
		if (!pParams)
			CBuildSystem::fs_ThrowError(_Position, "Function token does not have valid Params member");

		auto ParamsEJson = CEJsonSorted::fs_FromJson(*pParams);
		auto &Params = ParamsEJson.f_Array();

		if (FunctionCall.m_bPostFunction && Params.f_GetLen() < 1)
			CBuildSystem::fs_ThrowError(_Position, "Post function call needs at least one parmeter");

		for (auto &Param : Params)
		{
			if (!Param.f_IsUserType())
			{
				FunctionCall.m_Params.f_Insert(CParam::fs_FromJson(o_StringCache, Param, _Position, {}, true));
				continue;
			}

			auto &UserType = Param.f_UserType();
			auto &Value = UserType.m_Value;

			auto pType = Value.f_GetMember(gc_ConstString_Type, EJsonType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

			FunctionCall.m_Params.f_Insert(CParam::fs_FromJson(o_StringCache, Param, _Position, pType->f_String(), true));
		}

		return FunctionCall;
	}

	bool CBuildSystemSyntax::CExpression::f_IsParam() const
	{
		return m_Expression.f_IsOfType<CParam>();
	}

	CBuildSystemSyntax::CParam const &CBuildSystemSyntax::CExpression::f_Param() const
	{
		return m_Expression.f_GetAsType<CParam>();
	}

	bool CBuildSystemSyntax::CExpression::f_IsFunctionCall() const
	{
		return m_Expression.f_IsOfType<CFunctionCall>();
	}

	CBuildSystemSyntax::CFunctionCall const &CBuildSystemSyntax::CExpression::f_FunctionCall() const
	{
		return m_Expression.f_GetAsType<CFunctionCall>();
	}

	bool CBuildSystemSyntax::CExpression::f_IsJsonAccessor() const
	{
		return m_Expression.f_IsOfType<NStorage::TCIndirection<CJsonAccessor>>();
	}

	CBuildSystemSyntax::CJsonAccessor const &CBuildSystemSyntax::CExpression::f_JsonAccessor() const
	{
		return m_Expression.f_GetAsType<NStorage::TCIndirection<CJsonAccessor>>().f_Get();
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CExpression::f_ToJson(bool _bAppendExpression) const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;
		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = _bAppendExpression ? gc_ConstString_AppendExpression : gc_ConstString_Expression;
		Object[gc_ConstString_Paren] = m_bParen;

		auto &Param = Object[gc_ConstString_Param];
		switch (m_Expression.f_GetTypeID())
		{
		case 0: Param = m_Expression.f_Get<0>().f_ToJson().f_ToJson(); break;
		case 1: Param = m_Expression.f_Get<1>().f_ToJson().f_ToJson(); break;
		case 2: Param = m_Expression.f_Get<2>().f_Get().f_ToJson().f_ToJson(); break;
		default: DMibNeverGetHere;
		}

		return Return;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CExpression::f_ToJsonRaw() const
	{
		switch (m_Expression.f_GetTypeID())
		{
		case 0: return m_Expression.f_Get<0>().f_ToJson();
		case 1: return m_Expression.f_Get<1>().f_ToJson();
		case 2: return m_Expression.f_Get<2>().f_Get().f_ToJson();
		default: DMibNeverGetHere;
		}

		return {};
	}

	auto CBuildSystemSyntax::CExpression::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bParen) -> CExpression
	{
		if (!_Json.f_IsUserType())
			return CExpression{CParam::fs_FromJson(o_StringCache, _Json, _Position, {}, false)};

		auto &UserType = _Json.f_UserType();
		if (UserType.m_Type != gc_ConstString_BuildSystemToken.m_String)
			CBuildSystem::fs_ThrowError(_Position, "Invalid type for expression");

		auto &Value = UserType.m_Value;

		auto pType = Value.f_GetMember(gc_ConstString_Type, EJsonType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Expression does not have valid Type member");

		if (pType->f_String() == gc_ConstString_Function.m_String || pType->f_String() == gc_ConstString_PostFunction.m_String)
			return CExpression{CFunctionCall::fs_FromJson(o_StringCache, Value, _Position, pType->f_String()), _bParen};
		else if (pType->f_String() == gc_ConstString_JsonAccessor.m_String)
			return CExpression{NStorage::TCIndirection<CJsonAccessor>(CJsonAccessor::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(Value), _Position)), _bParen};
		else
			return CExpression{CParam::fs_FromJson(o_StringCache, _Json, _Position, pType->f_String(), false), _bParen};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CExpressionAppend::f_ToJson() const
	{
		return static_cast<CExpression const &>(*this).f_ToJson(true);
	}

	auto CBuildSystemSyntax::CExpressionAppend::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bParen) -> CExpressionAppend
	{
		CExpressionAppend Return;
		static_cast<CExpression &>(Return) = CExpression::fs_FromJson(o_StringCache, _Json, _Position, _bParen);
		return Return;
	}
}
