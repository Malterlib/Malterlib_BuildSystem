// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	NEncoding::CEJsonSorted CBuildSystemSyntax::COperator::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_Operator;
		auto &Operator = Object[gc_ConstString_Operator];

		switch (m_Operator)
		{
		case EOperator_LessThan: Operator = gc_ConstString_Symbol_OperatorLessThan; break;
		case EOperator_LessThanEqual: Operator = gc_ConstString_Symbol_OperatorLessThanEqual; break;
		case EOperator_GreaterThan: Operator = gc_ConstString_Symbol_OperatorGreaterThan; break;
		case EOperator_GreaterThanEqual: Operator = gc_ConstString_Symbol_OperatorGreaterThanEqual; break;
		case EOperator_Equal: Operator = gc_ConstString_Symbol_OperatorEqual; break;
		case EOperator_NotEqual: Operator = gc_ConstString_Symbol_OperatorNotEqual; break;
		case EOperator_MatchEqual: Operator = gc_ConstString_Symbol_OperatorMatchEqual; break;
		case EOperator_MatchNotEqual: Operator = gc_ConstString_Symbol_OperatorMatchNotEqual; break;
		case EOperator_Append: Operator = gc_ConstString_Symbol_OperatorAppend; break;
		case EOperator_Prepend: Operator = gc_ConstString_Symbol_OperatorPrepend; break;
		}

		Object[gc_ConstString_Right] = m_Right.f_Get().f_ToJson().f_ToJson();

		return Return;
	}

	auto CBuildSystemSyntax::COperator::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed) -> COperator
	{
		auto pOperator = _Json.f_GetMember(gc_ConstString_Operator, EJsonType_String);
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "Operator token has no valid Operator member");

		COperator Return;

		auto &Operator = pOperator->f_String();

		if (Operator == gc_ConstString_Symbol_OperatorLessThan.m_String)
			Return.m_Operator = EOperator_LessThan;
		else if (Operator == gc_ConstString_Symbol_OperatorLessThanEqual.m_String)
			Return.m_Operator = EOperator_LessThanEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorGreaterThan.m_String)
			Return.m_Operator = EOperator_GreaterThan;
		else if (Operator == gc_ConstString_Symbol_OperatorGreaterThanEqual.m_String)
			Return.m_Operator = EOperator_GreaterThanEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorEqual.m_String)
			Return.m_Operator = EOperator_Equal;
		else if (Operator == gc_ConstString_Symbol_OperatorNotEqual.m_String)
			Return.m_Operator = EOperator_NotEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorMatchEqual.m_String)
			Return.m_Operator = EOperator_MatchEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorMatchNotEqual.m_String)
			Return.m_Operator = EOperator_MatchNotEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorPrepend.m_String)
			Return.m_Operator = EOperator_Prepend;
		else if (Operator == gc_ConstString_Symbol_OperatorAppend.m_String)
			Return.m_Operator = EOperator_Append;
		else
			CBuildSystem::fs_ThrowError(_Position, "Operator token has unknown operator {}"_f << Operator);

		auto pRight = _Json.f_GetMember(gc_ConstString_Right);
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "Operator token has no valid Right member");

		Return.m_Right = CValue::fs_FromJson(o_StringCache, *pRight, _Position, _bAppendAllowed);

		return Return;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CBinaryOperator::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_BinaryOperator;
		auto &Operator = Object[gc_ConstString_Operator];

		switch (m_Operator)
		{
		case EOperator_LessThan: Operator = gc_ConstString_Symbol_OperatorLessThan; break;
		case EOperator_LessThanEqual: Operator = gc_ConstString_Symbol_OperatorLessThanEqual; break;
		case EOperator_GreaterThan: Operator = gc_ConstString_Symbol_OperatorGreaterThan; break;
		case EOperator_GreaterThanEqual: Operator = gc_ConstString_Symbol_OperatorGreaterThanEqual; break;
		case EOperator_Equal: Operator = gc_ConstString_Symbol_OperatorEqual; break;
		case EOperator_NotEqual: Operator = gc_ConstString_Symbol_OperatorNotEqual; break;
		case EOperator_MatchEqual: Operator = gc_ConstString_Symbol_OperatorMatchEqual; break;
		case EOperator_MatchNotEqual: Operator = gc_ConstString_Symbol_OperatorMatchNotEqual; break;
		case EOperator_Add: Operator = gc_ConstString_Symbol_OperatorAdd; break;
		case EOperator_Subtract: Operator = gc_ConstString_Symbol_OperatorSubtract; break;
		case EOperator_Divide: Operator = gc_ConstString_Symbol_OperatorDivide; break;
		case EOperator_Multiply: Operator = gc_ConstString_Symbol_OperatorMultiply; break;
		case EOperator_Modulus: Operator = gc_ConstString_Symbol_OperatorModulus; break;
		case EOperator_BitwiseLeftShift: Operator = gc_ConstString_Symbol_OperatorBitwiseLeftShift; break;
		case EOperator_BitwiseRightShift: Operator = gc_ConstString_Symbol_OperatorBitwiseRightShift; break;
		case EOperator_BitwiseAnd: Operator = gc_ConstString_Symbol_OperatorBitwiseAnd; break;
		case EOperator_BitwiseXor: Operator = gc_ConstString_Symbol_OperatorBitwiseXor; break;
		case EOperator_BitwiseOr: Operator = gc_ConstString_Symbol_OperatorBitwiseOr; break;
		case EOperator_And: Operator = gc_ConstString_Symbol_OperatorAnd; break;
		case EOperator_Or: Operator = gc_ConstString_Symbol_OperatorOr; break;
		case EOperator_NullishCoalescing: Operator = gc_ConstString_Symbol_OperatorNullishCoalescing; break;
		}

		Object[gc_ConstString_Left] = m_Left.f_ToJson().f_ToJson();
		Object[gc_ConstString_Right] = m_Right.f_ToJson().f_ToJson();

		return Return;
	}

	auto CBuildSystemSyntax::CBinaryOperator::fs_FromJson(CStringCache &o_StringCache, CJsonSorted const &_Json, CFilePosition const &_Position) -> CBinaryOperator
	{
		CBinaryOperator BinaryOperator;

		auto pOperator = _Json.f_GetMember(gc_ConstString_Operator);
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "BinaryOperator token does not have valid Operator member");

		auto pLeft = _Json.f_GetMember(gc_ConstString_Left);
		if (!pLeft)
			CBuildSystem::fs_ThrowError(_Position, "BinaryOperator token does not have valid Left member");

		auto pRight = _Json.f_GetMember(gc_ConstString_Right);
		if (!pRight)
			CBuildSystem::fs_ThrowError(_Position, "BinaryOperator token does not have valid Right member");

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

		auto &Operator = pOperator->f_String();

		if (Operator == gc_ConstString_Symbol_OperatorLessThan.m_String)
			BinaryOperator.m_Operator = EOperator_LessThan;
		else if (Operator == gc_ConstString_Symbol_OperatorLessThanEqual.m_String)
			BinaryOperator.m_Operator = EOperator_LessThanEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorGreaterThan.m_String)
			BinaryOperator.m_Operator = EOperator_GreaterThan;
		else if (Operator == gc_ConstString_Symbol_OperatorGreaterThanEqual.m_String)
			BinaryOperator.m_Operator = EOperator_GreaterThanEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorEqual.m_String)
			BinaryOperator.m_Operator = EOperator_Equal;
		else if (Operator == gc_ConstString_Symbol_OperatorNotEqual.m_String)
			BinaryOperator.m_Operator = EOperator_NotEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorMatchEqual.m_String)
			BinaryOperator.m_Operator = EOperator_MatchEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorMatchNotEqual.m_String)
			BinaryOperator.m_Operator = EOperator_MatchNotEqual;
		else if (Operator == gc_ConstString_Symbol_OperatorAdd.m_String)
			BinaryOperator.m_Operator = EOperator_Add;
		else if (Operator == gc_ConstString_Symbol_OperatorSubtract.m_String)
			BinaryOperator.m_Operator = EOperator_Subtract;
		else if (Operator == gc_ConstString_Symbol_OperatorDivide.m_String)
			BinaryOperator.m_Operator = EOperator_Divide;
		else if (Operator == gc_ConstString_Symbol_OperatorMultiply.m_String)
			BinaryOperator.m_Operator = EOperator_Multiply;
		else if (Operator == gc_ConstString_Symbol_OperatorModulus.m_String)
			BinaryOperator.m_Operator = EOperator_Modulus;
		else if (Operator == gc_ConstString_Symbol_OperatorBitwiseLeftShift.m_String)
			BinaryOperator.m_Operator = EOperator_BitwiseLeftShift;
		else if (Operator == gc_ConstString_Symbol_OperatorBitwiseRightShift.m_String)
			BinaryOperator.m_Operator = EOperator_BitwiseRightShift;
		else if (Operator == gc_ConstString_Symbol_OperatorBitwiseAnd.m_String)
			BinaryOperator.m_Operator = EOperator_BitwiseAnd;
		else if (Operator == gc_ConstString_Symbol_OperatorBitwiseXor.m_String)
			BinaryOperator.m_Operator = EOperator_BitwiseXor;
		else if (Operator == gc_ConstString_Symbol_OperatorBitwiseOr.m_String)
			BinaryOperator.m_Operator = EOperator_BitwiseOr;
		else if (Operator == gc_ConstString_Symbol_OperatorAnd.m_String)
			BinaryOperator.m_Operator = EOperator_And;
		else if (Operator == gc_ConstString_Symbol_OperatorOr.m_String)
			BinaryOperator.m_Operator = EOperator_Or;
		else if (Operator == gc_ConstString_Symbol_OperatorNullishCoalescing.m_String)
			BinaryOperator.m_Operator = EOperator_NullishCoalescing;
		else
			CBuildSystem::fs_ThrowError(_Position, "BinaryOperator token has unknown operator {}"_f << Operator);

		BinaryOperator.m_Left = fParseParam(CEJsonSorted::fs_FromJson(*pLeft));
		BinaryOperator.m_Right = fParseParam(CEJsonSorted::fs_FromJson(*pRight));

		return BinaryOperator;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CPrefixOperator::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();

		Object[gc_ConstString_Type] = gc_ConstString_PrefixOperator;
		auto &Operator = Object[gc_ConstString_Operator];

		switch (m_Operator)
		{
		case EOperator_LogicalNot: Operator = gc_ConstString_Symbol_LogicalNot; break;
		case EOperator_BitwiseNot: Operator = gc_ConstString_Symbol_BitwiseNot; break;
		case EOperator_UnaryPlus: Operator = gc_ConstString_Symbol_OperatorAdd; break;
		case EOperator_UnaryMinus: Operator = gc_ConstString_Symbol_OperatorSubtract; break;
		}

		Object[gc_ConstString_Right] = m_Right.f_ToJson().f_ToJson();

		return Return;
	}

	auto CBuildSystemSyntax::CPrefixOperator::fs_FromJson(CStringCache &o_StringCache, CJsonSorted const &_Json, CFilePosition const &_Position) -> CPrefixOperator
	{
		CPrefixOperator PrefixOperator;

		auto pOperator = _Json.f_GetMember(gc_ConstString_Operator);
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "PrefixOperator token does not have valid Operator member");

		auto pRight = _Json.f_GetMember(gc_ConstString_Right);
		if (!pRight)
			CBuildSystem::fs_ThrowError(_Position, "PrefixOperator token does not have valid Right member");

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

		auto &Operator = pOperator->f_String();

		if (Operator == gc_ConstString_Symbol_LogicalNot.m_String)
			PrefixOperator.m_Operator = EOperator_LogicalNot;
		else if (Operator == gc_ConstString_Symbol_BitwiseNot.m_String)
			PrefixOperator.m_Operator = EOperator_BitwiseNot;
		else if (Operator == gc_ConstString_Symbol_OperatorAdd.m_String)
			PrefixOperator.m_Operator = EOperator_UnaryPlus;
		else if (Operator == gc_ConstString_Symbol_OperatorSubtract.m_String)
			PrefixOperator.m_Operator = EOperator_UnaryMinus;
		else
			CBuildSystem::fs_ThrowError(_Position, "PrefixOperator token has unknown operator {}"_f << Operator);

		PrefixOperator.m_Right = fParseParam(CEJsonSorted::fs_FromJson(*pRight));

		return PrefixOperator;
	}
}
