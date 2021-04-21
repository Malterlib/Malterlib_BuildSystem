// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	namespace
	{
		EConditionType fg_OperatorToConditionType(CBuildSystemSyntax::CBinaryOperator::EOperator _Operator)
		{
			switch (_Operator)
			{
			default:
			case CBuildSystemSyntax::CBinaryOperator::EOperator_MatchEqual: return EConditionType_MatchEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_MatchNotEqual: return EConditionType_MatchNotEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Equal: return EConditionType_CompareEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_NotEqual: return EConditionType_CompareNotEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_LessThan: return EConditionType_CompareLessThan;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_LessThanEqual: return EConditionType_CompareLessThanEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_GreaterThan: return EConditionType_CompareGreaterThan;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_GreaterThanEqual: return EConditionType_CompareGreaterThanEqual;
			}
		}

		ch8 const *fg_OperatorName(CBuildSystemSyntax::CBinaryOperator::EOperator _Operator)
		{
			switch (_Operator)
			{
			case CBuildSystemSyntax::CBinaryOperator::EOperator_LessThan: return "<";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_LessThanEqual: return "<=";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_GreaterThan: return ">";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_GreaterThanEqual: return ">=";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Equal: return "==";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_NotEqual: return "!=";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_MatchEqual: return "<==>";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_MatchNotEqual: return "<!=>";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Add: return "+";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Subtract: return "-";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Divide: return "/";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Multiply: return "*";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Modulus: return "%";

			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseLeftShift: return "<<";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseRightShift: return ">>";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseAnd: return "&";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseXor: return "|";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseOr: return "^";

			case CBuildSystemSyntax::CBinaryOperator::EOperator_And: return "&&";
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Or: return "||";
			}

			return "ERROR";
		}

		ch8 const *fg_OperatorName(CBuildSystemSyntax::CPrefixOperator::EOperator _Operator)
		{
			switch (_Operator)
			{
			case CBuildSystemSyntax::CPrefixOperator::EOperator_LogicalNot: return "!";
			case CBuildSystemSyntax::CPrefixOperator::EOperator_BitwiseNot: return "~";
			case CBuildSystemSyntax::CPrefixOperator::EOperator_UnaryPlus: return "+";
			case CBuildSystemSyntax::CPrefixOperator::EOperator_UnaryMinus: return "-";
			}

			return "ERROR";
		}
	}

	bool fg_IsFalsy(CEJSON const &_Value)
	{
		if (!_Value.f_IsValid())
			return true;
		else if (_Value.f_IsNull())
			return true;
		else if (_Value.f_IsBoolean())
			return !_Value.f_Boolean();
		else if (_Value.f_IsInteger())
			return _Value.f_Integer() == 0;
		else if (_Value.f_IsFloat())
			return _Value.f_Float().f_IsNan() || !_Value.f_Float();
		else if (_Value.f_IsString())
			return !_Value.f_String();

		return false;
	}

	bool fg_IsTruthy(CEJSON const &_Value)
	{
		return !fg_IsFalsy(_Value);
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueBinaryOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CBinaryOperator const &_Value) const
	{
		using COp = CBuildSystemSyntax::CBinaryOperator;
		switch (_Value.m_Operator)
		{
		case COp::EOperator_And:
			{
				auto Left = fp_EvaluatePropertyValueParam(_Context, _Value.m_Left);
				if (fg_IsTruthy(Left))
					return fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);
				else
					return Left;
			}
			break;
		case COp::EOperator_Or:
			{
				auto Left = fp_EvaluatePropertyValueParam(_Context, _Value.m_Left);
				if (fg_IsTruthy(Left))
					return Left;
				else
					return fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);
			}
			break;
		default:
			break;
		}

		auto Left = fp_EvaluatePropertyValueParam(_Context, _Value.m_Left);
		auto Right = fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);

		switch (_Value.m_Operator)
		{
		case COp::EOperator_MatchEqual:
		case COp::EOperator_MatchNotEqual:
		case COp::EOperator_Equal:
		case COp::EOperator_NotEqual:
		case COp::EOperator_LessThan:
		case COp::EOperator_GreaterThan:
		case COp::EOperator_LessThanEqual:
		case COp::EOperator_GreaterThanEqual:
			return fsp_CompareValueRecursive
				(
					Left
					, Right
					, fg_OperatorToConditionType(_Value.m_Operator)
					, [&](CStr const &_Error)
					{
						fsp_ThrowError(_Context, _Error);
					}
				)
			;
		default:
			break;
		}

		if (Left.f_IsFloat() && Right.f_IsInteger())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add:
			case COp::EOperator_Subtract:
			case COp::EOperator_Divide:
			case COp::EOperator_Multiply:
			case COp::EOperator_Modulus:
				Right = Right.f_AsFloat();
				break;
			default:
				break;
			}
		}
		else if (Left.f_IsInteger() && Right.f_IsFloat())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add:
			case COp::EOperator_Subtract:
			case COp::EOperator_Divide:
			case COp::EOperator_Multiply:
			case COp::EOperator_Modulus:
				Left = Left.f_AsFloat();
				break;
			default:
				break;
			}
		}

		if (Left.f_EType() != Right.f_EType())
			fsp_ThrowError(_Context, "Trying to operate on values of different types:\n{}\n{}"_f << Left << Right);

		if (Left.f_IsObject())
			fsp_ThrowError(_Context, "Objects are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << Left << Right);
		else if (Left.f_IsArray())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add:
				{
					auto Return = Left;
					Return.f_Array().f_Insert(Right.f_Array());
					return Return;
				}
				break;
			default:
				fsp_ThrowError(_Context, "Arrays are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << Left << Right);
				break;
			}
		}
		else if (Left.f_IsString())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add: return Left.f_String() + Right.f_String();
			case COp::EOperator_Divide: return Left.f_String() / Right.f_String();
			default:
				fsp_ThrowError(_Context, "Strings are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << Left << Right);
				break;
			}
		}
		else if (Left.f_IsInteger())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add: return Left.f_Integer() + Right.f_Integer();
			case COp::EOperator_Subtract: return Left.f_Integer() - Right.f_Integer();
			case COp::EOperator_Divide: return Left.f_Integer() / Right.f_Integer();
			case COp::EOperator_Multiply: return Left.f_Integer() * Right.f_Integer();
			case COp::EOperator_Modulus: return Left.f_Integer() % Right.f_Integer();
			case COp::EOperator_BitwiseLeftShift: return Left.f_Integer() << Right.f_Integer();
			case COp::EOperator_BitwiseRightShift: return Left.f_Integer() >> Right.f_Integer();
			case COp::EOperator_BitwiseAnd: return Left.f_Integer() & Right.f_Integer();
			case COp::EOperator_BitwiseXor: return Left.f_Integer() ^ Right.f_Integer();
			case COp::EOperator_BitwiseOr: return Left.f_Integer() | Right.f_Integer();
			default:
				fsp_ThrowError(_Context, "Integers are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << Left << Right);
				break;
			}
		}
		else if (Left.f_IsFloat())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add: return Left.f_Float() + Right.f_Float();
			case COp::EOperator_Subtract: return Left.f_Float() - Right.f_Float();
			case COp::EOperator_Divide: return Left.f_Float() / Right.f_Float();
			case COp::EOperator_Multiply: return Left.f_Float() * Right.f_Float();
			case COp::EOperator_Modulus: return Left.f_Float().f_Mod(Right.f_Float());
			default:
				fsp_ThrowError(_Context, "Floats are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << Left << Right);
				break;
			}
		}
		else if (Left.f_IsBoolean())
			fsp_ThrowError(_Context, "Booleans are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << Left << Right);
		else if (Left.f_IsUserType())
			fsp_ThrowError(_Context, "User types ({}) not supported for binary operator {}:\n{}\n{}"_f << Left.f_UserType().m_Type << fg_OperatorName(_Value.m_Operator) << Left << Right);
		else
			fsp_ThrowError(_Context, "Value type not supported for binary operator {}:\n{}\n{}"_f << Left.f_UserType().m_Type << fg_OperatorName(_Value.m_Operator) << Left << Right);

		return {};
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValuePrefixOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CPrefixOperator const &_Value) const
	{
		using COp = CBuildSystemSyntax::CPrefixOperator;

		auto Right = fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);

		if (Right.f_IsObject())
			fsp_ThrowError(_Context, "Objects not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << Right);
		else if (Right.f_IsArray())
			fsp_ThrowError(_Context, "Arrays not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << Right);
		else if (Right.f_IsString())
			fsp_ThrowError(_Context, "Arrays not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << Right);
		else if (Right.f_IsInteger())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_LogicalNot: return !Right.f_Integer();
			case COp::EOperator_BitwiseNot: return ~Right.f_Integer();
			case COp::EOperator_UnaryPlus: return +Right.f_Integer();
			case COp::EOperator_UnaryMinus: return -Right.f_Integer();
			default:
				fsp_ThrowError(_Context, "Integers not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << Right);
				break;
			}
		}
		else if (Right.f_IsFloat())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_UnaryPlus: return +Right.f_Float();
			case COp::EOperator_UnaryMinus: return -Right.f_Float();
			default:
				fsp_ThrowError(_Context, "Floats not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << Right);
				break;
			}
		}
		else if (Right.f_IsBoolean())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_LogicalNot: return !Right.f_Boolean();
			default:
				fsp_ThrowError(_Context, "Floats not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << Right);
				break;
			}
		}
		else if (Right.f_IsUserType())
			fsp_ThrowError(_Context, "User types ({}) not supported for prefix operator {}:\n{}"_f << Right.f_UserType().m_Type << fg_OperatorName(_Value.m_Operator) << Right);
		else
			fsp_ThrowError(_Context, "Value type not supported for prefix operator {}:\n{}"_f << Right.f_UserType().m_Type << fg_OperatorName(_Value.m_Operator) << Right);

		return {};
	}
}
