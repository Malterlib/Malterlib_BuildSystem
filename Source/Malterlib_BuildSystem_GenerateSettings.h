// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	enum EGenerationFlag // : uint32
	{
		EGenerationFlag_None 				= 0
		, EGenerationFlag_AbsoluteFilePaths 	= DMibBit(0)
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
	};

	enum EHandleRepositoryRemovedAction
	{
		EHandleRepositoryRemovedAction_None
		, EHandleRepositoryRemovedAction_Leave
		, EHandleRepositoryRemovedAction_Delete
	};

	struct CGenerateSettings
	{
		bool operator == (CGenerateSettings const &_Right) const;

		CStr m_SourceFile;
		CStr m_OutputDir;
		CStr m_Generator;
		CStr m_Workspace;
		CStr m_Action = "Build";
		EGenerationFlag m_GenerationFlags = EGenerationFlag_None;
	};

	struct CGenerateOptions
	{
		void f_ParseReconcileActions(NEncoding::CEJSON const &_Params);

		CGenerateSettings m_Settings;
		TCMap<CStr, EHandleRepositoryAction> m_ReconcileActions;
		TCMap<CStr, EHandleRepositoryRemovedAction> m_ReconcileRemovedActions;
		bool m_bReconcileForce = false;
		bool m_bReconcileNoOptions = false;
		bool m_bSkipUpdate = false;
	};
}
