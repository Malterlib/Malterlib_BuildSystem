// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Concurrency/ActorSequencerActor>
#include <Mib/Concurrency/LogError>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::fp_BranchRootRepo(CGenerateEphemeralState &_GenerateState, TCOptional<CStr> const &_Branch, ERepoBranchFlag _Flags)
	{
		bool bIsGitRoot = CFile::fs_FileExists(f_GetBaseDir() + "/.git", EFileAttrib_Directory | EFileAttrib_File);

		auto ReposOrdered = fg_GetRepos(*this, mp_Data, EGetRepoFlag::mc_None);
		auto MainBranchInfo = fg_GetMainRepoBranchInfo(f_GetBaseDir(), ReposOrdered);

		CStr Branch = _Branch ? *_Branch : MainBranchInfo.m_DefaultBranch;
		if (Branch.f_IsEmpty() || MainBranchInfo.m_Branch == Branch)
			co_return ERetry_None;

		if (!bIsGitRoot)
		{
			// Non-git root: update .malterlib-branch file
			if (_Flags & ERepoBranchFlag_Pretend)
				f_OutputConsole("Pretend mode: nested repository operations are not previewed\n"_f);
			else
			{
				CStr BranchFile = f_GetBaseDir() + "/.malterlib-branch";
				CStateHandler StateHandler{f_GetBaseDir(), mp_OutputDir, mp_AnsiFlags, mp_fOutputConsole};
				StateHandler.f_AddGitIgnore(BranchFile, *this, EGitIgnoreType::mc_GitIgnore);
				// "master" is the sentinel for the unbranched/default state in non-git
				// roots (see fg_GetMainRepoBranchInfo, which hard-codes the main-repo
				// default branch to "master" when there is no git metadata to consult).
				// Deleting the file returns to that default state; individual repos map
				// it to their own configured default branch via fg_GetExpectedBranch().
				//
				// This is deliberately the same behavior as a real git root whose default
				// branch is "master": in that setup `mib branch master` also checks out the
				// main repo's default branch and then fg_GetExpectedBranch() resolves each
				// nested repo to its own Repository.DefaultBranch (which may be "main",
				// "master", or anything else). Persisting the literal string "master" here
				// instead would diverge from the git-root case and force every nested repo
				// onto a literal "master" branch that may not even exist. The non-git root
				// cannot distinguish "I want the workspace default" from "I want the literal
				// branch named master" because it has no authoritative default-branch
				// metadata — by convention the default is always the sentinel, matching the
				// git-root-with-master-default behavior.
				if (Branch == "master")
					CFile::fs_DeleteFile(BranchFile);
				else
					CFile::fs_WriteStringToFile(BranchFile, Branch, false);

				// The branch file changed, so the build system data (mp_Data) parsed during
				// fp_GeneratePrepare is now stale. Retry so the outer loop creates a fresh
				// CBuildSystem that re-parses from the new branch's configuration files.
				// Use ForceUpdate so fp_HandleRepositories runs even with --skip-update,
				// otherwise nested repos would not be reconciled to the new branch.
				co_return ERetry_Again_ForceUpdate;
			}

			co_return ERetry_None;
		}

		CGitLaunches Launches{f_GetGitLaunchOptions("branch-root"), "Switching root repo branch"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		auto BranchExistsResult = co_await Launches.f_Launch(f_GetBaseDir(), {"rev-parse", "--verify", "refs/heads/{}"_f << Branch}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
		auto RemoteBranchExistsResult = co_await Launches.f_Launch
			(
				f_GetBaseDir()
				, {"rev-parse", "--verify", "refs/remotes/origin/{}"_f << Branch}
				, {}
				, CProcessLaunchActor::ESimpleLaunchFlag_None
			)
		;

		TCVector<CStr> ParamsCheckout;
		// Intentionally use plain checkout for an existing branch. If the root repo itself is a
		// secondary worktree and the target branch is already checked out in another worktree,
		// Git must reject the operation. That is the correct behavior because mib should not
		// silently detach HEAD or create a duplicate branch state just to bypass Git's worktree rules.
		if (BranchExistsResult.m_ExitCode == 0)
			ParamsCheckout = {"checkout", Branch};
		else if (RemoteBranchExistsResult.m_ExitCode == 0)
		{
			// In the git-only fallback we may only have a cached local origin/<branch>. Use it as
			// the most specific branch source available and keep origin/<branch> as upstream for
			// normal git push / pull ergonomics, even though the cached ref may be stale.
			ParamsCheckout = {"checkout", "--track", "-b", Branch, "origin/{}"_f << Branch};
		}
		else
			ParamsCheckout = {"checkout", "-b", Branch};

		// The stash name embeds a per-invocation random suffix so the failure-recovery
		// cleanup can pop *exactly* the stash this invocation created and never touch
		// an older stash with the same branch name. The cross-invocation pop logic
		// further down matches on the branch portion only and accepts either the old
		// (no suffix) or new (with suffix) format.
		auto fGetStashNameForBranch = [](CStr const &_BranchName) -> CStr
			{
				return "mib-branch-switch:.:{}"_f << _BranchName;
			}
		;

		CStr UniqueStashName = "{}:{}"_f << fGetStashNameForBranch(MainBranchInfo.m_Branch) << fg_FastRandomID();

		auto fOutputRepoInfo = [&](CStr const &_Info)
			{
				fg_OutputRepositoryInfo
					(
						EOutputType_Normal
						, _Info
						, mp_AnsiFlags
						, gc_Str<".">.m_Str
						, mp_MaxRepoWidth
						, [&](CStr const &_Line) { f_OutputConsole(_Line); }
					)
				;
			}
		;

		bool bStashedLocalChanges = false;

		// If checkout never succeeds (e.g. Git refuses because the target branch is checked
		// out in another worktree), restore the just-created stash so the user's local
		// changes are not silently orphaned in the stash list.
		auto RestoreStashOnFailure = co_await fg_AsyncDestroy
			(
				[&]() -> TCFuture<void>
				{
					if (!bStashedLocalChanges)
						co_return {};

					// Snapshot everything into the coroutine frame before the first co_await.
					// After the first suspension, all captured references are unsafe.
					CGitLaunches LocalLaunches       = Launches;
					CStr LocalBaseDir                = f_GetBaseDir();
					CStr LocalUniqueStashName        = UniqueStashName;
					EAnsiEncodingFlag LocalAnsiFlags = mp_AnsiFlags;
					umint LocalMaxRepoWidth          = mp_MaxRepoWidth;
					auto LocalOutputConsole          = mp_fOutputConsole;

					auto fLocalOutputInfo = [&](EOutputType _OutputType, CStr const &_Info)
						{
							fg_OutputRepositoryInfo
								(
									_OutputType
									, _Info
									, LocalAnsiFlags
									, gc_Str<".">.m_Str
									, LocalMaxRepoWidth
									, [&](CStr const &_Line) { LocalOutputConsole(_Line, false); }
								)
							;
						}
					;

					co_await ECoroutineFlag_CaptureExceptions;

					auto StashListResult = co_await LocalLaunches.f_Launch(LocalBaseDir, {"stash", "list"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
					if (StashListResult.m_ExitCode != 0)
					{
						fLocalOutputInfo(EOutputType_Warning, "Failed to list stashes while restoring: {}"_f << StashListResult.f_GetCombinedOut());
						co_return {};
					}

					for (auto &Line : StashListResult.f_GetStdOut().f_SplitLine<true>())
					{
						if (!Line.f_EndsWith(LocalUniqueStashName))
							continue;

						CStr StashRef;
						(CStr::CParse("{}:") >> StashRef).f_Parse(Line);
						if (StashRef.f_IsEmpty())
							break;

						fLocalOutputInfo(EOutputType_Normal, "Checkout failed; restoring stashed changes");

						auto PopResult = co_await LocalLaunches.f_Launch(LocalBaseDir, {"stash", "pop", StashRef}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
						if (PopResult.m_ExitCode != 0)
							fLocalOutputInfo(EOutputType_Warning, "Stash pop conflict while restoring: {}"_f << PopResult.f_GetCombinedOut());
						break;
					}
					co_return {};
				}
			)
		;

		if (mp_GenerateOptions.m_bStash)
		{
			auto StatusResult = co_await Launches.f_Launch(f_GetBaseDir(), {"status", "--porcelain"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);
			if (!StatusResult.f_GetStdOut().f_Trim().f_IsEmpty())
			{
				if (_Flags & ERepoBranchFlag_Pretend)
					Launches.f_Output(EOutputType_Normal, f_GetBaseDir(), "git stash push --include-untracked -m {}"_f << UniqueStashName);
				else
				{
					fOutputRepoInfo("Stashing local changes on branch '{}'"_f << MainBranchInfo.m_Branch);
					co_await Launches.f_Launch
						(
							f_GetBaseDir()
							, {"stash", "push", "--include-untracked", "-m", UniqueStashName}
							, {}
							, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode
						)
					;
					bStashedLocalChanges = true;
				}
			}
		}

		if (_Flags & ERepoBranchFlag_Pretend)
			Launches.f_Output(EOutputType_Normal, f_GetBaseDir(), "git {}"_f << CProcessLaunchParams::fs_GetParams(ParamsCheckout));
		else
			co_await Launches.f_Launch(f_GetBaseDir(), ParamsCheckout, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);

		RestoreStashOnFailure.f_Clear();

		{
			auto StashListResult = co_await Launches.f_Launch(f_GetBaseDir(), {"stash", "list"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);
			CStr StashBranchPrefix = fGetStashNameForBranch(Branch);
			for (auto &Line : StashListResult.f_GetStdOut().f_SplitLine<true>())
			{
				// Match either the legacy format (line ends with the prefix) or the
				// new format (prefix followed by ":<random>"). The trailing colon
				// avoids matching `:foobar` when looking for `:foo`.
				auto PrefixPos = Line.f_Find(StashBranchPrefix);
				if (PrefixPos < 0)
					continue;
				auto AfterPrefix = PrefixPos + StashBranchPrefix.f_GetLen();
				if (AfterPrefix != Line.f_GetLen() && Line[AfterPrefix] != ':')
					continue;

				CStr StashRef;
				(CStr::CParse("{}:") >> StashRef).f_Parse(Line);
				if (!StashRef.f_IsEmpty())
				{
					if (_Flags & ERepoBranchFlag_Pretend)
						Launches.f_Output(EOutputType_Normal, f_GetBaseDir(), "git stash pop {}"_f << StashRef);
					else
					{
						fOutputRepoInfo("Restoring stashed changes for branch '{}'"_f << Branch);
						auto PopResult = co_await Launches.f_Launch(f_GetBaseDir(), {"stash", "pop", StashRef}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
						if (PopResult.m_ExitCode != 0)
							fOutputRepoInfo("Stash pop conflict: {}"_f << PopResult.f_GetCombinedOut());
					}
				}
				break;
			}
		}

		if (_Flags & ERepoBranchFlag_Pretend)
			fOutputRepoInfo("Pretend mode: nested repository operations are not previewed");
		else
		{
			// The root repo now points at the new branch, so the build system data
			// (mp_Data) parsed during fp_GeneratePrepare is stale. Retry so the outer
			// loop creates a fresh CBuildSystem that re-parses from the new branch's
			// configuration files before updating nested repositories.
			// Use ForceUpdate so fp_HandleRepositories runs even with --skip-update,
			// otherwise nested repos would not be reconciled to the new branch.
			co_return ERetry_Again_ForceUpdate;
		}

		co_return ERetry_None;
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::fp_ForceBranchRepos(CRepoFilter const &_Filter, TCOptional<CStr> const &_Branch, ERepoBranchFlag _Flags)
	{
		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None);

		CGitLaunches Launches{f_GetGitLaunchOptions("force-branch"), "Switching repo branches"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCFutureVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				auto DynamicInfo = fg_GetRepositoryDynamicInfo(Repo);

				CStr TargetBranch = _Branch ? *_Branch : Repo.m_OriginProperties.m_DefaultBranch;
				if (fg_GetBranch(Repo, DynamicInfo) == TargetBranch)
					continue;

				TCVector<CStr> ParamsCheckout = {"checkout", "-B", TargetBranch};

				if (_Flags & ERepoBranchFlag_Pretend)
					Launches.f_Output(EOutputType_Normal, Repo, "git {}"_f << CProcessLaunchParams::fs_GetParams(ParamsCheckout));
				else
				{
					fg_DirectDispatch
						(
							[=]() -> TCFuture<void>
							{
								auto Result = co_await Launches.f_Launch(Repo, ParamsCheckout, fg_LogAllFunctor()).f_Wrap();
								Launches.f_RepoDone();

								co_return fg_Move(Result);
							}
						)
						> Results
					;
				}
			}

			LaunchResults.f_Insert(co_await fg_AllDoneWrapped(Results));
			if (*mp_pCancelled)
				break;
		}

		co_await (fg_Move(LaunchResults) | g_Unwrap);

		co_await f_CheckCancelled();

		co_return ERetry_None;
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_Branch
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, CStr const &_Branch
			, ERepoBranchFlag _Flags
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		// fp_GeneratePrepare parses the build system data and reconciles nested repos
		// against the current branch. This is intentional: the user should see whether
		// the workspace is clean before switching to another branch. If reconciliation
		// is not desired the user can pass --skip-update to bypass it.
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		if (!(_Flags & ERepoBranchFlag_Force))
		{
			if (!_Filter.f_IsEmpty())
				co_return DMibErrorInstance("Cannot use repository filters with non-forced branch. Use --force to branch individual repositories.");

			co_return co_await fp_BranchRootRepo(GenerateState, _Branch, _Flags);
		}

		co_return co_await fp_ForceBranchRepos(_Filter, _Branch, _Flags);
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_Unbranch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoBranchFlag _Flags)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		if (!(_Flags & ERepoBranchFlag_Force))
		{
			if (!_Filter.f_IsEmpty())
				co_return DMibErrorInstance("Cannot use repository filters with non-forced unbranch. Use --force to unbranch individual repositories.");

			co_return co_await fp_BranchRootRepo(GenerateState, {}, _Flags);
		}

		co_return co_await fp_ForceBranchRepos(_Filter, {}, _Flags);
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_CleanupBranches
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, ERepoCleanupBranchesFlag _Flags
			, TCVector<CStr> const &_Branches
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None);

		if (_Flags & ERepoCleanupBranchesFlag_UpdateRemotes)
			co_await fg_UpdateRemotes(*this, FilteredRepositories);

		CGitLaunches Launches{f_GetGitLaunchOptions("cleanup-branches"), "Cleaning up branches"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCFutureVector<void> LaunchResults;

		TCMap<CRepository *, CSequencer> DeleteLaunchSequencers;

		auto AsyncDestroy = co_await fg_AsyncDestroyLogError
			(
				[&]() -> TCFuture<void>
				{
					auto DeleteLaunchSequencersToDestroy = fg_Move(DeleteLaunchSequencers);

					TCFutureVector<void> DestroyResults;

					for (auto &ToDestory : DeleteLaunchSequencersToDestroy)
						fg_Move(ToDestory).f_Destroy() > DestroyResults;

					co_await fg_AllDone(DestroyResults);

					co_return {};
				}
			)
		;

		for (auto &[RepoBound, iSequence] : FilteredRepositories.f_GetAllRepos())
		{
			auto &Repo = RepoBound;
			auto *pRepo = &Repo;

			auto DynamicInfo = fg_GetRepositoryDynamicInfo(Repo);

			auto CurrentBranch = fg_GetBranch(Repo, DynamicInfo);
			auto &DeleteLaunchSequencer = *DeleteLaunchSequencers(pRepo, "BuildSystem Action Repository CleanupBranches DeleteLaunchSequencer {}"_f << Repo.f_GetName());
			auto RepoDoneScope = Launches.f_RepoDoneScope();

			fg_DirectDispatch
				(
					[=]() mutable -> TCFuture<void>
					{
						TCVector<CStr> Params = {"branch", "--format", "%(refname:short)"};

						TCFuture<TCVector<CStr>> RemotesFuture;

						if (_Flags & ERepoCleanupBranchesFlag_AllRemotes)
						{
							Params.f_Insert("-a");
							RemotesFuture = fg_GetRemotes(Launches, Repo);
						}
						else
							RemotesFuture = TCVector<CStr>();

						auto [LaunchResult, RemotesVector] = co_await
							(
								Launches.f_Launch(Repo, Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_None)
								+ fg_Move(RemotesFuture)
							)
						;

						if (LaunchResult.m_ExitCode != 0)
						{
							Launches.f_Output(EOutputType_Error, Repo, "Failed to list branches: {}"_f << LaunchResult.f_GetCombinedOut().f_Trim());
							co_return {};
						}

						auto Remotes = TCSet<CStr>::fs_FromContainer(fg_Move(RemotesVector));

						TCFutureVector<void> Results;

						auto FullBranches = LaunchResult.f_GetStdOut().f_SplitLine<true>();

						TCSet<CStr> LoggedRemote;

						for (auto &FullBranch : FullBranches)
						{
							if (FullBranch.f_IsEmpty())
								continue;

							auto Components = FullBranch.f_Split<true>("/");

							CStr Remote;
							CStr Branch;
							if ((_Flags & ERepoCleanupBranchesFlag_AllRemotes) && Remotes.f_FindEqual(Components[0]))
							{
								Remote = Components[0];
								Components.f_Remove(0);
								Branch = CStr::fs_Join(Components, "/");
							}
							else
								Branch = FullBranch;

							if (!_Branches.f_IsEmpty())
							{
								bool bMatchedBranch = false;
								for (auto &BranchWildcard : _Branches)
								{
									if (fg_StrMatchWildcard(Branch.f_GetStr(), BranchWildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
									{
										bMatchedBranch = true;
										break;
									}
								}

								if (!bMatchedBranch)
									continue;
							}

							if (Branch == "HEAD")
								continue;

							CStr CompareRemote = Remote ? Remote : CStr("origin");

							if (Remote && (_Flags & ERepoCleanupBranchesFlag_AllRemotes))
							{
								if (Remote != "origin")
								{
									auto pRemote = Repo.m_Remotes.m_Remotes.f_FindEqual(Remote);
									if (!pRemote)
									{
										if (_Flags & ERepoCleanupBranchesFlag_Verbose)
										{
											if (LoggedRemote(Remote).f_WasCreated())
												Launches.f_Output(EOutputType_Normal, Repo, "{}/* - remote not in build system"_f << Remote);
										}
										continue;
									}
									if (!pRemote->m_bCanPush)
									{
										if (_Flags & ERepoCleanupBranchesFlag_Verbose)
										{
											if (LoggedRemote(Remote).f_WasCreated())
												Launches.f_Output(EOutputType_Normal, Repo, "{}/* - no write access to remote"_f << Remote);
										}
										continue;
									}
								}
							}

							if (Remote && Branch == fg_GetRemoteHead(Repo, DynamicInfo, Remote))
							{
								if (_Flags & ERepoCleanupBranchesFlag_Verbose)
									Launches.f_Output(EOutputType_Normal, Repo, "{} - default remote branch protected"_f << FullBranch);
								continue;
							}

							if (Branch == Repo.m_OriginProperties.m_DefaultBranch)
							{
								if (_Flags & ERepoCleanupBranchesFlag_Verbose)
									Launches.f_Output(EOutputType_Normal, Repo, "{} - default branch protected"_f << FullBranch);
								continue;
							}

							if (Branch == CurrentBranch)
							{
								if (_Flags & ERepoCleanupBranchesFlag_Verbose)
									Launches.f_Output(EOutputType_Normal, Repo, "{} - current branch protected"_f << FullBranch);
								continue;
							}

							bool bIsProtected = false;
							for (auto &BranchWildcard : Repo.m_ProtectedBranches)
							{
								if (fg_StrMatchWildcard(Branch.f_GetStr(), BranchWildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
								{
									bIsProtected = true;
									break;
								}
							}

							if (bIsProtected)
							{
								if (_Flags & ERepoCleanupBranchesFlag_Verbose)
									Launches.f_Output(EOutputType_Normal, Repo, "{} - branch protected by Repository.ProtectedBranches"_f << FullBranch);
								continue;
							}

							fg_DirectDispatch
								(
									[=]() mutable -> TCFuture<void>
									{
										auto [MergeResult, RebaseResult] = co_await
											(
												Launches.f_Launch
												(
													Repo
													, {"merge-base", "--is-ancestor", FullBranch, "{}/{}"_f << CompareRemote << Repo.m_OriginProperties.m_DefaultBranch}
													, {}
													, CProcessLaunchActor::ESimpleLaunchFlag_None
												)
												+ Launches.f_Launch
												(
													Repo
													, {"log", "--oneline", "--cherry", "{}/{}...{}"_f << CompareRemote << Repo.m_OriginProperties.m_DefaultBranch << FullBranch}
													, {}
													, CProcessLaunchActor::ESimpleLaunchFlag_None
												)
											)
										;

										bool bIsInDefault = MergeResult.m_ExitCode == 0;
										CStr DeleteReason = "[ancestor]";
										if (!bIsInDefault)
										{
											bool bAllEquals = true;
											for (auto &Line : RebaseResult.f_GetStdOut().f_Trim().f_SplitLine<true>())
											{
												if (!Line.f_StartsWith("= "))
													bAllEquals = false;
											}
											if (bAllEquals)
											{
												DeleteReason = "[rebased]";
												bIsInDefault = true;
											}
										}
										if (!bIsInDefault && !(_Flags & ERepoCleanupBranchesFlag_Force))
										{
											Launches.f_Output(EOutputType_Error, Repo, "{} - not fully merged"_f << FullBranch);
											co_return {};
										}

										if (!bIsInDefault)
											DeleteReason = "({}WARNING{} - forced)"_f << Colors.f_StatusError() << Colors.f_Default();

										if (_Flags & ERepoCleanupBranchesFlag_Pretend)
										{
											Launches.f_Output(EOutputType_Warning, Repo, "{} - would have deleted {}"_f << FullBranch << DeleteReason);
											co_return {};
										}

										auto DeleteSubscription = co_await DeleteLaunchSequencer.f_Sequence();

										Launches.f_Output(EOutputType_Warning, Repo, "{} - deleting {}"_f << FullBranch << DeleteReason);

										if (Remote)
											co_await Launches.f_Launch(Repo, {"push", Remote, "--delete", "refs/heads/{}"_f << Branch}, fg_LogAllFunctor());
										else
											co_await Launches.f_Launch(Repo, {"branch", "-D", FullBranch}, fg_LogAllFunctor());

										co_return {};
									}
								)
								> Results
							;
						}

						co_await fg_AllDone(Results);

						(void)RepoDoneScope;

						co_return {};
					}
				)
				> LaunchResults
			;
		}

		co_await fg_AllDone(LaunchResults);
		co_await f_CheckCancelled();

		co_return ERetry_None;
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_CleanupTags
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, ERepoCleanupTagsFlag _Flags
			, TCVector<CStr> const &_Tags
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None);

		if (_Flags & ERepoCleanupTagsFlag_UpdateRemotes)
			co_await fg_UpdateRemotes(*this, FilteredRepositories);

		CGitLaunches Launches{f_GetGitLaunchOptions("cleanup-tags"), "Cleaning up tags"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCFutureVector<void> LaunchResults;

		TCMap<CRepository *, CSequencer> DeleteLaunchSequencers;

		auto AsyncDestroy = co_await fg_AsyncDestroyLogError
			(
				[&]() -> TCFuture<void>
				{
					auto DeleteLaunchSequencersToDestroy = fg_Move(DeleteLaunchSequencers);

					TCFutureVector<void> DestroyResults;

					for (auto &ToDestory : DeleteLaunchSequencersToDestroy)
						fg_Move(ToDestory).f_Destroy() > DestroyResults;

					co_await fg_AllDone(DestroyResults);

					co_return {};
				}
			)
		;

		for (auto &[RepoBound, iSequence] : FilteredRepositories.f_GetAllRepos())
		{
			auto &Repo = RepoBound;
			auto *pRepo = &Repo;

			auto &DeleteLaunchSequencer = *DeleteLaunchSequencers(pRepo, "BuildSystem Action Repository CleanupTags DeleteLaunchSequencer {}"_f << Repo.f_GetName());

			auto RepoDoneScope = Launches.f_RepoDoneScope();

			fg_DirectDispatch
				(
					[=]() mutable -> TCFuture<void>
					{
						TCFuture<TCVector<CStr>> RemotesFuture;

						if (_Flags & ERepoCleanupTagsFlag_AllRemotes)
							RemotesFuture = fg_GetRemotes(Launches, Repo);
						else
							RemotesFuture = TCVector<CStr>();

						auto [TagResult, RemotesVector] = co_await
							(
								Launches.f_Launch(Repo, {"tag"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None)
								+ fg_Move(RemotesFuture)
							)
						;

						if (TagResult.m_ExitCode != 0)
						{
							Launches.f_Output(EOutputType_Error, Repo, "Failed to list tags: {}"_f << TagResult.f_GetCombinedOut().f_Trim());
							co_return {};
						}

						auto Remotes = TCSet<CStr>::fs_FromContainer(fg_Move(RemotesVector));

						auto FullTags = TagResult.f_GetStdOut().f_SplitLine<true>();

						TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> RemoteTagsResults;
						for (auto &Remote : Remotes)
							Launches.f_Launch(Repo, {"ls-remote", "--tags", Remote}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None) > RemoteTagsResults[Remote];

						auto RemoteTags = co_await fg_AllDoneWrapped(RemoteTagsResults);

						TCSet<CStr> LoggedRemote;

						struct CTag
						{
							CStr f_GetName() const
							{
								if (!m_Remote)
									return m_Tag;
								return "{}/{}"_f << m_Remote << m_Tag;
							}

							CStr f_GetRef() const
							{
								if (m_Hash)
									return m_Hash;
								return "refs/tags/{}"_f << m_Tag;
							}

							auto operator <=> (CTag const &_Right) const noexcept
							{
								return fg_TupleReferences(m_Remote, m_Tag) <=> fg_TupleReferences(_Right.m_Remote, _Right.m_Tag);
							}

							CStr m_Remote;
							CStr m_Tag;
							CStr m_Hash;
						};

						TCSet<CTag> AllTags;

						TCSet<CStr> ReferencedTags;

						for (auto &LaunchResult : RemoteTags)
						{
							auto &Remote = RemoteTags.fs_GetKey(LaunchResult);
							if (!LaunchResult)
							{
								Launches.f_Output(EOutputType_Error, Repo, "{} - Failed to query tags: {}"_f << Remote << LaunchResult.f_GetExceptionStr());
								continue;
							}
							else if ((*LaunchResult).m_ExitCode != 0)
							{
								Launches.f_Output(EOutputType_Error, Repo, "{} - Failed to query tags: {}"_f << Remote << (*LaunchResult).f_GetCombinedOut().f_Trim());
								continue;
							}

							bool bWritable = true;
							if (Remote && Remote != "origin")
							{
								auto pRemote = Repo.m_Remotes.m_Remotes.f_FindEqual(Remote);
								if (!pRemote)
								{
									if (_Flags & ERepoCleanupTagsFlag_Verbose)
										Launches.f_Output(EOutputType_Normal, Repo, "{}/* - remote not in build system"_f << Remote);
									bWritable = false;
								}
								else if (!pRemote->m_bCanPush)
								{
									if (_Flags & ERepoCleanupTagsFlag_Verbose)
										Launches.f_Output(EOutputType_Normal, Repo, "{}/* - no write access to remote"_f << Remote);
									bWritable = false;
								}
							}

							for (auto &Line : (*LaunchResult).f_GetStdOut().f_SplitLine<true>())
							{
								CStr Hash;
								CStr Tag;
								aint nParsed = 0;
								(CStr::CParse("{}	refs/tags/{}") >> Hash >> Tag).f_Parse(Line, nParsed);
								if (nParsed == 2)
								{
									if (Tag.f_EndsWith("^{}"))
										Tag = Tag.f_Left(Tag.f_GetLen() - 3);

									if (bWritable)
										AllTags[CTag{.m_Remote = Remote, .m_Tag = Tag, .m_Hash = Hash}];
									else
										ReferencedTags[Tag];
								}
							}
						}

						for (auto &Tag : FullTags)
						{
							if (ReferencedTags.f_FindEqual(Tag))
							{
								if (_Flags & ERepoCleanupTagsFlag_Verbose)
									Launches.f_Output(EOutputType_Normal, Repo, "{} - referenced by remote"_f << Tag);
								continue;
							}
							AllTags[CTag{"", Tag}];
						}

						TCFutureVector<void> Results;

						for (auto &Tag : AllTags)
						{
							CStr CompareRemote = Tag.m_Remote ? Tag.m_Remote : CStr("origin");

							if (!_Tags.f_IsEmpty())
							{
								bool bMatchedTag = false;
								for (auto &TagWildcard : _Tags)
								{
									if (fg_StrMatchWildcard(Tag.m_Tag.f_GetStr(), TagWildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
									{
										bMatchedTag = true;
										break;
									}
								}

								if (!bMatchedTag)
									continue;
							}

							bool bIsProtected = false;
							for (auto &TagWildcard : Repo.m_ProtectedTags)
							{
								if (fg_StrMatchWildcard(Tag.m_Tag.f_GetStr(), TagWildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
								{
									bIsProtected = true;
									break;
								}
							}

							if (bIsProtected)
							{
								if (_Flags & ERepoCleanupTagsFlag_Verbose)
									Launches.f_Output(EOutputType_Normal, Repo, "{} - tag protected by Repository.ProtectedTags"_f << Tag.f_GetName());
								continue;
							}

							fg_DirectDispatch
								(
									[=]() mutable -> TCFuture<void>
									{
										auto [MergeResult, RebaseResult] = co_await
											(
												Launches.f_Launch
												(
													Repo
													, {"merge-base", "--is-ancestor", Tag.f_GetRef(), "{}/{}"_f << CompareRemote << Repo.m_OriginProperties.m_DefaultBranch}
													, {}
													, CProcessLaunchActor::ESimpleLaunchFlag_None
												)
												+ Launches.f_Launch
												(
													Repo
													, {"log", "--oneline", "--cherry", "{}/{}...{}"_f << CompareRemote << Repo.m_OriginProperties.m_DefaultBranch << Tag.f_GetRef()}
													, {}
													, CProcessLaunchActor::ESimpleLaunchFlag_None
												)
											)
										;

										bool bIsInDefault = MergeResult.m_ExitCode == 0;
										CStr DeleteReason = "[ancestor]";
										if (!bIsInDefault)
										{
											bool bAllEquals = true;
											for (auto &Line : RebaseResult.f_GetStdOut().f_Trim().f_SplitLine<true>())
											{
												if (!Line.f_StartsWith("= "))
													bAllEquals = false;
											}
											if (bAllEquals)
											{
												DeleteReason = "[rebased]";
												bIsInDefault = true;
											}
										}
										if (!bIsInDefault && !(_Flags & ERepoCleanupTagsFlag_Force))
										{
											Launches.f_Output(EOutputType_Error, Repo, "{} - not fully merged"_f << Tag.f_GetName());
											co_return {};
										}

										if (!bIsInDefault)
											DeleteReason = "({}WARNING{} - forced)"_f << Colors.f_StatusError() << Colors.f_Default();

										if (_Flags & ERepoCleanupTagsFlag_Pretend)
										{
											Launches.f_Output(EOutputType_Warning, Repo, "{} - would have deleted {}"_f << Tag.f_GetName() << DeleteReason);
											co_return {};
										}

										auto DeleteSubscription = co_await DeleteLaunchSequencer.f_Sequence();

										Launches.f_Output(EOutputType_Warning, Repo, "{} - deleting {}"_f << Tag.f_GetName() << DeleteReason);

										if (Tag.m_Remote)
											co_await Launches.f_Launch(Repo, {"push", Tag.m_Remote, "--delete", "refs/tags/{}"_f << Tag.m_Tag}, fg_LogAllFunctor());
										else
											co_await Launches.f_Launch(Repo, {"tag", "-d", Tag.m_Tag}, fg_LogAllFunctor());

										co_return {};
									}
								)
								> Results;
							;
						}

						co_await fg_AllDone(Results);
						(void)RepoDoneScope;

						co_return {};
					}
				)
				> LaunchResults
			;
		}

		co_await fg_AllDone(LaunchResults);
		co_await f_CheckCancelled();

		co_return ERetry_None;
	}
}
