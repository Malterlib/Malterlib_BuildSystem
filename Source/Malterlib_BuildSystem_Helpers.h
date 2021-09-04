// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	template <typename t_CSortKey>
	class TCSortedPerform
	{
	public:
		template <typename tf_CFunctor, typename tf_CKey>
		void f_Add(tf_CKey const &_Key, tf_CFunctor &&_Functor);
		void f_Perform();
		
	private:
		struct CToSortBy
		{
			CToSortBy(t_CSortKey const &_SortKey);
			CToSortBy() = default;

			COrdering_Weak operator <=> (CToSortBy const &_Other) const;
			
			t_CSortKey m_SortBy;
			TCFunction<void ()> *m_pFunctor;
		};
		
		TCLinkedList<TCFunction<void ()>> m_ToPerform;
		TCVector<CToSortBy> m_ToSort;
	};

#ifndef DDocumentation_Doxygen
	template <typename t_CSortKey>
	class TCSortedPerform<t_CSortKey &>
	{
	public:
		template <typename tf_CFunctor, typename tf_CKey>
		void f_Add(tf_CKey &&_Key, tf_CFunctor &&_Functor);
		void f_Perform();

	private:
		struct CToSortBy
		{
			CToSortBy(t_CSortKey &_SortKey);
			CToSortBy() = default;
			COrdering_Weak operator <=> (CToSortBy const &_Other) const;

			t_CSortKey *m_pSortBy;
			TCFunction<void ()> *m_pFunctor;
		};
		
		TCLinkedList<TCFunction<void ()>> m_ToPerform;
		TCVector<CToSortBy> m_ToSort;
	};
#endif
	
	CStr fg_EscapeXcodeProjectVar(CStr const &_Var);
}

#include "Malterlib_BuildSystem_Helpers.hpp"
