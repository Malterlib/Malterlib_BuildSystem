// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Condition.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	bool CCondition::f_IsCompare() const
	{
		return m_Type == EConditionType_MatchEqual
			|| m_Type == EConditionType_MatchNotEqual
			|| m_Type == EConditionType_CompareEqual
			|| m_Type == EConditionType_CompareNotEqual
			|| m_Type == EConditionType_CompareLessThan
			|| m_Type == EConditionType_CompareLessThanEqual
			|| m_Type == EConditionType_CompareGreaterThan
			|| m_Type == EConditionType_CompareGreaterThanEqual
		;
	}

	bool CCondition::f_IsDefault() const
	{
		return m_Type == EConditionType_Root && m_Children.f_IsEmpty();
	}

	namespace
	{
		auto fg_ConditionTuple(CCondition const &_Condition)
		{
			return fg_TupleReferences(_Condition.m_Children, _Condition.m_Left, _Condition.m_Right, _Condition.m_Type);
		}
	}

	COrdering_Partial CCondition::operator <=> (CCondition const &_Right) const
	{
		return fg_ConditionTuple(*this) <=> fg_ConditionTuple(_Right);
	}

	bool CCondition::f_NeedPerFile() const
	{
		for (auto iChild = m_Children.f_GetIterator(); iChild; ++iChild)
		{
			auto &Child = *iChild;
			if (Child.f_IsCompare() && Child.m_Left.f_IsIdentifier())
			{
				auto &LeftIdentifier = Child.m_Left.f_Identifier();
				if
					(
						LeftIdentifier.m_PropertyType == EPropertyType_This
						&& LeftIdentifier.m_Name == gc_ConstString_Type
						&& Child.m_Right.f_IsConstantString()
						&& Child.m_Right.f_ConstantString() == gc_ConstString_File.m_String
					)
				{
					return true;
				}
			}

			if (Child.f_NeedPerFile())
				return true;
		}
		return false;
	}

	namespace
	{
		bool fg_ConvertOperator(EConditionType &o_ConditionType, CBuildSystemSyntax::COperator::EOperator _Operator, CFilePosition const &_Position)
		{
			switch (_Operator)
			{
			case CBuildSystemSyntax::COperator::EOperator_LessThan: o_ConditionType = EConditionType_CompareLessThan; return true;
			case CBuildSystemSyntax::COperator::EOperator_LessThanEqual: o_ConditionType = EConditionType_CompareLessThanEqual; return true;
			case CBuildSystemSyntax::COperator::EOperator_GreaterThan: o_ConditionType = EConditionType_CompareGreaterThan; return true;
			case CBuildSystemSyntax::COperator::EOperator_GreaterThanEqual: o_ConditionType = EConditionType_CompareGreaterThanEqual; return true;
			case CBuildSystemSyntax::COperator::EOperator_Equal: o_ConditionType = EConditionType_CompareEqual; return true;
			case CBuildSystemSyntax::COperator::EOperator_NotEqual: o_ConditionType = EConditionType_CompareNotEqual; return true;
			case CBuildSystemSyntax::COperator::EOperator_MatchEqual: o_ConditionType = EConditionType_MatchEqual; return true;
			case CBuildSystemSyntax::COperator::EOperator_MatchNotEqual: o_ConditionType = EConditionType_MatchNotEqual; return true;
			case CBuildSystemSyntax::COperator::EOperator_Append: break;
			case CBuildSystemSyntax::COperator::EOperator_Prepend: break;
			}

			return false;
		}
	}

	bool CCondition::fs_TryParseCondition(CBuildSystemRegistry const &_Registry, CCondition &_ParentCondition, bool _bRoot)
	{
		bool bHandled = false;
		CCondition *pParent = nullptr;
		bool bParseChildren = false;
		auto const &Name = _Registry.f_GetName();

		do
		{
			if (Name.f_IsNamespace())
				return false;
			else if (Name.f_IsKeyPrefixOperator())
			{
				auto &PrefixOperator = Name.f_KeyPrefixOperator();

				switch (PrefixOperator.m_Operator)
				{
				case CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Equal:
					{
						if (!_bRoot)
							CBuildSystem::fs_ThrowError(_Registry, "!! condition only supported at root level");

						auto ConditionType = EConditionType_MatchEqual;

						auto &RightValue = _Registry.f_GetThisValue().m_Value;

						if (RightValue.m_Value.f_IsOfType<CBuildSystemSyntax::COperator>())
							CBuildSystem::fs_ThrowError(_Registry, "You cannot specify both !! condition and explicit operator");

						auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
						Condition.m_Type = ConditionType;
						Condition.m_Left = PrefixOperator.m_Right;
						Condition.m_Position = _Registry;
						Condition.m_Right = RightValue;
					}
					break;
				case CBuildSystemSyntax::CKeyPrefixOperator::EOperator_NotEqual:
					{
						auto *pParentCondition = &_ParentCondition;
						auto ConditionType = EConditionType_MatchNotEqual;

						auto &RightValue = _Registry.f_GetThisValue().m_Value;

						if (RightValue.m_Value.f_IsOfType<CBuildSystemSyntax::COperator>())
							CBuildSystem::fs_ThrowError(_Registry, "You cannot specify both ! condition and explicit operator");

						auto &Condition = pParentCondition->m_Children.f_Insert(fg_Construct());
						Condition.m_Type = ConditionType;
						Condition.m_Left = PrefixOperator.m_Right;
						Condition.m_Position = _Registry;
						Condition.m_Right = RightValue;
					}
					break;
				default:
					return false;
				}

				bHandled = true;
			}
			else if (Name.f_IsKeyLogicalOperator())
			{
				auto &KeyLogicalOperator = Name.f_KeyLogicalOperator();

				switch (KeyLogicalOperator.m_Operator)
				{
				case CBuildSystemSyntax::CKeyLogicalOperator::EOperator_And:
					{
						if (_Registry.f_GetThisValue().m_Value.f_IsValid())
							CBuildSystem::fs_ThrowError(_Registry, "And condition can only be specified as a container for children, not directly");

						auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
						Condition.m_Type = EConditionType_And;
						Condition.m_Position = _Registry;
						pParent = &Condition;
						bParseChildren = true;
					}
					break;
				case CBuildSystemSyntax::CKeyLogicalOperator::EOperator_Or:
					{
						if (_Registry.f_GetThisValue().m_Value.f_IsValid())
							CBuildSystem::fs_ThrowError(_Registry, "Or condition can only be specified as a container for children, not directly");

						auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
						Condition.m_Type = EConditionType_Or;
						Condition.m_Position = _Registry;
						pParent = &Condition;
						bParseChildren = true;
					}
					break;
				case CBuildSystemSyntax::CKeyLogicalOperator::EOperator_Not:
					{
						if (_Registry.f_GetChildren().f_GetLen() != 1)
							CBuildSystem::fs_ThrowError(_Registry, "Not condition must have excatly one child");

						if (_Registry.f_GetThisValue().m_Value.f_IsValid())
							CBuildSystem::fs_ThrowError(_Registry, "You cannot specify a value here");

						auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
						Condition.m_Type = EConditionType_Not;
						Condition.m_Position = _Registry;
						pParent = &Condition;
						bParseChildren = true;
					}
					break;
				}

				bHandled = true;
			}
		}
		while (false)
			;

		if (!bHandled && Name.f_IsValue())
		{
			auto &LeftValue = Name.f_Value();
			auto *pRightValue = &_Registry.f_GetThisValue().m_Value;

			EConditionType ConditionType = EConditionType_MatchEqual;
			if (pRightValue->m_Value.f_IsOfType<CBuildSystemSyntax::COperator>())
			{
				auto &Operator = pRightValue->m_Value.f_GetAsType<CBuildSystemSyntax::COperator>();

				if (!fg_ConvertOperator(ConditionType, Operator.m_Operator, _Registry))
					return false;

				pRightValue = &Operator.m_Right.f_Get();
			}
			else
			{
				if (_bRoot)
					return false;
			}

			auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
			Condition.m_Type = ConditionType;
			Condition.m_Left = LeftValue;
			Condition.m_Position = _Registry;
			Condition.m_Right = *pRightValue;
		}

		if (bParseChildren)
		{
			if (!_Registry.f_HasChildren())
				CBuildSystem::fs_ThrowError(_Registry, "No child conditions found");

			for (auto iReg = _Registry.f_GetChildIterator(); iReg; ++iReg)
				fs_ParseCondition(*iReg, *pParent, false);
		}
		else
		{
			if (_Registry.f_HasChildren())
				CBuildSystem::fs_ThrowError(_Registry, "You cannot specify children here");
		}

		return true;
	}

	void CCondition::fs_ParseCondition(CBuildSystemRegistry const &_Registry, CCondition &_ParentCondition, bool _bRoot)
	{
		if (!fs_TryParseCondition(_Registry, _ParentCondition, _bRoot))
			CBuildSystem::fs_ThrowError(_Registry, "Root conditions only support conditions !!, !, | and & at root level");
	}

	bool CCondition::f_SimpleEval(TCMap<CStr, CStr> const &_Values) const
	{
		switch (m_Type)
		{
		case EConditionType_Root:
		case EConditionType_Or:
			{
				for (auto &Condition : m_Children)
				{
					if (Condition.f_SimpleEval(_Values))
						return true;
				}
				if (m_Type == EConditionType_Root)
				{
					if (m_Children.f_IsEmpty())
						return true;
				}
				return false;
			}
			break;
		case EConditionType_And:
			{
				for (auto &Condition : m_Children)
				{
					if (!Condition.f_SimpleEval(_Values))
						return false;
				}
				return true;
			}
			break;
		case EConditionType_Not:
			{
				for (auto &Condition : m_Children)
					return !Condition.f_SimpleEval(_Values);
				DNeverGetHere;
			}
			break;
		case EConditionType_MatchEqual:
		case EConditionType_MatchNotEqual:
		case EConditionType_CompareEqual:
		case EConditionType_CompareNotEqual:
		case EConditionType_CompareLessThan:
		case EConditionType_CompareLessThanEqual:
		case EConditionType_CompareGreaterThan:
		case EConditionType_CompareGreaterThanEqual:
			{
				CStr Value;

				if (!m_Left.f_IsIdentifier())
					CBuildSystem::fs_ThrowError(m_Position, "Preprocessor conditions only support identifiers as left comparison");

				auto &LeftIdentifier = m_Left.f_Identifier();

				if (!LeftIdentifier.f_NameConstantString())
					CBuildSystem::fs_ThrowError(m_Position, "Preprocessor conditions does not support dynamic identifiers");

				if (auto pValue = _Values.f_FindEqual(LeftIdentifier.f_NameConstantString()))
					Value = *pValue;

				if (!m_Right.f_IsConstantString())
					CBuildSystem::fs_ThrowError(m_Position, "Preprocessor conditions only support strings");

				switch (m_Type)
				{
				case EConditionType_MatchEqual:
				case EConditionType_CompareEqual:
					return m_Right.f_ConstantString() == Value;
				case EConditionType_MatchNotEqual:
				case EConditionType_CompareNotEqual:
					return m_Right.f_ConstantString() != Value;
				case EConditionType_CompareLessThan: return m_Right.f_ConstantString() < Value;
				case EConditionType_CompareLessThanEqual: return m_Right.f_ConstantString() <= Value;
				case EConditionType_CompareGreaterThan: return m_Right.f_ConstantString() > Value;
				case EConditionType_CompareGreaterThanEqual: return m_Right.f_ConstantString() >= Value;
				default: DMibNeverGetHere; break;
				}
			}
		}
		DNeverGetHere;
		return false;
	}
}
