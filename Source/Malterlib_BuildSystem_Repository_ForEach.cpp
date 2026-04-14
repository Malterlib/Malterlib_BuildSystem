// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_ForEachRepo
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, bool _bParallel
			, TCVector<CStr> const &_Params
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None);

		CGitLaunches Launches{f_GetGitLaunchOptions("git"), "Running for each repo"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCFutureVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				TCPromiseFuturePair<void> Result;
				Launches.f_Launch(Repo, _Params, fg_LogAllFunctor()) > [=, ResultPromise = fg_Move(Result.m_Promise)](TCAsyncResult<void> &&_Result)
					{
						ResultPromise.f_SetResult(fg_Move(_Result));
						Launches.f_RepoDone();
					}
				;

				if (_bParallel)
					fg_Move(Result.m_Future) > Results;
				else
					co_await fg_Move(Result.m_Future);
			}

			LaunchResults.f_Insert(co_await fg_AllDoneWrapped(Results));
		}

		co_await (fg_Move(LaunchResults) | g_Unwrap);

		co_return ERetry_None;
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_ForEachRepoDir
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, CForEachRepoDirOptions const &_Options
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None);

		CGitLaunches Launches{f_GetGitLaunchOptions("repo-run"), "Running for each repo"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCFutureVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				TCPromiseFuturePair<void> Result;
				Launches.f_Launch(Repo, _Options.m_Params, fg_LogAllFunctor(), {}, {}, _Options.m_Application) > [=, ResultPromise = fg_Move(Result.m_Promise)](TCAsyncResult<void> &&_Result)
					{
						ResultPromise.f_SetResult(fg_Move(_Result));
						Launches.f_RepoDone();
					}
				;

				if (_Options.m_bParallel)
					fg_Move(Result.m_Future) > Results;
				else
					co_await fg_Move(Result.m_Future);
			}

			LaunchResults.f_Insert(co_await fg_AllDoneWrapped(Results));
		}

		co_await (fg_Move(LaunchResults) | g_Unwrap);

		co_return ERetry_None;
	}
}
