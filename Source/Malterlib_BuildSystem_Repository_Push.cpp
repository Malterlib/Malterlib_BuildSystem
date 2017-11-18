// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem
{
	using namespace NRepository;
	
	void CBuildSystem::fp_Repository_Push(CRepoFilter const &_Filter, TCVector<CStr> const &_Remotes)
	{
		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{mp_BaseDir};

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
						Launches.f_Launch(Repo, {"push", Repo.m_DefaultBranch}, fg_LogAllFunctor()) > [Launches, LaunchResult](TCAsyncResult<void> &&_Result)
							{
								Launches.f_RepoDone();
								LaunchResult.f_SetResult(_Result);
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
