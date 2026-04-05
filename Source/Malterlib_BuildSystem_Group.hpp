// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem
{
	CGroupMember::CGroupMember(bool _bIsGroup)
		: m_bIsGroup(_bIsGroup)
	{
	}

	CGroupInfo::CGroupInfo()
		: CGroupMember(true)
	{
	}
}
