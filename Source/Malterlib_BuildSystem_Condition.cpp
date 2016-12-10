// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Condition.h"

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
}
