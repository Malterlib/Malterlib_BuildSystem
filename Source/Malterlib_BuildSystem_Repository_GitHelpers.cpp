// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

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

		CGitConfig GitConfig;

		auto pParse = Config.f_GetStr();
		CStr LastRemote;
		bool bIsUserConfig = false;
		while (*pParse)
		{
			fg_ParseWhiteSpace(pParse);
			if (fg_StrStartsWith(pParse, "[remote"))
			{
				pParse += 7;
				fg_ParseWhiteSpace(pParse);
				auto pStart = pParse;
				fg_ParseEscape<'\"'>(pParse, '\"');

				CStr RemoteName(pStart, pParse - pStart);
				LastRemote = fg_RemoveEscape<'\"'>(RemoteName);
			}
			else if (fg_StrStartsWith(pParse, "[user]"))
				bIsUserConfig = true;
			else if (bIsUserConfig && fg_StrStartsWith(pParse, "name ="))
			{
				pParse += 6;
				fg_ParseWhiteSpace(pParse);
				auto pStart = pParse;
				fg_ParseToEndOfLine(pParse);
				GitConfig.m_UserName = CStr(pStart, pParse - pStart);
			}
			else if (bIsUserConfig && fg_StrStartsWith(pParse, "email ="))
			{
				pParse += 7;
				fg_ParseWhiteSpace(pParse);
				auto pStart = pParse;
				fg_ParseToEndOfLine(pParse);
				GitConfig.m_UserEmail = CStr(pStart, pParse - pStart);
			}
			else if (!LastRemote.f_IsEmpty() && fg_StrStartsWith(pParse, "url ="))
			{
				pParse += 5;
				fg_ParseWhiteSpace(pParse);
				auto pStart = pParse;
				fg_ParseToEndOfLine(pParse);
				CStr URL(pStart, pParse - pStart);
				GitConfig.m_Remotes[LastRemote] = URL;
			}
			else if (*pParse == '[')
			{
				bIsUserConfig = false;
				LastRemote.f_Clear();
			}
			fg_ParseToEndOfLine(pParse);
			fg_ParseEndOfLine(pParse);
		}

		return GitConfig;
	}

	bool fg_IsSubmodule(CStr const &_GitRoot)
	{
		CStr GitDirectory = _GitRoot + "/.git";
		return CFile::fs_FileExists(GitDirectory, EFileAttrib_File);
	}

	TCFuture<bool> fg_RepoIsChanged(CGitLaunches const &_GitLaunches, CRepository const &_Repo, EFilterRepoFlag _Flags)
	{
		TCPromise<bool> Promise;

		CStr GitDirectory = fg_GetGitDataDir(_Repo.m_Location, _Repo.m_Position);

		CStr HeadRef = CFile::fs_ReadStringFromFile(GitDirectory + "/HEAD", true).f_TrimRight("\n");
		if (HeadRef.f_StartsWith("ref: "))
		{
			if (HeadRef != ("ref: refs/heads/{}"_f << _Repo.m_DefaultBranch).f_GetStr())
				return Promise <<= true;
		}
		else
			return Promise <<= true;

		CStr RepoName = _Repo.f_GetName();

		TCVector<CStr> Params = {"status", "-sb", "--porcelain"};

		if (_Flags & EFilterRepoFlag_OnlyTracked)
			Params.f_Insert("-uno");

		_GitLaunches.f_Launch(_Repo, Params) > Promise / [Promise, RepoName, _Flags](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				auto StdOut = _Result.f_GetStdOut().f_Trim();
				CStr OriginalStdOut = StdOut;
				CStr BranchLine = fg_GetStrLineSep(StdOut);

				if (BranchLine.f_StartsWith("## HEAD "))
					return Promise.f_SetResult(true); // Detached head
				else if (BranchLine.f_StartsWith("## "))
				{
					if (BranchLine.f_Find("...") < 0)
						return Promise.f_SetResult(true); // Non-pushed branch

					CStr LocalRef;
					CStr RemoteRef;
					CStr Changes;
					(CStr::CParse("## {}...{} [{}]") >> LocalRef >> RemoteRef >> Changes).f_Parse(BranchLine);

					if ((_Flags & EFilterRepoFlag_IncludePull) && !Changes.f_IsEmpty())
						return Promise.f_SetResult(true); // Non-pushed or pulled changes
					else if (Changes.f_Find("ahead") >= 0)
						return Promise.f_SetResult(true); // Non-pushed changes
				}

				Promise.f_SetResult(!StdOut.f_IsEmpty()); // Local changes
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<TCVector<CLocalFileChange>> fg_GetLocalFileChanges(CGitLaunches const &_GitLaunches, CRepository const &_Repo, bool _bIncludeUntracked)
	{
		TCPromise<TCVector<CLocalFileChange>> Promise;

		TCVector<CStr> Params = {"status", "-s"};

		if (!_bIncludeUntracked)
			Params.f_Insert("-uno");

		_GitLaunches.f_Launch(_Repo, Params) > Promise / [Promise](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					Promise.f_SetException(DMibErrorInstance(_Result.f_GetErrorOut()));
					return;
				}

				TCVector<CLocalFileChange> Changes;

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine<true>())
				{
					auto &Change = Changes.f_Insert();
					Change.m_ChangeType = Line.f_Left(2).f_Trim();
					if (Change.m_ChangeType == "??")
						Change.m_ChangeType = "?";
					Change.m_File = Line.f_Extract(3);
				}

				Promise.f_SetResult(fg_Move(Changes));
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<CGitBranches> fg_GetBranches(CGitLaunches const &_GitLaunches, CRepository const &_Repo, bool _bRemote)
	{
		TCPromise<CGitBranches> Promise;

		TCVector<CStr> Params = {"branch"};
		if (_bRemote)
			Params.f_Insert("-r");

		_GitLaunches.f_Launch(_Repo, Params) > Promise / [Promise](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					Promise.f_SetException(DMibErrorInstance(_Result.f_GetErrorOut()));
					return;
				}

				CGitBranches GitBranches;

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine<true>())
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

				Promise.f_SetResult(fg_Move(GitBranches));
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<TCVector<CStr>> fg_GetRemotes(CGitLaunches const &_GitLaunches, CRepository const &_Repo)
	{
		TCPromise<TCVector<CStr>> Promise;

		_GitLaunches.f_Launch(_Repo, {"remote"}) > Promise / [Promise](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					Promise.f_SetException(DMibErrorInstance(_Result.f_GetErrorOut()));
					return;
				}

				TCVector<CStr> Remotes;

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine<true>())
					Remotes.f_Insert(Line);

				Promise.f_SetResult(fg_Move(Remotes));
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<TCVector<CLogEntry>> fg_GetLogEntries(CGitLaunches const &_GitLaunches, CRepository const &_Repo, CStr const &_From, CStr const &_To, bool _bReportBadRevision)
	{
		TCPromise<TCVector<CLogEntry>> Promise;

		_GitLaunches.f_Launch(_Repo, {"log", "{}..{}"_f << _From << _To, "--oneline", "--"})
			> Promise / [Promise, _bReportBadRevision](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					CStr Output = _Result.f_GetCombinedOut().f_Trim();
					if (!_bReportBadRevision && Output.f_StartsWith("fatal: bad revision "))
					{
						TCVector<CLogEntry> LogEntries;
						auto &DummyEntry = LogEntries.f_Insert();
						DummyEntry.m_Description = Output;
						Promise.f_SetResult(fg_Move(LogEntries));
						return;
					}
					if (Output.f_IsEmpty())
						Output = "Error status from git: {}"_f << _Result.m_ExitCode;
					Promise.f_SetException(DMibErrorInstance(Output));
					return;
				}

				TCVector<CLogEntry> LogEntries;

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine<true>())
				{
					auto &LogEntry = LogEntries.f_Insert();
					LogEntry.m_Description = Line;
					LogEntry.m_Hash = fg_GetStrSep(LogEntry.m_Description, " ");
				}

				Promise.f_SetResult(fg_Move(LogEntries));
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<TCVector<CLogEntryFull>> fg_GetLogEntriesFull(CGitLaunches const &_GitLaunches, CRepository const &_Repo, CStr const &_From, CStr const &_To)
	{
		TCPromise<TCVector<CLogEntryFull>> Promise;

		_GitLaunches.f_Launch(_Repo, {"log", "{}..{}"_f << _From << _To, "--pretty=raw", "--"})
			> Promise / [Promise](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					Promise.f_SetException(DMibErrorInstance(_Result.f_GetErrorOut().f_Trim()));
					return;
				}

				TCVector<CLogEntryFull> LogEntries;

				CLogEntryFull *pCurrentEntry;

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

							o_Time = CTimeConvert::fs_FromCreateFromUnixSeconds(Date.f_ToInt());

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

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine<true>())
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

				Promise.f_SetResult(fg_Move(LogEntries));
			}
		;

		return Promise.f_MoveFuture();
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

	TCVector<TCTuple<CRepository, mint>> CFilteredRepos::f_GetAllRepos() const
	{
		TCVector<TCTuple<CRepository, mint>> AllRepos;

		mint iSequence = 0;
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

	void fg_UpdateRemotes(CBuildSystem &_BuildSystem, CFilteredRepos const &_FilteredRepositories, CStr const &_ExtraMessage)
	{
		CGitLaunches Launches{_BuildSystem.f_GetBaseDir(), "Fetching remotes" + _ExtraMessage, _BuildSystem.f_AnsiFlags()};
		Launches.f_MeasureRepos(_FilteredRepositories.m_FilteredRepositories);

		CCurrentlyProcessingActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		TCActorResultVector<void> Results;

		auto AllRepos = _FilteredRepositories.f_GetAllRepos();

		TCMap<CStr, CStr> FetchEnvironment = fg_FetchEnvironment(_BuildSystem);

		for (auto &[RepoBound, iSequence] : AllRepos)
		{
			auto &Repo = RepoBound;
			TCPromise<void> Promise;

			TCVector<CStr> FetchParams = {"fetch", "--all", "--prune", "--tags", "-q"};
			if (_BuildSystem.f_GetGenerateOptions().m_bForceUpdateRemotes)
				FetchParams.f_Insert("--force");

			struct CBranchState
			{
				CStr const &f_GetName() const
				{
					return TCMap<CStr, CBranchState>::fs_GetKey(*this);
				}

				CStr m_LocalBranch;
				CStr m_RemoteBranch;
			};

			TCActorResultMap<CStr, void> RemoteQueryResults;
			TCSharedPointer<TCMap<CStr, CBranchState>> pRemoteHeadBranches = fg_Construct();

			auto RepoDoneScope = Launches.f_RepoDoneScope();

			auto Remotes = Repo.m_Remotes;
			Remotes["origin"].m_URL = Repo.m_URL;

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
					> RemoteQueryResults.f_AddResult(Remote.f_Name())
				;
			}

			Launches.f_Launch(Repo, FetchParams, fg_LogAllFunctor(), {}, FetchEnvironment)
				+ RemoteQueryResults.f_GetResults()
				+ Launches.f_Launch(Repo, {"for-each-ref", "--format=%(upstream:short)", "refs/heads/{}"_f << Repo.m_DefaultBranch})
				> [=]
				(
				 	TCAsyncResult<void> &&_FetchResult
				 	, TCAsyncResult<TCMap<CStr, TCAsyncResult<void>>> &&_RemoteHeadResults
				 	, TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult> &&_TrackingResult
				)
				{
					fg_CombineResults(Promise, fg_Move(_RemoteHeadResults));

					if (!_FetchResult && !Promise.f_IsSet())
						Promise.f_SetException(_FetchResult);

					TCActorResultVector<void> SetHeadResults;

					CStr ExpectedTracking = "origin/{}"_f << Repo.m_DefaultBranch;
					CStr CurrentTracking = _TrackingResult ? _TrackingResult->f_GetStdOut().f_Trim() : CStr();
					if (CurrentTracking != ExpectedTracking)
					{
						Launches.f_Output(EOutputType_Normal, Repo, "Updating default branch remote tracking branch from {} to {}"_f << CurrentTracking << ExpectedTracking);
						Launches.f_Launch(Repo, {"branch", "-u", ExpectedTracking, Repo.m_DefaultBranch}, fg_LogAllFunctor()) > SetHeadResults.f_AddResult();
					}

					for (auto &Remote : *pRemoteHeadBranches)
					{
						auto &RemoteName = Remote.f_GetName();
						if (Remote.m_LocalBranch != Remote.m_RemoteBranch && Remote.m_RemoteBranch)
						{
							Launches.f_Output(EOutputType_Normal, Repo, "Updating remote HEAD to {} on {}"_f << Remote.m_RemoteBranch << RemoteName);
							Launches.f_Launch(Repo, {"remote", "set-head", RemoteName, Remote.m_RemoteBranch}, fg_LogAllFunctor()) > SetHeadResults.f_AddResult();
						}
					}

					SetHeadResults.f_GetResults() > Promise / [=](TCVector<TCAsyncResult<void>> &&_SetHeadResults)
						{
							TCPromise<void> ResultPromise;
							if (!fg_CombineResults(ResultPromise, fg_Move(_SetHeadResults)))
							{
								if (!Promise.f_IsSet())
									ResultPromise.f_MoveFuture() > Promise;
								return;
							}

							(void)RepoDoneScope;

							if (!Promise.f_IsSet())
								Promise.f_SetResult();
						}
					;
				}
			;

			Promise.f_MoveFuture() > Results.f_AddResult();
		}

		for (auto &Result : Results.f_GetResults().f_CallSync())
			Result.f_Access();
	}

	CGitVersion fg_GetGitVersion()
	{
		auto fLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir)
			{
				CProcessLaunchParams Params{_WorkingDir};
				Params.m_bShowLaunched = false;
				return CProcessLaunch::fs_LaunchTool("git", _Params, Params);
			}
		;

		static CGitVersion GitVersion;
		static CMutualSpin Lock;

		DMibLock(Lock);

		if (GitVersion.m_Major)
			return GitVersion;

		CStr VersionStr = fLaunchGit({"--version"}, "").f_Trim();

		CGitVersion Version;

		aint nParsed = 0;
		(CStr::CParse("git version {}.{}.{}") >> Version.m_Major >> Version.m_Minor >> Version.m_Patch).f_Parse(VersionStr, nParsed);
		if (nParsed != 3)
			DMibError("Failed to parse git version");

		GitVersion = Version;

		return GitVersion;
	}

	TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> fg_LogAllFunctor()
	{
		return [](CProcessLaunchActor::CSimpleLaunchResult const &_Result)
			{
				return _Result.f_GetCombinedOut();
			}
		;
	}
}
