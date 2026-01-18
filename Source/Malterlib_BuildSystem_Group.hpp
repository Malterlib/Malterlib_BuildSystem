// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
