// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

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
		bool operator == (CGenerateSettings const &_Right) const;

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
		void f_ParseReconcileActions(NEncoding::CEJSONSorted const &_Params);

		CGenerateSettings m_Settings;
		NContainer::TCMap<NStr::CStr, EHandleRepositoryAction> m_ReconcileActions;
		NContainer::TCMap<NStr::CStr, EHandleRepositoryRemovedAction> m_ReconcileRemovedActions;
		EDetailedPositions m_DetailedPositions = EDetailedPositions_OnDemand;
		uint32 m_GitFetchTimeout = 5;
		bool m_bReconcileForce = false;
		bool m_bReconcileNoOptions = false;
		bool m_bSkipUpdate = false;
		bool m_bForceUpdateRemotes = true;
		bool m_bDetailedValues = false;

	};
}
