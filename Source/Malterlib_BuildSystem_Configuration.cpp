// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Condition.h"

namespace NMib::NBuildSystem
{
	bool CConfiguration::operator < (CConfiguration const &_Right) const
	{
		return fg_TupleReferences(m_Platform, m_PlatformBase, m_Configuration) < fg_TupleReferences(_Right.m_Platform, _Right.m_PlatformBase, _Right.m_Configuration);
	}
	
	bool CConfiguration::operator == (CConfiguration const &_Right) const
	{
		return fg_TupleReferences(m_Platform, m_PlatformBase, m_Configuration) == fg_TupleReferences(_Right.m_Platform, _Right.m_PlatformBase, _Right.m_Configuration);
	}
	
	CStr CConfiguration::f_GetFullName() const
	{
		return CStr::CFormat("{} {}") << m_Platform << m_Configuration;
	}

	CStr CConfiguration::f_GetFullSafeName() const
	{
		return (CStr::CFormat("{}_{}") << m_Platform << m_Configuration).f_GetStr().f_Replace(" ", "").f_Replace("(", "").f_Replace(")", "");
	}
}
