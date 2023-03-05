// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	inline_always NEncoding::CEJSONSorted *CValuePotentiallyByRef::f_MakeMutable()
	{
		switch (mp_ValueVariant.f_GetTypeID())
		{
		case 0:
			{
				NEncoding::CEJSONSorted NewValue = *mp_ValueVariant.f_Get<0>();
				mp_ValueVariant = fg_Move(NewValue);
				return &mp_ValueVariant.f_Get<2>();
			}
		case 1:
			{
				return mp_ValueVariant.f_Get<1>();
			}
		case 2: break;
		}
		return &mp_ValueVariant.f_Get<2>();
	}

	inline_always NEncoding::CEJSONSorted const &CValuePotentiallyByRef::f_Get() const
	{
		switch (mp_ValueVariant.f_GetTypeID())
		{
		case 0: return *mp_ValueVariant.f_Get<0>();
		case 1: return *mp_ValueVariant.f_Get<1>();
		}
		return mp_ValueVariant.f_Get<2>();
	}

	inline_always NEncoding::CEJSONSorted CValuePotentiallyByRef::f_Move()
	{
		switch (mp_ValueVariant.f_GetTypeID())
		{
		case 0: return *mp_ValueVariant.f_Get<0>();
		case 1: return fg_Move(*mp_ValueVariant.f_Get<1>());
		}
		return fg_Move(mp_ValueVariant.f_Get<2>());
	}

	inline_always CValuePotentiallyByRef CValuePotentiallyByRef::f_GetSubObject(NEncoding::CEJSONSorted const &_SubObject)
	{
		switch (mp_ValueVariant.f_GetTypeID())
		{
		case 0: return &_SubObject;
		case 1: return {&fg_RemoveQualifiers(_SubObject), true};
		}

		return fg_Move(fg_RemoveQualifiers(_SubObject));
	}

	inline_always NContainer::TCVector<NEncoding::CEJSONSorted> CValuePotentiallyByRef::f_MoveArray()
	{
		DMibFastCheck(f_Get().f_IsArray());

		switch (mp_ValueVariant.f_GetTypeID())
		{
		case 0: return mp_ValueVariant.f_Get<0>()->f_Array();
		case 1: return fg_Move(mp_ValueVariant.f_Get<1>()->f_Array());
		}
		return fg_Move(mp_ValueVariant.f_Get<2>().f_Array());
	}

	inline_always NContainer::TCVector<NStr::CStr> CValuePotentiallyByRef::f_MoveStringArray()
	{
		DMibFastCheck(f_Get().f_IsStringArray());

		switch (mp_ValueVariant.f_GetTypeID())
		{
		case 0: return mp_ValueVariant.f_Get<0>()->f_StringArray();
		case 1: return fg_Move(*mp_ValueVariant.f_Get<1>()).f_StringArray();
		case 2: break;
		}
		return fg_Move(mp_ValueVariant.f_Get<2>()).f_StringArray();
	}

	inline_always NStr::CStr CValuePotentiallyByRef::f_MoveString()
	{
		DMibFastCheck(f_Get().f_IsString());

		switch (mp_ValueVariant.f_GetTypeID())
		{
		case 0:
			{
				return mp_ValueVariant.f_Get<0>()->f_String();
			}
			break;
		case 1:
			{
				return fg_Move(mp_ValueVariant.f_Get<1>()->f_String());
			}
			break;
		case 2:
			break;
		}
		return fg_Move(mp_ValueVariant.f_Get<2>().f_String());
	}
}
