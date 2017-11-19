// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	void CBuildSystem::fp_Repository_ForEachRepo(CRepoFilter const &_Filter, bool _bParallell, TCVector<CStr> const &_Params)
	{
		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{mp_BaseDir, "Running for each repo"};
		
		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCActorResultVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				TCContinuation<void> Result;
				Launches.f_Launch(Repo, _Params, fg_LogAllFunctor()) > [=](TCAsyncResult<void> &&_Result)
					{
						Result.f_SetResult(_Result);
						Launches.f_RepoDone();
					}
				;

				if (_bParallell)
					Result.f_Dispatch() > Results.f_AddResult();
				else
					Result.f_Dispatch().f_CallSync();
			}

			LaunchResults.f_Insert(Results.f_GetResults().f_CallSync());
		}

		for (auto &Result : LaunchResults)
			Result.f_Access();
	}
}
