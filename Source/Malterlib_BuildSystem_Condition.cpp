// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Condition.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	bool CCondition::f_NeedPerFile() const
	{
		for (auto iChild = m_Children.f_GetIterator(); iChild; ++iChild)
		{
			auto &Child = *iChild;
			if (Child.m_Type == EConditionType_Compare && Child.m_Subject == "this.Type" && Child.m_Value == "File")
				return true;

			if (Child.f_NeedPerFile())
				return true;
		}
		return false;
	}

	void CCondition::fs_ParseCondition(CRegistryPreserveAll &_Registry, CCondition &_ParentCondition, bool _bRoot)
	{
		CStr Name = _Registry.f_GetName();
		CStr Value = _Registry.f_GetThisValue();

		ch8 Character = Name[0];

		bool bParseChildren = false;
		CCondition *pParent = nullptr;

		switch (Character)
		{
		case '&':
			{
				if (Name != "&")
					CBuildSystem::fs_ThrowError(_Registry, "And condition can only be specified as a container for children, not directly");

				auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
				Condition.m_Type = EConditionType_And;
				Condition.m_Position = _Registry;
				pParent = &Condition;
				bParseChildren = true;
			}
			break;
		case '|':
			{
				if (Name != "|")
					CBuildSystem::fs_ThrowError(_Registry, "Or condition can only be specified as a container for children, not directly");
				auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
				Condition.m_Type = EConditionType_Or;
				Condition.m_Position = _Registry;
				pParent = &Condition;
				bParseChildren = true;
			}
			break;
		case '!':
			if (Name.f_StartsWith("!!"))
			{
				if (!_bRoot)
					CBuildSystem::fs_ThrowError(_Registry, "!! condition only supported at root level");
				auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
				Condition.m_Type = EConditionType_Compare;
				Condition.m_Subject = Name.f_Extract(2);
				Condition.m_Value = Value;
				Condition.m_Position = _Registry;
			}
			else if (Name != "!")
			{
				auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
				Condition.m_Type = EConditionType_CompareNot;
				Condition.m_Subject = Name.f_Extract(1);
				Condition.m_Value = Value;
				Condition.m_Position = _Registry;
			}
			else
			{
				if (_Registry.f_GetChildren().f_GetLen() != 1)
					CBuildSystem::fs_ThrowError(_Registry, "Not condition must have excatly one child");
				auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
				Condition.m_Type = EConditionType_Not;
				Condition.m_Position = _Registry;
				pParent = &Condition;
				bParseChildren = true;
			}
			break;
		default:
			{
				if (_bRoot)
					CBuildSystem::fs_ThrowError(_Registry, "Root conditions only support conditions !!, !, | and & at root level");

				auto &Condition = _ParentCondition.m_Children.f_Insert(fg_Construct());
				Condition.m_Type = EConditionType_Compare;
				Condition.m_Subject = Name;
				Condition.m_Value = Value;
				Condition.m_Position = _Registry;
			}
			break;
		}

		if (bParseChildren)
		{
			if (!Value.f_IsEmpty() || _Registry.f_GetForceEscapedValue())
				CBuildSystem::fs_ThrowError(_Registry, "You cannot specify a value here");

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
		case EConditionType_Compare:
			{
				CStr Value;

				if (auto pValue = _Values.f_FindEqual(m_Subject))
					Value = *pValue;

				return m_Value == Value;
			}
		case EConditionType_CompareNot:
			{
				CStr Value;

				if (auto pValue = _Values.f_FindEqual(m_Subject))
					Value = *pValue;

				return m_Value != Value;
			}
		default:
			{
				DNeverGetHere;
			}
			break;
		}
		return false;
	}
}
