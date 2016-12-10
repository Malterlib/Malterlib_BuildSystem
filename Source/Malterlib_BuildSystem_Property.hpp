// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	CPropertyKey::CPropertyKey()
		: m_Type(EPropertyType_Property)
	{
	}

	CPropertyKey::CPropertyKey(CStr const &_Name)
		: m_Type(EPropertyType_Property)
		, m_Name(_Name)
	{
	}

	CPropertyKey::CPropertyKey(EPropertyType _Type, CStr const &_Name)
		: m_Type(_Type)
		, m_Name(_Name)
	{
	}

	bool CPropertyKey::operator < (CPropertyKey const &_Right) const
	{
		if (m_Type < _Right.m_Type)
			return true;
		else if (m_Type > _Right.m_Type)
			return false;
		return m_Name < _Right.m_Name;
	}
	
	EPropertyType CProperty::f_GetType() const
	{
		return m_Key.m_Type;
	}
	
	CStr const &CProperty::f_GetName() const
	{
		return m_Key.m_Name;
	}

	CEvaluatedProperty::CEvaluatedProperty()
		: m_Type(EEvaluatedPropertyType_Implicit)
	{
	}
}
