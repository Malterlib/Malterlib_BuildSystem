// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	CFilePosition::CFilePosition(CBuildSystemRegistry const &_Position)
		: NStr::CParseLocation(_Position.f_GetLocation())
	{
	}

	CFilePosition::CFilePosition(NStr::CParseLocation const &_Position)
		: NStr::CParseLocation(_Position)
	{
	}

	CFilePosition::CFilePosition() = default;

	CFilePosition &CFilePosition::operator = (CBuildSystemRegistry const &_Position)
	{
		(NStr::CParseLocation &)*this = _Position.f_GetLocation();
		return *this;
	}

	CFilePosition &CFilePosition::operator = (NStr::CParseLocation const &_Position)
	{
		(NStr::CParseLocation &)*this = _Position;
		return *this;
	}

	inline NStr::CParseLocation const &CFilePosition::f_Location() const
	{
		return *this;
	}
}
