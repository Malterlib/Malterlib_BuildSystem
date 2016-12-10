// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Target.h"

namespace NMib::NBuildSystem
{
	CStr const &CTargetDependencyInfo::f_GetName() const
	{
		return TCMap<CStr, CTargetDependencyInfo>::fs_GetKey(*this);
	}
	
	CStr const &CTargetFileInfo::f_GetName() const
	{
		return TCMap<CStr, CTargetFileInfo>::fs_GetKey(*this);
	}
	
	CStr CTargetFileInfo::f_GetGroupPath() const
	{
		if (m_pGroup)
			return m_pGroup->f_GetPath();
		return CStr();
	}
	
	CTargetInfo::CTargetInfo() = default;

	CTargetInfo::CTargetInfo(CWorkspaceInfo *_pWorkspace)
		: m_pWorkspace(_pWorkspace)
	{
	}
	
	CTargetInfo::~CTargetInfo()
	{
		m_DependenciesOrdered.f_Clear();
	}

	CStr const &CTargetInfo::f_GetName() const
	{
		return TCMap<CStr, CTargetInfo>::fs_GetKey(*this);
	}

	CStr CTargetInfo::f_GetPath() const
	{
		if (m_pGroup)
			return m_pGroup->f_GetPath() + "/" + f_GetName();
		return f_GetName();
	}
}
