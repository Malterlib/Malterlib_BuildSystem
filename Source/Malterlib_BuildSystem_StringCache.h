// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CPropertyKeyReference;
	struct CStringAndHash;

	struct CStringCache
	{
		void f_AddConstantString(CStringAndHash const &_String);
		void f_AddConstantString(CPropertyKeyReference const &_Key);
		void f_AddConstantStringWithoutHash(NStr::CStr const &_String);

		NStr::CStr const &f_AddString(NStr::CStr const &_String, uint32 _Hash);

		NThread::CMutualManyRead m_Lock;
		NContainer::TCMap<uint32, NStr::CStr> m_Strings;

	private:
		void fp_AddConstantString(NStr::CStr const &_String, uint32 _Hash);
	};

	struct CAssertAddedToStringCache
	{
	};

	struct CStringAndHash
	{
		CStringAndHash(CStringCache &o_StringCache, NStr::CStr const &_String, uint32 _Hash)
			: m_String(o_StringCache.f_AddString(_String, _Hash))
			, m_Hash(_Hash)
		{
		}

		constexpr CStringAndHash(CAssertAddedToStringCache, NStr::CStr const &_String, uint32 _Hash)
			: m_String(_String)
			, m_Hash(_Hash)
		{
		}

		template <mint t_nChars>
		constexpr CStringAndHash(NStr::TCStrConstDataAndStr<t_nChars> const &_String)
			: m_Hash(fg_StrHash(_String.m_StrData.m_Data))
			, m_String(_String)
		{
		}

		constexpr operator NStr::CStr const &() const
		{
			return m_String;
		}

		CStringAndHash(CStringAndHash &&) = default;
		CStringAndHash(CStringAndHash const &) = default;

		auto operator <=> (CStringAndHash const &) const = default;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("{}") << m_String;
		}

		uint32 m_Hash = 0;
		NStr::CStr m_String;
	};
}
