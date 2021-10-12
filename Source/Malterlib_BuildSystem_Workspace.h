// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem_Group.h"
#include "Malterlib_BuildSystem_Target.h"

namespace NMib::NBuildSystem
{
	struct align_cacheline CWorkspaceInfo
	{
		CWorkspaceInfo();
		~CWorkspaceInfo();

		NStorage::TCUniquePointer<CBuildSystemData> m_pEvaluated;
		NStr::CStr m_EntityName;
		CEntityMutablePointer m_pEntity;
		NContainer::TCMap<NStr::CStr, CGroupInfo> m_Groups;
		CGroupInfo m_RootGroup;
		
		NContainer::TCMap<NStr::CStr, CTargetInfo> m_Targets;
	};
}
