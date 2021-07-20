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

	CPropertyKey::CPropertyKey(NStr::CStr const &_Name)
		: m_Type(EPropertyType_Property)
		, m_Name(_Name)
	{
	}

	CPropertyKey::CPropertyKey(EPropertyType _Type, NStr::CStr const &_Name)
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
	
	NStr::CStr const &CProperty::f_GetName() const
	{
		return m_Key.m_Name;
	}

	CEvaluatedProperty::CEvaluatedProperty()
		: m_Type(EEvaluatedPropertyType_Implicit)
	{
	}

	inline bool CEvaluatedProperty::f_IsExternal() const
	{
		return m_Type == EEvaluatedPropertyType_External || m_Type == EEvaluatedPropertyType_ExternalEnvironment;
	}

	template <typename tf_CStr>
	void CPropertyKey::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}.{}") << fg_PropertyTypeToStr(m_Type) << m_Name;
	}

	template <typename tf_CContext>
	CPropertyKey CPropertyKey::fs_FromString(CStr const &_String, tf_CContext &&_Context)
	{
		if (auto iPoint = _String.f_FindChar('.'); iPoint >= 0)
		{
			CStr PropertyType = _String.f_Left(iPoint);

			EPropertyType Type = fg_PropertyTypeFromStr(_String.f_Left(iPoint));
			if (PropertyType.f_IsEmpty() || Type == EPropertyType_Invalid)
				CBuildSystem::fs_ThrowError(_Context, CStr::CFormat("Unrecognized property '{}'") << PropertyType);

			return {Type, _String.f_Extract(iPoint + 1)};
		}

		return {EPropertyType_Property, _String};
	}
}
