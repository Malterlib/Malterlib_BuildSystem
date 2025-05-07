// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_BuildSystem_ValuePotentiallyByRef.h"

namespace NMib::NBuildSystem
{
	CValuePotentiallyByRef::CValuePotentiallyByRef(NEncoding::CEJsonSorted const *_pValue)
		: mp_ValueVariant(_pValue)
	{
	}

	CValuePotentiallyByRef::CValuePotentiallyByRef(NEncoding::CEJsonSorted *_pValue, bool _bConfirm)
		: mp_ValueVariant(_pValue)
	{
	}
	CValuePotentiallyByRef::CValuePotentiallyByRef(NEncoding::CEJsonSorted &&_Value)
		: mp_ValueVariant(fg_Move(_Value))
	{
	}

	CValuePotentiallyByRef::CValuePotentiallyByRef(CValuePotentiallyByRef const &) = default;

	CValuePotentiallyByRef::CValuePotentiallyByRef(CValuePotentiallyByRef &&) = default;

	void CValuePotentiallyByRef::f_Set(NEncoding::CEJsonSorted &&_Other)
	{
		DMibFastCheck(f_Get().f_IsString());

		switch (mp_ValueVariant.f_GetTypeID())
		{
		case 1:
			{
				*mp_ValueVariant.f_Get<1>() = fg_Move(_Other);
			}
			return;
		case 0:
		case 2:
			break;
		}
		mp_ValueVariant = fg_Move(_Other);
	}

}
