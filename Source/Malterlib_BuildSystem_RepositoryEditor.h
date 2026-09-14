// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/String/String>
#include <Mib/Container/Vector>

namespace NMib::NBuildSystem::NRepository
{
	struct CRepoEditor
	{
		void f_SetCommandLine(NStr::CStr const &_CommandLine);

		NStr::CStr m_Application;
		NStr::CStr m_WorkingDir;
		NContainer::TCVector<NStr::CStr> m_Params;
		fp32 m_Sleep = 0.0;
		bool m_bOpenSequential = false;
	};
}
