// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CGroupMember
	{
		inline CGroupMember(bool _bIsGroup = false);
		
		DMibListLinkDS_Link(CGroupMember, m_GroupMemberLink);
		bool m_bIsGroup;
	};
	
	struct CGroupInfo : public CGroupMember
	{
		inline CGroupInfo();
		
		NStr::CStr const &f_GetPath() const;
		NStr::CStr f_GetGroupPath() const;
		void fr_PruneEmpty();
		
		NStr::CStr m_Name;
		NStorage::TCPointer<CGroupInfo> m_pParent;
		DMibListLinkDS_List(CGroupMember, m_GroupMemberLink) m_Children;
		
	private:
		NStr::CStr mp_GUID;
	};
}

#include "Malterlib_BuildSystem_Group.hpp"
