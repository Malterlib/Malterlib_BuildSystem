// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	TCFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_ForEachRepo
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, bool _bParallel
			, TCVector<CStr> const &_Params
		)
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureExceptions);
		
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{f_GetBaseDir(), "Running for each repo", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCActorResultVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				TCPromise<void> Result;
				Launches.f_Launch(Repo, _Params, fg_LogAllFunctor()) > [=](TCAsyncResult<void> &&_Result)
					{
						Result.f_SetResult(fg_Move(_Result));
						Launches.f_RepoDone();
					}
				;

				if (_bParallel)
					Result.f_MoveFuture() > Results.f_AddResult();
				else
					co_await Result.f_MoveFuture();
			}

			LaunchResults.f_Insert(co_await Results.f_GetResults());
		}

		fg_Move(LaunchResults) | g_Unwrap;

		co_return ERetry_None;
	}
}
