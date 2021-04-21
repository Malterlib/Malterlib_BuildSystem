// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct align_cacheline CWorkspaceInfo
	{
		CWorkspaceInfo(CBuildSystemData const &_Data);
		~CWorkspaceInfo();

		CBuildSystemData m_Evaluated;
		NStr::CStr m_EntityName;
		CEntityMutablePointer m_pEntity;
		NContainer::TCMap<NStr::CStr, CGroupInfo> m_Groups;
		CGroupInfo m_RootGroup;
		
		NContainer::TCMap<NStr::CStr, CTargetInfo> m_Targets;
	};
}
