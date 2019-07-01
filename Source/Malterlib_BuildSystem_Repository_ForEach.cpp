// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_ForEachRepo(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, bool _bParallel, TCVector<CStr> const &_Params)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;

		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{mp_BaseDir, "Running for each repo", mp_AnsiFlags};
		
		CCurrentlyProcessingActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

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
						Result.f_SetResult(_Result);
						Launches.f_RepoDone();
					}
				;

				if (_bParallel)
					Result.f_MoveFuture() > Results.f_AddResult();
				else
					Result.f_MoveFuture().f_CallSync();
			}

			LaunchResults.f_Insert(Results.f_GetResults().f_CallSync());
		}

		for (auto &Result : LaunchResults)
			Result.f_Access();

		return ERetry_None;
	}
}
