// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	template <mint tf_nChars>
	constexpr CPropertyKeyReference::CPropertyKeyReference(EPropertyType _Type, NStr::TCStrConstDataAndStr<tf_nChars, ch8> const &_Name)
		: m_TypeAndHash(_Type, fg_StrHash(_Name.m_StrData.m_Data))
		, m_Name(_Name)
	{
	}

	constexpr CPropertyKeyReference::CPropertyKeyReference(EPropertyType _Type, CStringAndHash const &_Name)
		: m_TypeAndHash(_Type, _Name.m_Hash)
		, m_Name(_Name.m_String)
	{
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
		o_Str += typename tf_CStr::CFormat("{}.{}") << fg_PropertyTypeToStr(f_GetType()) << m_Name;
	}

	template <typename tf_CContext>
	CPropertyKey CPropertyKey::fs_FromString(CStringCache &o_StringCache, NStr::CStr const &_String, tf_CContext &&_Context)
	{
		if (auto iPoint = _String.f_FindChar('.'); iPoint >= 0)
		{
			NStr::CStr PropertyType = _String.f_Left(iPoint);

			EPropertyType Type = fg_PropertyTypeFromStr(_String.f_Left(iPoint));
			if (PropertyType.f_IsEmpty() || Type == EPropertyType_Invalid)
				CBuildSystem::fs_ThrowError(_Context, NStr::CStr::CFormat("Unrecognized property '{}'") << PropertyType);

			return {o_StringCache, Type, _String.f_Extract(iPoint + 1)};
		}

		return {o_StringCache, EPropertyType_Property, _String};
	}

	template <typename tf_CStr>
	void CPropertyKeyReference::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}.{}") << fg_PropertyTypeToStr(f_GetType()) << m_Name;
	}

	inline_never COrdering_Strong fg_CompareStringNoInline(NStr::CStr const &_Left, NStr::CStr const &_Right);

	constexpr EPropertyType CPropertyKeyReference::f_GetType() const
	{
		return m_TypeAndHash.f_GetType();
	}

	constexpr uint32 CPropertyKeyReference::f_GetNameHash() const
	{
		return m_TypeAndHash.f_GetNameHash();
	}

	COrdering_Strong CPropertyKeyReference::operator <=> (CPropertyKey const &_Right) const
	{
		return m_TypeAndHash.m_Data <=> _Right.m_TypeAndHash.m_Data;
	}

	COrdering_Strong CPropertyKeyReference::operator <=> (CPropertyKeyReference const &_Right) const
	{
		return m_TypeAndHash.m_Data <=> _Right.m_TypeAndHash.m_Data;
	}

	bool CPropertyKeyReference::operator == (CPropertyKey const &_Right) const
	{
		return m_TypeAndHash.m_Data == _Right.m_TypeAndHash.m_Data;
	}

	bool CPropertyKeyReference::operator == (CPropertyKeyReference const &_Right) const
	{
		return m_TypeAndHash.m_Data == _Right.m_TypeAndHash.m_Data;
	}

	COrdering_Strong CPropertyKey::operator <=> (CPropertyKey const &_Right) const
	{
		return m_TypeAndHash.m_Data <=> _Right.m_TypeAndHash.m_Data;
	}

	COrdering_Strong CPropertyKey::operator <=> (CPropertyKeyReference const &_Right) const
	{
		return m_TypeAndHash.m_Data <=> _Right.m_TypeAndHash.m_Data;
	}

	inline_always COrdering_Strong CPropertyKey::CCompareByString::operator() (CPropertyKey const &_Left, CPropertyKey const &_Right) const
	{
		if (auto Compare = _Left.m_Name <=> _Right.m_Name; Compare != 0)
			return Compare;

		return _Left.f_GetType() <=> _Right.f_GetType();
	}	

	bool CPropertyKey::operator == (CPropertyKey const &_Right) const
	{
		return m_TypeAndHash.m_Data == _Right.m_TypeAndHash.m_Data;
	}

	bool CPropertyKey::operator == (CPropertyKeyReference const &_Right) const
	{
		return m_TypeAndHash.m_Data == _Right.m_TypeAndHash.m_Data;
	}

	constexpr bool CProperty::f_NeedPerFile() const
	{
		return m_Flags & EPropertyFlag_NeedPerFile;
	}
}
