// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
		NContainer::TCVector<CEntityMutablePointer> m_ReplacedEntities; // Of the stage None, beside the declaration that orders the build
		NContainer::TCMap<CEntity const *, bool> m_MergedPassedOn;      // The merged declarations of targets that pass the dependency on, and whether as indirect
		CFilePosition m_Position;
		bool m_bLink = false;
		bool m_bExternal = false;
		bool m_bIndirect = false;
		bool m_bIndirectOrdered = false;
		bool m_bObjectLibrary = false;
		bool m_bOrdered = true;                                         // A dependency of the stage None only gives the target the properties of its target
		bool m_bPassesOn = false;                                       // Injects and passes on its dependencies: links it or is of another stage than None
		// The most restrictive of the declarations of the dependency in the target, which the generators read (Dependency.*Evaluated)
		NStr::CStr m_MergedStage;
		bool m_bMergedLink = false;
		bool m_bMergedCompileAfterGenerated = false;
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
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		auto operator <=> (CFileKey const &_Other) const noexcept = default;

		NStr::CStr m_FileName;
		NStr::CStr m_GroupPath;
	};

	struct align_cacheline CTargetInfo : public CGroupMember
	{
		struct CCompareByName
		{
			COrdering_Strong operator () (CTargetInfo const *_pLeft, CTargetInfo const *_pRight) const;
		};

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
		NEncoding::CEJsonSorted m_MergedDependencies; // The merged values of the dependencies at the last evaluation, which settle as its lists do

		bool m_bIsExpanded = false;
		bool m_bObjectLibrary = false;
	};
}

#include "Malterlib_BuildSystem_Target.hpp"
