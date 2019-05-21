// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	CFilePosition::CFilePosition(CRegistryPreserveAll const &_Position)
		: m_FileName(_Position.f_GetFile())
		, m_Line(_Position.f_GetLine())
	{
	}

	CFilePosition::CFilePosition()
		: m_Line(0)
	{
	}

	CFilePosition &CFilePosition::operator = (CRegistryPreserveAll const &_Position)
	{
		m_FileName = _Position.f_GetFile();
		m_Line = _Position.f_GetLine();
		return *this;
	}
}
