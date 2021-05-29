// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct align_cacheline CConfiguraitonData
	{
		CConfiguraitonData();
		virtual ~CConfiguraitonData();
		
		NContainer::TCVector<CConfigurationTuple> m_Tuples;
		CBuildSystemData m_Evaluated;
		NContainer::TCMap<NStr::CStr, NStorage::TCUniquePointer<CWorkspaceInfo>> m_Workspaces;
		NContainer::TCMap<NStr::CStr, CEntityMutablePointer> m_Targets;
	};
}
