// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CTargetDependencyInfo
	{
		CStr const &f_GetName() const;

		DLinkDS_Link(CTargetDependencyInfo, m_Link);
		CEntityMutablePointer m_pEntity;
		zbool m_bLink;
	};

	struct CTargetFileInfo : public CGroupMember
	{
		CStr const &f_GetName() const;
		CStr f_GetGroupPath() const;

		TCPointer<CGroupInfo> m_pGroup;
		CEntityMutablePointer m_pEntity;
	};
	
	struct align_cacheline CTargetInfo : public CGroupMember
	{
		CTargetInfo(CWorkspaceInfo *_pSolution);
		CTargetInfo();
		~CTargetInfo();

		CStr const &f_GetName() const;
		CStr f_GetPath() const;
		
		TCMap<CStr, CGroupInfo> m_Groups;
		CGroupInfo m_RootGroup;
		CStr m_EntityName;
		TCPointer<CWorkspaceInfo> m_pWorkspace;
		TCPointer<CGroupInfo> m_pGroup;
		
		CEntityMutablePointer m_pOuterEntity;
		CEntityMutablePointer m_pInnerEntity;

		TCMap<CStr, CTargetDependencyInfo> m_DependenciesMap;
		DLinkDS_List(CTargetDependencyInfo, m_Link) m_DependenciesOrdered;
		TCSet<CStr> m_TriedDependenciesMap;
		
		TCMap<CStr, CTargetFileInfo> m_Files;
		
		CDependenciesBackup m_DependenciesBackup;
	};
}
