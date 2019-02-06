// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
		TCFuture<void> fg_Fetch(CBuildSystem const &_BuildSystem, CGitLaunches const &_Launches, CRepository const &_Repo)
		{
			TCVector<CStr> FetchParams = {"fetch", "--all", "--prune", "--tags", "-q"};

			TCPromise<void> Promise;
			_Launches.f_Launch
				(
					_Repo
					, FetchParams
				 	, fg_FetchEnvironment(_BuildSystem)
				)
				> Promise / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
				{
					if (_Result.m_ExitCode)
					{
						_Launches.f_Output
							(
								EOutputType_Error
								, _Repo
								, "Error fetching remotes: {}\n"_f
								<< _Result.f_GetCombinedOut().f_Trim()
							)
						;
						Promise.f_SetException(DMibErrorInstance("Could not fetch remote"));
						return;
					}
					Promise.f_SetResult();
				}
			;
			return Promise.f_MoveFuture();
		}

		TCFuture<TCSet<CStr>> fg_CanPush(CGitLaunches const &_Launches, CRepository const &_Repo, TCVector<CStr> const &_Remotes, CGitBranches const &_Branches)
		{
			TCPromise<TCSet<CStr>> Promise;
			{
				TCActorResultMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> CanPushResults;

				for (auto &Remote : _Remotes)
				{
					_Launches.f_Launch
						(
							_Repo
							, {"merge-base", "--is-ancestor", "{}/{}"_f << Remote << _Branches.m_Current, _Branches.m_Current}
						)
						> CanPushResults.f_AddResult(Remote)
					;
				}

				CanPushResults.f_GetResults() > Promise / [=](TCMap<CStr, TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult>> &&_CanPushResults)
					{
						TCSet<CStr> NewPush;
						bool bAllFastForward = true;
						if
							(
								!fg_CombineResults
								(
									Promise
									, fg_Move(_CanPushResults)
									, [&](CStr const &_Remote, CProcessLaunchActor::CSimpleLaunchResult &&_Result)
									{
										if (_Result.m_ExitCode == 1)
										{
											_Launches.f_Output
												(
													EOutputType_Error
													, _Repo
													, "Cannot fast forward to {2}{}/{}{3}\n"_f
													<< _Remote
													<< _Branches.m_Current
													<< CColors::ms_RepositoryName
													<< CColors::ms_Default
												)
											;
											bAllFastForward = false;
										}
										else if (_Result.m_ExitCode)
										{
											CStr Output = _Result.f_GetCombinedOut().f_Trim();
											if (Output == CStr{"fatal: Not a valid object name {}/{}"_f << _Remote << _Branches.m_Current})
												NewPush[_Remote];
											else
											{
												_Launches.f_Output
													(
														EOutputType_Error
														, _Repo
														, "Error determining fast forward to {3}{}/{}{4}: {}\n"_f
														<< _Remote
														<< _Branches.m_Current
														<< Output
														<< CColors::ms_RepositoryName
														<< CColors::ms_Default
													)
												;
												bAllFastForward = false;
											}
										}
									}
								)
							)
						{
							return;
						}

						if (!bAllFastForward)
						{
							Promise.f_SetException(DMibErrorInstance("Could not fast-forward against all remotes, please resolve."));
							return;
						}

						Promise.f_SetResult(fg_Move(NewPush));
					}
				;
			}
			return Promise.f_MoveFuture();
		}

		TCFuture<TCSet<CStr>> fg_NeedPush(CGitLaunches const &_Launches, CRepository const &_Repo, TCVector<CStr> const &_Remotes, CGitBranches const &_Branches)
		{
			TCPromise<TCSet<CStr>> Promise;
			{
				TCActorResultMap<CStr, TCVector<CLogEntry>> NeedPushResults;

				for (auto &Remote : _Remotes)
					fg_GetLogEntries(_Launches, _Repo, "{}/{}"_f << Remote << _Branches.m_Current, _Branches.m_Current, false) > NeedPushResults.f_AddResult(Remote);

				NeedPushResults.f_GetResults() > Promise / [=](TCMap<CStr, TCAsyncResult<TCVector<CLogEntry>>> &&_NeedPushResults)
					{
						TCSet<CStr> NeedPush;
						bool bAllFastForward = true;
						if
							(
								!fg_CombineResults
								(
									Promise
									, fg_Move(_NeedPushResults)
									, [&](CStr const &_Remote, TCVector<CLogEntry> &&_Commits)
									{
										if (!_Commits.f_IsEmpty())
											NeedPush[_Remote];
									}
								)
							)
						{
							return;
						}

						if (!bAllFastForward)
						{
							Promise.f_SetException(DMibErrorInstance("Could not fast-forward against all remotes, please resolve."));
							return;
						}

						Promise.f_SetResult(fg_Move(NeedPush));
					}
				;
			}
			return Promise.f_MoveFuture();
		}
	}
	
	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_Push
		(
		 	CGenerateOptions const &_GenerateOptions
		 	, CRepoFilter const &_Filter
		 	, TCVector<CStr> const &_Remotes
		 	, ERepoPushFlag _PushFlags
		)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;

		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{mp_BaseDir, (_PushFlags & ERepoPushFlag_Pretend) ? "Pretending to push repos" : "Pushing repos"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCSet<CStr>> OutputOrder;

		mint PushOrderGroup = 0;
		for (auto &Repos : FilteredRepositories.m_FilteredRepositories.f_Reverse())
		{
			TCActorResultVector<bool> Results;
			TCSet<CStr> OutputOrderSet;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;
				OutputOrderSet[Launches.f_GetRepoName(Repo)];

				TCFuture<TCVector<CStr>> RemotesFuture;

				if (_Remotes.f_IsEmpty())
					RemotesFuture = fg_GetRemotes(Launches, Repo);
				else
					RemotesFuture = fg_Explicit(_Remotes);

				TCPromise<bool> LaunchResult;
				RemotesFuture + fg_GetBranches(Launches, Repo, false)
					> LaunchResult / [=](TCVector<CStr> &&_Remotes, CGitBranches &&_Branches)
					{
						{
							TCVector<CStr> NewRemotes;

							for (auto &Remote : _Remotes)
							{
								if (auto pRemote = Repo.m_Remotes.f_FindEqual(Remote); pRemote && !pRemote->m_bCanPush)
									continue;

								if (!(_PushFlags & ERepoPushFlag_NonDefaultToAll) && _Branches.m_Current != Repo.m_DefaultBranch && Remote != "origin")
									continue;

								NewRemotes.f_Insert(Remote);
							}

							_Remotes = fg_Move(NewRemotes);
						}
						if (_Branches.m_Current.f_IsEmpty())
						{
							Launches.f_RepoDone();
							LaunchResult.f_SetResult(false);
							return;
						}

						fg_NeedPush(Launches, Repo, _Remotes, _Branches) > LaunchResult / [=](TCSet<CStr> &&_NeedRemotes)
							{
								if (_NeedRemotes.f_IsEmpty())
								{
									Launches.f_RepoDone();
									LaunchResult.f_SetResult(false);
									return;
								}

								fg_Fetch(*this, Launches, Repo) > LaunchResult / [=]
									{
										fg_CanPush(Launches, Repo, _Remotes, _Branches) > LaunchResult / [=](TCSet<CStr> &&_NewPush)
											{
												TCActorResultVector<void> PushResults;

												for (auto &Remote : _Remotes)
												{
													if (_PushFlags & ERepoPushFlag_Pretend)
													{
														if (_NewPush.f_FindEqual(Remote))
														{
															Launches.f_Output
																(
																	EOutputType_Normal
																	, Repo
																	, "New branch {2}{}{3} on {2}{}{3}\n"_f
																	<< _Branches.m_Current
																	<< Remote
																	<< CColors::ms_RepositoryName
																	<< CColors::ms_Default
																)
															;
														}
														else if (_NeedRemotes.f_FindEqual(Remote))
														{
															Launches.f_Output
																(
																	EOutputType_Normal
																	, Repo
																	, "Update {2}{}{3} on {2}{}{3}\n"_f
																	<< _Branches.m_Current
																	<< Remote
																	<< CColors::ms_RepositoryName
																	<< CColors::ms_Default
																)
															;
														}
													}
													else if (_NeedRemotes.f_FindEqual(Remote))
													{
														TCVector<CStr> Params;

														Params = {"push", Remote, _Branches.m_Current};

														if (_PushFlags & ERepoPushFlag_FollowTags)
															Params.f_InsertAfter(0, "--follow-tags");

														if (Remote == "origin")
															Params.f_InsertAfter(0, "-u");

														Launches.f_Launch(Repo, Params, fg_LogAllFunctor()) > PushResults.f_AddResult();
													}
												}

												PushResults.f_GetResults() > LaunchResult / [=](TCVector<TCAsyncResult<void>> &&_PushResults)
													{
														Launches.f_RepoDone();

														if (!fg_CombineResults(LaunchResult, fg_Move(_PushResults)))
															return;

														LaunchResult.f_SetResult(true);
													}
												;
											}
										;
									}
								;
							}
						;
					}
				;

				LaunchResult > Results.f_AddResult();
			}

			bool bDidPush = false;
			for (auto &Result : Results.f_GetResults().f_CallSync())
				bDidPush |= *Result;

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

		return ERetry_None;
	}
}
