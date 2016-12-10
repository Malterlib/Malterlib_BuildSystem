// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CGroupMember
	{
		CGroupMember(bool _bIsGroup = false);
		
		DLinkDS_Link(CGroupMember, m_GroupMemberLink);
		bool m_bIsGroup;
	};
	
	struct CGroupInfo : public CGroupMember
	{
		CGroupInfo();
		
		CStr const &f_GetPath() const;
		CStr f_GetGroupPath() const;
		void fr_PruneEmpty();
		
		CStr m_Name;
		TCPointer<CGroupInfo> m_pParent;
		DLinkDS_List(CGroupMember, m_GroupMemberLink) m_Children;
		
	private:
		CStr mp_GUID;
	};
}

#include "Malterlib_BuildSystem_Group.hpp"
