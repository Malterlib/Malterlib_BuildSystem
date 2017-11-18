// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem::NRepository
{
	CBranchSettings::CBranchSettings(CStr const &_OutputDir)
		: m_OutputDir(_OutputDir)
	{
	}

	void CBranchSettings::f_WriteSettings()
	{
		if (!m_bDirty)
			return;

		CStr BranchSettingsFile = "{}/BranchSettings.json"_f << m_OutputDir;

		CEJSON SettingsJson = EJSONType_Object;

		for (auto &Branch : m_Branches)
		{
			auto &OutBranch = SettingsJson[Branch.f_GetType()];
			OutBranch["BranchName"] = Branch.m_Name;
			OutBranch["OnlyChanged"] = Branch.m_bOnlyChanged;
		}

		CFile::fs_CreateDirectory(CFile::fs_GetPath(BranchSettingsFile));
		CFile::fs_WriteStringToFile(BranchSettingsFile + ".temp", SettingsJson.f_ToString());

		if (CFile::fs_FileExists(BranchSettingsFile))
			CFile::fs_AtomicReplaceFile(BranchSettingsFile + ".temp", BranchSettingsFile);
		else
			CFile::fs_RenameFile(BranchSettingsFile + ".temp", BranchSettingsFile);
	}

	void CBranchSettings::f_ReadSettings()
	{
		CStr BranchSettingsFile = "{}/BranchSettings.json"_f << m_OutputDir;

		if (!CFile::fs_FileExists(BranchSettingsFile))
			return;

		CEJSON SettingsJson = CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(BranchSettingsFile), BranchSettingsFile);
		for (auto &Branch : fg_Const(SettingsJson).f_Object())
		{
			auto &OutBranch = m_Branches[Branch.f_Name()];
			OutBranch.m_Name = Branch.f_Value()["BranchName"].f_String();
			OutBranch.m_bOnlyChanged = Branch.f_Value()["OnlyChanged"].f_Boolean();
		}
	}

	void CBranchSettings::f_RemoveBranch(CStr const &_Type)
	{
		if (m_Branches.f_Remove(_Type))
			m_bDirty = true;
	}

	void CBranchSettings::f_SetBranch(CStr const &_Type, CStr const &_Branch, bool _bOnlyChanged)
	{
		if (m_Branches(_Type).f_WasCreated())
			m_bDirty = true;

		auto &Branch = m_Branches[_Type];
		if (Branch.m_Name != _Branch)
		{
			Branch.m_Name = _Branch;
			m_bDirty = true;
		}

		if (Branch.m_bOnlyChanged != _bOnlyChanged)
		{
			Branch.m_bOnlyChanged = _bOnlyChanged;
			m_bDirty = true;
		}
	}
}

namespace NMib::NBuildSystem
{
	using namespace NRepository;
	
	void CBuildSystem::fp_Repository_Branch(CRepoFilter const &_Filter, CStr const &_Branch)
	{
		CBranchSettings BranchSettings{mp_OutputDir};

		BranchSettings.f_ReadSettings();

		bool bHasEmpty = BranchSettings.m_Branches.f_FindEqual("");
		if (_Branch.f_IsEmpty())
		{
			if (bHasEmpty && !BranchSettings.m_Branches.f_IsEmpty())
				DMibError("You cannot mix branch settings for empty repo type with non-empty type");
		}
		else
		{
			if (bHasEmpty)
				DMibError("You cannot mix branch settings for empty repo type with non-empty type");
		}

		BranchSettings.f_SetBranch(_Filter.m_Type, _Branch, _Filter.m_bOnlyChanged);
		BranchSettings.f_WriteSettings();

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

				TCVector<CStr> Params;
				if (fg_GetBranch(Repo) == _Branch)
					continue;

				if (fg_BranchExists(Repo, _Branch))
					Params = {"checkout", _Branch};
				else
					Params = {"checkout", "-b", _Branch};

				TCContinuation<void> LaunchResult;

				Launches.f_Launch(Repo, Params, fg_LogAllFunctor()) > [Launches, LaunchResult](TCAsyncResult<void> &&_Result)
					{
						Launches.f_RepoDone();
						LaunchResult.f_SetResult(_Result);
					}
				;

				LaunchResult > Results.f_AddResult();
			}

			LaunchResults.f_Insert(Results.f_GetResults().f_CallSync());
		}

		for (auto &Result : LaunchResults)
			Result.f_Access();
	}

	void CBuildSystem::fp_Repository_Unbranch(CRepoFilter const &_Filter)
	{
		CBranchSettings BranchSettings{mp_OutputDir};

		BranchSettings.f_ReadSettings();

		auto *pOldBranch = BranchSettings.m_Branches.f_FindEqual(_Filter.m_Type);

		if (!pOldBranch)
			DMibError("Repo type '{}' has not been branched"_f << _Filter.m_Type);

		CStr OldBranch = pOldBranch->m_Name;

		BranchSettings.f_RemoveBranch(_Filter.m_Type);
		BranchSettings.f_WriteSettings();

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

				CStr CurrentBranch = fg_GetBranch(Repo);
				if (CurrentBranch == Repo.m_DefaultBranch)
					continue;

				TCContinuation<void> LaunchResult;

				Launches.f_Launch(Repo, {"checkout", Repo.m_DefaultBranch}, fg_LogAllFunctor()) > [Launches, LaunchResult](TCAsyncResult<void> &&_Result)
					{
						Launches.f_RepoDone();
						LaunchResult.f_SetResult(_Result);
					}
				;

				LaunchResult > Results.f_AddResult();
			}

			LaunchResults.f_Insert(Results.f_GetResults().f_CallSync());

		}

		for (auto &Result : LaunchResults)
			Result.f_Access();
	}

	void CBuildSystem::fp_Repository_CleanupBranches(CRepoFilter const &_Filter, ERepoCleanupBranchesFlag _Flags)
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

				TCContinuation<void> Continuation;

				TCVector<CStr> Params = {"branch", "--format", "%(refname:short)"};
				if (_Flags & ERepoCleanupBranchesFlag_Remote)
					Params.f_Insert("-r");

				Launches.f_Launch(Repo, Params) > Continuation / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result) mutable
					{
						TCActorResultVector<void> Results;

						for (auto &Branch : _Result.f_GetStdOut().f_SplitLine())
						{
							TCContinuation<void> Continuation;

							if (_Flags & ERepoCleanupBranchesFlag_Remote)
							{
								if
									(
									 	Branch.f_EndsWith(("/{}"_f << Repo.m_DefaultBranch).f_GetStr())
									 	|| Branch.f_EndsWith(("/{}"_f << fg_GetBranch(Repo)).f_GetStr())
									 	|| Branch.f_EndsWith("/HEAD")
									)
								{
									continue;
								}
							}
							else
							{
								if (Branch == Repo.m_DefaultBranch || Branch == Repo.m_DefaultUpstreamBranch || Branch == "master" || Branch == fg_GetBranch(Repo))
									continue;
							}

							Launches.f_Launch(Repo, {"merge-base", "--is-ancestor", Branch, Repo.m_DefaultBranch})
								> Continuation / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result) mutable
								{
									bool bIsAncestor = _Result.m_ExitCode == 0;
									if (!bIsAncestor)
									{
										Launches.f_Output(EOutputType_Error, Repo, "{} - not fully merged"_f << Branch);
										Continuation.f_SetResult();
										return;
									}

									TCVector<CStr> Params = {"branch", "-d", Branch};
									if (_Flags & ERepoCleanupBranchesFlag_Remote)
										Params.f_Insert("-r");

									if (_Flags & ERepoCleanupBranchesFlag_Pretend)
									{
										Launches.f_Output(EOutputType_Warning, Repo, "{} - would have deleted"_f << Branch);
										return;
									}

									Launches.f_Output(EOutputType_Warning, Repo, "{} - deleting"_f << Branch);
									Launches.f_Launch(Repo, Params, fg_LogAllFunctor()) > Continuation;
								}
							;
						}

						Results.f_GetResults() > Continuation / [=](TCVector<TCAsyncResult<void>> &&_Results)
							{
								if (!fg_CombineResults(Continuation, fg_Move(_Results)))
									return;

								Launches.f_RepoDone();

								Continuation.f_SetResult();
							}
						;
					}
				;

				Continuation.f_Dispatch() > Results.f_AddResult();
			}

			LaunchResults.f_Insert(Results.f_GetResults().f_CallSync());
		}

		for (auto &Result : LaunchResults)
			Result.f_Access();
	}
}
