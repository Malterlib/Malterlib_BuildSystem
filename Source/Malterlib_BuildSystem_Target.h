// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CTargetDependencyInfo
	{
		NStr::CStr const &f_GetName() const;

		DMibListLinkDS_Link(CTargetDependencyInfo, m_Link);
		CEntityMutablePointer m_pEntity;
		CFilePosition m_Position;
		bool m_bLink = false;
	};

	struct CTargetFileInfo : public CGroupMember
	{
		NStr::CStr const &f_GetName() const;
		NStr::CStr f_GetGroupPath() const;

		NStorage::TCPointer<CGroupInfo> m_pGroup;
		CEntityMutablePointer m_pEntity;
	};

	struct CFileKey
	{
		NStr::CStr m_FileName;
		NStr::CStr m_GroupPath;

		bool operator < (CFileKey const &_Other) const
		{
			return NStorage::fg_TupleReferences(m_FileName, m_GroupPath) < NStorage::fg_TupleReferences(_Other.m_FileName, _Other.m_GroupPath);
		}
	};

	struct align_cacheline CTargetInfo : public CGroupMember
	{
		CTargetInfo(CWorkspaceInfo *_pSolution);
		CTargetInfo();
		~CTargetInfo();

		NStr::CStr const &f_GetName() const;
		NStr::CStr f_GetPath() const;

		NContainer::TCMap<NStr::CStr, CGroupInfo> m_Groups;
		CGroupInfo m_RootGroup;
		NStr::CStr m_EntityName;
		NStorage::TCPointer<CWorkspaceInfo> m_pWorkspace;
		NStorage::TCPointer<CGroupInfo> m_pGroup;

		CEntityMutablePointer m_pOuterEntity;
		CEntityMutablePointer m_pInnerEntity;

		NContainer::TCMap<NStr::CStr, CTargetDependencyInfo> m_DependenciesMap;
		DMibListLinkDS_List(CTargetDependencyInfo, m_Link) m_DependenciesOrdered;
		NContainer::TCSet<NStr::CStr> m_TriedDependenciesMap;

		NContainer::TCMap<CFileKey, CTargetFileInfo> m_Files;

		CDependenciesBackup m_DependenciesBackup;

		bool m_bIsExpanded = false;
	};
}
