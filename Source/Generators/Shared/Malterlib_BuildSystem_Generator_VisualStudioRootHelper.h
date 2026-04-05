// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../../Malterlib_BuildSystem.h"
#include <Mib/Process/ProcessLaunch>

namespace NMib::NBuildSystem
{
	struct CVisualStudioRootHelper
	{
		CVisualStudioRootHelper(CBuildSystem const &_BuildSystem);

		CEJsonSorted const &f_GetVisualStudioRoot(uint32 _Version) const;
		CSystemEnvironment f_GetBuildEnvironment(uint32 _Version, CStr const &_Platform, CStr const &_Architecture) const;

		CBuildSystem const &m_BuildSystem;
		mutable CLowLevelLock m_VisualStudioRootLock;
		mutable TCAtomic<bool> m_VisualStudioRootCached;
		mutable CEJsonSorted m_VisualStudioRoot;

		mutable CMutual m_GetEnvironmentLock;
		mutable TCMap<CStr, CMutual> m_GetEnvironmentLocks;
		mutable TCMap<CStr, TCMap<CStr, CStr>> m_CachedBuildEnvironment;
	};
}
