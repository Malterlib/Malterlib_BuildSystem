// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	struct CValuePotentiallyByRef
	{
		CValuePotentiallyByRef(NEncoding::CEJSONSorted const &_Value) = delete;
		CValuePotentiallyByRef(NEncoding::CEJSONSorted const *_pValue);
		CValuePotentiallyByRef(NEncoding::CEJSONSorted *_pValue, bool _bConfirm);
		CValuePotentiallyByRef(NEncoding::CEJSONSorted &&_Value);

		CValuePotentiallyByRef(CValuePotentiallyByRef const &);
		CValuePotentiallyByRef(CValuePotentiallyByRef &&);

		CValuePotentiallyByRef &operator = (CValuePotentiallyByRef const &) = delete;
		CValuePotentiallyByRef &operator = (CValuePotentiallyByRef &&) = delete;

		inline_always NEncoding::CEJSONSorted *f_MakeMutable();
		inline_always NEncoding::CEJSONSorted const &f_Get();
		inline_always NEncoding::CEJSONSorted f_Move();
		inline_always CValuePotentiallyByRef f_GetSubObject(NEncoding::CEJSONSorted const &_SubObject);
		inline_always NContainer::TCVector<NEncoding::CEJSONSorted> f_MoveArray();
		inline_always NContainer::TCVector<NStr::CStr> f_MoveStringArray();
		inline_always NStr::CStr f_MoveString();

		void f_Set(NEncoding::CEJSONSorted &&_Other);
		
	private:
		NStorage::TCVariant<NEncoding::CEJSONSorted const *, NEncoding::CEJSONSorted *, NEncoding::CEJSONSorted> mp_ValueVariant;
	};
}
