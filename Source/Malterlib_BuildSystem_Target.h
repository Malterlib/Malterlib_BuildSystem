// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem_DataForwardDeclare.h"
#include "Malterlib_BuildSystem_Group.h"
#include "Malterlib_BuildSystem_FilePosition.h"
#include "Malterlib_BuildSystem_Data.h"

namespace NMib::NBuildSystem
{
	struct CWorkspaceInfo;

	struct CTargetDependencyInfo
	{
		NStr::CStr const &f_GetName() const;

		DMibListLinkDS_Link(CTargetDependencyInfo, m_Link);
		CEntityMutablePointer m_pEntity;
		CFilePosition m_Position;
		bool m_bLink = false;
		bool m_bExternal = false;
		bool m_bIndirect = false;
		bool m_bIndirectOrdered = false;
		bool m_bObjectLibrary = false;
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

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("{} - {}") << m_GroupPath << m_FileName;
		}

		auto operator <=> (CFileKey const &_Other) const = default;
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

		NContainer::TCSet<NStr::CStr> m_AlreadyAddedGroups;

		bool m_bIsExpanded = false;
		bool m_bObjectLibrary = false;
	};
}
