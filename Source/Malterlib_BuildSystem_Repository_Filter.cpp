// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem::NRepository
{
	CFilteredRepos fg_GetFilteredRepos(CBuildSystem::CRepoFilter const &_Filter, CBuildSystem &_BuildSystem, CBuildSystemData &_Data)
	{
		CGitLaunches Launches{_BuildSystem.f_GetBaseDir(), "Getting filtered repos"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		CFilteredRepos FilteredRepos;
		FilteredRepos.m_ReposOrdered = fg_GetRepos(_BuildSystem, _Data);
		TCMap<mint, TCVector<CRepository *>> FilteredPerStage;

		if (CFile::fs_FileExists(_BuildSystem.f_GetBaseDir() + "/.git"))
		{
			TCMap<CStr, CReposLocation> RootRepos;
			auto &RootRepoByX = RootRepos[CFile::fs_GetPath(_BuildSystem.f_GetBaseDir())];
			auto &RootRepo = RootRepoByX.m_Repositories[CFile::fs_GetFile(_BuildSystem.f_GetBaseDir())];
			RootRepo.m_Location = _BuildSystem.f_GetBaseDir();
			RootRepo.m_Type = "Root";
			RootRepo.m_Position.m_FileName = _BuildSystem.f_GetGenerateSettings().m_SourceFile;
			RootRepo.m_Position.m_Line = 1;
			FilteredRepos.m_ReposOrdered.f_InsertFirst(fg_Move(RootRepos));
		}

		TCActorResultMap<mint, TCMap<CRepository *, TCAsyncResult<bool>>> DeferredResultsOrdered;

		CStr BaseDir = _BuildSystem.f_GetBaseDir();

		mint iStage = 0;
		for (auto &Repos : FilteredRepos.m_ReposOrdered)
		{
			TCActorResultMap<CRepository *, bool> DeferredResults;
			for (auto &RepoLocation : Repos)
			{
				for (auto &Repo : RepoLocation.m_Repositories)
				{
					if
						(
							!_Filter.m_NameWildcard.f_IsEmpty()
						)
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

					if (_Filter.m_bOnlyChanged)
					{
						fg_RepoIsChanged(Launches, Repo) > DeferredResults.f_AddResult(&Repo);
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
