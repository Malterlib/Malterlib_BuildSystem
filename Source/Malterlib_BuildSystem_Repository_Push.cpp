// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
		struct CCanPushResult
		{
			TCSet<CStr> m_NewPush;
			TCSet<CStr> m_ForcePush;
		};

		TCFuture<TCOptional<CStr>> fg_GetRemoteCommitHash(CGitLaunches _Launches, CRepository _Repo, CStr _Remote, CStr _Branch)
		{
			using namespace NStr;

			auto Result = co_await _Launches.f_Launch(_Repo, {"ls-remote", _Remote, "refs/heads/{}"_f << _Branch}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			if (Result.m_ExitCode != 0)
				co_return {};

			CStr Output = Result.f_GetStdOut().f_Trim();
			if (Output.f_IsEmpty())
				co_return {};

			// Parse: "<hash>\trefs/heads/<branch>"
			CStr Hash;
			aint nParsed = 0;
			(CStr::CParse("{}\t") >> Hash).f_Parse(Output, nParsed);
			if (nParsed == 1 && !Hash.f_IsEmpty())
				co_return Hash;

			co_return {};
		}

		TCFuture<CStr> fg_GetShortHash(CGitLaunches _Launches, CRepository _Repo, CStr _FullHash)
		{
			auto Result = co_await _Launches.f_Launch(_Repo, {"rev-parse", "--short", _FullHash}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			if (Result.m_ExitCode != 0)
				co_return _FullHash;

			co_return Result.f_GetStdOut().f_Trim();
		}

		TCFuture<void> fg_Fetch(CBuildSystem *_pBuildSystem, CGitLaunches _Launches, CRepository _Repo)
		{
			TCVector<CStr> FetchParams = {"fetch", "--all", "--prune", "--tags", "-q"};

			if (_pBuildSystem->f_GetGenerateOptions().m_bForceUpdateRemotes)
				FetchParams.f_Insert("--force");

			auto Result = co_await _Launches.f_Launch
				(
					_Repo
					, FetchParams
					, fg_FetchEnvironment(*_pBuildSystem)
					, CProcessLaunchActor::ESimpleLaunchFlag_None
				)
			;

			if (Result.m_ExitCode)
			{
				_Launches.f_Output
					(
						EOutputType_Error
						, _Repo
						, "Error fetching remotes: {}\n"_f
						<< Result.f_GetCombinedOut().f_Trim()
					)
				;
				co_return DMibErrorInstance("Could not fetch remote");
			}

			co_return {};
		}

		TCFuture<CCanPushResult> DMibWorkaroundUBSanSectionErrors fg_CanPush(CGitLaunches _Launches, CRepository _Repo, TCVector<CStr> _Remotes, CGitBranches _Branches, bool _bForce)
		{
			using namespace NStr;

			CColors Colors(_Launches.m_pState->m_AnsiFlags);

			TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> CanPushResultsMap;

			for (auto &Remote : _Remotes)
			{
				_Launches.f_Launch
					(
						_Repo
						, {"merge-base", "--is-ancestor", "{}/{}"_f << Remote << _Branches.m_Current, _Branches.m_Current}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
					> CanPushResultsMap[Remote]
				;
			}

			auto CanPushResults = co_await fg_AllDone(CanPushResultsMap);

			CCanPushResult PushResult;
			bool bAllFastForward = true;
			for (auto &Result : CanPushResults)
			{
				auto &Remote = CanPushResults.fs_GetKey(Result);
				if (Result.m_ExitCode == 1)
				{
					PushResult.m_ForcePush[Remote];
					if (!_bForce)
					{
						_Launches.f_Output
							(
								EOutputType_Error
								, _Repo
								, "Cannot fast forward to {2}{}/{}{3}\n"_f
								<< Remote
								<< _Branches.m_Current
								<< Colors.f_RepositoryName()
								<< Colors.f_Default()
							)
						;
						bAllFastForward = false;
					}
				}
				else if (Result.m_ExitCode)
				{
					CStr Output = Result.f_GetCombinedOut().f_Trim();
					if (Output == CStr{"fatal: Not a valid object name {}/{}"_f << Remote << _Branches.m_Current})
						PushResult.m_NewPush[Remote];
					else
					{
						if (!_bForce)
						{
							_Launches.f_Output
								(
									EOutputType_Error
									, _Repo
									, "Error determining fast forward to {3}{}/{}{4}: {}\n"_f
									<< Remote
									<< _Branches.m_Current
									<< Output
									<< Colors.f_RepositoryName()
									<< Colors.f_Default()
								)
							;
							bAllFastForward = false;
						}
					}
				}
			}

			if (!bAllFastForward)
				co_return DMibErrorInstance("Could not fast-forward against all remotes, please resolve.");

			co_return fg_Move(PushResult);
		}

		TCFuture<TCSet<CStr>> DMibWorkaroundUBSanSectionErrors fg_NeedPush(CGitLaunches _Launches, CRepository _Repo, TCVector<CStr> _Remotes, CGitBranches _Branches)
		{
			TCFutureMap<CStr, TCVector<CLogEntry>> NeedPushResults;

			for (auto &Remote : _Remotes)
				fg_GetLogEntries(_Launches, _Repo, "{}/{}"_f << Remote << _Branches.m_Current, _Branches.m_Current, false) > NeedPushResults[Remote];

			auto ResultsUnwrapped = co_await fg_AllDone(NeedPushResults);

			TCSet<CStr> NeedPush;
			for (auto &Commits : ResultsUnwrapped)
			{
				auto &Remote = ResultsUnwrapped.fs_GetKey(Commits);
				if (!Commits.f_IsEmpty())
				{
					// Skip branches whose tip is already reachable from the remote default branch.
					// This intentionally excludes freshly created branches that haven't diverged yet:
					// the main branch is always synced to all sub-repositories, so only branches with
					// commits not yet visible via the default branch need to be pushed (i.e. commits
					// that another pull would need to resolve).
					CStr DefaultBranch = _Repo.m_OriginProperties.m_DefaultBranch;
					if (auto pRemote = _Repo.m_Remotes.f_FindEqual(Remote); pRemote && pRemote->m_Properties.m_DefaultBranch)
						DefaultBranch = pRemote->m_Properties.m_DefaultBranch;

					auto MergeBaseResult = co_await _Launches.f_Launch
						(
							_Repo
							, {"merge-base", "--is-ancestor", _Branches.m_Current, "{}/{}"_f << Remote << DefaultBranch}
							, {}
							, CProcessLaunchActor::ESimpleLaunchFlag_None
						)
					;
					if (MergeBaseResult.m_ExitCode != 0)
						NeedPush[Remote];
				}
			}

			co_return fg_Move(NeedPush);
		}
	}

	TCUnsafeFuture<CBuildSystem::ERetry> DMibWorkaroundUBSanSectionErrors CBuildSystem::f_Action_Repository_Push
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, TCVector<CStr> const &_Remotes
			, ERepoPushFlag _PushFlags
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		TCSharedPointer<CGenerateEphemeralState> pGenerateState = fg_Construct();

		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, *pGenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None);

		CGitLaunches Launches{f_GetGitLaunchOptions("push"), (_PushFlags & ERepoPushFlag_Pretend) ? "Pretending to push repos" : "Pushing repos"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCSet<CStr>> OutputOrder;

		umint PushOrderGroup = 0;
		for (auto &Repos : FilteredRepositories.m_FilteredRepositories.f_Reverse())
		{
			TCFutureVector<bool> Results;
			TCSet<CStr> OutputOrderSet;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;
				OutputOrderSet[Launches.f_GetRepoName(Repo)];

				g_Dispatch / [this, Launches, Repo, _Remotes, _PushFlags, Colors]() -> TCFuture<bool>
					{
						auto OnExit = g_OnScopeExit / [&]
							{
								Launches.f_RepoDone();
							}
						;

						auto [PushRemotes, Branches] = co_await (fg_GetPushRemotes(Launches, Repo, _Remotes) + fg_GetBranches(Launches, Repo, false));

						TCVector<CStr> Remotes;
						{
							TCSet<CStr> AddedUrls;
							for (auto &Remote : PushRemotes)
							{
								if (auto pRemote = Repo.m_Remotes.f_FindEqual(Remote.f_Name()); pRemote && !pRemote->m_bCanPush)
									continue;

								if (!(_PushFlags & ERepoPushFlag_NonDefaultToAll) && Branches.m_Current != Repo.m_OriginProperties.m_DefaultBranch && Remote.f_Name() != "origin")
									continue;

								if (!AddedUrls(Remote.m_Properties.m_URL).f_WasCreated())
									continue;

								Remotes.f_Insert(Remote.f_Name());
							}
						}

						if (Branches.m_Current.f_IsEmpty())
							co_return false;

						TCSet<CStr> NeedRemotes = co_await fg_NeedPush(Launches, Repo, Remotes, Branches);

						if (NeedRemotes.f_IsEmpty())
							co_return false;

						if (!(_PushFlags & ERepoPushFlag_Force))
							co_await fg_Fetch(this, Launches, Repo);

						auto CanPushResult = co_await fg_CanPush(Launches, Repo, Remotes, Branches, _PushFlags & ERepoPushFlag_Force);

						struct CTagInfo
						{
							CStr m_RemoteHash;
							CStr m_TagName;
						};

						TCMap<CStr, CTagInfo> TagInfos;

						if (_PushFlags & ERepoPushFlag_Force)
						{
							TCFutureMap<CStr, TCOptional<CStr>> RemoteHashFutures;
							for (auto &Remote : Remotes)
							{
								CRemoteProperties const *pRemoteProps = nullptr;
								if (auto pRemote = PushRemotes.f_FindEqual(Remote))
									pRemoteProps = &pRemote->m_Properties;
								if (!pRemoteProps && Remote == "origin")
									pRemoteProps = &Repo.m_OriginProperties;

								if (!pRemoteProps || !pRemoteProps->m_bTagPreviousOnForcePush)
									continue;

								CStr const &DefaultBranch = !pRemoteProps->m_DefaultBranch.f_IsEmpty() ? pRemoteProps->m_DefaultBranch : Repo.m_OriginProperties.m_DefaultBranch;
								if (Branches.m_Current != DefaultBranch)
									continue;

								if (CanPushResult.m_ForcePush.f_FindEqual(Remote))
									fg_GetRemoteCommitHash(Launches, Repo, Remote, Branches.m_Current) > RemoteHashFutures[Remote];
							}
							auto RemoteHashes = co_await fg_AllDone(RemoteHashFutures);

							TCFutureMap<CStr, CStr> ShortHashFutures;
							for (auto &RemoteHash : RemoteHashes)
							{
								if (RemoteHash)
									fg_GetShortHash(Launches, Repo, *RemoteHash) > ShortHashFutures[RemoteHashes.fs_GetKey(RemoteHash)];
							}
							auto ShortHashes = co_await fg_AllDone(ShortHashFutures);

							for (auto &RemoteHashEntry : RemoteHashes.f_Entries())
							{
								auto &Remote = RemoteHashEntry.f_Key();
								if (RemoteHashEntry.f_Value())
								{
									auto &TagInfo = TagInfos[Remote];
									TagInfo.m_RemoteHash = *RemoteHashEntry.f_Value();
									TagInfo.m_TagName = "{}_{}"_f << Branches.m_Current << ShortHashes[Remote];
								}
							}
						}

						TCFutureVector<void> PushResults;

						for (auto &Remote : Remotes)
						{
							CTagInfo const *pTagInfo = TagInfos.f_FindEqual(Remote);

							TCVector<CStr> Params;

							Params = {"push", Remote, Branches.m_Current};

							if (_PushFlags & ERepoPushFlag_FollowTags)
								Params.f_InsertAfter(0, "--follow-tags");

							bool bDoForcePush = (_PushFlags & ERepoPushFlag_Force) && CanPushResult.m_ForcePush.f_FindEqual(Remote);

							if (bDoForcePush)
								Params.f_InsertAfter(0, "--force-with-lease");

							if (Remote == "origin")
								Params.f_InsertAfter(0, "-u");

							if (_PushFlags & ERepoPushFlag_Pretend)
							{
								if (pTagInfo)
								{
									Launches.f_Output
										(
											EOutputType_Normal
											, Repo
											, "Tag previous {3}{}{4} commit as {3}{}{4}: git {}"_f
											<< Branches.m_Current
											<< pTagInfo->m_TagName
											<< CProcessLaunchParams::fs_GetParams({"tag", pTagInfo->m_TagName, pTagInfo->m_RemoteHash})
											<< Colors.f_RepositoryName()
											<< Colors.f_Default()
										)
									;
									Launches.f_Output
										(
											EOutputType_Normal
											, Repo
											, "Push tag {3}{}{4} on {3}{}{4}: git {}"_f
											<< pTagInfo->m_TagName
											<< Remote
											<< CProcessLaunchParams::fs_GetParams({"push", Remote, "refs/tags/{}"_f << pTagInfo->m_TagName})
											<< Colors.f_RepositoryName()
											<< Colors.f_Default()
										)
									;
								}

								if (CanPushResult.m_NewPush.f_FindEqual(Remote))
								{
									Launches.f_Output
										(
											EOutputType_Normal
											, Repo
											, "New branch {3}{}{4} on {3}{}{4}: git {}"_f
											<< Branches.m_Current
											<< Remote
											<< CProcessLaunchParams::fs_GetParams(Params)
											<< Colors.f_RepositoryName()
											<< Colors.f_Default()
										)
									;
								}
								else if (NeedRemotes.f_FindEqual(Remote))
								{
									Launches.f_Output
										(
											EOutputType_Normal
											, Repo
											, "{} {4}{}{5} on {4}{}{5}: git {}"_f
											<< (bDoForcePush ? "Force update" : "Update")
											<< Branches.m_Current
											<< Remote
											<< CProcessLaunchParams::fs_GetParams(Params)
											<< Colors.f_RepositoryName()
											<< Colors.f_Default()
										)
									;
								}
							}
							else if (NeedRemotes.f_FindEqual(Remote))
							{
								if (pTagInfo)
								{
 									g_Dispatch / [Launches, Repo, TagInfo = *pTagInfo, Colors, Remote, CurrentBranch = Branches.m_Current, Params]() -> TCFuture<void>
										{
											auto TagResult = co_await Launches.f_Launch
												(
													Repo
													, {"tag", TagInfo.m_TagName, TagInfo.m_RemoteHash}
													, {}
													, CProcessLaunchActor::ESimpleLaunchFlag_None
												)
											;

											if (TagResult.m_ExitCode == 0)
											{
												Launches.f_Output
													(
														EOutputType_Normal
														, Repo
														, "Created previous {3}{}{4} commit as tag {3}{}{4}:\n{}\n"_f
														<< CurrentBranch
														<< TagInfo.m_TagName
														<< TagResult.f_GetCombinedOut().f_Trim()
														<< Colors.f_RepositoryName()
														<< Colors.f_Default()
													)
												;

												auto TagPushResult = co_await Launches.f_Launch
													(
														Repo
														, {"push", Remote, "refs/tags/{}"_f << TagInfo.m_TagName}
														, {}
														, CProcessLaunchActor::ESimpleLaunchFlag_None
													)
												;

												if (TagPushResult.m_ExitCode == 0)
												{
													Launches.f_Output
														(
															EOutputType_Normal
															, Repo
															, "Pushed previous {4}{}{5} commit as {4}{}{5} on remote {4}{}{5}:\n{}\n"_f
															<< CurrentBranch
															<< TagInfo.m_TagName
															<< Remote
															<< TagPushResult.f_GetCombinedOut().f_Trim()
															<< Colors.f_RepositoryName()
															<< Colors.f_Default()
														)
													;
												}
												else
												{
													Launches.f_Output
														(
															EOutputType_Error
															, Repo
															, "Failed to push tag {}: {}"_f
															<< TagInfo.m_TagName
															<< TagPushResult.f_GetCombinedOut().f_Trim()
														)
													;

													co_return DMibErrorInstance("Error status");
												}
											}
											else if (TagResult.f_GetCombinedOut().f_Find("already exists") < 0)
											{
												Launches.f_Output
													(
														EOutputType_Error
														, Repo
														, "Failed to create tag {}: {}"_f
														<< TagInfo.m_TagName
														<< TagResult.f_GetCombinedOut().f_Trim()
													)
												;

												co_return DMibErrorInstance("Error status");
											}

											co_await Launches.f_Launch(Repo, Params, fg_LogAllFunctor());

											co_return {};
										}
										> PushResults
									;
								}
								else
									Launches.f_Launch(Repo, Params, fg_LogAllFunctor()) > PushResults;
							}
						}

						co_await fg_AllDone(PushResults);

						co_return true;
					}
					> Results
				;
			}

			bool bDidPush = false;
			for (auto ResultsUnwrapped = co_await fg_AllDone(Results); auto &bResult : ResultsUnwrapped)
				bDidPush = bDidPush || bResult;

			if (bDidPush)
			{
				CStr Heading = "Push group {}"_f << PushOrderGroup;
				OutputOrder.f_Insert(TCSet<CStr>{Heading});
				Launches.f_Output(EOutputType_Warning, Heading, "ForceSection");

				OutputOrder.f_Insert(OutputOrderSet);
				++PushOrderGroup;
			}
		}

		Launches.f_SetOutputOrder(OutputOrder);

		co_return ERetry_None;
	}
}
