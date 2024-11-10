// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
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

		TCFuture<TCSet<CStr>> DMibWorkaroundUBSanSectionErrors fg_CanPush(CGitLaunches _Launches, CRepository _Repo, TCVector<CStr> _Remotes, CGitBranches _Branches, bool _bForce)
		{
			CColors Colors(_Launches.m_pState->m_AnsiFlags);

			TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> CanPushResultsMap;

			for (auto &Remote : _Remotes)
			{
				_Launches.f_Launch
					(
						_Repo
						, {"merge-base", "--is-ancestor", "{}/{}"_f << Remote << _Branches.m_Current, _Branches.m_Current}
					)
					> CanPushResultsMap[Remote]
				;
			}

			auto CanPushResults = co_await fg_AllDone(CanPushResultsMap);

			TCSet<CStr> NewPush;
			bool bAllFastForward = true;
			for (auto &Result : CanPushResults)
			{
				auto &Remote = CanPushResults.fs_GetKey(Result);
				if (Result.m_ExitCode == 1)
				{
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
						NewPush[Remote];
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

			co_return fg_Move(NewPush);
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
				if (!Commits.f_IsEmpty())
					NeedPush[ResultsUnwrapped.fs_GetKey(Commits)];
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

		CGitLaunches Launches{f_GetBaseDir(), (_PushFlags & ERepoPushFlag_Pretend) ? "Pretending to push repos" : "Pushing repos", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCSet<CStr>> OutputOrder;

		mint PushOrderGroup = 0;
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

						auto NewPush = co_await fg_CanPush(Launches, Repo, Remotes, Branches, _PushFlags & ERepoPushFlag_Force);

						TCFutureVector<void> PushResults;

						for (auto &Remote : Remotes)
						{
							TCVector<CStr> Params;

							Params = {"push", Remote, Branches.m_Current};

							if (_PushFlags & ERepoPushFlag_FollowTags)
								Params.f_InsertAfter(0, "--follow-tags");

							if (_PushFlags & ERepoPushFlag_Force)
								Params.f_InsertAfter(0, "--force-with-lease");

							if (Remote == "origin")
								Params.f_InsertAfter(0, "-u");

							if (_PushFlags & ERepoPushFlag_Pretend)
							{
								if (NewPush.f_FindEqual(Remote))
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
											<< ((_PushFlags & ERepoPushFlag_Force) ? "Force update" : "Update")
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
								Launches.f_Launch(Repo, Params, fg_LogAllFunctor()) > PushResults;
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
