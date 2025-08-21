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

		NStr::CStr fg_OperatorName(CBuildSystemSyntax::CBinaryOperator::EOperator _Operator)
		{
			switch (_Operator)
			{
			case CBuildSystemSyntax::CBinaryOperator::EOperator_LessThan: return gc_ConstString_Symbol_OperatorLessThan;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_LessThanEqual: return gc_ConstString_Symbol_OperatorLessThanEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_GreaterThan: return gc_ConstString_Symbol_OperatorGreaterThan;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_GreaterThanEqual: return gc_ConstString_Symbol_OperatorGreaterThanEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Equal: return gc_ConstString_Symbol_OperatorEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_NotEqual: return gc_ConstString_Symbol_OperatorNotEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_MatchEqual: return gc_ConstString_Symbol_OperatorMatchEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_MatchNotEqual: return gc_ConstString_Symbol_OperatorMatchNotEqual;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Add: return gc_ConstString_Symbol_OperatorAdd;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Subtract: return gc_ConstString_Symbol_OperatorSubtract;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Divide: return gc_ConstString_Symbol_OperatorDivide;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Multiply: return gc_ConstString_Symbol_OperatorMultiply;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Modulus: return gc_ConstString_Symbol_OperatorModulus;

			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseLeftShift: return gc_ConstString_Symbol_OperatorBitwiseLeftShift;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseRightShift: return gc_ConstString_Symbol_OperatorBitwiseRightShift;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseAnd: return gc_ConstString_Symbol_OperatorBitwiseAnd;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseXor: return gc_ConstString_Symbol_OperatorBitwiseOr;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_BitwiseOr: return gc_ConstString_Symbol_OperatorBitwiseXor;

			case CBuildSystemSyntax::CBinaryOperator::EOperator_And: return gc_ConstString_Symbol_OperatorAnd;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_Or: return gc_ConstString_Symbol_OperatorOr;
			case CBuildSystemSyntax::CBinaryOperator::EOperator_NullishCoalescing: return gc_ConstString_Symbol_OperatorNullishCoalescing;
			}

			return gc_ConstString_ERROR;
		}

		NStr::CStr fg_OperatorName(CBuildSystemSyntax::CPrefixOperator::EOperator _Operator)
		{
			switch (_Operator)
			{
			case CBuildSystemSyntax::CPrefixOperator::EOperator_LogicalNot: return gc_ConstString_Symbol_LogicalNot;
			case CBuildSystemSyntax::CPrefixOperator::EOperator_BitwiseNot: return gc_ConstString_Symbol_BitwiseNot;
			case CBuildSystemSyntax::CPrefixOperator::EOperator_UnaryPlus: return gc_ConstString_Symbol_OperatorAdd;
			case CBuildSystemSyntax::CPrefixOperator::EOperator_UnaryMinus: return gc_ConstString_Symbol_OperatorSubtract;
			}

			return gc_ConstString_ERROR;
		}
	}

	bool fg_IsFalsy(CEJsonSorted const &_Value)
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

	bool fg_IsTruthy(CEJsonSorted const &_Value)
	{
		return !fg_IsFalsy(_Value);
	}

	CStr fg_IndentValue(CEJsonSorted const &_Value)
	{
		return CStr::fs_ToStr(_Value).f_Indent("    ");
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluatePropertyValueBinaryOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CBinaryOperator const &_Value) const
	{
		using COp = CBuildSystemSyntax::CBinaryOperator;
		switch (_Value.m_Operator)
		{
		case COp::EOperator_And:
			{
				auto Left = fp_EvaluatePropertyValueParam(_Context, _Value.m_Left);
				if (fg_IsTruthy(Left.f_Get()))
					return fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);
				else
					return Left;
			}
			break;
		case COp::EOperator_Or:
			{
				auto Left = fp_EvaluatePropertyValueParam(_Context, _Value.m_Left);
				if (fg_IsTruthy(Left.f_Get()))
					return Left;
				else
					return fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);
			}
			break;
		case COp::EOperator_NullishCoalescing:
			{
				auto Left = fp_EvaluatePropertyValueParam(_Context, _Value.m_Left);
				if (Left.f_Get().f_IsValid() && !Left.f_Get().f_IsNull())
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
		{
			auto &LeftRef = Left.f_Get();
			auto &RightRef = Right.f_Get();

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
				return CEJsonSorted
					(
						fsp_CompareValueRecursive
						(
							LeftRef
							, RightRef
							, fg_OperatorToConditionType(_Value.m_Operator)
							, [&](CStr const &_Error)
							{
								fs_ThrowError(_Context, _Error);
							}
						)
					)
				;
			default:
				break;
			}

			if (LeftRef.f_IsFloat() && RightRef.f_IsInteger())
			{
				switch (_Value.m_Operator)
				{
				case COp::EOperator_Add:
				case COp::EOperator_Subtract:
				case COp::EOperator_Divide:
				case COp::EOperator_Multiply:
				case COp::EOperator_Modulus:
					Right.f_Set(CEJsonSorted(RightRef.f_AsFloat()));
					break;
				default:
					break;
				}
			}
			else if (LeftRef.f_IsInteger() && RightRef.f_IsFloat())
			{
				switch (_Value.m_Operator)
				{
				case COp::EOperator_Add:
				case COp::EOperator_Subtract:
				case COp::EOperator_Divide:
				case COp::EOperator_Multiply:
				case COp::EOperator_Modulus:
					Left.f_Set(CEJsonSorted(LeftRef.f_AsFloat()));
					break;
				default:
					break;
				}
			}
		}

		auto &LeftRef = Left.f_Get();
		auto &RightRef = Right.f_Get();

		if (LeftRef.f_EType() != RightRef.f_EType())
			fs_ThrowError(_Context, "Trying to operate on values of different types:\n{}\n{}"_f << fg_IndentValue(LeftRef) << fg_IndentValue(RightRef));

		if (LeftRef.f_IsObject())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add:
				{
					auto NewValue = Left.f_Move();

					auto &OriginalObject = NewValue.f_Object();

					for (auto &Member : RightRef.f_Object())
					{
						if (Member.f_Name().f_GetUserData() == EJsonStringType_NoQuote)
						{
							auto *pObject = &NewValue;
							for (auto &Component : Member.f_Name().f_Split("."))
							{
								if (pObject->f_IsValid() && !pObject->f_IsObject())
									fs_ThrowError(_Context, "Encountered non object subobject of wrong type while appending '{}'"_f << Member.f_Name());

								pObject = &(*pObject)[Component];
							}
							*pObject = Member.f_Value();
						}
						else
							OriginalObject[Member.f_Name()] = Member.f_Value();
					}

					return NewValue;
				}
				break;
			default:
				fs_ThrowError
					(
						_Context
						, "Objects are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(LeftRef) << fg_IndentValue(RightRef)
					)
				;
				break;
			}
		}
		else if (LeftRef.f_IsArray())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add:
				{
					auto Return = Left.f_Move();
					Return.f_Array().f_Insert(Right.f_MoveArray());
					return Return;
				}
				break;
			default:
				fs_ThrowError
					(
						_Context
						, "Arrays are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(LeftRef) << fg_IndentValue(RightRef)
					)
				;
				break;
			}
		}
		else if (LeftRef.f_IsString())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add: return CEJsonSorted(LeftRef.f_String() + RightRef.f_String());
			case COp::EOperator_Divide: return CEJsonSorted(LeftRef.f_String() / RightRef.f_String());
			default:
				fs_ThrowError
					(
						_Context
						, "Strings are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(LeftRef) << fg_IndentValue(RightRef)
					)
				;
				break;
			}
		}
		else if (LeftRef.f_IsInteger())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add: return CEJsonSorted(LeftRef.f_Integer() + RightRef.f_Integer());
			case COp::EOperator_Subtract: return CEJsonSorted(LeftRef.f_Integer() - RightRef.f_Integer());
			case COp::EOperator_Divide: return CEJsonSorted(LeftRef.f_Integer() / RightRef.f_Integer());
			case COp::EOperator_Multiply: return CEJsonSorted(LeftRef.f_Integer() * RightRef.f_Integer());
			case COp::EOperator_Modulus: return CEJsonSorted(LeftRef.f_Integer() % RightRef.f_Integer());
			case COp::EOperator_BitwiseLeftShift: return CEJsonSorted(LeftRef.f_Integer() << RightRef.f_Integer());
			case COp::EOperator_BitwiseRightShift: return CEJsonSorted(LeftRef.f_Integer() >> RightRef.f_Integer());
			case COp::EOperator_BitwiseAnd: return CEJsonSorted(LeftRef.f_Integer() & RightRef.f_Integer());
			case COp::EOperator_BitwiseXor: return CEJsonSorted(LeftRef.f_Integer() ^ RightRef.f_Integer());
			case COp::EOperator_BitwiseOr: return CEJsonSorted(LeftRef.f_Integer() | RightRef.f_Integer());
			default:
				fs_ThrowError
					(
						_Context
						, "Integers are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(LeftRef) << fg_IndentValue(RightRef)
					)
				;
				break;
			}
		}
		else if (LeftRef.f_IsFloat())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_Add: return CEJsonSorted(LeftRef.f_Float() + RightRef.f_Float());
			case COp::EOperator_Subtract: return CEJsonSorted(LeftRef.f_Float() - RightRef.f_Float());
			case COp::EOperator_Divide: return CEJsonSorted(LeftRef.f_Float() / RightRef.f_Float());
			case COp::EOperator_Multiply: return CEJsonSorted(LeftRef.f_Float() * RightRef.f_Float());
			case COp::EOperator_Modulus: return CEJsonSorted(LeftRef.f_Float().f_Mod(RightRef.f_Float()));
			default:
				fs_ThrowError
					(
						_Context
						, "Floats are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(LeftRef) << fg_IndentValue(RightRef)
					)
				;
				break;
			}
		}
		else if (LeftRef.f_IsBoolean())
		{
			fs_ThrowError
				(
					_Context
					, "Booleans are not supported for binary operator {}:\n{}\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(LeftRef) << fg_IndentValue(RightRef)
				)
			;
		}
		else if (LeftRef.f_IsUserType())
		{
			fs_ThrowError
				(
					_Context
					, "User types ({}) not supported for binary operator {}:\n{}\n{}"_f
					<< LeftRef.f_UserType().m_Type
					<< fg_OperatorName(_Value.m_Operator)
					<< fg_IndentValue(LeftRef)
					<< fg_IndentValue(RightRef)
				)
			;
		}
		else
		{
			fs_ThrowError
				(
					_Context
					, "Value type not supported for binary operator {}:\n{}\n{}"_f
					<< LeftRef.f_UserType().m_Type
					<< fg_OperatorName(_Value.m_Operator)
					<< fg_IndentValue(LeftRef)
					<< fg_IndentValue(RightRef)
				)
			;
		}

		return CEJsonSorted();
	}

	CEJsonSorted CBuildSystem::fp_EvaluatePropertyValuePrefixOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CPrefixOperator const &_Value) const
	{
		using COp = CBuildSystemSyntax::CPrefixOperator;

		auto Right = fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);
		auto &RightRef = Right.f_Get();

		if (RightRef.f_IsObject())
			fs_ThrowError(_Context, "Objects not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(RightRef));
		else if (RightRef.f_IsArray())
			fs_ThrowError(_Context, "Arrays not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(RightRef));
		else if (RightRef.f_IsString())
			fs_ThrowError(_Context, "Arrays not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(RightRef));
		else if (RightRef.f_IsInteger())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_LogicalNot: return !RightRef.f_Integer();
			case COp::EOperator_BitwiseNot: return ~RightRef.f_Integer();
			case COp::EOperator_UnaryPlus: return +RightRef.f_Integer();
			case COp::EOperator_UnaryMinus: return -RightRef.f_Integer();
			default:
				fs_ThrowError(_Context, "Integers not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(RightRef));
				break;
			}
		}
		else if (RightRef.f_IsFloat())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_UnaryPlus: return +RightRef.f_Float();
			case COp::EOperator_UnaryMinus: return -RightRef.f_Float();
			default:
				fs_ThrowError(_Context, "Floats not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(RightRef));
				break;
			}
		}
		else if (RightRef.f_IsBoolean())
		{
			switch (_Value.m_Operator)
			{
			case COp::EOperator_LogicalNot: return !RightRef.f_Boolean();
			default:
				fs_ThrowError(_Context, "Floats not supported for prefix operator {}:\n{}"_f << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(RightRef));
				break;
			}
		}
		else if (RightRef.f_IsUserType())
		{
			fs_ThrowError
				(
					_Context
					, "User types ({}) not supported for prefix operator {}:\n{}"_f << RightRef.f_UserType().m_Type << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(RightRef)
				)
			;
		}
		else
		{
			fs_ThrowError
				(
					_Context
					, "Value type not supported for prefix operator {}:\n{}"_f << RightRef.f_UserType().m_Type << fg_OperatorName(_Value.m_Operator) << fg_IndentValue(RightRef)
				)
			;
		}

		return {};
	}
}
