// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct align_cacheline CConfiguraitonData
	{
		CConfiguraitonData();
		virtual ~CConfiguraitonData();
		
		TCVector<CConfigurationTuple> m_Tuples;
		CBuildSystemData m_Evaluated;
		TCMap<CStr, CWorkspaceInfo> m_Workspaces;
	};
}
