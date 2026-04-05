// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_BuildSystem_Configuration.h"
#include "Malterlib_BuildSystem_Data.h"
#include "Malterlib_BuildSystem_Workspace.h"

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
