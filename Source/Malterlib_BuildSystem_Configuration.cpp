// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Configuration.h"

namespace NMib::NBuildSystem
{
	CStr CConfiguration::f_GetFullName() const
	{
		return CStr::CFormat("{} {}") << m_Platform << m_Configuration;
	}

	CStr CConfiguration::f_GetFullSafeName() const
	{
		return (CStr::CFormat("{}_{}") << m_Platform << m_Configuration).f_GetStr().f_Replace(" ", "").f_Replace("(", "").f_Replace(")", "");
	}
}
