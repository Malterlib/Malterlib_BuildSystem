// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Group.h"

namespace NMib::NBuildSystem
{
	CStr const &CGroupInfo::f_GetPath() const
	{
		return TCMap<CStr, CGroupInfo>::fs_GetKey(*this);
	}

	CStr CGroupInfo::f_GetGroupPath() const
	{
		if (m_pParent)
			return m_pParent->f_GetPath();
		return CStr();
	}

	void CGroupInfo::fr_PruneEmpty()
	{
		for (auto iChild = m_Children.f_GetIterator(); iChild; )
		{
			auto *pChild = iChild.f_GetCurrent();
			++iChild;

			if (pChild->m_bIsGroup)
			{
				((CGroupInfo *)pChild)->fr_PruneEmpty();
				if (((CGroupInfo *)pChild)->m_Children.f_IsEmpty())
					pChild->m_GroupMemberLink.f_Unlink();
			}
		}
	}
}
