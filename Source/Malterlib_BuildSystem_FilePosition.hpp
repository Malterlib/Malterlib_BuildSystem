// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	CFilePosition::CFilePosition(CBuildSystemRegistry const &_Position)
		: NStr::CParseLocation(_Position.f_GetLocation())
	{
		m_FileHash = NStr::fg_StrHash(m_File);
	}

	CFilePosition::CFilePosition(NStr::CParseLocation const &_Position)
		: NStr::CParseLocation(_Position)
	{
		m_FileHash = NStr::fg_StrHash(m_File);
	}

	constexpr CFilePosition::CFilePosition() = default;

	CFilePosition &CFilePosition::operator = (CBuildSystemRegistry const &_Position)
	{
		(NStr::CParseLocation &)*this = _Position.f_GetLocation();
		m_FileHash = NStr::fg_StrHash(m_File);
		return *this;
	}

	CFilePosition &CFilePosition::operator = (NStr::CParseLocation const &_Position)
	{
		(NStr::CParseLocation &)*this = _Position;
		m_FileHash = NStr::fg_StrHash(m_File);
		return *this;
	}

	inline NStr::CParseLocation const &CFilePosition::f_Location() const
	{
		return *this;
	}

	template <typename t_CType>
	template <typename tf_CValue>
	void TCValueWithPositions<t_CType>::f_SetFrom(tf_CValue &&_Value, NStr::CStr const &_Name, CBuildSystemPropertyInfo const &_PropertyInfo, t_CType &&_DefaultValue)
	{
		using namespace NStr;

		auto *pMember = _Value.f_GetMember(_Name);
		if (!pMember)
		{
			m_Value = fg_Move(_DefaultValue);
			return;
		}

		auto *pPositions = pMember->f_GetMember(gc_ConstString_Positions);

		if (!pPositions)
			CBuildSystem::fs_ThrowError(_PropertyInfo, "Missing Positions member for '{}'"_f << _Name);

		if (pPositions)
			m_Positions = CBuildSystemUniquePositions::fs_FromJson(*pPositions);

		if constexpr (NTraits::cIsSame<t_CType, NEncoding::CEJsonSorted>)
			m_Value = fg_Forward<tf_CValue>(pMember->f_GetMemberValue(gc_ConstString_Value, fg_Move(_DefaultValue)));
		else if constexpr (NTraits::cIsSame<t_CType, NStr::CStr>)
			m_Value = fg_ForwardAs<tf_CValue>(pMember->f_GetMemberValue(gc_ConstString_Value, fg_Move(_DefaultValue)).f_String());
		else if constexpr (NTraits::cIsSame<t_CType, NContainer::TCVector<NStr::CStr>>)
			m_Value = fg_ForwardAs<tf_CValue>(pMember->f_GetMemberValue(gc_ConstString_Value, fg_Move(_DefaultValue)).f_StringArray());
		else if constexpr (NTraits::cIsSame<t_CType, int32>)
			m_Value = pMember->f_GetMemberValue(gc_ConstString_Value, _DefaultValue).f_Integer();
		else if constexpr (NTraits::cIsSame<t_CType, bool>)
			m_Value = pMember->f_GetMemberValue(gc_ConstString_Value, _DefaultValue).f_Boolean();
		else
			static_assert(NTraits::cIsSame<t_CType, CVoidTag>, "Unsupported type");
	}

	template <typename t_CType>
	template <typename tf_CValue>
	void TCValueWithPositions<t_CType>::f_SetFrom(tf_CValue &&_Value, NStr::CStr const &_Name, CBuildSystemPropertyInfo const &_PropertyInfo)
	{
		using namespace NStr;

		auto *pMember = _Value.f_GetMember(_Name);
		if (!pMember)
			CBuildSystem::fs_ThrowError(_PropertyInfo, "Missing member '{}'"_f << _Name);

		auto *pPositions = pMember->f_GetMember(gc_ConstString_Positions);

		if (!pPositions)
			CBuildSystem::fs_ThrowError(_PropertyInfo, "Missing Positions member for '{}'"_f << _Name);

		if (pPositions)
			m_Positions = CBuildSystemUniquePositions::fs_FromJson(*pPositions);

		auto *pValue = pMember->f_GetMember(gc_ConstString_Value);
		if (!pValue)
			CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Missing Value member for '{}'"_f << _Name);

		if constexpr (NTraits::cIsSame<t_CType, NEncoding::CEJsonSorted>)
			m_Value = fg_Forward<tf_CValue>(*pValue);
		else if constexpr (NTraits::cIsSame<t_CType, NStr::CStr>)
		{
			if (!pValue->f_IsString())
				CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Expected value to be string for '{}'"_f << _Name);

			m_Value = fg_ForwardAs<tf_CValue>(pValue->f_String());
		}
		else if constexpr (NTraits::cIsSame<t_CType, NContainer::TCVector<NStr::CStr>>)
		{
			if (!pValue->f_IsStringArray())
				CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Expected value to be a string array for '{}'"_f << _Name);

			m_Value = fg_ForwardAs<tf_CValue>(pValue->f_StringArray());
		}
		else if constexpr (NTraits::cIsSame<t_CType, int32>)
		{
			if (!pValue->f_IsInteger())
				CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Expected value to be integer for '{}'"_f << _Name);

			m_Value = pValue->f_Integer();
		}
		else if constexpr (NTraits::cIsSame<t_CType, bool>)
		{
			if (!pValue->f_IsBoolean())
				CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Expected value to be boolean for '{}'"_f << _Name);

			m_Value = pValue->f_Boolean();
		}
		else if constexpr (NTraits::cIsSame<t_CType, NStorage::TCOptional<NStr::CStr>>)
		{
			if (pValue->f_IsString())
				m_Value = fg_ForwardAs<tf_CValue>(pValue->f_String());
			else if (pValue->f_IsValid())
				CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Expected value to be string or undefined for '{}'"_f << _Name);
		}
		else if constexpr (NTraits::cIsSame<t_CType, NStorage::TCOptional<NContainer::TCVector<NStr::CStr>>>)
		{
			if (pValue->f_IsStringArray())
				m_Value = fg_ForwardAs<tf_CValue>(pValue->f_StringArray());
			else if (pValue->f_IsValid())
				CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Expected value to be a string array or undefined for '{}'"_f << _Name);
		}
		else if constexpr (NTraits::cIsSame<t_CType, NStorage::TCOptional<int32>>)
		{
			if (pValue->f_IsInteger())
				m_Value = pValue->f_Integer();
			else if (pValue->f_IsValid())
				CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Expected value to be integer or undefined for '{}'"_f << _Name);
		}
		else if constexpr (NTraits::cIsSame<t_CType, NStorage::TCOptional<bool>>)
		{
			if (pValue->f_IsBoolean())
				m_Value = pValue->f_Boolean();
			else if (pValue->f_IsValid())
				CBuildSystem::fs_ThrowError(_PropertyInfo, m_Positions, "Expected value to be boolean or undefined for '{}'"_f << _Name);
		}
		else
			static_assert(NTraits::cIsSame<t_CType, CVoidTag>, "Unsupported type");
	}
}
