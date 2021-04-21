// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Target.h"

namespace NMib::NBuildSystem
{
	CWorkspaceInfo::CWorkspaceInfo(CBuildSystemData const &_Data)
		: m_Evaluated(_Data)
	{
	}

	CWorkspaceInfo::~CWorkspaceInfo() = default;
}
