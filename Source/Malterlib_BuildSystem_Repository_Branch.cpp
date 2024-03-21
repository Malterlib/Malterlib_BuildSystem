// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Concurrency/ActorSequencerActor>
#include <Mib/Concurrency/LogError>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	TCFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_Branch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, CStr const &_Branch, ERepoBranchFlag _Flags)
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{f_GetBaseDir(), "Branching repos", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCActorResultVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				if (fg_GetBranch(Repo) == _Branch)
					continue;

				TCVector<CStr> ParamsCheckout;

				if (_Flags & ERepoBranchFlag_Force)
					ParamsCheckout = {"checkout", "-B", _Branch};
				else if (!fg_BranchExists(Repo, _Branch))
					ParamsCheckout = {"checkout", "-b", _Branch};
				else
					ParamsCheckout = {"checkout", _Branch};

				if (_Flags & ERepoBranchFlag_Pretend)
					Launches.f_Output(EOutputType_Normal, Repo, "git {}"_f << CProcessLaunchParams::fs_GetParams(ParamsCheckout));
				else
				{
					TCPromise<void> LaunchResult;
					Launches.f_Launch(Repo, ParamsCheckout, fg_LogAllFunctor()) > [=](TCAsyncResult<void> &&_Result)
						{
							Launches.f_RepoDone();
							LaunchResult.f_SetResult(fg_Move(_Result));
						}
					;
					LaunchResult.f_MoveFuture() > Results.f_AddResult();
				}
			}

			LaunchResults.f_Insert(co_await Results.f_GetResults());
			if (*mp_pCancelled)
				break;
		}

		co_await (fg_Move(LaunchResults) | g_Unwrap);

		co_await f_CheckCancelled();

		co_return ERetry_None;
	}

	TCFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_Unbranch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoBranchFlag _Flags)
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{f_GetBaseDir(), "Unbranching repos", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

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

				TCVector<CStr> ParamsCheckout;

				if (_Flags & ERepoBranchFlag_Force)
					ParamsCheckout = {"checkout", "-B", Repo.m_DefaultBranch};
				else if (!fg_BranchExists(Repo, Repo.m_DefaultBranch))
					ParamsCheckout = {"checkout", "-b", Repo.m_DefaultBranch};
				else
					ParamsCheckout = {"checkout", Repo.m_DefaultBranch};

				if (_Flags & ERepoBranchFlag_Pretend)
					Launches.f_Output(EOutputType_Normal, Repo, "git {}"_f << CProcessLaunchParams::fs_GetParams(ParamsCheckout));
				else
				{
					TCPromise<void> LaunchResult;
					Launches.f_Launch(Repo, ParamsCheckout, fg_LogAllFunctor()) > [Launches, LaunchResult](TCAsyncResult<void> &&_Result)
						{
							Launches.f_RepoDone();
							LaunchResult.f_SetResult(fg_Move(_Result));
						}
					;
					LaunchResult.f_MoveFuture() > Results.f_AddResult();
				}

			}

			LaunchResults.f_Insert(co_await Results.f_GetResults());
			if (*mp_pCancelled)
				break;
		}

		co_await (fg_Move(LaunchResults) | g_Unwrap);
		co_await f_CheckCancelled();

		co_return ERetry_None;
	}

	TCFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_CleanupBranches
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, ERepoCleanupBranchesFlag _Flags
			, TCVector<CStr> const &_Branches
		)
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data);

		if (_Flags & ERepoCleanupBranchesFlag_UpdateRemotes)
			co_await fg_UpdateRemotes(*this, FilteredRepositories);

		CGitLaunches Launches{f_GetBaseDir(), "Cleaning up branches", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCActorResultVector<void> LaunchResults;

		TCMap<CRepository *, CSequencer> DeleteLaunchSequencers;

		auto AsyncDestroy = co_await fg_AsyncDestroyLogError
			(
				[&]() -> TCFuture<void>
				{
					auto DeleteLaunchSequencersToDestroy = fg_Move(DeleteLaunchSequencers);

					TCActorResultVector<void> DestroyResults;

					for (auto &ToDestory : DeleteLaunchSequencersToDestroy)
						fg_Move(ToDestory).f_Destroy() > DestroyResults.f_AddResult();

					co_await DestroyResults.f_GetUnwrappedResults();

					co_return {};
				}
			)
		;

		for (auto &[RepoBound, iSequence] : FilteredRepositories.f_GetAllRepos())
		{
			auto &Repo = RepoBound;
			auto *pRepo = &Repo;

			auto CurrentBranch = fg_GetBranch(Repo);

			TCPromise<void> Promise;

			TCVector<CStr> Params = {"branch", "--format", "%(refname:short)"};

			TCFuture<TCVector<CStr>> RemotesFuture;

			if (_Flags & ERepoCleanupBranchesFlag_AllRemotes)
			{
				Params.f_Insert("-a");
				RemotesFuture = fg_GetRemotes(Launches, Repo);
			}
			else
				RemotesFuture = TCPromise<TCVector<CStr>>() <<= g_Void;

			auto &DeleteLaunchSequencer = *DeleteLaunchSequencers(pRepo, "BuildSystem Action Repository CleanupBranches DeleteLaunchSequencer {}"_f << Repo.f_GetName());

			auto RepoDoneScope = Launches.f_RepoDoneScope();

			Launches.f_Launch(Repo, Params) + fg_Move(RemotesFuture)
				> Promise / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result, TCVector<CStr> &&_Remotes) mutable
				{
					TCActorResultVector<void> Results;

					TCSet<CStr> Remotes;
					Remotes.f_AddContainer(_Remotes);

					auto FullBranches = _Result.f_GetStdOut().f_SplitLine<true>();

					TCSet<CStr> LoggedRemote;

					for (auto &FullBranch : _Result.f_GetStdOut().f_SplitLine<true>())
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
								auto pRemote = Repo.m_Remotes.f_FindEqual(Remote);
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

						if (Remote && Branch == fg_GetRemoteHead(Repo, Remote))
						{
							if (_Flags & ERepoCleanupBranchesFlag_Verbose)
								Launches.f_Output(EOutputType_Normal, Repo, "{} - default remote branch protected"_f << FullBranch);
							continue;
						}

						if (Branch == Repo.m_DefaultBranch)
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

						TCPromise<void> Promise;

						Launches.f_Launch(Repo, {"merge-base", "--is-ancestor", FullBranch, "{}/{}"_f << CompareRemote << Repo.m_DefaultBranch})
							+ Launches.f_Launch(Repo, {"log", "--oneline", "--cherry", "{}/{}...{}"_f << CompareRemote << Repo.m_DefaultBranch << FullBranch})
							> Promise / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result, CProcessLaunchActor::CSimpleLaunchResult &&_ResultRebase) mutable
							{
								bool bIsInDefault = _Result.m_ExitCode == 0;
								CStr DeleteReason = "[ancestor]";
								if (!bIsInDefault)
								{
									bool bAllEquals = true;
									for (auto &Line : _ResultRebase.f_GetStdOut().f_Trim().f_SplitLine<true>())
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
									Promise.f_SetResult();
									return;
								}

								if (!bIsInDefault)
									DeleteReason = "({}WARNING{} - forced)"_f << Colors.f_StatusError() << Colors.f_Default();

								if (_Flags & ERepoCleanupBranchesFlag_Pretend)
								{
									Launches.f_Output(EOutputType_Warning, Repo, "{} - would have deleted {}"_f << FullBranch << DeleteReason);
									Promise.f_SetResult();
									return;
								}

								DeleteLaunchSequencer.f_RunSequenced
									( 
										g_ActorFunctorWeak / [=](CActorSubscription &&_Subscription) -> TCFuture<void>
										{
											Launches.f_Output(EOutputType_Warning, Repo, "{} - deleting {}"_f << FullBranch << DeleteReason);

											if (Remote)
												co_await Launches.f_Launch(Repo, {"push", Remote, "--delete", "refs/heads/{}"_f << Branch}, fg_LogAllFunctor());
											else
												co_await Launches.f_Launch(Repo, {"branch", "-D", FullBranch}, fg_LogAllFunctor());

											(void)_Subscription;

											co_return {};
										}
									)
									> Promise;
								;
							}
						;

						Promise.f_MoveFuture() > Results.f_AddResult();
					}

					Results.f_GetResults() > Promise / [=](TCVector<TCAsyncResult<void>> &&_Results)
						{
							if (!fg_CombineResults(Promise, fg_Move(_Results)))
								return;

							(void)RepoDoneScope;
							Promise.f_SetResult();
						}
					;
				}
			;

			Promise.f_MoveFuture() > LaunchResults.f_AddResult();
		}

		co_await (co_await LaunchResults.f_GetResults() | g_Unwrap);
		co_await f_CheckCancelled();

		co_return ERetry_None;
	}

	TCFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_CleanupTags
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, ERepoCleanupTagsFlag _Flags
			, TCVector<CStr> const &_Tags
		)
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);
		
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data);

		if (_Flags & ERepoCleanupTagsFlag_UpdateRemotes)
			co_await fg_UpdateRemotes(*this, FilteredRepositories);

		CGitLaunches Launches{f_GetBaseDir(), "Cleaning up tags", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCActorResultVector<void> LaunchResults;

		TCMap<CRepository *, CSequencer> DeleteLaunchSequencers;

		auto AsyncDestroy = co_await fg_AsyncDestroyLogError
			(
				[&]() -> TCFuture<void>
				{
					auto DeleteLaunchSequencersToDestroy = fg_Move(DeleteLaunchSequencers);

					TCActorResultVector<void> DestroyResults;

					for (auto &ToDestory : DeleteLaunchSequencersToDestroy)
						fg_Move(ToDestory).f_Destroy() > DestroyResults.f_AddResult();

					co_await DestroyResults.f_GetUnwrappedResults();

					co_return {};
				}
			)
		;

		for (auto &[RepoBound, iSequence] : FilteredRepositories.f_GetAllRepos())
		{
			auto &Repo = RepoBound;
			auto *pRepo = &Repo;

			TCPromise<void> Promise;

			TCFuture<TCVector<CStr>> RemotesFuture;

			if (_Flags & ERepoCleanupTagsFlag_AllRemotes)
				RemotesFuture = fg_GetRemotes(Launches, Repo);
			else
				RemotesFuture = TCPromise<TCVector<CStr>>() <<= g_Void;

			auto &DeleteLaunchSequencer = *DeleteLaunchSequencers(pRepo, "BuildSystem Action Repository CleanupTags DeleteLaunchSequencer {}"_f << Repo.f_GetName());

			auto RepoDoneScope = Launches.f_RepoDoneScope();

			Launches.f_Launch(Repo, {"tag"}) + fg_Move(RemotesFuture)
				> Promise / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result, TCVector<CStr> &&_Remotes) mutable
				{
					TCSet<CStr> Remotes;
					Remotes.f_AddContainer(_Remotes);

					auto FullTags = _Result.f_GetStdOut().f_SplitLine<true>();

					TCActorResultMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> RemoteTagsResults;
					for (auto &Remote : Remotes)
						Launches.f_Launch(Repo, {"ls-remote", "--tags", Remote}) > RemoteTagsResults.f_AddResult(Remote);

					RemoteTagsResults.f_GetResults() > Promise / [=](TCMap<CStr, TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult>> &&_RemoteTags)
						{
							TCActorResultVector<void> Results;

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

								auto operator <=> (CTag const &_Right) const
								{
									return fg_TupleReferences(m_Remote, m_Tag) <=> fg_TupleReferences(_Right.m_Remote, _Right.m_Tag);
								}

								CStr m_Remote;
								CStr m_Tag;
								CStr m_Hash;
							};

							TCSet<CTag> AllTags;

							TCSet<CStr> ReferencedTags;

							for (auto &LaunchResult : _RemoteTags)
							{
								auto &Remote = _RemoteTags.fs_GetKey(LaunchResult);
								if (!LaunchResult)
								{
									Launches.f_Output(EOutputType_Error, Repo, "{} - Failed to query tags: {}"_f << Remote << LaunchResult.f_GetExceptionStr());
									continue;
								}

								bool bWritable = true;
								if (Remote && Remote != "origin")
								{
									auto pRemote = Repo.m_Remotes.f_FindEqual(Remote);
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

							for (auto &Tag : _Result.f_GetStdOut().f_SplitLine<true>())
							{
								if (ReferencedTags.f_FindEqual(Tag))
								{
									if (_Flags & ERepoCleanupTagsFlag_Verbose)
										Launches.f_Output(EOutputType_Normal, Repo, "{} - referenced by remote"_f << Tag);
									continue;
								}
								AllTags[CTag{"", Tag}];
							}

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

								TCPromise<void> Promise;

								Launches.f_Launch(Repo, {"merge-base", "--is-ancestor", Tag.f_GetRef(), "{}/{}"_f << CompareRemote << Repo.m_DefaultBranch})
									+ Launches.f_Launch(Repo, {"log", "--oneline", "--cherry", "{}/{}...{}"_f << CompareRemote << Repo.m_DefaultBranch << Tag.f_GetRef()})
									> Promise / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result, CProcessLaunchActor::CSimpleLaunchResult &&_ResultRebase) mutable
									{
										bool bIsInDefault = _Result.m_ExitCode == 0;
										CStr DeleteReason = "[ancestor]";
										if (!bIsInDefault)
										{
											bool bAllEquals = true;
											for (auto &Line : _ResultRebase.f_GetStdOut().f_Trim().f_SplitLine<true>())
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
											Promise.f_SetResult();
											return;
										}

										if (!bIsInDefault)
											DeleteReason = "({}WARNING{} - forced)"_f << Colors.f_StatusError() << Colors.f_Default();

										if (_Flags & ERepoCleanupTagsFlag_Pretend)
										{
											Launches.f_Output(EOutputType_Warning, Repo, "{} - would have deleted {}"_f << Tag.f_GetName() << DeleteReason);
											Promise.f_SetResult();
											return;
										}

										DeleteLaunchSequencer.f_RunSequenced
											(
												g_ActorFunctorWeak / [=](CActorSubscription &&_Subscription) -> TCFuture<void>
												{
													Launches.f_Output(EOutputType_Warning, Repo, "{} - deleting {}"_f << Tag.f_GetName() << DeleteReason);

													if (Tag.m_Remote)
														co_await Launches.f_Launch(Repo, {"push", Tag.m_Remote, "--delete", "refs/tags/{}"_f << Tag.m_Tag}, fg_LogAllFunctor());
													else
														co_await Launches.f_Launch(Repo, {"tag", "-d", Tag.m_Tag}, fg_LogAllFunctor());

													(void)_Subscription;

													co_return {};
												}
											)
											> Promise;
										;
									}
								;

								Promise.f_MoveFuture() > Results.f_AddResult();
							}

							Results.f_GetResults() > Promise / [=](TCVector<TCAsyncResult<void>> &&_Results)
								{
									if (!fg_CombineResults(Promise, fg_Move(_Results)))
										return;

									(void)RepoDoneScope;
									Promise.f_SetResult();
								}
							;
						}
					;
				}
			;

			Promise.f_MoveFuture() > LaunchResults.f_AddResult();
		}

		co_await (co_await LaunchResults.f_GetResults() | g_Unwrap);
		co_await f_CheckCancelled();

		co_return ERetry_None;
	}
}
