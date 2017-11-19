// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem
{
	using namespace NRepository;
	
	void CBuildSystem::fp_Repository_Push(CRepoFilter const &_Filter, TCVector<CStr> const &_Remotes)
	{
		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{mp_BaseDir, "Pushing repos"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCActorResultVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				TCContinuation<TCVector<CStr>> Remotes;

				if (_Remotes.f_IsEmpty())
					Remotes = fg_GetRemotes(Launches, Repo);
				else
					Remotes.f_SetResult(_Remotes);

				TCContinuation<void> LaunchResult;
				Remotes + fg_GetBranches(Launches, Repo, false)
					> LaunchResult / [=](TCVector<CStr> &&_Remotes, CGitBranches &&_Branches)
					{
						if (_Branches.m_Current.f_IsEmpty())
						{
							Launches.f_RepoDone();
							LaunchResult.f_SetResult();
							return;
						}

						TCActorResultVector<void> PushResults;

						for (auto &Remote : _Remotes)
							Launches.f_Launch(Repo, {"push", Remote, _Branches.m_Current}, fg_LogAllFunctor()) > PushResults.f_AddResult();

						PushResults.f_GetResults() > LaunchResult / [=](TCVector<TCAsyncResult<void>> &&_PushResults)
							{
								Launches.f_RepoDone();

								if (!fg_CombineResults(LaunchResult, fg_Move(_PushResults)))
									return;

								LaunchResult.f_SetResult();
							}
						;
					}
				;

				LaunchResult > Results.f_AddResult();
			}

			LaunchResults.f_Insert(Results.f_GetResults().f_CallSync());
		}

		for (auto &Result : LaunchResults)
			Result.f_Access();
	}
}
