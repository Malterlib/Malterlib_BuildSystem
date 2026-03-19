// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Git/Helpers/ConfigParser>

namespace NMib::NBuildSystem::NRepository
{
	CStr fg_GetGitRoot(CStr const &_Directory)
	{
		CStr CurrentDirectory = _Directory;
		while (!CurrentDirectory.f_IsEmpty())
		{
			if (CFile::fs_FileExists(CurrentDirectory + "/.git", EFileAttrib_Directory))
				return CurrentDirectory;
			CurrentDirectory = CFile::fs_GetPath(CurrentDirectory);
		}

		return {};
	}

	CStr fg_GetGitDataDir(CStr const &_GitRoot, CFilePosition const &_Position)
	{
		CStr GitDirectory = _GitRoot + "/.git";
		if (CFile::fs_FileExists(GitDirectory, EFileAttrib_File))
		{
			CStr FileContents = CFile::fs_ReadStringFromFile(GitDirectory, true).f_TrimRight("\n");
			if (!FileContents.f_StartsWith("gitdir: "))
				CBuildSystem::fs_ThrowError(_Position, fg_Format("Unsupported git directory. Expected 'gitdir: ' in '{}'", GitDirectory));
			GitDirectory = CFile::fs_GetExpandedPath(FileContents.f_Extract(8), _GitRoot);
		}
		if (!CFile::fs_FileExists(GitDirectory, EFileAttrib_Directory))
			CBuildSystem::fs_ThrowError(_Position, fg_Format("Missing git directory: {}", GitDirectory));
		return GitDirectory;
	}

	CStr fg_GetGitHeadHash(CStr const &_GitRoot, CFilePosition const &_Position)
	{
		CStr GitDirectory = fg_GetGitDataDir(_GitRoot, _Position);

		CStr HeadRef = CFile::fs_ReadStringFromFile(GitDirectory + "/HEAD", true).f_TrimRight("\n");
		if (HeadRef.f_StartsWith("ref: "))
		{
			HeadRef = HeadRef.f_Extract(5);
			{
				CStr RefFile = "{}/{}"_f << GitDirectory << + HeadRef;
				if (CFile::fs_FileExists(RefFile))
					return CFile::fs_ReadStringFromFile(RefFile, true).f_TrimRight("\n");
			}

			CStr PackedRefs;
			{
				CStr PackedRefFile = "{}/packed-refs"_f << GitDirectory;
				if (CFile::fs_FileExists(PackedRefFile))
					PackedRefs = CFile::fs_ReadStringFromFile(PackedRefFile, true).f_TrimRight("\n");
			}

			for (auto &Line : PackedRefs.f_SplitLine<true>())
			{
				if (Line.f_StartsWith("#"))
					continue;
				CStr CommitHash;
				CStr Ref;
				aint nParsed = 0;
				(CStr::CParse("{} {}") >> CommitHash >> Ref).f_Parse(Line, nParsed);
				if (nParsed != 2)
					continue;
				if (Ref == HeadRef)
					return CommitHash;
			}

			DMibError("Hash for {} was not found in {}"_f << HeadRef << GitDirectory);
		}
		else
			return HeadRef;
	}

	CGitConfig fg_GetGitConfig(CStr const &_GitRoot, CFilePosition const &_Position)
	{
		CStr GitDirectory = fg_GetGitDataDir(_GitRoot, _Position);

		CStr Config = CFile::fs_ReadStringFromFile(GitDirectory + "/config", true);

		auto ConfigContents = CGitConfigParser::fs_Parse(Config);

		CGitConfig GitConfig;

		if (auto *pValue = ConfigContents.f_GetValue("user", "name"))
			GitConfig.m_UserName = *pValue;

		if (auto *pValue = ConfigContents.f_GetValue("user", "email"))
			GitConfig.m_UserEmail = *pValue;

		if (auto *pRemotes = ConfigContents.m_Sections.f_FindEqual("remote"))
		{
			for (auto &SubSection : pRemotes->m_SubSections.f_Entries())
			{
				auto &Value = SubSection.f_Value();

				if (auto *pValue = Value.m_Values.f_FindEqual("url"))
					GitConfig.m_Remotes[SubSection.f_Key()].m_Url = pValue->f_GetLast();

				if (auto *pValue = Value.m_Values.f_FindEqual("fetch"))
					GitConfig.m_Remotes[SubSection.f_Key()].m_Fetch = *pValue;

				if (auto *pValue = Value.m_Values.f_FindEqual("tagopt"))
					GitConfig.m_Remotes[SubSection.f_Key()].m_TagOptions = pValue->f_GetLast();

				if (auto *pValue = Value.m_Values.f_FindEqual("malterlib-lfs-setup"))
					GitConfig.m_Remotes[SubSection.f_Key()].m_bMalterlibLfsSetup = CGitConfigParser::fs_ToBoolean(pValue->f_GetLast());
			}
		}

		if (auto *pLfs = ConfigContents.m_Sections.f_FindEqual("lfs"))
		{
			if (auto *pCustomTransfer = pLfs->m_SubSections.f_FindEqual("customtransfer.malterlib-release"))
			{
				auto &CustomTransfer = GitConfig.m_MalterlibCustomTransfer;

				if (auto *pValue = pCustomTransfer->m_Values.f_FindEqual("args"))
					CustomTransfer.m_Arguments = pValue->f_GetLast();

				if (auto *pValue = pCustomTransfer->m_Values.f_FindEqual("path"))
					CustomTransfer.m_Path = pValue->f_GetLast();

				if (auto *pValue = pCustomTransfer->m_Values.f_FindEqual("concurrent"))
					CustomTransfer.m_bConcurrent = CGitConfigParser::fs_ToBoolean(pValue->f_GetLast());
			}

			for (auto &SubSection : pLfs->m_SubSections.f_Entries())
			{
				auto &Value = SubSection.f_Value();

				if (auto *pValue = Value.m_Values.f_FindEqual("standalonetransferagent"))
					GitConfig.m_CustomLfsTransferAgents[SubSection.f_Key()] = pValue->f_GetLast();
			}
		}

		return GitConfig;
	}

	bool fg_IsSubmodule(CStr const &_GitRoot)
	{
		CStr GitDirectory = _GitRoot + "/.git";
		return CFile::fs_FileExists(GitDirectory, EFileAttrib_File);
	}

	TCFuture<bool> fg_RepoIsChanged(CGitLaunches _GitLaunches, CRepository _Repo, EFilterRepoFlag _Flags)
	{
		CStr GitDirectory = fg_GetGitDataDir(_Repo.m_Location, _Repo.m_Position);

		CStr HeadRef = CFile::fs_ReadStringFromFile(GitDirectory + "/HEAD", true).f_TrimRight("\n");
		if (HeadRef.f_StartsWith("ref: "))
		{
			if (HeadRef != ("ref: refs/heads/{}"_f << _Repo.m_OriginProperties.m_DefaultBranch).f_GetStr())
				co_return true;
		}
		else
			co_return true;

		CStr RepoName = _Repo.f_GetName();

		TCVector<CStr> Params = {"status", "-sb", "--porcelain"};

		if (_Flags & EFilterRepoFlag_OnlyTracked)
			Params.f_Insert("-uno");

		auto StdOut = (co_await _GitLaunches.f_Launch(_Repo, Params)).f_GetStdOut().f_Trim();

		CStr OriginalStdOut = StdOut;
		CStr BranchLine = fg_GetStrLineSep(StdOut);

		if (BranchLine.f_StartsWith("## HEAD "))
			co_return true; // Detached head
		else if (BranchLine.f_StartsWith("## "))
		{
			if (BranchLine.f_Find(gc_ConstString_Symbol_Ellipsis.m_String) < 0)
				co_return true; // Non-pushed branch

			CStr LocalRef;
			CStr RemoteRef;
			CStr Changes;
			(CStr::CParse("## {}...{} [{}]") >> LocalRef >> RemoteRef >> Changes).f_Parse(BranchLine);

			if ((_Flags & EFilterRepoFlag_IncludePull) && !Changes.f_IsEmpty())
				co_return true; // Non-pushed or pulled changes
			else if (Changes.f_Find("ahead") >= 0)
				co_return true; // Non-pushed changes
		}

		co_return !StdOut.f_IsEmpty(); // Local changes
	}

	TCFuture<TCVector<CLocalFileChange>> fg_GetLocalFileChanges(CGitLaunches _GitLaunches, CRepository _Repo, bool _bIncludeUntracked)
	{
		TCVector<CStr> Params = {"status", "-s"};

		if (!_bIncludeUntracked)
			Params.f_Insert("-uno");

		auto Result = co_await _GitLaunches.f_Launch(_Repo, Params);

		if (Result.m_ExitCode)
			co_return DMibErrorInstance(Result.f_GetErrorOut().f_Trim());

		TCVector<CLocalFileChange> Changes;

		for (auto &Line : Result.f_GetStdOut().f_SplitLine<true>())
		{
			auto &Change = Changes.f_Insert();
			Change.m_ChangeType = Line.f_Left(2).f_Trim();
			if (Change.m_ChangeType == "??")
				Change.m_ChangeType = "?";
			Change.m_File = Line.f_Extract(3);
		}

		co_return fg_Move(Changes);
	}

	TCFuture<CGitBranches> fg_GetBranches(CGitLaunches _GitLaunches, CRepository _Repo, bool _bRemote)
	{
		TCVector<CStr> Params = {"branch"};
		if (_bRemote)
			Params.f_Insert("-r");

		auto Result = co_await _GitLaunches.f_Launch(_Repo, Params);

		if (Result.m_ExitCode)
			co_return DMibErrorInstance(Result.f_GetErrorOut().f_Trim());

		CGitBranches GitBranches;

		for (auto &Line : Result.f_GetStdOut().f_SplitLine<true>())
		{
			CStr Branch = Line.f_Extract(2);
			if (Branch.f_IsEmpty())
				continue;

			if (Branch.f_StartsWith("(HEAD detached"))
				Branch = "HEAD";

			GitBranches.m_Branches[Branch];

			if (Line.f_StartsWith("* "))
				GitBranches.m_Current = Branch;
		}

		co_return fg_Move(GitBranches);
	}

	TCFuture<TCVector<CStr>> fg_GetRemotes(CGitLaunches _GitLaunches, CRepository _Repo)
	{
		auto Result = co_await _GitLaunches.f_Launch(_Repo, {"remote"});

		if (Result.m_ExitCode)
			co_return DMibErrorInstance(Result.f_GetErrorOut().f_Trim());

		TCVector<CStr> Remotes;

		for (auto &Line : Result.f_GetStdOut().f_SplitLine<true>())
			Remotes.f_Insert(Line);

		co_return fg_Move(Remotes);
	}

	TCFuture<TCMap<CStr, CRemote>> fg_GetPushRemotes(CGitLaunches _GitLaunches, CRepository _Repo, TCVector<CStr> _Remotes)
	{
		auto Result = co_await _GitLaunches.f_Launch(_Repo, {"remote"});

		if (Result.m_ExitCode)
			co_return DMibErrorInstance(Result.f_GetErrorOut().f_Trim());

		TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> UrlResults;

		for (auto &Remote : Result.f_GetStdOut().f_SplitLine<true>())
			_GitLaunches.f_Launch(_Repo, {"remote", "get-url", Remote, "--push"}) > UrlResults[Remote];

		auto LaunchResults = co_await fg_AllDone(UrlResults);

		TCMap<CStr, CRemote> Remotes;

		for (auto &LaunchResult : LaunchResults.f_Entries())
		{
			if (LaunchResult.f_Value().m_ExitCode)
				co_return DMibErrorInstance(LaunchResult.f_Value().f_GetErrorOut().f_Trim());

			Remotes[LaunchResult.f_Key()] = CRemote{.m_Properties = CRemoteProperties{.m_URL = LaunchResult.f_Value().f_GetStdOut().f_Trim()}};
		}

		co_return fg_Move(Remotes);
	}

	TCFuture<TCVector<CLogEntry>> fg_GetLogEntries(CGitLaunches _GitLaunches, CRepository _Repo, CStr _From, CStr _To, bool _bReportBadRevision)
	{
		auto LaunchResult = co_await _GitLaunches.f_Launch(_Repo, {"log", "{}..{}"_f << _From << _To, "--oneline", "--"});

		if (LaunchResult.m_ExitCode)
		{
			CStr Output = LaunchResult.f_GetCombinedOut().f_Trim();
			if (!_bReportBadRevision && Output.f_StartsWith("fatal: bad revision "))
			{
				TCVector<CLogEntry> LogEntries;
				auto &DummyEntry = LogEntries.f_Insert();
				DummyEntry.m_Description = Output;
				co_return fg_Move(LogEntries);
			}
			if (Output.f_IsEmpty())
				Output = "Error status from git: {}"_f << LaunchResult.m_ExitCode;
			co_return DMibErrorInstance(Output);
		}

		TCVector<CLogEntry> LogEntries;

		for (auto &Line : LaunchResult.f_GetStdOut().f_SplitLine<true>())
		{
			auto &LogEntry = LogEntries.f_Insert();
			LogEntry.m_Description = Line;
			LogEntry.m_Hash = fg_GetStrSep(LogEntry.m_Description, " ");
		}

		co_return fg_Move(LogEntries);
	}

	TCFuture<TCVector<CLogEntryFull>> fg_GetLogEntriesFull(CGitLaunches _GitLaunches, CRepository _Repo, CStr _From, CStr _To)
	{
		auto LogResult = co_await _GitLaunches.f_Launch(_Repo, {"log", "{}..{}"_f << _From << _To, "--pretty=raw", "--"});
		if (LogResult.m_ExitCode)
			co_return DMibErrorInstance(LogResult.f_GetErrorOut().f_Trim());

		TCVector<CLogEntryFull> LogEntries;

		auto fParseDate = [](CStr const &_String, CTime &o_Time) -> CStr
			{
				ch8 const *pParseStart = _String.f_GetStr();
				ch8 const *pParse = pParseStart + _String.f_GetLen() - 1;
				while (pParse >= pParseStart && *pParse != ' ')
					--pParse;
				--pParse;
				while (pParse >= pParseStart && *pParse != ' ')
					--pParse;

				if (pParse >= pParseStart)
				{
					CStr User(pParseStart, pParse - pParseStart);

					++pParse;
					CStr TimeZone = pParse;
					CStr Date = fg_GetStrSep(TimeZone, " ");

					if (TimeZone.f_GetLen() != 5)
						return _String;

					o_Time = CTimeConvert::fs_FromUnixSeconds(Date.f_ToInt());

					bool bNegative = TimeZone[0] != '+';
					auto TimeSpan = CTimeSpanConvert::fs_CreateHourSpan(TimeZone.f_Extract(1, 2).f_ToInt());
					TimeSpan += CTimeSpanConvert::fs_CreateMinuteSpan(TimeZone.f_Extract(3, 2).f_ToInt());

					if (bNegative)
						o_Time -= TimeSpan;
					else
						o_Time += TimeSpan;

					return User;
				}

				return _String;
			}
		;

		CLogEntryFull *pCurrentEntry = nullptr;
		for (auto &Line : LogResult.f_GetStdOut().f_SplitLine<true>())
		{
			if (Line.f_StartsWith("commit "))
			{
				pCurrentEntry = &LogEntries.f_Insert();
				pCurrentEntry->m_Commit = Line.f_Extract(7);
				continue;
			}

			if (!pCurrentEntry)
				continue;

			auto &LogEntry = *pCurrentEntry;

			if (Line.f_StartsWith("tree "))
				LogEntry.m_Tree = Line.f_Extract(5);
			else if (Line.f_StartsWith("parent "))
				LogEntry.m_Parent = Line.f_Extract(7);
			else if (Line.f_StartsWith("author "))
				LogEntry.m_Author = fParseDate(Line.f_Extract(7), LogEntry.m_AuthorDate);
			else if (Line.f_StartsWith("committer "))
				LogEntry.m_Committer = fParseDate(Line.f_Extract(10), LogEntry.m_CommitterDate);
			else if (Line.f_StartsWith("    "))
			{
				if (LogEntry.m_FirstLine.f_IsEmpty())
				{
					if (Line.f_IsEmpty())
						continue;
					LogEntry.m_FirstLine = Line.f_Extract(4);
					LogEntry.m_Message = LogEntry.m_FirstLine;
				}
				else
					fg_AddStrSep(LogEntry.m_Message, Line.f_Extract(4), "\n");
			}
		}

		co_return fg_Move(LogEntries);
	}

	bool fg_BranchExists(CRepository const &_Repo, CStr const &_Branch)
	{
		CStr GitDirectory = fg_GetGitDataDir(_Repo.m_Location, _Repo.m_Position);

		if (CFile::fs_FileExists("{}/refs/heads/{}"_f << GitDirectory << _Branch))
			return true;

		CStr PackedRefs;
		{
			CStr PackedRefFile = "{}/packed-refs"_f << GitDirectory;
			if (!CFile::fs_FileExists(PackedRefFile))
				return false;
			PackedRefs = CFile::fs_ReadStringFromFile(PackedRefFile, true).f_TrimRight("\n");
		}

		CStr BranchRef = "refs/heads/{}"_f << _Branch;

		for (auto &Line : PackedRefs.f_SplitLine<true>())
		{
			if (Line.f_StartsWith("#"))
				continue;
			CStr CommitHash;
			CStr Ref;
			aint nParsed = 0;
			(CStr::CParse("{} {}") >> CommitHash >> Ref).f_Parse(Line, nParsed);
			if (nParsed != 2)
				continue;
			if (Ref == BranchRef)
				return true;
		}

		return false;
	}

	CStr fg_GetBranch(CRepository const &_Repo)
	{
		CStr GitDirectory = fg_GetGitDataDir(_Repo.m_Location, _Repo.m_Position);

		CStr HeadRef = CFile::fs_ReadStringFromFile(GitDirectory + "/HEAD", true).f_TrimRight("\n");
		if (!HeadRef.f_StartsWith("ref: "))
			return {};

		CStr Branch;
		(CStr::CParse("ref: refs/heads/{}") >> Branch).f_Parse(HeadRef);
		return Branch;
	}

	CStr fg_GetRemoteHead(CRepository const &_Repo, CStr const &_Remote)
	{
		CStr GitDirectory = fg_GetGitDataDir(_Repo.m_Location, _Repo.m_Position);

		CStr File = "{}/refs/remotes/{}/HEAD"_f << GitDirectory << _Remote;

		if (!CFile::fs_FileExists(File))
			return {};

		CStr HeadRef = CFile::fs_ReadStringFromFile(File, true).f_TrimRight("\n");
		if (!HeadRef.f_StartsWith("ref: "))
			return {};

		CStr Branch;
		(CStr::CParse(("ref: refs/remotes/{}/{{}"_f << _Remote).f_GetStr()) >> Branch).f_Parse(HeadRef);
		return Branch;
	}

	TCVector<TCTuple<CRepository, umint>> CFilteredRepos::f_GetAllRepos() const
	{
		TCVector<TCTuple<CRepository, umint>> AllRepos;

		umint iSequence = 0;
		for (auto &Repos : m_FilteredRepositories)
		{
			for (auto *pRepo : Repos)
				AllRepos.f_Insert({*pRepo, iSequence});
			++iSequence;
		}

		return AllRepos;
	}

	TCMap<CStr, CStr> fg_FetchEnvironment(CBuildSystem const &_BuildSystem)
	{
		auto &Options = _BuildSystem.f_GetGenerateOptions();
		return {{"GIT_HTTP_CONNECT_TIMEOUT", CStr::fs_ToStr(Options.m_GitFetchTimeout)}};
	}

	TCUnsafeFuture<void> DMibWorkaroundUBSanSectionErrors fg_UpdateRemotes(CBuildSystem &_BuildSystem, CFilteredRepos const &_FilteredRepositories, CStr const &_ExtraMessage)
	{
		co_await (ECoroutineFlag_CaptureMalterlibExceptions);

		CGitLaunches Launches
			{
				_BuildSystem.f_GetBaseDir()
				, "Fetching remotes" + _ExtraMessage
				, _BuildSystem.f_AnsiFlags()
				, _BuildSystem.f_OutputConsoleFunctor()
				, _BuildSystem.f_GetCancelledPointer()
			}
		;

		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(_FilteredRepositories.m_FilteredRepositories);

		TCFutureVector<void> Results;

		auto AllRepos = _FilteredRepositories.f_GetAllRepos();

		TCMap<CStr, CStr> FetchEnvironment = fg_FetchEnvironment(_BuildSystem);

		for (auto &[RepoBound, iSequence] : AllRepos)
		{
			auto &Repo = RepoBound;

			co_await _BuildSystem.f_CheckCancelled();

			struct CBranchState
			{
				CStr const &f_GetName() const
				{
					return TCMap<CStr, CBranchState>::fs_GetKey(*this);
				}

				CStr m_LocalBranch;
				CStr m_RemoteBranch;
			};

			g_Dispatch / [&_BuildSystem, Launches, Repo, FetchEnvironment]() DMibWorkaroundUBSanSectionErrors -> TCFuture<void>
				{
					TCFutureMap<CStr, void> RemoteQueryResults;
					TCSharedPointer<TCMap<CStr, CBranchState>> pRemoteHeadBranches = fg_Construct();

					auto RepoDoneScope = Launches.f_RepoDoneScope();

					auto Remotes = Repo.m_Remotes;
					Remotes["origin"].m_Properties.m_URL = Repo.m_OriginProperties.m_URL;

					for (auto &Remote : Remotes)
					{
						auto &RemoteName = Remote.f_Name();
						(*pRemoteHeadBranches)[RemoteName].m_LocalBranch = fg_GetRemoteHead(Repo, RemoteName);

						Launches.f_Launch
							(
								Repo
								, {"ls-remote", "--symref", RemoteName, "HEAD"}
								, [=](CProcessLaunchActor::CSimpleLaunchResult const &_Result) -> CStr
								{
									CStr StdOut = _Result.f_GetStdOut();
									for (auto &Line : StdOut.f_SplitLine<true>())
									{
										if (Line.f_StartsWith("ref: refs/heads/") && Line.f_EndsWith("	HEAD"))
										{
											CStr RemoteBranch;
											(CStr::CParse("ref: refs/heads/{}	HEAD") >> RemoteBranch).f_Parse(Line);
											(*pRemoteHeadBranches)[RemoteName].m_RemoteBranch = RemoteBranch;
											break;
										}
									}
									return {};
								}
								, {}
								, FetchEnvironment
							)
							> RemoteQueryResults[Remote.f_Name()]
						;
					}

					TCVector<CStr> FetchParams = {"fetch", "--all", "--prune", "--tags", "-q"};
					if (_BuildSystem.f_GetGenerateOptions().m_bForceUpdateRemotes)
						FetchParams.f_Insert("--force");

					auto [FetchResult, RemoteHeadResults, TrackingResult] = co_await
						(
							Launches.f_Launch(Repo, FetchParams, fg_LogAllFunctor(), {}, FetchEnvironment)
							+ fg_AllDoneWrapped(RemoteQueryResults)
							+ Launches.f_Launch(Repo, {"for-each-ref", "--format=%(upstream:short)", "refs/heads/{}"_f << Repo.m_OriginProperties.m_DefaultBranch})
						).f_Wrap()
					;

					co_await _BuildSystem.f_CheckCancelled();

					co_await (fg_Move(FetchResult) | g_Unwrap);
					co_await ((co_await (fg_Move(RemoteHeadResults) | g_Unwrap)) | g_Unwrap);

					TCFutureVector<void> SetHeadResults;

					CStr ExpectedTracking = "origin/{}"_f << Repo.m_OriginProperties.m_DefaultBranch;
					CStr CurrentTracking = TrackingResult ? TrackingResult->f_GetStdOut().f_Trim() : CStr();
					if (CurrentTracking != ExpectedTracking)
					{
						Launches.f_Output(EOutputType_Normal, Repo, "Updating default branch remote tracking branch from {} to {}"_f << CurrentTracking << ExpectedTracking);
						Launches.f_Launch(Repo, {"branch", "-u", ExpectedTracking, Repo.m_OriginProperties.m_DefaultBranch}, fg_LogAllFunctor()) > SetHeadResults;
					}

					for (auto &Remote : *pRemoteHeadBranches)
					{
						auto &RemoteName = Remote.f_GetName();
						if (Remote.m_LocalBranch != Remote.m_RemoteBranch && Remote.m_RemoteBranch)
						{
							Launches.f_Output(EOutputType_Normal, Repo, "Updating remote HEAD to {} on {}"_f << Remote.m_RemoteBranch << RemoteName);
							Launches.f_Launch(Repo, {"remote", "set-head", RemoteName, Remote.m_RemoteBranch}, fg_LogAllFunctor()) > SetHeadResults;
						}
					}

					co_await fg_AllDone(SetHeadResults);

					co_return {};
				}
				> Results;
			;
		}

		co_await fg_AllDone(Results);

		co_return {};
	}

	TCUnsafeFuture<CGitVersion> DMibWorkaroundUBSanSectionErrors fg_GetGitVersion(CGitLaunches &_Launches)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		static CGitVersion GitVersion;
		static constinit CLowLevelLockAggregate Lock;

		{
			DMibLock(Lock);

			if (GitVersion.m_Major)
				co_return GitVersion;
		}

		CStr VersionStr = (co_await _Launches.f_Launch(CStr(""), {"--version"})).f_GetStdOut().f_Trim();

		CGitVersion Version;

		aint nParsed = 0;
		(CStr::CParse("git version {}.{}.{}") >> Version.m_Major >> Version.m_Minor >> Version.m_Patch).f_Parse(VersionStr, nParsed);
		if (nParsed != 3)
			co_return DMibErrorInstance("Failed to parse git version");

		{
			DMibLock(Lock);
			GitVersion = Version;
		}

		co_return Version;
	}

	TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> fg_LogAllFunctor()
	{
		return [](CProcessLaunchActor::CSimpleLaunchResult const &_Result)
			{
				return _Result.f_GetCombinedOut().f_Trim();
			}
		;
	}
}
