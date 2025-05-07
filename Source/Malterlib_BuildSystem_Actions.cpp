// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include <Mib/Encoding/EJson>

namespace NMib::NBuildSystem
{
	TCUnsafeFuture<CBuildSystem::ERetry> DMibWorkaroundUBSanSectionErrors CBuildSystem::fs_RunBuildSystem
		(
			NFunction::TCFunctionMovable<TCFuture<CBuildSystem::ERetry> (CBuildSystem *_pBuildSystem)> _fCommand
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			, CGenerateOptions const &_GenerateOptions
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		TCSharedPointer<TCAtomic<bool>> pCancelled = fg_Construct();

		auto CancellationSubscription = _pCommandLine->f_RegisterForCancellation
			(
				g_ActorFunctor
				(
					g_ActorSubscription / []()
					{
					}
				)
				/ [pCancelled, _pCommandLine, nAborts = 0]() mutable -> TCFuture<bool>
				{
					if (nAborts > 5)
						co_await _pCommandLine->f_StdErr("\33[2K\rAborting again won't make it go faster\n");
					else if (nAborts > 0)
						co_await _pCommandLine->f_StdErr("\33[2K\rAlready aborting\n");
					else
						co_await _pCommandLine->f_StdErr("\33[2K\rAborting\n");

					++nAborts;
					pCancelled->f_Exchange(true);
					co_return false;
				}
			)
		;

		co_await fg_ContinueRunningOnActor(fg_ConcurrentActorLowPrio());

		CBuildSystem::ERetry Retry = CBuildSystem::ERetry_Again;
		while (Retry != CBuildSystem::ERetry_None)
		{
			auto AnsiFlags = _pCommandLine->f_AnsiEncoding().f_Flags();
			CBuildSystem BuildSystem(AnsiFlags, _fOutputConsole, pCancelled);

			if (_GenerateOptions.m_DetailedPositions == EDetailedPositions_Enable)
				BuildSystem.f_SetEnablePositions();

			if (_GenerateOptions.m_bDetailedValues)
				BuildSystem.f_SetEnableValues();

			if (Retry == CBuildSystem::ERetry_Again_NoReconcileOptions)
				BuildSystem.f_NoReconcileOptions();
			else if (Retry == CBuildSystem::ERetry_Again_EnablePositions)
				BuildSystem.f_SetEnablePositions();

			auto RetryResult = co_await _fCommand(&BuildSystem).f_Wrap();
			if (!RetryResult)
			{
				if (RetryResult.f_HasExceptionType<NStr::CExceptionParse>() && !BuildSystem.f_EnablePositions() && _GenerateOptions.m_DetailedPositions == EDetailedPositions_OnDemand)
				{
					CAnsiEncoding AnsiEncoding(AnsiFlags);

					_fOutputConsole
						(
							"\n"
							"{}Provisional errors:{}\n"
							"{}\n"_f
							<< AnsiEncoding.f_StatusError()
							<< AnsiEncoding.f_Default()
							<< RetryResult.f_GetExceptionStr()
							, true
						)
					;
					_fOutputConsole
						(
							"\n"
							"{}Re-running with detailed positions.{} See: --detailed-positions. {}\n"
							"\n"_f
							<< AnsiEncoding.f_StatusWarning()
							<< AnsiEncoding.f_Default()
							<< (BuildSystem.f_EnableValues() ? "" : "Detailed values is not enabled. See --detailed-values")
							, true
						)
					;

					Retry = CBuildSystem::ERetry_Again_EnablePositions;
				}
				else
					co_return RetryResult.f_GetException();
			}
			else
				Retry = *RetryResult;

			if (Retry == CBuildSystem::ERetry_Relaunch || Retry == CBuildSystem::ERetry_Relaunch_NoReconcileOptions)
				co_return Retry;
		}
		co_return CBuildSystem::ERetry_None;
	}

	CBuildSystem::CRepoFilter CBuildSystem::CRepoFilter::fs_ParseParams(NEncoding::CEJsonSorted const &_Params)
	{
		CBuildSystem::CRepoFilter Filter;

		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoName))
			Filter.m_NameWildcard = pValue->f_String();
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoType))
			Filter.m_Type = pValue->f_String();
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoBranch))
			Filter.m_Branch = pValue->f_String();
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoTags))
			Filter.m_Tags.f_AddContainer(pValue->f_String().f_Split<true>(";"));
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoOnlyChanged))
			Filter.m_bOnlyChanged = pValue->f_Boolean();

		return Filter;
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_Update(CGenerateOptions const &_GenerateOptions)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;
		
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;
		co_return ERetry_None;
	}
}
