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

	TCFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_Status_Async(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoStatusFlag _Flags)
	{
		TCSharedPointer<CGenerateEphemeralState> pGenerateState = fg_Construct();
		try
		{
			if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, *pGenerateState, nullptr); Retry != ERetry_None)
				co_return Retry;

		}
		catch (CException const &_Exception)
		{
			co_return _Exception.f_ExceptionPointer();
		}

		CRepoFilter Filter = _Filter;

		EFilterRepoFlag FilterFlags = EFilterRepoFlag_IncludePull;

		if (_Flags & CBuildSystem::ERepoStatusFlag_OnlyTracked)
			FilterFlags |= EFilterRepoFlag_OnlyTracked;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredReposAsync(Filter, *this, mp_Data, FilterFlags);

		if (_Flags & ERepoStatusFlag_UpdateRemotes)
			co_await fg_UpdateRemotesAsync(*this, FilteredRepositories);

		CGitLaunches Launches{mp_BaseDir, "Getting repo status", mp_AnsiFlags, mp_fOutputConsole};

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		bool bOpenEditor = _Flags & ERepoStatusFlag_OpenEditor;
		bool bUseDefaultUpstream = _Flags & ERepoStatusFlag_UseDefaultUpstreamBranch;

		TCActorResultVector<TCTuple<bool, mint, CRepository>> RepoResults;

		auto AllRepos = FilteredRepositories.f_GetAllRepos();

		for (auto &[Repo, iSequence] : AllRepos)
		{
			g_Dispatch / [=, Repo = Repo, iSequence = iSequence]() -> TCFuture<TCTuple<bool, mint, CRepository>>
				{
					auto [LocalChanges, Remotes, LocalBranches, RemoteBranches] = co_await 
						(
							fg_GetLocalFileChanges(Launches, Repo, !(_Flags & ERepoStatusFlag_OnlyTracked))
							+ fg_GetRemotes(Launches, Repo)
							+ fg_GetBranches(Launches, Repo, false)
							+ fg_GetBranches(Launches, Repo, true)
						)
					;

					TCSharedPointer<CRepoStatusState> pState = fg_Construct();

					auto &State = *pState;

					State.m_LocalChanges = fg_Move(LocalChanges);
					State.m_Remotes = fg_Move(Remotes);
					State.m_LocalBranches = fg_Move(LocalBranches);
					State.m_RemoteBranches = fg_Move(RemoteBranches);

					TCSet<CStr> Branches;

					if (_Flags & ERepoStatusFlag_AllBranches)
						Branches = State.m_LocalBranches.m_Branches;
					else if (!State.m_LocalBranches.m_Current.f_IsEmpty())
						Branches = {State.m_LocalBranches.m_Current};

					TCActorResultVector<bool> BranchResults;
					for (auto &Branch : Branches)
					{
						g_Dispatch / [=]() -> TCFuture<bool>
							{
								TCActorResultMap<CStr, TCVector<CLogEntry>> ToPush;
								TCActorResultMap<CStr, TCVector<CLogEntry>> ToPull;
								TCSet<CStr> ToPushMissing;

								for (auto &Remote : State.m_Remotes)
								{
									CStr RemoteBranch = "{}/{}"_f << Remote << Branch;
									CStr PullRemoteBranch = RemoteBranch;
									CStr MissingRemoteBranch = RemoteBranch;

									CStr RemotePullBranchName;

									if (bUseDefaultUpstream && Branch == Repo.m_DefaultBranch && !Repo.m_DefaultUpstreamBranch.f_IsEmpty() && !State.f_HasRemoteBranch(RemoteBranch))
										PullRemoteBranch = "{}/{}"_f << Remote << Repo.m_DefaultUpstreamBranch;

									if (Branch == Repo.m_DefaultBranch && !Repo.m_DefaultUpstreamBranch.f_IsEmpty() && !State.f_HasRemoteBranch(RemoteBranch))
										MissingRemoteBranch = "{}/{}"_f << Remote << Repo.m_DefaultUpstreamBranch;

									if (!(_Flags & ERepoStatusFlag_NonDefaultToAll) && Remote != "origin" && Branch != Repo.m_DefaultBranch)
										;
									else
									{
										if (State.f_HasRemoteBranch(RemoteBranch))
											fg_GetLogEntries(Launches, Repo, RemoteBranch, Branch) > ToPush.f_AddResult(Remote);
										else if ((Remote == "origin" && Branch == Repo.m_DefaultBranch) || !State.f_HasRemoteBranch(MissingRemoteBranch))
											ToPushMissing[Remote];

										if (State.f_HasRemoteBranch(PullRemoteBranch))
											fg_GetLogEntries(Launches, Repo, Branch, PullRemoteBranch) > ToPull.f_AddResult(Remote);
									}

									if (Branch != Repo.m_DefaultBranch && !Repo.m_DefaultBranch.f_IsEmpty())
									{
										CStr AgainstDefaultName = "{}/{}"_f << Remote << Repo.m_DefaultBranch;
										CStr ExtraRemoteBranch = "{}/{}"_f << Remote << Repo.m_DefaultBranch;
										if (State.f_HasRemoteBranch(ExtraRemoteBranch))
										{
											fg_GetLogEntries(Launches, Repo, ExtraRemoteBranch, Branch) > ToPush.f_AddResult(AgainstDefaultName);
											fg_GetLogEntries(Launches, Repo, Branch, ExtraRemoteBranch) > ToPull.f_AddResult(AgainstDefaultName);
										}
									}
								}

								auto [_ToPush, _ToPull] = co_await 
									(
										ToPush.f_GetResults()
										+ ToPull.f_GetResults()
									)
								;

								TCMap<CStr, TCVector<CLogEntry>> ToRemotePush;
								TCMap<CStr, TCVector<CLogEntry>> ToLocalPull;

								TCMap<TCVector<CStr>, TCTuple<TCVector<CStr>, TCVector<CLogEntry>>> ToRemotePushByHashes;
								TCMap<TCVector<CStr>, TCTuple<TCVector<CStr>, TCVector<CLogEntry>>> ToLocalPullByHashes;

								auto fGetHashes = [](TCVector<CLogEntry> const &_LogEntries)
									{
										TCVector<CStr> Hashes;
										for (auto &Entry : _LogEntries)
											Hashes.f_Insert(Entry.m_Hash);

										return Hashes;
									}
								;

								TCPromise<void> CombineResultsPromise;
								if
									(
										!fg_CombineResults
										(
											CombineResultsPromise
											, fg_Move(_ToPush)
											, [&](CStr const &_Remote, TCVector<CLogEntry> &&_Log)
											{
												auto &[Remotes, LogEntries] = ToRemotePushByHashes[fGetHashes(_Log)];
												Remotes.f_Insert(_Remote);
												LogEntries = fg_Move(_Log);
											}
										)
									)
								{
									auto Result = CombineResultsPromise.f_MoveResult();
									CStr Error = Result.f_GetExceptionStr();
									Launches.f_Output(EOutputType_Error, Repo, "Error getting to push for branch '{}': {}"_f << Branch << Error);
									co_return Result.f_GetException();
								}

								if
									(
										!fg_CombineResults
										(
											CombineResultsPromise
											, fg_Move(_ToPull)
											, [&](CStr const &_Remote, TCVector<CLogEntry> &&_Log)
											{
												auto &[Remotes, LogEntries] = ToLocalPullByHashes[fGetHashes(_Log)];
												Remotes.f_Insert(_Remote);
												LogEntries = fg_Move(_Log);
											}
										)
									)
								{
									auto Result = CombineResultsPromise.f_MoveResult();
									CStr Error = Result.f_GetExceptionStr();
									Launches.f_Output(EOutputType_Error, Repo, "Error getting to pull for branch '{}': {}"_f << Branch << Error);
									co_return Result.f_GetException();
								}

								auto fJoinNames = [](TCSet<CStr> const &_Names) -> CStr
									{
										if (_Names.f_GetLen() == 1)
											return *_Names.f_FindSmallest();
										return "<{}>"_f << CStr::fs_Join(_Names, ", ");
									}
								;
								auto fPriority = [](mint _Priority) -> CStr
									{
										return "{sj*}"_f << "" << _Priority;
									}
								;

								auto fHandleRemotes = [&](auto &o_Entries, auto &_SourceEntries, bool _bOtherBranches)
									{
										for (auto &[Remotes, LogEntries] : _SourceEntries)
										{
											TCSet<CStr> RemotesForName;
											TCSet<CStr> Branches;
											for (auto &RemoteBranch : Remotes)
											{
												if (_bOtherBranches != RemoteBranch.f_FindChar('/') >= 0)
													continue;

												if (_bOtherBranches)
												{
													CStr Branch = RemoteBranch;
													CStr Remote = fg_GetStrSep(Branch, "/");
													RemotesForName[Remote];
													Branches[Branch];
												}
												else
													RemotesForName[RemoteBranch];
											}

											if (Branches.f_IsEmpty() && RemotesForName.f_IsEmpty())
												continue;

											CStr RemoteName = fJoinNames(RemotesForName);

											if (!Branches.f_IsEmpty())
											{
												RemoteName += "/";
												RemoteName += fJoinNames(Branches);
											}

											if (!_bOtherBranches)
												RemoteName = fPriority(12) + RemoteName;
											else
												RemoteName = fPriority(2) + RemoteName;

											o_Entries[RemoteName] = LogEntries;
										}
									}
								;

								fHandleRemotes(ToRemotePush, ToRemotePushByHashes, false);
								fHandleRemotes(ToRemotePush, ToRemotePushByHashes, true);
								fHandleRemotes(ToLocalPull, ToLocalPullByHashes, false);
								fHandleRemotes(ToLocalPull, ToLocalPullByHashes, true);

								bool bHasRemotes = !ToRemotePush.f_IsEmpty() || !ToLocalPull.f_IsEmpty();
								bool bIsChanged = !ToPushMissing.f_IsEmpty();
								bool bNeedAction = false;

								if ((_Flags & ERepoStatusFlag_NeedActionOnPush) && !ToPushMissing.f_IsEmpty())
									bNeedAction = true;

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
									bNeedAction = true;

									Messages.f_Insert
										(
											"{}Local{}[{}Commit {}{}]"_f
											<< Colors.f_RepositoryName()
											<< Colors.f_Default()
											<< Colors.f_ToCommit()
											<< Colors.f_Default()
											<< State.m_LocalChanges.f_GetLen()
										)
									;
								}

								if (!ToPushMissing.f_IsEmpty())
								{
									CStr NewMissing = fPriority(11) + fJoinNames(ToPushMissing);
									ToPushMissing = {NewMissing};
								}

								TCSet<CStr> RemotesWithAction = ToPushMissing;

								TCSet<CStr> NoPull;
								TCSet<CStr> NoPush;

								for (auto &ToPush : ToRemotePush)
								{
									if (!ToPush.f_IsEmpty())
									{
										bIsChanged = true;
										if (_Flags & ERepoStatusFlag_NeedActionOnPush)
											bNeedAction = true;
										RemotesWithAction[ToRemotePush.fs_GetKey(ToPush)];
									}
									else
										NoPush[ToRemotePush.fs_GetKey(ToPush)];
								}

								for (auto &ToPull : ToLocalPull)
								{
									if (!ToPull.f_IsEmpty())
									{
										bIsChanged = true;
										bNeedAction = true;
										RemotesWithAction[ToLocalPull.fs_GetKey(ToPull)];
									}
									else
										NoPull[ToRemotePush.fs_GetKey(ToPull)];
								}

								TCSet<CStr> NoPullNoPush;
								for (auto &Remote : NoPull)
								{
									if (NoPush.f_FindEqual(Remote))
									{
										if (Remote.f_FindChar('/') >= 0)
											NoPullNoPush[fPriority(3) + Remote];
										else
											NoPullNoPush[fPriority(13) + Remote];
									}
								}

								RemotesWithAction += NoPullNoPush;

								bool bWasOther = false;
								for (auto &RemoteName : RemotesWithAction)
								{
									TCVector<CStr> RemoteMessages;
									auto pToPush = ToRemotePush.f_FindEqual(RemoteName);
									auto pToPull = ToLocalPull.f_FindEqual(RemoteName);
									bool bMissing = ToPushMissing.f_FindEqual(RemoteName);
									bool bNoPullNoPush = NoPullNoPush.f_FindEqual(RemoteName);

									if (pToPush && !pToPush->f_IsEmpty())
									{
										RemoteMessages.f_Insert
											(
												"{}Push {}{}"_f
												<< Colors.f_ToPush()
												<< Colors.f_Default()
												<< pToPush->f_GetLen()
											)
										;
									}

									if (bMissing)
									{
										RemoteMessages.f_Insert
											(
												"{}Push {}?"_f
												<< Colors.f_ToPush()
												<< Colors.f_Default()
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
													<< Colors.f_ToPull()
													<< Colors.f_Foreground256(242)
													<< Repo.m_DefaultUpstreamBranch
													<< Colors.f_Default()
													<< pToPull->f_GetLen()
												)
											;
										}
										else
										{
											RemoteMessages.f_Insert
												(
													"{}Pull {}{}"_f
													<< Colors.f_ToPull()
													<< Colors.f_Default()
													<< pToPull->f_GetLen()
												)
											;
										}
									}

									if (RemoteName.f_FindChar('/') >= 0 && !bWasOther)
									{
										bWasOther = true;
										Messages.f_Insert("   (");
									}

									if (bNoPullNoPush)
									{
										Messages.f_Insert
											(
												"{}{}{}{}"_f
												<< Colors.f_Default()
												<< Colors.f_Foreground256(248)
												<< RemoteName.f_Trim()
												<< Colors.f_Default()
											)
										;
									}
									else
									{
										Messages.f_Insert
											(
												"{}{}{}[{}]"_f
												<< Colors.f_RepositoryName()
												<< RemoteName.f_Trim()
												<< Colors.f_Default()
												<< CStr::fs_Join(RemoteMessages, " ")
											)
										;
									}
								}

								if (bWasOther)
									Messages.f_Insert(")");

								if (!bIsChanged && !(_Flags & ERepoStatusFlag_ShowUnchanged))
									co_return bIsChanged;

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
										<< Colors.f_BranchName()
										<< (bIsCurrentBranch ? "*" : " ")
										<< Branch
										<< Colors.f_Default()
										<< CStr::fs_Join(Messages, " ")
									)
								;

								if (!(_Flags & ERepoStatusFlag_Verbose))
									co_return bNeedAction;

								if (bIsCurrentBranch && !State.m_LocalChanges.f_IsEmpty())
								{
									Launches.f_Output
										(
											OutputType
											, Repo
											, "   {}Local {}To Commit{}"_f
											<< Colors.f_RepositoryName()
											<< Colors.f_ToCommit()
											<< Colors.f_Default()
										)
									;

									for (auto &Change : State.m_LocalChanges)
									{
										Launches.f_Output
											(
												OutputType
												, Repo
												, "      {}{} {}{}"_f
												<< Colors.f_ToCommit()
												<< Change.m_ChangeType
												<< Colors.f_Default()
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
											<< Colors.f_RepositoryName()
											<< RemoteName.f_Trim()
											<< Colors.f_ToPush()
											<< Colors.f_Default()
										)
									;

									for (auto &Commit : ToPush)
									{
										Launches.f_Output
											(
												OutputType
												, Repo
												, "      {}{} {}{}"_f
												<< Colors.f_ToPush()
												<< Commit.m_Hash
												<< Colors.f_Default()
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
											<< Colors.f_RepositoryName()
											<< RemoteName.f_Trim()
											<< Colors.f_ToPush()
											<< Colors.f_Default()
										)
									;
									Launches.f_Output
										(
											OutputType
											, Repo
											, "      {}??????? {}Branch missing on remote"_f
											<< Colors.f_ToPush()
											<< Colors.f_Default()
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
												<< Colors.f_RepositoryName()
												<< RemoteName.f_Trim()
												<< Colors.f_ToPull()
												<< Colors.f_Default()
												<< Colors.f_Foreground256(242)
												<< Repo.m_DefaultUpstreamBranch
												<< Colors.f_Default()
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
												<< Colors.f_RepositoryName()
												<< RemoteName.f_Trim()
												<< Colors.f_ToPull()
												<< Colors.f_Default()
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
												<< Colors.f_ToPull()
												<< Commit.m_Hash
												<< Colors.f_Default()
												<< Commit.m_Description
											)
										;
									}
								}

								co_return bNeedAction;
							}
							> BranchResults.f_AddResult()
						;
					}

					auto Results = co_await BranchResults.f_GetResults() | g_Unwrap;
					bool bActionNeeded = false;
					for (auto &bActionNeededResult : Results)
						bActionNeeded = bActionNeeded || bActionNeededResult;
			
					if (bActionNeeded && bOpenEditor)
					{
						Launches.f_RepoDone();
						co_return TCTuple<bool, mint, CRepository>{bActionNeeded, iSequence, Repo};
					}

					Launches.f_RepoDone();
					co_return TCTuple<bool, mint, CRepository>{bActionNeeded, iSequence, {""}};
				}
				> RepoResults.f_AddResult()
			;
		}

		auto Results = co_await RepoResults.f_GetResults() | g_Unwrap;

		TCMap<mint, TCVector<CRepository>> EditorsToLaunch;
		bool bActionNeeded = false;
		for (auto &Result : Results)
		{
			auto &[bThisActionNeeded, iSequence, Repo] = Result;

			if (!Repo.m_Location.f_IsEmpty())
				EditorsToLaunch[iSequence].f_Insert(Repo);

			bActionNeeded = bActionNeeded || bThisActionNeeded;
		}

		if (!EditorsToLaunch.f_IsEmpty())
		{
			CRepoEditor RepoEditor = fg_GetRepoEditor(*this, mp_Data);

			TCActorSequencer<void> EditorLaunchSequencer(RepoEditor.m_bOpenSequential ? 1 : 16);

			for (auto &EditorLaunches : EditorsToLaunch)
			{
				TCActorResultVector<void> EditorLaunchResults;
				for (auto &Repo : EditorLaunches)
				{
					EditorLaunchSequencer / [=]() -> TCFuture<void>
						{
							auto Result = co_await Launches.f_OpenRepoEditor(RepoEditor, Repo.m_Location);
							if (Result.m_ExitCode)
							{
								Launches.f_Output
									(
										EOutputType_Error
										, Repo
										, "Failed to launch repository editor: {}"_f
										<< Result.f_GetCombinedOut()
									)
								;
							}

							co_return {};
						}
						> EditorLaunchResults.f_AddResult();
					;

				}
				co_await EditorLaunchResults.f_GetResults() | g_Unwrap;
			}
		}

		if (bActionNeeded)
			f_OutputConsole("\a", true);

		co_return ERetry_None;
	}

	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_Status(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoStatusFlag _Flags)
	{
		TCSharedPointer<CDefaultRunLoop> pRunLoop = fg_Construct();
		auto CleanupRunLoop = g_OnScopeExit > [&]
			{
				while (pRunLoop->f_RefCountGet() > 0)
					pRunLoop->f_WaitOnceTimeout(0.1);
			}
		;
		TCActor<CDispatchingActor> HelperActor(fg_Construct(), pRunLoop->f_Dispatcher());
		auto CleanupHelperActor = g_OnScopeExit > [&]
			{
				HelperActor->f_BlockDestroy(pRunLoop->f_ActorDestroyLoop());
			}
		;
		CCurrentlyProcessingActorScope CurrentActor{HelperActor};

		return fg_CallSafe(this, &CBuildSystem::f_Action_Repository_Status_Async, _GenerateOptions, _Filter, _Flags).f_CallSync(pRunLoop);
	}
}
