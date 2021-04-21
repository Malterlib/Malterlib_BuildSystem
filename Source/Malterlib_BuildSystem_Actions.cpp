// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include <Mib/Encoding/EJSON>

namespace NMib::NBuildSystem
{
	auto CBuildSystem::fs_RunBuildSystem
		(
			NFunction::TCFunction<CBuildSystem::ERetry (CBuildSystem &_BuildSystem)> &&_fCommand
			, EAnsiEncodingFlag _AnsiFlags
			, NFunction::TCFunction<void (NStr::CStr const &_Output)> const &_fOutputConsole
		) -> ERetry
	{
		CBuildSystem::ERetry Retry = CBuildSystem::ERetry_Again;
		while (Retry != CBuildSystem::ERetry_None)
		{
			CBuildSystem BuildSystem(_AnsiFlags, _fOutputConsole);
			if (Retry == CBuildSystem::ERetry_Again_NoReconcileOptions)
				BuildSystem.f_NoReconcileOptions();

			Retry = _fCommand(BuildSystem);
			if (Retry == CBuildSystem::ERetry_Relaunch || Retry == CBuildSystem::ERetry_Relaunch_NoReconcileOptions)
				return Retry;
		}
		return CBuildSystem::ERetry_None;
	}

	CBuildSystem::CRepoFilter CBuildSystem::CRepoFilter::fs_ParseParams(NEncoding::CEJSON const &_Params)
	{
		CBuildSystem::CRepoFilter Filter;

		if (auto pValue = _Params.f_GetMember("RepoName"))
			Filter.m_NameWildcard = pValue->f_String();
		if (auto pValue = _Params.f_GetMember("RepoType"))
			Filter.m_Type = pValue->f_String();
		if (auto pValue = _Params.f_GetMember("RepoBranch"))
			Filter.m_Branch = pValue->f_String();
		if (auto pValue = _Params.f_GetMember("RepoTags"))
			Filter.m_Tags.f_AddContainer(pValue->f_String().f_Split<true>(";"));
		if (auto pValue = _Params.f_GetMember("RepoOnlyChanged"))
			Filter.m_bOnlyChanged = pValue->f_Boolean();

		return Filter;
	}

	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_Update(CGenerateOptions const &_GenerateOptions)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;
		return ERetry_None;
	}
}
