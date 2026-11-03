// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem
{
	constexpr CStringAndHash::CStringAndHash(CAssertAddedToStringCache _Dummy, NStr::CStr const &_String, uint32 _Hash)
		: m_String(_String)
		, m_Hash(_Hash)
	{
	}

	template <umint tf_nChars>
	constexpr CStringAndHash::CStringAndHash(NStr::TCStrConstDataAndStr<tf_nChars, ch8> const &_String)
		: m_Hash(fg_StrHash(_String.m_StrData.m_Data))
		, m_String(_String)
	{
	}

	constexpr CStringAndHash::operator NStr::CStr const & () const
	{
		return m_String;
	}

	template <typename tf_CStr>
	void CStringAndHash::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}") << m_String;
	}
}
