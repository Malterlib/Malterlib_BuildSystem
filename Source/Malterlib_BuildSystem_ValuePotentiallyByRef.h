// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	struct CValuePotentiallyByRef
	{
		CValuePotentiallyByRef(NEncoding::CEJsonSorted const &_Value) = delete;
		CValuePotentiallyByRef(NEncoding::CEJsonSorted const *_pValue);
		CValuePotentiallyByRef(NEncoding::CEJsonSorted *_pValue, bool _bConfirm);
		CValuePotentiallyByRef(NEncoding::CEJsonSorted &&_Value);

		CValuePotentiallyByRef(CValuePotentiallyByRef const &_Other);
		CValuePotentiallyByRef(CValuePotentiallyByRef &&_Other);

		CValuePotentiallyByRef &operator = (CValuePotentiallyByRef const &_Other) = delete;
		CValuePotentiallyByRef &operator = (CValuePotentiallyByRef &&_Other) = delete;

		inline_always NEncoding::CEJsonSorted *f_MakeMutable();
		inline_always NEncoding::CEJsonSorted const &f_Get() const;
		inline_always NEncoding::CEJsonSorted f_Move();
		inline_always CValuePotentiallyByRef f_GetSubObject(NEncoding::CEJsonSorted const &_SubObject);
		inline_always NContainer::TCVector<NEncoding::CEJsonSorted> f_MoveArray();
		inline_always NContainer::TCVector<NStr::CStr> f_MoveStringArray();
		inline_always NStr::CStr f_MoveString();

		void f_Set(NEncoding::CEJsonSorted &&_Other);

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

	private:
		NStorage::TCVariant<NEncoding::CEJsonSorted const *, NEncoding::CEJsonSorted *, NEncoding::CEJsonSorted> mp_ValueVariant;
	};
}
