// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem::NRepository
{
	CFilteredRepos fg_GetFilteredRepos(CBuildSystem::CRepoFilter const &_Filter, CBuildSystem &_BuildSystem, CBuildSystemData &_Data, EFilterRepoFlag _Flags)
	{
		CGitLaunches Launches{_BuildSystem.f_GetBaseDir(), "Filtering repos"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		CFilteredRepos FilteredRepos;
		FilteredRepos.m_ReposOrdered = fg_GetRepos(_BuildSystem, _Data);
		TCMap<mint, TCVector<CRepository *>> FilteredPerStage;

		TCActorResultMap<mint, TCMap<CRepository *, TCAsyncResult<bool>>> DeferredResultsOrdered;

		CStr BaseDir = _BuildSystem.f_GetBaseDir();

		mint nLaunchRepos = 0;
		mint iStage = 0;
		for (auto &Repos : FilteredRepos.m_ReposOrdered)
		{
			TCActorResultMap<CRepository *, bool> DeferredResults;
			for (auto &RepoLocation : Repos)
			{
				for (auto &Repo : RepoLocation.m_Repositories)
				{
					if (!_Filter.m_NameWildcard.f_IsEmpty())
					{
						auto RelativePath = CFile::fs_MakePathRelative(Repo.m_Location, BaseDir);
						CStr Name;
						if (!RelativePath.f_IsEmpty())
							Name = "{}"_f << CFile::fs_MakePathRelative(Repo.m_Location, BaseDir);
						else
							Name = ".";

						if (NStr::fg_StrMatchWildcard(Name.f_GetStr(), _Filter.m_NameWildcard.f_GetStr()) != NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							continue;
					}

					if (!_Filter.m_Type.f_IsEmpty() && Repo.m_Type != _Filter.m_Type)
						continue;

					if (!_Filter.m_Tags.f_IsEmpty() && Repo.m_Tags.f_And(_Filter.m_Tags) != _Filter.m_Tags)
						continue;

					if (!_Filter.m_Branch.f_IsEmpty() && fg_GetBranch(Repo) != _Filter.m_Branch)
						continue;

					if (_Filter.m_bOnlyChanged)
					{
						++nLaunchRepos;
						g_Dispatch(Launches.m_pState->m_OutputActor) / [=]() mutable
							{
								Launches.f_SetNumRepos(nLaunchRepos);
							}
							> fg_DiscardResult()
						;
						TCPromise<bool> ChangedDonePromise;
						ChangedDonePromise > DeferredResults.f_AddResult(&Repo);
						fg_RepoIsChanged(Launches, Repo, _Flags) > [Launches, ChangedDonePromise](TCAsyncResult<bool> &&_bChanged)
							{
								Launches.f_RepoDone();
								ChangedDonePromise.f_SetResult(_bChanged);
							}
						;
						continue;
					}

					FilteredPerStage[iStage].f_Insert(&Repo);
				}
			}
			DeferredResults.f_GetResults() > DeferredResultsOrdered.f_AddResult(iStage);
			++iStage;
		}

		auto SyncedResults = DeferredResultsOrdered.f_GetResults().f_CallSync();
		for (auto &Repos : SyncedResults)
		{
			mint iStage = SyncedResults.fs_GetKey(Repos);

			for (auto &IsChanged : *Repos)
			{
				CRepository *pRepo = (*Repos).fs_GetKey(IsChanged);
				if (!*IsChanged)
					continue;

				FilteredPerStage[iStage].f_Insert(pRepo);
			}
		}

		for (auto &Repos : FilteredPerStage)
		{
			if (Repos.f_IsEmpty())
				continue;
			FilteredRepos.m_FilteredRepositories.f_Insert(fg_Move(Repos));
		}

		return FilteredRepos;
	}
}
