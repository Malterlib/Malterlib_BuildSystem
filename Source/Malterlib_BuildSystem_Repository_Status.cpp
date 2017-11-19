// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
		struct CRepoStatusState
		{
			bool m_bIsChange = false;
			CStr m_ToPush;
			CStr m_ToPull;

			TCVector<CLocalFileChange> m_LocalChanges;
			TCVector<CStr> m_Remotes;
			CGitBranches m_LocalBranches;
			CGitBranches m_RemoteBranches;

			bool f_HasRemoteBranch(CStr const &_FullBranch) const
			{
				return m_RemoteBranches.m_Branches.f_FindEqual(_FullBranch);
			}
		};
	}

	void CBuildSystem::fp_Repository_Status(CRepoFilter const &_Filter, ERepoStatusFlag _Flags)
	{
		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		TCVector<CRepository> AllRepos;
		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			for (auto *pRepo : Repos)
				AllRepos.f_Insert(*pRepo);
		}

		if (_Flags & ERepoStatusFlag_UpdateRemotes)
		{
			CGitLaunches Launches{mp_BaseDir, "Fetching remotes"};
			Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

			CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

			TCActorResultVector<void> Results;

			for (auto Repo : AllRepos)
			{
				TCContinuation<void> Continuation;
				Launches.f_Launch(Repo, {"fetch", "--all", "-q"}, fg_LogAllFunctor()) > [=](TCAsyncResult<void> &&_Result)
					{
						Continuation.f_SetResult(_Result);
						Launches.f_RepoDone();
					}
				;

				Continuation.f_Dispatch() > Results.f_AddResult();
			}

			for (auto &Result : Results.f_GetResults().f_CallSync())
				Result.f_Access();
		}

		CGitLaunches Launches{mp_BaseDir, "Getting repo status"};
		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};
		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		bool bOpenSourceTree = _Flags & ERepoStatusFlag_OpenSourceTree;
		bool bUseDefaultUpstream = _Flags & ERepoStatusFlag_UseDefaultUpstreamBranch;

		TCActorResultVector<bool> Results;

		for (auto Repo : AllRepos)
		{
			TCContinuation<bool> Continuation;

			fg_GetLocalFileChanges(Launches, Repo, _Flags & ERepoStatusFlag_ShowUntracked)
				+ fg_GetRemotes(Launches, Repo)
				+ fg_GetBranches(Launches, Repo, false)
				+ fg_GetBranches(Launches, Repo, true)
				> Continuation / [=](TCVector<CLocalFileChange> &&_LocalChanges, TCVector<CStr> &&_Remotes, CGitBranches &&_LocalBranches, CGitBranches &&_RemoteBranches)
				{
					TCSharedPointer<CRepoStatusState> pState = fg_Construct();

					auto &State = *pState;

					State.m_LocalChanges = fg_Move(_LocalChanges);
					State.m_Remotes = fg_Move(_Remotes);
					State.m_LocalBranches = fg_Move(_LocalBranches);
					State.m_RemoteBranches = fg_Move(_RemoteBranches);

					TCSet<CStr> Branches;

					if (_Flags & ERepoStatusFlag_AllBranches)
						Branches = State.m_LocalBranches.m_Branches;
					else if (!State.m_LocalBranches.m_Current.f_IsEmpty())
						Branches = {State.m_LocalBranches.m_Current};

					TCActorResultVector<bool> BranchResults;
					for (auto &Branch : Branches)
					{
						TCContinuation<bool> Continuation;

						TCActorResultMap<CStr, TCVector<CLogEntry>> ToPush;
						TCActorResultMap<CStr, TCVector<CLogEntry>> ToPull;
						TCSet<CStr> ToPushMissing;

						for (auto &Remote : State.m_Remotes)
						{
							CStr RemoteBranch = "{}/{}"_f << Remote << Branch;
							CStr PullRemoteBranch = RemoteBranch;
							CStr MissingRemoteBranch = RemoteBranch;

							if (bUseDefaultUpstream && Branch == Repo.m_DefaultBranch && !Repo.m_DefaultUpstreamBranch.f_IsEmpty() && !State.f_HasRemoteBranch(RemoteBranch))
								PullRemoteBranch = "{}/{}"_f << Remote << Repo.m_DefaultUpstreamBranch;

							if (Branch == Repo.m_DefaultBranch && !Repo.m_DefaultUpstreamBranch.f_IsEmpty() && !State.f_HasRemoteBranch(RemoteBranch))
								MissingRemoteBranch = "{}/{}"_f << Remote << Repo.m_DefaultUpstreamBranch;

							if (State.f_HasRemoteBranch(RemoteBranch))
								fg_GetLogEntries(Launches, Repo, RemoteBranch, Branch) > ToPush.f_AddResult(Remote);
							else if (!State.f_HasRemoteBranch(MissingRemoteBranch))
								ToPushMissing[Remote];

							if (State.f_HasRemoteBranch(PullRemoteBranch))
								fg_GetLogEntries(Launches, Repo, Branch, PullRemoteBranch) > ToPull.f_AddResult(Remote);
						}

						ToPush.f_GetResults()
							+ ToPull.f_GetResults()
							> [=](TCAsyncResult<TCMap<CStr, TCAsyncResult<TCVector<CLogEntry>>>> &&_ToPush, TCAsyncResult<TCMap<CStr, TCAsyncResult<TCVector<CLogEntry>>>> &&_ToPull)
							{
								auto &State = *pState;

								TCMap<CStr, TCVector<CLogEntry>> ToRemotePush;
								TCMap<CStr, TCVector<CLogEntry>> ToLocalPull;

								if
									(
									 	!fg_CombineResults
									 	(
										 	Continuation
										 	, fg_Move(_ToPush)
										 	, [&](CStr const &_Remote, TCVector<CLogEntry> &&_Log)
										 	{
												ToRemotePush[_Remote] = fg_Move(_Log);
											}
										)
									)
								{
									return;
								}

								if
									(
									 	!fg_CombineResults
									 	(
										 	Continuation
										 	, fg_Move(_ToPull)
										 	, [&](CStr const &_Remote, TCVector<CLogEntry> &&_Log)
										 	{
												ToLocalPull[_Remote] = fg_Move(_Log);
											}
										)
									)
								{
									return;
								}

								bool bHasRemotes = !ToRemotePush.f_IsEmpty() || !ToLocalPull.f_IsEmpty();
								bool bIsChanged = !ToPushMissing.f_IsEmpty();
								bool bIsCurrentBranch = Branch == State.m_LocalBranches.m_Current;

								bool bHasDifferentUpstream =
									bUseDefaultUpstream
									&& Branch == Repo.m_DefaultBranch
									&& !Repo.m_DefaultUpstreamBranch.f_IsEmpty()
									&& Repo.m_DefaultBranch != Repo.m_DefaultUpstreamBranch
								;

								TCVector<CStr> Messages;

								if (bIsCurrentBranch && !State.m_LocalChanges.f_IsEmpty())
								{
									bIsChanged = true;

									Messages.f_Insert
										(
											"{}Local{}[{}Commit {}{}]"_f
											<< CColors::mc_RepositoryName
											<< CColors::mc_Default
											<< CColors::mc_ToCommit
											<< CColors::mc_Default
											<< State.m_LocalChanges.f_GetLen()
										)
									;
								}

								TCSet<CStr> RemotesWithAction = ToPushMissing;

								for (auto &ToPush : ToRemotePush)
								{
									if (ToPush.f_IsEmpty())
										continue;
									bIsChanged = true;
									RemotesWithAction[ToRemotePush.fs_GetKey(ToPush)];
								}

								for (auto &ToPull : ToLocalPull)
								{
									if (ToPull.f_IsEmpty())
										continue;
									bIsChanged = true;
									RemotesWithAction[ToLocalPull.fs_GetKey(ToPull)];
								}

								for (auto &RemoteName : RemotesWithAction)
								{
									TCVector<CStr> RemoteMessages;
									auto pToPush = ToRemotePush.f_FindEqual(RemoteName);
									auto pToPull = ToLocalPull.f_FindEqual(RemoteName);
									bool bMissing = ToPushMissing.f_FindEqual(RemoteName);

									if (pToPush && !pToPush->f_IsEmpty())
									{
										RemoteMessages.f_Insert
											(
												"{}Push {}{}"_f
												<< CColors::mc_ToPush
												<< CColors::mc_Default
												<< pToPush->f_GetLen()
											)
										;
									}

									if (bMissing)
									{
										RemoteMessages.f_Insert
											(
												"{}Push {}?"_f
												<< CColors::mc_ToPush
												<< CColors::mc_Default
											)
										;
									}

									if (pToPull && !pToPull->f_IsEmpty())
									{
										if (bHasDifferentUpstream && !State.f_HasRemoteBranch("{}/{}"_f << RemoteName << Branch))
										{
											RemoteMessages.f_Insert
												(
													"{}Pull {}{}{} {}"_f
													<< CColors::mc_ToPull
												 	<< DColor_256(242)
												 	<< Repo.m_DefaultUpstreamBranch
													<< CColors::mc_Default
													<< pToPull->f_GetLen()
												)
											;
										}
										else
										{
											RemoteMessages.f_Insert
												(
													"{}Pull {}{}"_f
													<< CColors::mc_ToPull
													<< CColors::mc_Default
													<< pToPull->f_GetLen()
												)
											;
										}
									}

									Messages.f_Insert
										(
											"{}{}{}[{}]"_f
											<< CColors::mc_RepositoryName
											<< RemoteName
											<< CColors::mc_Default
										 	<< CStr::fs_Join(RemoteMessages, " ")
										)
									;
								}

								if (!bIsChanged && (_Flags & ERepoStatusFlag_Quiet))
								{
									Continuation.f_SetResult(bIsChanged);
									return;
								}

								EOutputType OutputType = EOutputType_Normal;

								if (bIsChanged)
									OutputType = EOutputType_Error;
								else if (!bHasRemotes)
									OutputType = EOutputType_Warning;

								Launches.f_Output
									(
									 	OutputType
									 	, Repo
									 	, "{}{}{}{} {}"_f
									 	<< CColors::mc_BranchName
									 	<< (bIsCurrentBranch ? "*" : " ")
									 	<< Branch
									 	<< CColors::mc_Default
									 	<< CStr::fs_Join(Messages, " ")
									)
								;

								if (!(_Flags & ERepoStatusFlag_Verbose))
								{
									Continuation.f_SetResult(bIsChanged);
									return;
								}

								if (bIsCurrentBranch && !State.m_LocalChanges.f_IsEmpty())
								{
									Launches.f_Output
										(
											OutputType
											, Repo
										 	, "   {}Local {}To Commit{}"_f
											<< CColors::mc_RepositoryName
											<< CColors::mc_ToCommit
											<< CColors::mc_Default
										)
									;

									for (auto &Change : State.m_LocalChanges)
									{
										Launches.f_Output
											(
												OutputType
												, Repo
												, "      {}{} {}{}"_f
												<< CColors::mc_ToCommit
											 	<< Change.m_ChangeType
												<< CColors::mc_Default
											 	<< Change.m_File
											)
										;
									}
								}

								for (auto &ToPush : ToRemotePush)
								{
									if (ToPush.f_IsEmpty())
										continue;

									auto &RemoteName = ToRemotePush.fs_GetKey(ToPush);

									Launches.f_Output
										(
											OutputType
											, Repo
										 	, "   {}{} {}To Push{}"_f
											<< CColors::mc_RepositoryName
										 	<< RemoteName
											<< CColors::mc_ToPush
											<< CColors::mc_Default
										)
									;

									for (auto &Commit : ToPush)
									{
										Launches.f_Output
											(
												OutputType
												, Repo
											 	, "      {}{} {}{}"_f
												<< CColors::mc_ToPush
											 	<< Commit.m_Hash
												<< CColors::mc_Default
											 	<< Commit.m_Description
											)
										;
									}
								}

								for (auto &RemoteName : ToPushMissing)
								{
									Launches.f_Output
										(
											OutputType
											, Repo
										 	, "   {}{} {}To Push{}"_f
											<< CColors::mc_RepositoryName
										 	<< RemoteName
											<< CColors::mc_ToPush
											<< CColors::mc_Default
										)
									;
									Launches.f_Output
										(
											OutputType
											, Repo
											, "      {}??????? {}Branch missing on remote"_f
											<< CColors::mc_ToPush
											<< CColors::mc_Default
										)
									;
								}

								for (auto &ToPull : ToLocalPull)
								{
									if (ToPull.f_IsEmpty())
										continue;

									auto &RemoteName = ToLocalPull.fs_GetKey(ToPull);

									if (bHasDifferentUpstream && !State.f_HasRemoteBranch("{}/{}"_f << RemoteName << Branch))
									{
										Launches.f_Output
											(
												OutputType
												, Repo
											 	, "   {}{} {}To Pull{} {}{}{}"_f
												<< CColors::mc_RepositoryName
												<< RemoteName
												<< CColors::mc_ToPull
												<< CColors::mc_Default
												<< DColor_256(242)
												<< Repo.m_DefaultUpstreamBranch
												<< CColors::mc_Default
											)
										;
									}
									else
									{
										Launches.f_Output
											(
												OutputType
												, Repo
												, "   {}{} {}To Pull{}"_f
												<< CColors::mc_RepositoryName
												<< RemoteName
												<< CColors::mc_ToPull
												<< CColors::mc_Default
											)
										;
									}


									for (auto &Commit : ToPull)
									{
										Launches.f_Output
											(
												OutputType
												, Repo
											 	, "      {}{} {}{}"_f
												<< CColors::mc_ToPull
											 	<< Commit.m_Hash
												<< CColors::mc_Default
											 	<< Commit.m_Description
											)
										;
									}
								}

								Continuation.f_SetResult(bIsChanged);
							}
						;

						Continuation.f_Dispatch() > BranchResults.f_AddResult();
					}

					BranchResults.f_GetResults() > Continuation / [=](TCVector<TCAsyncResult<bool>> &&_Results)
						{
							bool bActionNeeded = false;

							if
								(
								 	!fg_CombineResults
								 	(
									 	Continuation
									 	, fg_Move(_Results)
									 	, [&](bool _bActionNeeded)
									 	{
											bActionNeeded = bActionNeeded || _bActionNeeded;
										}
									)
								)
							{
								return;
							}

							if (bActionNeeded && bOpenSourceTree)
							{
								Launches.f_OpenDocument("SourceTree", Repo.m_Location) > Continuation / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
									{
										Launches.f_RepoDone();
										Continuation.f_SetResult(bActionNeeded);
									}
								;
								return;
							}

							Launches.f_RepoDone();
							Continuation.f_SetResult(bActionNeeded);
						}
					;
				}
			;

			Continuation.f_Dispatch() > Results.f_AddResult();
		}

		bool bActionNeeded = false;
		for (auto &Result : Results.f_GetResults().f_CallSync())
			bActionNeeded = bActionNeeded || *Result;

		if (bActionNeeded)
			DConErrOut2("\a");
	}
}
