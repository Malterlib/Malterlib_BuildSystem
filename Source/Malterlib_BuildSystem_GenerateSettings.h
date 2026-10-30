// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_BuildSystem_Lock.h"

namespace NMib::NBuildSystem
{
	enum EGenerationFlag // : uint32
	{
		EGenerationFlag_None					= 0
		, EGenerationFlag_AbsoluteFilePaths		= DMibBit(0)
		, EGenerationFlag_SingleThreaded		= DMibBit(1)
		, EGenerationFlag_UseCachedEnvironment	= DMibBit(2)
		, EGenerationFlag_DisableUserSettings	= DMibBit(3)
	};

	enum EHandleRepositoryAction
	{
		EHandleRepositoryAction_None
		, EHandleRepositoryAction_Auto
		, EHandleRepositoryAction_ManualResolve
		, EHandleRepositoryAction_Reset
		, EHandleRepositoryAction_Rebase
		, EHandleRepositoryAction_Leave
		, EHandleRepositoryAction_Init
		, EHandleRepositoryAction_CloneRemote
		, EHandleRepositoryAction_ForkRemote
	};

	enum EHandleRepositoryRemovedAction
	{
		EHandleRepositoryRemovedAction_None
		, EHandleRepositoryRemovedAction_Leave
		, EHandleRepositoryRemovedAction_Delete
	};

	enum EDetailedPositions
	{
		EDetailedPositions_Enable
		, EDetailedPositions_Disable
		, EDetailedPositions_OnDemand
	};

	struct CGenerateSettings
	{
		bool operator == (CGenerateSettings const &_Right) const noexcept;

		NStr::CStr m_SourceFile;
		NStr::CStr m_OutputDir;
		NStr::CStr m_Generator;
		NStr::CStr m_Workspace;
		NStr::CStr m_Action = "Build";
		NContainer::TCMap<NStr::CStr, NStr::CStr> m_Environment = fg_GetSys()->f_Environment();
		EGenerationFlag m_GenerationFlags = EGenerationFlag_None;
	};

	struct CGenerateOptions
	{
		void f_ParseReconcileActions(NEncoding::CEJsonSorted const &_Params);

		CGenerateSettings m_Settings;
		NContainer::TCMap<NStr::CStr, EHandleRepositoryAction> m_ReconcileActions;
		NContainer::TCMap<NStr::CStr, EHandleRepositoryRemovedAction> m_ReconcileRemovedActions;
		CBuildSystemLockSettings m_LockSettings;
		NStorage::TCOptional<NStr::CStr> m_BaseBranch; // From --base-branch, overriding the base in .malterlib-base for one run
		NStr::CStr m_VerifyHook; // The command update-repos runs on each batch of changes before writing them; only from the command line
		NStorage::TCSharedPointer<NAtomic::TCAtomic<uint32>> m_pVerifyHookBatch; // The last batch number, shared by the passes of a run
		EDetailedPositions m_DetailedPositions = EDetailedPositions_OnDemand;
		uint32 m_GitFetchTimeout = 5;
		bool m_bReconcileForce = false;
		bool m_bReconcileNoOptions = false;
		bool m_bIgnoreLeave = false; // Removed repositories are reconciled as if none set Repository.ReconcileRemoved "Leave"
		bool m_bSkipUpdate = false;
		bool m_bForceUpdateRemotes = true;
		bool m_bForceUpdateHooks = false;
		bool m_bDetailedValues = false;
		bool m_bApplyRepoPolicy = false;
		bool m_bApplyRepoPolicyPretend = true;
		bool m_bApplyRepoPolicyCreateMissing = true;
		bool m_bUpdateLfsReleaseIndexes = false;
		bool m_bUpdateLfsReleaseIndexesPretend = false;
		bool m_bUpdateLfsReleaseIndexesPruneOrphanedAssets = false;
		bool m_bShowProgress = true;
		bool m_bStash = true; // Parsed from --stash in MTool (Malterlib_Tool_App_MTool_Malterlib.cpp), default true
	};
}
