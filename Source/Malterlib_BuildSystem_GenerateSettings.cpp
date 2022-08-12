// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_GenerateSettings.h"
#include <Mib/Encoding/EJSON>

namespace NMib::NBuildSystem
{
	bool CGenerateSettings::operator == (CGenerateSettings const &_Right) const
	{
		if (m_SourceFile != _Right.m_SourceFile)
			return false;
		if (m_OutputDir != _Right.m_OutputDir)
			return false;
		if (m_Generator != _Right.m_Generator)
			return false;
		if (m_Workspace != _Right.m_Workspace)
			return false;
		if (m_Action != _Right.m_Action)
			return false;
		if (m_GenerationFlags != _Right.m_GenerationFlags)
			return false;
		return true;
	}

	void CGenerateOptions::f_ParseReconcileActions(NEncoding::CEJSON const &_Params)
	{
		m_bReconcileForce = _Params[gc_ConstString_ReconcileForce].f_Boolean();
		m_bReconcileNoOptions = _Params[gc_ConstString_ReconcileNoOptions].f_Boolean();

		for (auto &RepoOptions : _Params[gc_ConstString_Reconcile].f_String().f_Split<true>(","))
		{
			CStr WildCard;
			CStr ActionStr;
			aint nParsed = 0;
			(CStr::CParse("{}:{}") >> WildCard >> ActionStr).f_Parse(RepoOptions, nParsed);
			if (nParsed != 2)
				DError("Invalid format for --reconcile. Expected --reconcile=Wildcard:Action[,Wildcard:Action]...");
			EHandleRepositoryAction Action;
			if (ActionStr == gc_ConstString_auto.m_String)
				Action = EHandleRepositoryAction_Auto;
			else if (ActionStr == gc_ConstString_reset.m_String)
				Action = EHandleRepositoryAction_Reset;
			else if (ActionStr == gc_ConstString_rebase.m_String)
				Action = EHandleRepositoryAction_Rebase;
			else
				DError("Invalid format for --reconcile. Expected action to be one of: [auto, reset, rebase]");

			m_ReconcileActions[WildCard] = Action;
		}

		for (auto &RepoOptions : _Params[gc_ConstString_ReconcileRemoved].f_String().f_Split<true>(","))
		{
			CStr WildCard;
			CStr ActionStr;
			aint nParsed = 0;
			(CStr::CParse("{}:{}") >> WildCard >> ActionStr).f_Parse(RepoOptions, nParsed);
			if (nParsed != 2)
				DError("Invalid format for --reconcile-removed. Expected --reconcile-revomed=Wildcard:Action[,Wildcard:Action]...");
			EHandleRepositoryRemovedAction Action;
			if (ActionStr == gc_ConstString_leave.m_String)
				Action = EHandleRepositoryRemovedAction_Leave;
			else if (ActionStr == gc_ConstString_delete.m_String)
				Action = EHandleRepositoryRemovedAction_Delete;
			else
				DError("Invalid format for --reconcile-removed. Expected action to be one of: [leave, delete]");

			m_ReconcileRemovedActions[WildCard] = Action;
		}
	}
}
