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
		bool m_bLink = false;
	};

	struct CTargetFileInfo : public CGroupMember
	{
		CStr const &f_GetName() const;
		CStr f_GetGroupPath() const;

		TCPointer<CGroupInfo> m_pGroup;
		CEntityMutablePointer m_pEntity;
	};

	struct CFileKey
	{
		CStr m_FileName;
		CStr m_GroupPath;

		bool operator < (CFileKey const &_Other) const
		{
			return fg_TupleReferences(m_FileName, m_GroupPath) < fg_TupleReferences(_Other.m_FileName, _Other.m_GroupPath);
		}
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

		TCMap<CFileKey, CTargetFileInfo> m_Files;

		CDependenciesBackup m_DependenciesBackup;
	};
}
