// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	auto CBuildSystemSyntax::COperator::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position, bool _bAppendAllowed) -> COperator
	{
		auto pOperator = _JSON.f_GetMember("Operator", EJSONType_String);
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "Operator token has no valid Operator member");

		COperator Return;

		auto &Operator = pOperator->f_String();

		if (Operator == "<")
			Return.m_Operator = EOperator_LessThan;
		else if (Operator == "<=")
			Return.m_Operator = EOperator_LessThanEqual;
		else if (Operator == ">")
			Return.m_Operator = EOperator_GreaterThan;
		else if (Operator == ">=")
			Return.m_Operator = EOperator_GreaterThanEqual;
		else if (Operator == "==")
			Return.m_Operator = EOperator_Equal;
		else if (Operator == "!=")
			Return.m_Operator = EOperator_NotEqual;
		else if (Operator == "<==>")
			Return.m_Operator = EOperator_MatchEqual;
		else if (Operator == "<!=>")
			Return.m_Operator = EOperator_MatchNotEqual;
		else if (Operator == "+=")
			Return.m_Operator = EOperator_Prepend;
		else if (Operator == "=+")
			Return.m_Operator = EOperator_Append;
		else
			CBuildSystem::fs_ThrowError(_Position, "Operator token has unknown operator {}"_f << Operator);

		auto pRight = _JSON.f_GetMember("Right");
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "Operator token has no valid Right member");

		Return.m_Right = CValue::fs_FromJSON(*pRight, _Position, _bAppendAllowed);

		return Return;
	}

	auto CBuildSystemSyntax::CBinaryOperator::fs_FromJSON(CJSON const &_JSON, CFilePosition const &_Position) -> CBinaryOperator
	{
		CBinaryOperator BinaryOperator;

		auto pOperator = _JSON.f_GetMember("Operator");
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "BinaryOperator token does not have valid Operator member");

		auto pLeft = _JSON.f_GetMember("Left");
		if (!pLeft)
			CBuildSystem::fs_ThrowError(_Position, "BinaryOperator token does not have valid Left member");

		auto pRight = _JSON.f_GetMember("Right");
		if (!pRight)
			CBuildSystem::fs_ThrowError(_Position, "BinaryOperator token does not have valid Right member");

		auto fParseParam = [&](CEJSON const &_JSON) -> CParam
			{
				if (!_JSON.f_IsUserType())
					return CParam::fs_FromJSON(_JSON, _Position, {}, true);

				auto &UserType = _JSON.f_UserType();
				auto &Value = UserType.m_Value;

				auto pType = Value.f_GetMember("Type", EJSONType_String);
				if (!pType)
					CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

				return CParam::fs_FromJSON(_JSON, _Position, pType->f_String(), true);
			}
		;

		auto &Operator = pOperator->f_String();

		if (Operator == "<")
			BinaryOperator.m_Operator = EOperator_LessThan;
		else if (Operator == "<=")
			BinaryOperator.m_Operator = EOperator_LessThanEqual;
		else if (Operator == ">")
			BinaryOperator.m_Operator = EOperator_GreaterThan;
		else if (Operator == ">=")
			BinaryOperator.m_Operator = EOperator_GreaterThanEqual;
		else if (Operator == "==")
			BinaryOperator.m_Operator = EOperator_Equal;
		else if (Operator == "!=")
			BinaryOperator.m_Operator = EOperator_NotEqual;
		else if (Operator == "<==>")
			BinaryOperator.m_Operator = EOperator_MatchEqual;
		else if (Operator == "<!=>")
			BinaryOperator.m_Operator = EOperator_MatchNotEqual;
		else if (Operator == "+")
			BinaryOperator.m_Operator = EOperator_Add;
		else if (Operator == "-")
			BinaryOperator.m_Operator = EOperator_Subtract;
		else if (Operator == "/")
			BinaryOperator.m_Operator = EOperator_Divide;
		else if (Operator == "*")
			BinaryOperator.m_Operator = EOperator_Multiply;
		else if (Operator == "%")
			BinaryOperator.m_Operator = EOperator_Modulus;
		else if (Operator == "<<")
			BinaryOperator.m_Operator = EOperator_BitwiseLeftShift;
		else if (Operator == ">>")
			BinaryOperator.m_Operator = EOperator_BitwiseRightShift;
		else if (Operator == "&")
			BinaryOperator.m_Operator = EOperator_BitwiseAnd;
		else if (Operator == "^")
			BinaryOperator.m_Operator = EOperator_BitwiseXor;
		else if (Operator == "|")
			BinaryOperator.m_Operator = EOperator_BitwiseOr;
		else if (Operator == "&&")
			BinaryOperator.m_Operator = EOperator_And;
		else if (Operator == "||")
			BinaryOperator.m_Operator = EOperator_Or;
		else
			CBuildSystem::fs_ThrowError(_Position, "BinaryOperator token has unknown operator {}"_f << Operator);

		BinaryOperator.m_Left = fParseParam(CEJSON::fs_FromJSON(*pLeft));
		BinaryOperator.m_Right = fParseParam(CEJSON::fs_FromJSON(*pRight));

		return BinaryOperator;
	}

	auto CBuildSystemSyntax::CPrefixOperator::fs_FromJSON(CJSON const &_JSON, CFilePosition const &_Position) -> CPrefixOperator
	{
		CPrefixOperator PrefixOperator;

		auto pOperator = _JSON.f_GetMember("Operator");
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "PrefixOperator token does not have valid Operator member");

		auto pRight = _JSON.f_GetMember("Right");
		if (!pRight)
			CBuildSystem::fs_ThrowError(_Position, "PrefixOperator token does not have valid Right member");

		auto fParseParam = [&](CEJSON const &_JSON) -> CParam
			{
				if (!_JSON.f_IsUserType())
					return CParam::fs_FromJSON(_JSON, _Position, {}, true);

				auto &UserType = _JSON.f_UserType();
				auto &Value = UserType.m_Value;

				auto pType = Value.f_GetMember("Type", EJSONType_String);
				if (!pType)
					CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

				return CParam::fs_FromJSON(_JSON, _Position, pType->f_String(), true);
			}
		;

		auto &Operator = pOperator->f_String();

		if (Operator == "!")
			PrefixOperator.m_Operator = EOperator_LogicalNot;
		else if (Operator == "~")
			PrefixOperator.m_Operator = EOperator_BitwiseNot;
		else if (Operator == "+")
			PrefixOperator.m_Operator = EOperator_UnaryPlus;
		else if (Operator == "-")
			PrefixOperator.m_Operator = EOperator_UnaryMinus;
		else
			CBuildSystem::fs_ThrowError(_Position, "PrefixOperator token has unknown operator {}"_f << Operator);

		PrefixOperator.m_Right = fParseParam(CEJSON::fs_FromJSON(*pRight));

		return PrefixOperator;
	}
}
