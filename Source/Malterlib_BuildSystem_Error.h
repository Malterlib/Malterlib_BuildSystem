// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem
{
	struct CBuildSystemError
	{
		CBuildSystemUniquePositions m_Positions;
		NStr::CStr m_Error;
	};
}
