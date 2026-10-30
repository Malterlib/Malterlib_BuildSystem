// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
		enum class EPushKind
		{
			mc_New
			, mc_Update
			, mc_ForceUpdate
			, mc_SeedDefault
			, mc_FollowTags // The branch is up to date on the remote, so the push only publishes tags
		};

		struct CBackupTag
		{
			CStr m_Name;
			CStr m_Commit;
			bool m_bCreate = true; // Otherwise the tag already exists locally, from an earlier run, and is only published
		};

		struct CPlannedPush
		{
			CStr m_Remote;
			CStr m_Branch;
			CStr m_RemoteHead; // Empty when the branch doesn't exist on the remote
			CStr m_Target;
			CStr m_Lease; // The head the remote has to have for a forced push to go ahead; empty when the push isn't forced
			CStr m_LeaseSource;
			TCVector<CBackupTag> m_BackupTags;
			EPushKind m_Kind = EPushKind::mc_Update;
		};

		struct CRepoPushPlan
		{
			TCVector<CPlannedPush> m_Pushes;
			TCVector<CStr> m_Errors;
		};

		struct CPlannedRepository
		{
			CRepository m_Repo;
			CRepoPushPlan m_Plan;
		};

		// The repositories that pin others, and the default branch of a root that no repository defines
		struct CPushContext
		{
			TCMap<CStr, CRepository> m_RepositoryByLocation;
			CStr m_BaseDir;
			CStr m_RootDefaultBranch;
		};

		CStr fg_PushKindName(EPushKind _Kind)
		{
			switch (_Kind)
			{
			case EPushKind::mc_New: return "New";
			case EPushKind::mc_Update: return "Update";
			case EPushKind::mc_ForceUpdate: return "ForceUpdate";
			case EPushKind::mc_SeedDefault: return "SeedDefault";
			case EPushKind::mc_FollowTags: return "FollowTags";
			}

			return {};
		}

		struct CPushDestinationRemote
		{
			TCVector<CStr> m_Params; // The -c parameters that define it, which go before the git command
			CStr m_Name;
		};

		// A remote, defined only on the command line, with the URL of one push destination of the remote and all its other
		// settings, such as its proxy and upload-pack command, which querying the URL alone would leave out. Its name is one that
		// no remote of the repository has, since a configured URL of that name would come before the one given here.
		TCFuture<CPushDestinationRemote> fg_PushDestinationRemote(CGitLaunches _Launches, CRepository _Repo, CStr _Remote, CStr _Url)
		{
			auto Result = co_await _Launches.f_Launch(_Repo, {"config", "--get-regexp", "^remote\\."}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			auto Lines = Result.f_GetStdOut().f_SplitLine<true>();

			auto fIsUsed = [&](CStr const &_Name)
				{
					CStr NamePrefix = "remote.{}."_f << _Name;
					for (auto &Line : Lines)
					{
						if (Line.f_StartsWith(NamePrefix))
							return true;
					}
					return false;
				}
			;

			CPushDestinationRemote Remote;
			Remote.m_Name = "mib-push-destination";
			for (umint iSuffix = 2; fIsUsed(Remote.m_Name); ++iSuffix)
				Remote.m_Name = "mib-push-destination-{}"_f << iSuffix;

			CStr Prefix = "remote.{}."_f << _Remote;

			// _Url is rewritten already by the url.*.insteadOf rules, which would rewrite it again as the URL of a remote. The URL
			// given is the first, of _Url and the URLs as configured, that git rewrites to _Url. Deliberately, when none is, as when
			// pushInsteadOf pushes over SSH what insteadOf fetches over HTTPS, it is _Url as git rewrites it: git has no query that
			// takes a URL unrewritten, and those rules rewrite between the URLs of one repository.
			TCVector<CStr> Candidates{_Url};
			for (auto &Line : Lines)
			{
				for (auto *pSetting : {"pushurl ", "url "})
				{
					CStr KeyAndSpace = Prefix + pSetting;
					if (Line.f_StartsWith(KeyAndSpace))
						Candidates.f_Insert(Line.f_Extract(KeyAndSpace.f_GetLen()));
				}
			}

			CStr Url;
			for (auto &Candidate : Candidates)
			{
				auto Rewritten = co_await _Launches.f_Launch
					(
						_Repo
						, {"-c", "remote.{}.url={}"_f << Remote.m_Name << Candidate, "ls-remote", "--get-url", Remote.m_Name}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;
				if (Rewritten.m_ExitCode == 0 && Rewritten.f_GetStdOut().f_Trim() == _Url)
				{
					Url = Candidate;
					break;
				}
			}

			if (Url.f_IsEmpty())
				Url = _Url;

			Remote.m_Params = {"-c", "remote.{}.url={}"_f << Remote.m_Name << Url};

			for (auto &Line : Lines)
			{
				aint iSpace = Line.f_FindChar(' ');
				CStr Key = iSpace >= 0 ? Line.f_Left(iSpace) : Line;
				if (!Key.f_StartsWith(Prefix))
					continue;

				CStr Setting = Key.f_Extract(Prefix.f_GetLen());
				if (Setting.f_FindChar('.') >= 0)
					continue;
				if (Setting == "url" || Setting == "pushurl" || Setting == "fetch" || Setting == "push" || Setting == "mirror")
					continue;

				Remote.m_Params.f_Insert("-c");
				if (iSpace >= 0)
					Remote.m_Params.f_Insert("remote.{}.{}={}"_f << Remote.m_Name << Setting << Line.f_Extract(iSpace + 1));
				else
					Remote.m_Params.f_Insert("remote.{}.{}"_f << Remote.m_Name << Setting);
			}

			co_return fg_Move(Remote);
		}

		// The head of a branch where a push to the remote goes, its push URL, empty when it doesn't have the branch. A failure to
		// ask is an error: an empty answer would push without a backup tag or lease.
		TCFuture<CStr> fg_GetDestinationHead(CGitLaunches _Launches, CRepository _Repo, CStr _Remote, CStr _PushUrl, CStr _Branch)
		{
			auto Destination = co_await fg_PushDestinationRemote(_Launches, _Repo, _Remote, _PushUrl);
			TCVector<CStr> Params = Destination.m_Params;
			Params.f_Insert({"ls-remote", Destination.m_Name, "refs/heads/{}"_f << _Branch});
			auto Result = co_await _Launches.f_Launch(_Repo, Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			if (Result.m_ExitCode != 0)
				co_return DMibErrorInstance("Failed to read the head of {}/{}: {}"_f << _Remote << _Branch << Result.f_GetCombinedOut().f_Trim());

			// Lines of "<hash>\t<ref>". The pattern also matches refs that only end with it, such as refs/heads/refs/heads/<branch>,
			// so only the exact ref counts.
			CStr Ref = CStr("refs/heads/") + _Branch;
			for (auto &Line : Result.f_GetStdOut().f_SplitLine<true>())
			{
				aint iTab = Line.f_FindChar('\t');
				if (iTab <= 0)
					co_return DMibErrorInstance("Unexpected output reading the head of {}/{}: {}"_f << _Remote << _Branch << Line);

				if (Line.f_Extract(iTab + 1) == Ref)
					co_return Line.f_Left(iTab);
			}

			co_return {};
		}

		TCFuture<CStr> fg_RevParse(CGitLaunches _Launches, CRepository _Repo, CStr _Revision)
		{
			auto Result = co_await _Launches.f_Launch(_Repo, {"rev-parse", "--verify", "--quiet", _Revision}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			if (Result.m_ExitCode != 0)
				co_return {};

			co_return Result.f_GetStdOut().f_Trim();
		}

		TCFuture<bool> fg_IsAncestor(CGitLaunches _Launches, CRepository _Repo, CStr _Ancestor, CStr _Descendant)
		{
			auto Result = co_await _Launches.f_Launch(_Repo, {"merge-base", "--is-ancestor", _Ancestor, _Descendant}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			if (Result.m_ExitCode == 0)
				co_return true;
			if (Result.m_ExitCode == 1)
				co_return false;

			co_return DMibErrorInstance("Failed to tell whether {} is an ancestor of {}: {}"_f << _Ancestor << _Descendant << Result.f_GetCombinedOut().f_Trim());
		}

		TCFuture<CStr> fg_GetShortHash(CGitLaunches _Launches, CRepository _Repo, CStr _FullHash)
		{
			auto Result = co_await _Launches.f_Launch(_Repo, {"rev-parse", "--short", _FullHash}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			if (Result.m_ExitCode != 0)
				co_return DMibErrorInstance("Failed to shorten {}: {}"_f << _FullHash << Result.f_GetCombinedOut().f_Trim());

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

		// Returns true if the remote has at least one branch. Checks local remote-tracking
		// refs first (cheap, uses whatever was fetched) and falls back to ls-remote when
		// nothing is fetched locally. Used to detect "remote is freshly created, no
		// branches exist yet" so push can seed the configured default branch first.
		//
		// The local-first check is intentional — we don't want to pessimize the default
		// push path with an extra network round-trip for every remote. If a user has
		// externally deleted and recreated the remote, stale local refs could make this
		// return a false positive. That scenario is rare; running `./mib update-repos -r`
		// prunes stale refs and restores correct behavior.
		TCFuture<bool> fg_HasRemoteBranches(CGitLaunches _Launches, CRepository _Repo, CStr _Remote)
		{
			{
				auto Result = co_await _Launches.f_Launch
					(
						_Repo
						, {"for-each-ref", "--count=1", "refs/remotes/{}/"_f << _Remote}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;
				if (Result.m_ExitCode == 0 && !Result.f_GetStdOut().f_Trim().f_IsEmpty())
					co_return true;
			}

			auto Result = co_await _Launches.f_Launch
				(
					_Repo
					, {"ls-remote", "--exit-code", "--heads", _Remote}
					, {}
					, CProcessLaunchActor::ESimpleLaunchFlag_None
				)
			;
			co_return Result.m_ExitCode == 0;
		}

		// The remotes that the branch looks like it needs a push to, from what the checkout has fetched. The plan checks them
		// against the remotes' real heads.
		TCFuture<TCSet<CStr>> DMibWorkaroundUBSanSectionErrors fg_NeedPush(CGitLaunches _Launches, CRepository _Repo, TCVector<CStr> _Remotes, CGitBranches _Branches, bool _bPushPulls)
		{
			TCFutureMap<CStr, TCVector<CLogEntry>> NeedPushResults;
			TCFutureMap<CStr, TCVector<CLogEntry>> NeedPushPullResults;

			for (auto &Remote : _Remotes)
			{
				fg_GetLogEntries(_Launches, _Repo, "{}/{}"_f << Remote << _Branches.m_Current, _Branches.m_Current, false) > NeedPushResults[Remote];
				if (_bPushPulls)
					fg_GetLogEntries(_Launches, _Repo, _Branches.m_Current, "{}/{}"_f << Remote << _Branches.m_Current, false) > NeedPushPullResults[Remote];
			}

			auto [ResultsUnwrapped, PullResultsUnwrapped] = co_await (fg_AllDone(NeedPushResults) + fg_AllDone(NeedPushPullResults));

			auto fHasResolvedCommits = [](TCVector<CLogEntry> const &_Commits)
				{
					for (auto &Commit : _Commits)
					{
						if (!Commit.m_bUnresolved)
							return true;
					}

					return false;
				}
			;

			TCSet<CStr> NeedPush;
			for (auto &Commits : ResultsUnwrapped)
			{
				auto &Remote = ResultsUnwrapped.fs_GetKey(Commits);
				auto pPullCommits = PullResultsUnwrapped.f_FindEqual(Remote);
				bool bHasPull = _bPushPulls && pPullCommits && fHasResolvedCommits(*pPullCommits);

				if (!Commits.f_IsEmpty() || bHasPull)
				{
					CStr DefaultBranch = _Repo.m_OriginProperties.m_DefaultBranch;
					if (auto pRemote = _Repo.m_Remotes.m_Remotes.f_FindEqual(Remote); pRemote && pRemote->m_Properties.m_DefaultBranch)
						DefaultBranch = pRemote->m_Properties.m_DefaultBranch;

					// Skip branches whose tip is already reachable from the remote default branch.
					// This intentionally excludes freshly created branches that haven't diverged yet:
					// the main branch is always synced to all sub-repositories, so only branches with
					// commits not yet visible via the default branch need to be pushed (i.e. commits
					// that another pull would need to resolve).
					auto MergeBaseResult = co_await _Launches.f_Launch
						(
							_Repo
							, {"merge-base", "--is-ancestor", _Branches.m_Current, "{}/{}"_f << Remote << DefaultBranch}
							, {}
							, CProcessLaunchActor::ESimpleLaunchFlag_None
						)
					;
					if (MergeBaseResult.m_ExitCode != 0)
					{
						if (!Commits.f_IsEmpty())
							NeedPush[Remote];
					}
					else if (bHasPull)
						NeedPush[Remote];
				}
			}

			co_return fg_Move(NeedPush);
		}

		// The commit that the parent's default branch on origin pins the repository at, before the change being pushed lands
		// there. Children are pushed before their parents, so the parent's remote-tracking ref still has the previous pin.
		TCFuture<CStr> fg_GetPreviousPin(CGitLaunches _Launches, CRepository _Repo, TCSharedPointer<CPushContext const> _pContext)
		{
			if (_Repo.m_ConfigFile.f_IsEmpty())
				co_return {};

			auto &Context = *_pContext;

			CStr ParentLocation;
			auto *pParent = CBuildSystem::fs_FindContainingPath(Context.m_RepositoryByLocation, CFile::fs_GetPath(_Repo.m_ConfigFile), ParentLocation);
			CStr ParentDefaultBranch = Context.m_RootDefaultBranch;
			if (pParent)
				ParentDefaultBranch = pParent->m_OriginProperties.m_DefaultBranch;
			else if (_Repo.m_ConfigFile.f_StartsWith(Context.m_BaseDir + "/"))
				ParentLocation = Context.m_BaseDir;
			else
				co_return {};

			if (ParentDefaultBranch.f_IsEmpty())
				co_return {};

			CStr ConfigPath = CFile::fs_MakePathRelative(_Repo.m_ConfigFile, ParentLocation);
			auto Result = co_await _Launches.f_Launch
				(
					ParentLocation
					, {"show", "refs/remotes/origin/{}:{}"_f << ParentDefaultBranch << ConfigPath}
					, {}
					, CProcessLaunchActor::ESimpleLaunchFlag_None
				)
			;

			// A parent without the default branch, or a config file that doesn't have the repository yet: the repository is new
			if (Result.m_ExitCode != 0)
				co_return {};

			auto CaptureScope = co_await (g_CaptureExceptions % CStr("Exception parsing the previous config file {}"_f << _Repo.m_ConfigFile));
			auto ConfigFile = CStateHandler::fs_ParseConfigFile(Result.f_GetStdOut(), _Repo.m_ConfigFile);
			if (auto *pConfig = ConfigFile.f_GetConfig(_Repo, Context.m_BaseDir))
				co_return pConfig->m_Hash;

			co_return {};
		}

		TCFuture<CRepoPushPlan> DMibWorkaroundUBSanSectionErrors fg_PlanRepositoryPush
			(
				CBuildSystem *_pBuildSystem
				, CGitLaunches _Launches
				, CRepository _Repo
				, TCVector<CStr> _Remotes
				, CBuildSystem::ERepoPushFlag _PushFlags
				, TCSharedPointer<CPushContext const> _pContext
			)
		{
			CRepoPushPlan Plan;
			CStr RepoName = _Launches.f_GetRepoName(_Repo);

			auto [PushRemotes, Branches] = co_await (fg_GetPushRemotes(_Launches, _Repo, _Remotes) + fg_GetBranches(_Launches, _Repo, false));

			if (Branches.m_Current.f_IsEmpty())
				co_return fg_Move(Plan);

			// The configured remote, which the push remotes only have the URLs of
			auto fResolveRemoteProperties = [&](CStr const &_Remote) -> CRemoteProperties const *
				{
					if (_Remote == "origin")
						return &_Repo.m_OriginProperties;

					if (auto pRemote = _Repo.m_Remotes.m_Remotes.f_FindEqual(_Remote))
						return &pRemote->m_Properties;

					return nullptr;
				}
			;

			auto fResolveDefaultBranch = [&](CStr const &_Remote) -> CStr
				{
					if (auto pRemoteProperties = fResolveRemoteProperties(_Remote); pRemoteProperties && !pRemoteProperties->m_DefaultBranch.f_IsEmpty())
						return pRemoteProperties->m_DefaultBranch;

					return _Repo.m_OriginProperties.m_DefaultBranch;
				}
			;

			TCVector<CStr> Remotes;
			{
				TCSet<CStr> AddedUrls;
				for (auto &Remote : PushRemotes.m_OrderedRemotes)
				{
					if (auto pRemote = _Repo.m_Remotes.m_Remotes.f_FindEqual(Remote.m_Name); pRemote && !pRemote->m_bCanPush)
						continue;

					if
						(
							!(_PushFlags & CBuildSystem::ERepoPushFlag_NonDefaultToAll)
							&& Branches.m_Current != _Repo.m_OriginProperties.m_DefaultBranch
							&& Remote.m_Name != "origin"
						)
					{
						continue;
					}

					if (!AddedUrls(Remote.m_Properties.m_URL).f_WasCreated())
						continue;

					Remotes.f_Insert(Remote.m_Name);
				}
			}

			TCSet<CStr> NeedRemotes = co_await fg_NeedPush(_Launches, _Repo, Remotes, Branches, _PushFlags & CBuildSystem::ERepoPushFlag_PushPulls);

			// The remote-tracking ref only tells whether a push is needed when the push goes where the remote fetches from. A
			// remote with push URLs of its own has its heads there checked, also when the tracking ref has the commit.
			TCSet<CStr> OwnPushUrlRemotes;
			TCSet<CStr> SeparatePushUrlRemotes;
			for (auto &Remote : Remotes)
			{
				auto FetchUrlResult = co_await _Launches.f_Launch(_Repo, {"remote", "get-url", Remote}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
				auto PushUrlsResult = co_await _Launches.f_Launch
					(
						_Repo
						, {"remote", "get-url", "--push", "--all", Remote}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;

				CStr FetchUrl = FetchUrlResult.f_GetStdOut().f_Trim();
				bool bPushesWhereItFetches = FetchUrlResult.m_ExitCode == 0 && PushUrlsResult.m_ExitCode == 0;
				for (auto &Url : PushUrlsResult.f_GetStdOut().f_SplitLine<true>())
				{
					if (Url != FetchUrl)
						bPushesWhereItFetches = false;
				}

				if (bPushesWhereItFetches)
					continue;

				SeparatePushUrlRemotes[Remote];
				if (NeedRemotes.f_FindEqual(Remote))
					continue;

				// As for every remote, a branch whose tip the remote's default branch has is left alone
				auto OnDefaultResult = co_await _Launches.f_Launch
					(
						_Repo
						, {"merge-base", "--is-ancestor", Branches.m_Current, "{}/{}"_f << Remote << fResolveDefaultBranch(Remote)}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;
				if (OnDefaultResult.m_ExitCode == 0 && !(_PushFlags & CBuildSystem::ERepoPushFlag_PushPulls))
					continue;

				NeedRemotes[Remote];
				OwnPushUrlRemotes[Remote];
			}

			if (NeedRemotes.f_IsEmpty())
				co_return fg_Move(Plan);

			bool bForce = _PushFlags & CBuildSystem::ERepoPushFlag_Force;
			if (!bForce)
				co_await fg_Fetch(_pBuildSystem, _Launches, _Repo);

			CStr Target = co_await fg_RevParse(_Launches, _Repo, "refs/heads/{}^{{commit}"_f << Branches.m_Current);
			if (Target.f_IsEmpty())
			{
				Plan.m_Errors.f_Insert("{}: Failed to read the head of branch {}"_f << RepoName << Branches.m_Current);
				co_return fg_Move(Plan);
			}

			// Remotes with the same previous head share its backup tag, which only the first creates
			TCSet<CStr> CreatedTags;

			for (auto &Remote : Remotes)
			{
				// --follow-tags publishes tags with pushes of the branch: a remote that has the branch gets no push for its tags alone
				if (!NeedRemotes.f_FindEqual(Remote))
					continue;

				auto fError = [&](CStr const &_Error)
					{
						Plan.m_Errors.f_Insert("{}: {}"_f << RepoName << _Error);
					}
				;

				// git push <remote> goes to every push URL of the remote, with the same lease for all of them
				auto PushUrlsResult = co_await _Launches.f_Launch
					(
						_Repo
						, {"remote", "get-url", "--push", "--all", Remote}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;
				if (PushUrlsResult.m_ExitCode != 0)
				{
					fError("Failed to read the push URLs of {}: {}"_f << Remote << PushUrlsResult.f_GetCombinedOut().f_Trim());
					continue;
				}


				CStr PushUrl;
				CStr RemoteHead;
				bool bHeadsDiffer = false;
				bool bHeadFailed = false;
				for (auto &Url : PushUrlsResult.f_GetStdOut().f_SplitLine<true>())
				{
					auto HeadResult = co_await fg_GetDestinationHead(_Launches, _Repo, Remote, Url, Branches.m_Current).f_Wrap();
					if (!HeadResult)
					{
						fError(HeadResult.f_GetExceptionStr());
						bHeadFailed = true;
						break;
					}

					if (PushUrl.f_IsEmpty())
					{
						PushUrl = Url;
						RemoteHead = *HeadResult;
					}
					else if (*HeadResult != RemoteHead)
					{
						fError("The push URLs of {} have {} at different commits: {} at {}, {} at {}"_f << Remote << Branches.m_Current << PushUrl << RemoteHead << Url << *HeadResult);
						bHeadsDiffer = true;
						break;
					}
				}

				if (bHeadFailed || bHeadsDiffer)
					continue;

				if (PushUrl.f_IsEmpty())
				{
					fError("{} has no push URL"_f << Remote);
					continue;
				}

				if (RemoteHead == Target)
				{
					// As git push --follow-tags would, the tags of the commits are published also when the branch is up to date
					if (_PushFlags & CBuildSystem::ERepoPushFlag_FollowTags)
					{
						CPlannedPush Tags;
						Tags.m_Remote = Remote;
						Tags.m_Branch = Branches.m_Current;
						Tags.m_RemoteHead = RemoteHead;
						Tags.m_Target = Target;
						Tags.m_Kind = EPushKind::mc_FollowTags;
						Plan.m_Pushes.f_Insert(fg_Move(Tags));
					}
					continue;
				}

				// A head that the last fetch didn't see is fetched by its hash from where it was read, which leaves the
				// remote-tracking refs as they are
				if (!RemoteHead.f_IsEmpty() && (co_await fg_RevParse(_Launches, _Repo, "{}^{{commit}"_f << RemoteHead)).f_IsEmpty())
				{
					auto Destination = co_await fg_PushDestinationRemote(_Launches, _Repo, Remote, PushUrl);
					TCVector<CStr> FetchParams = Destination.m_Params;
					FetchParams.f_Insert({"fetch", "--no-tags", Destination.m_Name, RemoteHead});
					auto FetchResult = co_await _Launches.f_Launch
						(
							_Repo
							, FetchParams
							, fg_FetchEnvironment(*_pBuildSystem)
							, CProcessLaunchActor::ESimpleLaunchFlag_None
						)
					;
					if (FetchResult.m_ExitCode != 0)
					{
						fError
							(
								"Failed to fetch {}, the head of {}/{}: {}"_f
								<< RemoteHead
								<< Remote
								<< Branches.m_Current
								<< FetchResult.f_GetCombinedOut().f_Trim()
							)
						;
						continue;
					}
				}

				CPlannedPush Push;
				Push.m_Remote = Remote;
				Push.m_Branch = Branches.m_Current;
				Push.m_RemoteHead = RemoteHead;
				Push.m_Target = Target;

				// Where the tracking ref didn't tell, a destination that already has the commit needs no push, unless pushes are to
				// go to remotes that are ahead too
				if (!RemoteHead.f_IsEmpty() && OwnPushUrlRemotes.f_FindEqual(Remote) && !(_PushFlags & CBuildSystem::ERepoPushFlag_PushPulls))
				{
					auto bHasTarget = co_await fg_IsAncestor(_Launches, _Repo, Target, RemoteHead).f_Wrap();
					if (!bHasTarget)
					{
						fError(bHasTarget.f_GetExceptionStr());
						continue;
					}
					if (*bHasTarget)
						continue;
				}

				if (RemoteHead.f_IsEmpty())
					Push.m_Kind = EPushKind::mc_New;
				else
				{
					auto bIsAncestor = co_await fg_IsAncestor(_Launches, _Repo, RemoteHead, Target).f_Wrap();
					if (!bIsAncestor)
					{
						fError(bIsAncestor.f_GetExceptionStr());
						continue;
					}

					// The remote is ahead and has all of the branch's commits
					if (!*bIsAncestor)
					{
						auto bBehind = co_await fg_IsAncestor(_Launches, _Repo, Target, RemoteHead).f_Wrap();
						if (bBehind && *bBehind && !(_PushFlags & CBuildSystem::ERepoPushFlag_PushPulls))
							continue;
					}

					Push.m_Kind = *bIsAncestor ? EPushKind::mc_Update : EPushKind::mc_ForceUpdate;
				}

				if (Push.m_Kind == EPushKind::mc_ForceUpdate)
				{
					if (!bForce)
					{
						fError("Cannot fast forward to {}/{}"_f << Remote << Branches.m_Current);
						continue;
					}

					auto *pRemoteProperties = fResolveRemoteProperties(Remote);
					if (Branches.m_Current == fResolveDefaultBranch(Remote))
					{
						// The lease is the head the backup tag keeps, so that nothing the push overwrites is lost
						Push.m_Lease = RemoteHead;
						Push.m_LeaseSource = "RemoteHead";

						if (Remote == "origin")
						{
							auto PreviousPin = co_await fg_GetPreviousPin(_Launches, _Repo, _pContext).f_Wrap();
							if (!PreviousPin)
							{
								fError(PreviousPin.f_GetExceptionStr());
								continue;
							}

							if (!PreviousPin->f_IsEmpty())
							{
								// Either side can be behind: the fork moved, or the parent's default branch wasn't fetched
								if (*PreviousPin != RemoteHead)
								{
									fError
										(
											"The parent's origin default branch pins {}, but {}/{} is at {}. Fetch, and check that the change is based on the current"
											" default branches."_f
											<< *PreviousPin
											<< Remote
											<< Branches.m_Current
											<< RemoteHead
										)
									;
									continue;
								}

								Push.m_LeaseSource = "PreviousPin";
							}
						}

						// A remote that only git has, without properties in the build files, gets the default of tagging
						if (!pRemoteProperties || pRemoteProperties->m_bTagPreviousOnForcePush)
						{
							auto ShortHash = co_await fg_GetShortHash(_Launches, _Repo, RemoteHead).f_Wrap();
							if (!ShortHash)
							{
								fError(ShortHash.f_GetExceptionStr());
								continue;
							}

							CBackupTag Tag;
							Tag.m_Name = "{}_{}"_f << Branches.m_Current << *ShortHash;
							Tag.m_Commit = RemoteHead;

							CStr ExistingTag = co_await fg_RevParse(_Launches, _Repo, "refs/tags/{}^{{commit}"_f << Tag.m_Name);
							if (!ExistingTag.f_IsEmpty())
							{
								if (ExistingTag != RemoteHead)
								{
									fError
										(
											"Tag {} already exists at another commit than {}, the previous {} commit it has to keep"_f
											<< Tag.m_Name
											<< RemoteHead
											<< Branches.m_Current
										)
									;
									continue;
								}

								// A tag left by an earlier run that stopped before publishing it
								Tag.m_bCreate = false;
							}
							else if (!CreatedTags(Tag.m_Name).f_WasCreated())
								Tag.m_bCreate = false;

							Push.m_BackupTags.f_Insert(fg_Move(Tag));
						}
					}
					else
					{
						// Only overwrite what this checkout has seen, which protects commits pushed to the branch after the last fetch
						CStr Tracking = co_await fg_RevParse(_Launches, _Repo, "refs/remotes/{}/{}"_f << Remote << Branches.m_Current);
						if (Tracking != RemoteHead)
						{
							fError
								(
									"{}/{} is at {} on the remote, but this checkout last fetched {}. Fetch, and check what changed before force-pushing."_f
									<< Remote
									<< Branches.m_Current
									<< RemoteHead
									<< (Tracking.f_IsEmpty() ? CStr("no such branch") : Tracking)
								)
							;
							continue;
						}

						Push.m_Lease = Tracking;
						Push.m_LeaseSource = "RemoteTrackingRef";
					}
				}

				// For remotes that have no branches yet (freshly created origin from
				// init/clone-remote/fork-remote recovery), seed the configured default
				// branch from the current HEAD before pushing the current branch. Without
				// this, pushing only the current branch makes the hosting provider adopt
				// it as the default branch (e.g. when running ./mib push from a feature
				// branch on a subrepo whose origin was just created).
				//
				// Seeding from HEAD (the current feature-branch tip) is intentional even
				// when the user has already committed on top of the placeholder: HEAD is
				// the best available starting point for a brand-new default branch. For
				// init in particular the placeholder is meant to be amended/extended by
				// the user, and for clone-remote the source's default-branch tip was
				// already folded into the feature branch at recovery time, so using HEAD
				// gives the remote's default branch the user's current working state.
				CStr DefaultBranch = fResolveDefaultBranch(Remote);
				bool bDestinationHasBranches = true;
				if (!DefaultBranch.f_IsEmpty() && Branches.m_Current != DefaultBranch)
				{
					// A remote with push URLs of its own is seeded by what the push URLs have, not by what it fetches. The seed goes
					// to all of them, so they have to agree on having branches, or it would move an existing default branch.
					if (SeparatePushUrlRemotes.f_FindEqual(Remote))
					{
						umint nWithBranches = 0;
						umint nWithout = 0;
						bool bFailed = false;
						for (auto &Url : PushUrlsResult.f_GetStdOut().f_SplitLine<true>())
						{
							auto Destination = co_await fg_PushDestinationRemote(_Launches, _Repo, Remote, Url);
							TCVector<CStr> HeadsParams = Destination.m_Params;
							HeadsParams.f_Insert({"ls-remote", "--heads", Destination.m_Name});
							auto HeadsResult = co_await _Launches.f_Launch
								(
									_Repo
									, HeadsParams
									, {}
									, CProcessLaunchActor::ESimpleLaunchFlag_None
								)
							;
							if (HeadsResult.m_ExitCode != 0)
							{
								fError("Failed to read the branches of {}: {}"_f << Url << HeadsResult.f_GetCombinedOut().f_Trim());
								bFailed = true;
								break;
							}

							if (HeadsResult.f_GetStdOut().f_Trim().f_IsEmpty())
								++nWithout;
							else
								++nWithBranches;
						}

						if (bFailed)
							continue;

						if (nWithBranches && nWithout)
						{
							fError("Some push URLs of {} have branches and some none, so the default branch can't be seeded in all of them"_f << Remote);
							continue;
						}

						bDestinationHasBranches = nWithBranches != 0;
					}
					else
						bDestinationHasBranches = co_await fg_HasRemoteBranches(_Launches, _Repo, Remote);
				}

				if (!bDestinationHasBranches)
				{
					CPlannedPush Seed;
					Seed.m_Remote = Remote;
					Seed.m_Branch = DefaultBranch;
					Seed.m_Target = Target;
					Seed.m_Kind = EPushKind::mc_SeedDefault;
					Plan.m_Pushes.f_Insert(fg_Move(Seed));
				}

				Plan.m_Pushes.f_Insert(fg_Move(Push));
			}

			co_return fg_Move(Plan);
		}

		// The planned commit is pushed, not the branch, which can move after the plan was checked. Commits are named as commit
		// objects, since a branch or tag named like the hash would otherwise be resolved in its place.
		TCVector<CStr> fg_PushParams(CPlannedPush const &_Push, CBuildSystem::ERepoPushFlag _PushFlags)
		{
			CStr RefSpec = CStr(_Push.m_Target + "^{commit}:refs/heads/") + _Push.m_Branch;
			if (_Push.m_Kind == EPushKind::mc_SeedDefault)
				return {"push", _Push.m_Remote, RefSpec};

			TCVector<CStr> Params = {"push"};
			if (!_Push.m_Lease.f_IsEmpty())
				Params.f_Insert("--force-with-lease=refs/heads/{}:{}"_f << _Push.m_Branch << CStr(_Push.m_Lease + "^{commit}"));
			if (_PushFlags & CBuildSystem::ERepoPushFlag_FollowTags)
				Params.f_Insert("--follow-tags");
			Params.f_Insert({_Push.m_Remote, RefSpec});

			return Params;
		}

		CJsonOrdered fg_PushJson(CStr const &_Path, CPlannedPush const &_Push)
		{
			CJsonOrdered Json(EJsonType_Object);
			Json["Path"] = _Path;
			Json["Remote"] = _Push.m_Remote;
			Json["Branch"] = _Push.m_Branch;
			Json["RemoteHead"] = _Push.m_RemoteHead;
			Json["Target"] = _Push.m_Target;
			Json["Kind"] = fg_PushKindName(_Push.m_Kind);
			Json["Lease"] = _Push.m_Lease;
			Json["LeaseSource"] = _Push.m_LeaseSource;

			auto &BackupTags = Json["BackupTags"];
			BackupTags = CJsonOrdered(EJsonType_Array);
			for (auto &Tag : _Push.m_BackupTags)
			{
				auto &TagJson = BackupTags.f_Array().f_Insert(CJsonOrdered(EJsonType_Object));
				TagJson["Name"] = Tag.m_Name;
				TagJson["Commit"] = Tag.m_Commit;
				TagJson["Action"] = Tag.m_bCreate ? "Create" : "Publish";
			}

			return Json;
		}

		void fg_OutputPretend(CGitLaunches const &_Launches, CRepository const &_Repo, CPlannedPush const &_Push, CBuildSystem::ERepoPushFlag _PushFlags, CColors const &_Colors)
		{
			for (auto &Tag : _Push.m_BackupTags)
			{
				if (Tag.m_bCreate)
				{
					_Launches.f_Output
						(
							EOutputType_Normal
							, _Repo
							, "Tag previous {3}{}{4} commit as {3}{}{4}: git {}"_f
							<< _Push.m_Branch
							<< Tag.m_Name
							<< CProcessLaunchParams::fs_GetParams({"tag", Tag.m_Name, Tag.m_Commit})
							<< _Colors.f_RepositoryName()
							<< _Colors.f_Default()
						)
					;
				}

				_Launches.f_Output
					(
						EOutputType_Normal
						, _Repo
						, "Push tag {3}{}{4} on {3}{}{4}: git {}"_f
						<< Tag.m_Name
						<< _Push.m_Remote
						<< CProcessLaunchParams::fs_GetParams({"push", _Push.m_Remote, "refs/tags/{}"_f << Tag.m_Name})
						<< _Colors.f_RepositoryName()
						<< _Colors.f_Default()
					)
				;
			}

			CStr Action;
			switch (_Push.m_Kind)
			{
			case EPushKind::mc_SeedDefault: Action = "Seed default branch"; break;
			case EPushKind::mc_New: Action = "New branch"; break;
			case EPushKind::mc_Update: Action = "Update"; break;
			case EPushKind::mc_ForceUpdate: Action = "Force update"; break;
			case EPushKind::mc_FollowTags: Action = "Publish tags"; break;
			}

			_Launches.f_Output
				(
					EOutputType_Normal
					, _Repo
					, "{} {4}{}{5} on {4}{}{5}: git {}"_f
					<< Action
					<< _Push.m_Branch
					<< _Push.m_Remote
					<< CProcessLaunchParams::fs_GetParams(fg_PushParams(_Push, _PushFlags))
					<< _Colors.f_RepositoryName()
					<< _Colors.f_Default()
				)
			;
		}

		TCFuture<void> fg_ExecutePush(CGitLaunches _Launches, CRepository _Repo, CPlannedPush _Push, CBuildSystem::ERepoPushFlag _PushFlags, CColors _Colors)
		{
			for (auto &Tag : _Push.m_BackupTags)
			{
				if (Tag.m_bCreate)
				{
					auto TagResult = co_await _Launches.f_Launch(_Repo, {"tag", Tag.m_Name, Tag.m_Commit}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
					if (TagResult.m_ExitCode != 0)
					{
						_Launches.f_Output(EOutputType_Error, _Repo, "Failed to create tag {}: {}"_f << Tag.m_Name << TagResult.f_GetCombinedOut().f_Trim());
						co_return DMibErrorInstance("Error status");
					}

					_Launches.f_Output
						(
							EOutputType_Normal
							, _Repo
							, "Created previous {3}{}{4} commit as tag {3}{}{4}:\n{}\n"_f
							<< _Push.m_Branch
							<< Tag.m_Name
							<< TagResult.f_GetCombinedOut().f_Trim()
							<< _Colors.f_RepositoryName()
							<< _Colors.f_Default()
						)
					;
				}
				else
				{
					_Launches.f_Output
						(
							EOutputType_Normal
							, _Repo
							, "Previous {2}{}{3} commit already tagged as {2}{}{3}\n"_f
							<< _Push.m_Branch
							<< Tag.m_Name
							<< _Colors.f_RepositoryName()
							<< _Colors.f_Default()
						)
					;
				}

				auto TagPushResult = co_await _Launches.f_Launch
					(
						_Repo
						, {"push", _Push.m_Remote, "refs/tags/{}"_f << Tag.m_Name}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;

				if (TagPushResult.m_ExitCode != 0)
				{
					_Launches.f_Output(EOutputType_Error, _Repo, "Failed to push tag {}: {}"_f << Tag.m_Name << TagPushResult.f_GetCombinedOut().f_Trim());
					co_return DMibErrorInstance("Error status");
				}

				_Launches.f_Output
					(
						EOutputType_Normal
						, _Repo
						, "Pushed previous {4}{}{5} commit as {4}{}{5} on remote {4}{}{5}:\n{}\n"_f
						<< _Push.m_Branch
						<< Tag.m_Name
						<< _Push.m_Remote
						<< TagPushResult.f_GetCombinedOut().f_Trim()
						<< _Colors.f_RepositoryName()
						<< _Colors.f_Default()
					)
				;
			}

			auto Params = fg_PushParams(_Push, _PushFlags);
			if (_Push.m_Kind == EPushKind::mc_SeedDefault)
			{
				auto SeedResult = co_await _Launches.f_Launch(_Repo, Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
				if (SeedResult.m_ExitCode != 0)
				{
					_Launches.f_Output
						(
							EOutputType_Error
							, _Repo
							, "Failed to seed default branch {} on {}: {}"_f
							<< _Push.m_Branch
							<< _Push.m_Remote
							<< SeedResult.f_GetCombinedOut().f_Trim()
						)
					;

					co_return DMibErrorInstance("Error status");
				}

				_Launches.f_Output
					(
						EOutputType_Normal
						, _Repo
						, "Seeded default branch {3}{}{4} on {3}{}{4}:\n{}\n"_f
						<< _Push.m_Branch
						<< _Push.m_Remote
						<< SeedResult.f_GetCombinedOut().f_Trim()
						<< _Colors.f_RepositoryName()
						<< _Colors.f_Default()
					)
				;

				co_return {};
			}

			co_await _Launches.f_Launch(_Repo, Params, fg_LogAllFunctor());

			// Pushing a commit instead of the branch doesn't set the branch's upstream. It's configured directly, as with
			// git push -u, since a narrow fetch refspec leaves no remote-tracking ref for --set-upstream-to.
			if (_Push.m_Remote == "origin")
			{
				co_await _Launches.f_Launch(_Repo, {"config", "branch.{}.remote"_f << _Push.m_Branch, "origin"}, fg_LogAllFunctor());
				co_await _Launches.f_Launch(_Repo, {"config", "branch.{}.merge"_f << _Push.m_Branch, "refs/heads/{}"_f << _Push.m_Branch}, fg_LogAllFunctor());
			}

			co_return {};
		}
	}

	TCUnsafeFuture<CBuildSystem::ERetry> DMibWorkaroundUBSanSectionErrors CBuildSystem::f_Action_Repository_Push
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, TCVector<CStr> const &_Remotes
			, ERepoPushFlag _PushFlags
			, TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		TCSharedPointer<CGenerateEphemeralState> pGenerateState = fg_Construct();

		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, *pGenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None);

		bool bPretend = _PushFlags & ERepoPushFlag_Pretend;
		bool bJson = _PushFlags & ERepoPushFlag_Json;

		CGitLaunches Launches{f_GetGitLaunchOptions("push"), bPretend ? "Pretending to push repos" : "Pushing repos"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCSharedPointer<CPushContext> pContext = fg_Construct();
		pContext->m_BaseDir = f_GetBaseDir();
		pContext->m_RootDefaultBranch = fg_GetMainRepoBranchInfo(f_GetBaseDir(), FilteredRepositories.m_ReposOrdered).m_DefaultBranch;
		for (auto &Repos : FilteredRepositories.m_ReposOrdered)
		{
			for (auto &RepoLocation : Repos)
			{
				for (auto &Repo : RepoLocation.m_Repositories)
					pContext->m_RepositoryByLocation(Repo.m_Location, Repo);
			}
		}

		// Every push is planned and checked before anything is pushed. Children are pushed before their parents, so a check that
		// failed when its repository's turn came would leave the deeper groups pushed already.
		TCVector<TCVector<CPlannedRepository>> GroupPlans;
		{
			TCVector<TCFutureVector<CRepoPushPlan>> GroupResults;
			TCVector<TCVector<CRepository>> GroupRepos;
			for (auto &Repos : FilteredRepositories.m_FilteredRepositories.f_Reverse())
			{
				auto &Results = GroupResults.f_Insert();
				auto &Repositories = GroupRepos.f_Insert();
				for (auto *pRepo : Repos)
				{
					Repositories.f_Insert(*pRepo);
					fg_PlanRepositoryPush(this, Launches, *pRepo, _Remotes, _PushFlags, pContext.f_ShareAsConst()) > Results;
				}
			}

			TCVector<CStr> Errors;
			for (umint iGroup = 0; iGroup < GroupResults.f_GetLen(); ++iGroup)
			{
				auto Results = co_await fg_AllDoneWrapped(GroupResults[iGroup]);
				auto &Plans = GroupPlans.f_Insert();
				for (umint iRepo = 0; iRepo < Results.f_GetLen(); ++iRepo)
				{
					auto &Repo = GroupRepos[iGroup][iRepo];
					if (!Results[iRepo])
					{
						Errors.f_Insert("{}: {}"_f << Launches.f_GetRepoName(Repo) << Results[iRepo].f_GetExceptionStr());
						continue;
					}

					Errors.f_Insert(Results[iRepo]->m_Errors);

					Plans.f_Insert(CPlannedRepository{Repo, fg_Move(*Results[iRepo])});
				}
			}

			if (!Errors.f_IsEmpty())
				co_return DMibErrorInstance("Nothing was pushed:\n{}"_f << CStr::fs_Join(Errors, "\n"));
		}

		if (bJson)
		{
			CJsonOrdered Pushes(EJsonType_Array);
			for (auto &Plans : GroupPlans)
			{
				for (auto &Planned : Plans)
				{
					for (auto &Push : Planned.m_Plan.m_Pushes)
						Pushes.f_Array().f_Insert(fg_PushJson(fg_RelativeToBase(Planned.m_Repo.m_Location, f_GetBaseDir()), Push));
				}
			}

			*_pCommandLine += Pushes.f_ToString() + "\n";
			co_return ERetry_None;
		}

		TCVector<TCSet<CStr>> OutputOrder;

		umint PushOrderGroup = 0;
		for (auto &Plans : GroupPlans)
		{
			TCFutureVector<bool> Results;
			TCSet<CStr> OutputOrderSet;
			for (auto &Planned : Plans)
			{
				OutputOrderSet[Launches.f_GetRepoName(Planned.m_Repo)];

				g_Dispatch / [Launches, Repo = Planned.m_Repo, Plan = Planned.m_Plan, _PushFlags, Colors, bPretend]() -> TCFuture<bool>
					{
						auto OnExit = g_OnScopeExit / [&]
							{
								Launches.f_RepoDone();
							}
						;

						if (Plan.m_Pushes.f_IsEmpty())
							co_return false;

						if (bPretend)
						{
							for (auto &Push : Plan.m_Pushes)
								fg_OutputPretend(Launches, Repo, Push, _PushFlags, Colors);

							co_return true;
						}

						// In the order of the plan: a seed of the default branch before the branch, and a backup tag created before
						// another remote publishes it
						for (auto &Push : Plan.m_Pushes)
							co_await fg_ExecutePush(Launches, Repo, Push, _PushFlags, Colors);

						co_return true;
					}
					> Results
				;
			}

			// The results are collected wrapped so that a failing repository does not
			// abort before the group heading and the output order have been recorded.
			TCVector<TCAsyncResult<bool>> ResultsWrapped = co_await fg_AllDoneWrapped(Results);

			bool bDidPush = false;
			bool bFailed = false;

			for (auto &Result : ResultsWrapped)
			{
				// A failing repository has already emitted deferred output, so its group
				// needs a heading just like a successful push does.
				if (!Result)
					bFailed = true;

				bDidPush = bDidPush || !Result || *Result;
			}

			if (bDidPush)
			{
				CStr Heading = "Push group {}"_f << PushOrderGroup;
				OutputOrder.f_Insert(TCSet<CStr>{Heading});
				Launches.f_Output(EOutputType_Warning, Heading, "ForceSection");

				OutputOrder.f_Insert(OutputOrderSet);
				++PushOrderGroup;
			}

			// The deferred output is rendered when the launch state is destroyed, so the
			// order has to be applied before the failure propagates out of this coroutine.
			// Without this every repository ends up rendered in name order, followed by
			// the empty group headings.
			if (bFailed)
			{
				Launches.f_SetOutputOrder(OutputOrder);

				co_await (fg_Move(ResultsWrapped) | g_Unwrap);
			}
		}

		Launches.f_SetOutputOrder(OutputOrder);

		co_return ERetry_None;
	}
}
