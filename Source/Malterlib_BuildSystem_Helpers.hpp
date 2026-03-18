// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	template <typename t_CSortKey>
	TCSortedPerform<t_CSortKey>::CToSortBy::CToSortBy(t_CSortKey const &_SortKey)
		: m_SortBy(_SortKey)
	{
	}

	template <typename t_CSortKey>
	COrdering_Strong TCSortedPerform<t_CSortKey>::CToSortBy::operator <=> (CToSortBy const &_Other) const noexcept
	{
		return m_SortBy <=> _Other.m_SortBy;
	}

	template <typename t_CSortKey>
	template <typename tf_CFunctor, typename tf_CKey>
	void TCSortedPerform<t_CSortKey>::f_Add(tf_CKey const &_Key, tf_CFunctor &&_Functor)
	{
		auto &Inserted = m_ToPerform.f_Insert(fg_Construct(fg_Forward<tf_CFunctor>(_Functor)));
		m_ToSort.f_Insert(fg_Construct(_Key)).m_pFunctor = &Inserted;
	}

	template <typename t_CSortKey>
	void TCSortedPerform<t_CSortKey>::f_Perform()
	{
		m_ToSort.f_Sort();
		for (auto iPerform = m_ToSort.f_GetIterator(); iPerform; ++iPerform)
		{
			(*iPerform->m_pFunctor)();
		}
	}

	template <typename t_CSortKey>
	TCSortedPerform<t_CSortKey &>::CToSortBy::CToSortBy(t_CSortKey &_SortKey)
		: m_pSortBy(&_SortKey)
	{
	}

	template <typename t_CSortKey>
	COrdering_Strong TCSortedPerform<t_CSortKey &>::CToSortBy::operator <=> (CToSortBy const &_Other) const noexcept
	{
		return *m_pSortBy <=> *_Other.m_pSortBy;
	}

	template <typename t_CSortKey>
	template <typename tf_CFunctor, typename tf_CKey>
	void TCSortedPerform<t_CSortKey &>::f_Add(tf_CKey &&_Key, tf_CFunctor &&_Functor)
	{
		auto &Inserted = m_ToPerform.f_Insert(fg_Construct(fg_Forward<tf_CFunctor>(_Functor)));
		m_ToSort.f_Insert(fg_Construct(_Key)).m_pFunctor = &Inserted;
	}

	template <typename t_CSortKey>
	void TCSortedPerform<t_CSortKey &>::f_Perform()
	{
		m_ToSort.f_Sort();
		for (auto iPerform = m_ToSort.f_GetIterator(); iPerform; ++iPerform)
		{
			(*iPerform->m_pFunctor)();
		}
	}
}
