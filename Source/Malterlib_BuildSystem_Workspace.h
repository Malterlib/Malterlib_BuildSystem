// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct align_cacheline CWorkspaceInfo
	{
		CStr m_EntityName;
		CEntityMutablePointer m_pEntity;
		TCMap<CStr, CGroupInfo> m_Groups;
		CGroupInfo m_RootGroup;
		
		TCMap<CStr, CTargetInfo> m_Targets;
	};
}
