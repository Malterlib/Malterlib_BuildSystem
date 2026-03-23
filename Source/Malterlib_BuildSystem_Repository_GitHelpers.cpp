// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Git/Helpers/ConfigParser>

namespace NMib::NBuildSystem::NRepository
{
	namespace
	{
		aint fg_FindGitPath(CStr const &_Path, ch8 const *_pSearch)
		{
#ifdef DPlatformFamily_Windows
			return _Path.f_FindNoCase(_pSearch);
#else
			return _Path.f_Find(_pSearch);
#endif
		}

		aint fg_FindGitPath(CStr const &_Path, aint _Start, ch8 const *_pSearch)
		{
#ifdef DPlatformFamily_Windows
			return _Path.f_FindNoCase(_Start, _pSearch);
#else
			return _Path.f_Find(_Start, _pSearch);
#endif
		}

		bool fg_GitPathContains(CStr const &_Path, ch8 const *_pSearch)
		{
			return fg_FindGitPath(_Path, _pSearch) >= 0;
		}
	}

	CStr fg_GetGitRoot(CStr const &_Directory)
	{
		CStr CurrentDirectory = _Directory;
		while (!CurrentDirectory.f_IsEmpty())
		{
			if (CFile::fs_FileExists(CurrentDirectory + "/.git", EFileAttrib_Directory) || CFile::fs_FileExists(CurrentDirectory + "/.git", EFileAttrib_File))
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
				CBuildSystem::fs_ThrowError(_Position, "Unsupported git directory. Expected 'gitdir: ' in '{}'"_f << GitDirectory);
			GitDirectory = CFile::fs_GetExpandedPath(FileContents.f_Extract(8), _GitRoot);
		}
		if (!CFile::fs_FileExists(GitDirectory, EFileAttrib_Directory))
			CBuildSystem::fs_ThrowError(_Position, "Missing git directory for root '{}': {}"_f << _GitRoot << GitDirectory);
		return GitDirectory;
	}

	CStr fg_GetGitCommonDir(CStr const &_DataDir, CFilePosition const &_Position)
	{
		CStr CommonDirFile = _DataDir + "/commondir";
		if (CFile::fs_FileExists(CommonDirFile))
		{
			CStr CommonDirRelative = CFile::fs_ReadStringFromFile(CommonDirFile, true).f_TrimRight("\n");
			return CFile::fs_GetExpandedPath(CommonDirRelative, _DataDir);
		}
		return _DataDir;
	}

	CRepositoryDynamicInfo fg_GetRepositoryDynamicInfo(CRepository const &_Repo)
	{
		auto DataDir = fg_GetGitDataDir(_Repo.m_Location, _Repo.m_Position);
		return
			{
				.m_DataDir = DataDir
				, .m_CommonDir = fg_GetGitCommonDir(DataDir, _Repo.m_Position)
			}
		;
	}

	CStr fsg_ResolveRefHash(CStr const &_CommonDir, CStr const &_Ref)
	{
		{
			CStr RefFile = "{}/{}"_f << _CommonDir << _Ref;
			if (CFile::fs_FileExists(RefFile))
				return CFile::fs_ReadStringFromFile(RefFile, true).f_TrimRight("\n");
		}

		CStr PackedRefFile = "{}/packed-refs"_f << _CommonDir;
		if (CFile::fs_FileExists(PackedRefFile))
		{
			CStr PackedRefs = CFile::fs_ReadStringFromFile(PackedRefFile, true).f_TrimRight("\n");
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
				if (Ref == _Ref)
					return CommitHash;
			}
		}

		return {};
	}

	CStr fg_GetGitHeadHash(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo)
	{
		CStr HeadRef = CFile::fs_ReadStringFromFile(_DynamicInfo.m_DataDir + "/HEAD", true).f_TrimRight("\n");
		if (HeadRef.f_StartsWith("ref: "))
		{
			HeadRef = HeadRef.f_Extract(5);
			CStr Hash = fsg_ResolveRefHash(_DynamicInfo.m_CommonDir, HeadRef);
			if (Hash)
				return Hash;
			CBuildSystem::fs_ThrowError(_Repo.m_Position, "Hash for {} was not found in {}"_f << HeadRef << _DynamicInfo.m_CommonDir);
		}
		else
			return HeadRef;
	}

	CGitConfig fg_GetGitConfig(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, bool _bIsWorktree)
	{
		CGitConfig GitConfig;

		// Read shared config for remotes/LFS (these are shared between worktrees)
		{
			CStr SharedConfigFile = _DynamicInfo.m_CommonDir + "/config";
			if (CFile::fs_FileExists(SharedConfigFile))
			{
				CStr Config = CFile::fs_ReadStringFromFile(SharedConfigFile, true);
				auto ConfigContents = CGitConfigParser::fs_Parse(Config);

				if (auto *pValue = ConfigContents.f_GetValue("extensions", "worktreeconfig"))
					GitConfig.m_bWorktreeConfig = CGitConfigParser::fs_ToBoolean(*pValue);

				// For non-worktrees, also read user settings from shared config
				if (!_bIsWorktree)
				{
					if (auto *pValue = ConfigContents.f_GetValue("user", "name"))
						GitConfig.m_UserName = *pValue;

					if (auto *pValue = ConfigContents.f_GetValue("user", "email"))
						GitConfig.m_UserEmail = *pValue;
				}

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
			}
		}

		// For worktrees, read per-worktree config for user settings
		if (_bIsWorktree)
		{
			CStr WorktreeConfigFile = _DynamicInfo.m_DataDir + "/config.worktree";
			if (CFile::fs_FileExists(WorktreeConfigFile))
			{
				CStr Config = CFile::fs_ReadStringFromFile(WorktreeConfigFile, true);
				auto ConfigContents = CGitConfigParser::fs_Parse(Config);

				if (auto *pValue = ConfigContents.f_GetValue("user", "name"))
					GitConfig.m_UserName = *pValue;

				if (auto *pValue = ConfigContents.f_GetValue("user", "email"))
					GitConfig.m_UserEmail = *pValue;
			}
		}

		return GitConfig;
	}

	bool fg_IsSubmodule(CStr const &_DataDir)
	{
		CStr DataDir = CFile::fs_GetExpandedPath(_DataDir, false);

		// Main checkout: submodule data dir is <main>/.git/modules/<name>
		if (fg_GitPathContains(DataDir, "/.git/modules/"))
			return true;

		// Linked worktree: submodule data dir is <main>/.git/worktrees/<wt>/modules/<name>
		auto iWorktreePos = fg_FindGitPath(DataDir, "/.git/worktrees/");
		if (iWorktreePos >= 0)
			return fg_FindGitPath(DataDir, iWorktreePos, "/modules/") >= 0;

		return false;
	}

	bool fg_IsWorktree(CStr const &_DataDir)
	{
		CStr DataDir = CFile::fs_GetExpandedPath(_DataDir, false);

		// Worktrees have data dir pointing to <main>/.git/worktrees/<name>
		// Submodules have data dir pointing to <main>/.git/modules/<name>
		// Submodules inside a linked worktree have data dir
		// <main>/.git/worktrees/<wt>/modules/<name> — exclude those.
		auto iWorktreePos = fg_FindGitPath(DataDir, "/.git/worktrees/");
		if (iWorktreePos < 0)
			return false;
		return fg_FindGitPath(DataDir, iWorktreePos, "/modules/") < 0;
	}

	bool fg_AreGitPathsSame(CStr const &_PathA, CStr const &_PathB)
	{
		if (_PathA.f_IsEmpty() || _PathB.f_IsEmpty())
			return _PathA.f_IsEmpty() && _PathB.f_IsEmpty();

		CStr PathA = CFile::fs_GetExpandedPath(_PathA, false);
		CStr PathB = CFile::fs_GetExpandedPath(_PathB, false);
		return CFile::fs_MakePathRelative(PathA, PathB).f_IsEmpty();
	}

	TCUnsafeFuture<TCVector<CStr>> fg_ListWorktreePaths(CGitLaunches &_Launches, CStr const &_RepoDir)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		auto Result = co_await _Launches.f_Launch(_RepoDir, {"worktree", "list", "--porcelain"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

		TCVector<CStr> Paths;
		if (Result.m_ExitCode != 0)
			co_return fg_Move(Paths);

		for (auto &Line : Result.f_GetStdOut().f_SplitLine<true>())
		{
			if (Line.f_StartsWith("worktree "))
				Paths.f_Insert(CFile::fs_GetExpandedPath(Line.f_Extract(9), false));
		}

		co_return fg_Move(Paths);
	}

	TCUnsafeFuture<CStr> fg_FindSubRepoInWorktrees(CGitLaunches &_Launches, CStr const &_BaseDir, CStr const &_RelativeRepoPath)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		auto WorktreePaths = co_await fg_ListWorktreePaths(_Launches, _BaseDir);

		for (auto &WtRoot : WorktreePaths)
		{
			// Skip the main working tree itself
			if (fg_AreGitPathsSame(WtRoot, _BaseDir))
				continue;

			CStr SubRepoPath = WtRoot / _RelativeRepoPath;
			// This helper is only for finding a sibling standalone repository that can be
			// transferred back into the main working tree by moving its real .git directory.
			// Do not treat a .git file as a match here: that shape is already a git worktree,
			// and the transfer path below expects SubRepoPath/.git to be an actual directory.
			if (CFile::fs_FileExists(SubRepoPath + "/.git", EFileAttrib_Directory))
				co_return SubRepoPath;
		}

		co_return CStr{};
	}

	TCUnsafeFuture<void> fg_TransferGitDirMainToWorktree(CGitLaunches &_Launches, CStr const &_MainSubRepoDir, CStr const &_TargetWorktreeSubRepoDir)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CStr MainGitDir = _MainSubRepoDir + "/.git";
		CStr TargetGitFile = _TargetWorktreeSubRepoDir + "/.git";

		CStr TargetGitFileContents = CFile::fs_ReadStringFromFile(TargetGitFile, true).f_TrimRight("\n");
		if (!TargetGitFileContents.f_StartsWith("gitdir: "))
			DMibError("Expected worktree .git file at: {}"_f << TargetGitFile);

		CStr TargetGitDataDir = CFile::fs_GetExpandedPath(TargetGitFileContents.f_Extract(8), _TargetWorktreeSubRepoDir);
		CStr WorktreeName = CFile::fs_GetFile(TargetGitDataDir);

		CStr WorktreeEntryDir = MainGitDir + "/worktrees/" + WorktreeName;

		CStr WorktreeHead = CFile::fs_ReadStringFromFile(WorktreeEntryDir + "/HEAD", true);
		CFile::fs_WriteStringToFile(MainGitDir + "/HEAD", WorktreeHead, false);

		CStr WorktreeIndexFile = WorktreeEntryDir + "/index";
		CStr MainIndexFile = MainGitDir + "/index";
		if (CFile::fs_FileExists(WorktreeIndexFile))
		{
			if (CFile::fs_FileExists(MainIndexFile))
				CFile::fs_DeleteFile(MainIndexFile);
			CFile::fs_RenameFile(WorktreeIndexFile, MainIndexFile);
		}
		else if (CFile::fs_FileExists(MainIndexFile))
			CFile::fs_DeleteFile(MainIndexFile);

		CStr WorktreeConfigFile = WorktreeEntryDir + "/config.worktree";
		if (CFile::fs_FileExists(WorktreeConfigFile))
		{
			CStr MainConfigFile = MainGitDir + "/config";
			CStr MainConfigContents;
			if (CFile::fs_FileExists(MainConfigFile))
				MainConfigContents = CFile::fs_ReadStringFromFile(MainConfigFile, true);

			CStr WorktreeConfigContents = CFile::fs_ReadStringFromFile(WorktreeConfigFile, true);
			if (!MainConfigContents.f_IsEmpty() && !MainConfigContents.f_EndsWith("\n"))
				MainConfigContents += "\n";
			if (!MainConfigContents.f_IsEmpty() && !WorktreeConfigContents.f_IsEmpty() && !WorktreeConfigContents.f_StartsWith("\n"))
				MainConfigContents += "\n";
			MainConfigContents += WorktreeConfigContents;

			CFile::fs_WriteStringToFile(MainConfigFile, MainConfigContents, false);
		}

		CFile::fs_DeleteDirectoryRecursive(WorktreeEntryDir, true);
		CFile::fs_DeleteFile(TargetGitFile);
		CFile::fs_RenameFile(MainGitDir, _TargetWorktreeSubRepoDir + "/.git");

		co_await _Launches.f_Launch
			(
				_TargetWorktreeSubRepoDir
				, {"worktree", "repair"}
				, {}
				, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode
			)
		;

		CFile::fs_DeleteDirectoryRecursive(_MainSubRepoDir, true);

		co_return {};
	}

	TCUnsafeFuture<void> fg_TransferGitDirWorktreeToMain(CGitLaunches &_Launches, CStr const &_WorktreeSubRepoDir, CStr const &_MainSubRepoDir, CStr const &_WorktreeName)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CStr WorktreeGitDir = _WorktreeSubRepoDir + "/.git";

		CStr CurrentHead = CFile::fs_ReadStringFromFile(WorktreeGitDir + "/HEAD", true);

		CFile::fs_CreateDirectory(_MainSubRepoDir);

		CStr MainGitDir = _MainSubRepoDir + "/.git";
		CFile::fs_RenameFile(WorktreeGitDir, MainGitDir);

		// Detach the main HEAD so the branch is free for the worktree entry
		auto RevParseResult = co_await _Launches.f_Launch(_MainSubRepoDir, {"rev-parse", "HEAD"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
		if (RevParseResult.m_ExitCode == 0)
			CFile::fs_WriteStringToFile(MainGitDir + "/HEAD", RevParseResult.f_GetStdOut().f_TrimRight("\n") + "\n", false);

		CStr WorktreesDir = MainGitDir + "/worktrees";
		if (!CFile::fs_FileExists(WorktreesDir, EFileAttrib_Directory))
			CFile::fs_CreateDirectory(WorktreesDir);

		CStr WorktreeEntryDir = "{}/{}"_f << WorktreesDir << _WorktreeName;
		if (CFile::fs_FileExists(WorktreeEntryDir, EFileAttrib_Directory))
		{
			// Find a unique name if the entry already exists
			for (umint i = 1; ; ++i)
			{
				WorktreeEntryDir = "{}/{}{}"_f << WorktreesDir << _WorktreeName << i;
				if (!CFile::fs_FileExists(WorktreeEntryDir, EFileAttrib_Directory))
					break;
			}
		}

		CFile::fs_CreateDirectory(WorktreeEntryDir);
		CFile::fs_WriteStringToFile(WorktreeEntryDir + "/gitdir", _WorktreeSubRepoDir + "/.git\n", false);
		CFile::fs_WriteStringToFile(WorktreeEntryDir + "/commondir", "../..\n", false);
		CFile::fs_WriteStringToFile(WorktreeEntryDir + "/HEAD", CurrentHead, false);

		CStr MainIndexFile = MainGitDir + "/index";
		if (CFile::fs_FileExists(MainIndexFile))
			CFile::fs_RenameFile(MainIndexFile, WorktreeEntryDir + "/index");

		CFile::fs_WriteStringToFile(_WorktreeSubRepoDir + "/.git", "gitdir: {}\n"_f << WorktreeEntryDir, false);

		co_await _Launches.f_Launch
			(
				_MainSubRepoDir
				, {"worktree", "repair"}
				, {}
				, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode
			)
		;

		co_return {};
	}

	TCFuture<bool> fg_RepoIsChanged
		(
			CGitLaunches _GitLaunches
			, CRepository _Repo
			, CRepositoryDynamicInfo _DynamicInfo
			, EFilterRepoFlag _Flags
			, bool _bIncludePull
			, CStr _MainRepoBranch
			, CStr _MainRepoDefaultBranch
		)
	{
		CStr GitDirectory = _DynamicInfo.m_DataDir;

		CStr HeadRef = CFile::fs_ReadStringFromFile(GitDirectory + "/HEAD", true).f_TrimRight("\n");
		CStr ExpectedBranch;
		if (HeadRef.f_StartsWith("ref: "))
		{
			ExpectedBranch = fg_GetExpectedBranch(_Repo, _MainRepoBranch, _MainRepoDefaultBranch);
			if (HeadRef != ("ref: refs/heads/{}"_f << ExpectedBranch).f_GetStr())
				co_return true;
		}
		else
			co_return true;

		CStr RepoName = _Repo.f_GetName();

		TCVector<CStr> Params = {"status", "-sb", "--porcelain"};

		if (_Flags & EFilterRepoFlag_OnlyTracked)
			Params.f_Insert("-uno");

		auto StdOut = (co_await _GitLaunches.f_Launch(_Repo, Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode)).f_GetStdOut().f_Trim();

		CStr OriginalStdOut = StdOut;
		CStr BranchLine = fg_GetStrLineSep(StdOut);

		if (BranchLine.f_StartsWith("## HEAD "))
			co_return true; // Detached head
		else if (BranchLine.f_StartsWith("## "))
		{
			if (BranchLine.f_Find(gc_ConstString_Symbol_Ellipsis.m_String) < 0)
			{
				// Branch has no upstream tracking - if on a non-default expected branch,
				// check if HEAD is in the history of origin/{default_branch}
				if (ExpectedBranch != _Repo.m_OriginProperties.m_DefaultBranch && StdOut.f_IsEmpty())
				{
					auto MergeBaseResult = co_await _GitLaunches.f_Launch
						(
							_Repo
							, {"merge-base", "--is-ancestor", ExpectedBranch, "origin/{}"_f << _Repo.m_OriginProperties.m_DefaultBranch}
							, {}
							, CProcessLaunchActor::ESimpleLaunchFlag_None
						)
					;
					if (MergeBaseResult.m_ExitCode == 0)
						co_return false; // HEAD is in history of default branch on remote, only check for local changes
				}
				co_return true; // Non-pushed branch
			}

			CStr LocalRef;
			CStr RemoteRef;
			CStr Changes;
			(CStr::CParse("## {}...{} [{}]") >> LocalRef >> RemoteRef >> Changes).f_Parse(BranchLine);

			if (_bIncludePull && !Changes.f_IsEmpty())
				co_return true; // Non-pushed or pulled changes
			else if (Changes.f_Find("ahead") >= 0)
				co_return true; // Non-pushed changes
		}

		if (!StdOut.f_IsEmpty()) // Local changes
			co_return true;

		// Check if HEAD is in the history of origin/{default_branch}.
		// Note: origin is always required and enforced by the repo config system, so if
		// origin/<default> is missing the repo is in an unexpected state and reporting it
		// as changed is the correct behavior — the user needs to take action.
		{
			auto MergeBaseResult = co_await _GitLaunches.f_Launch
				(
					_Repo
					, {"merge-base", "--is-ancestor", ExpectedBranch, "origin/{}"_f << _Repo.m_OriginProperties.m_DefaultBranch}
					, {}
					, CProcessLaunchActor::ESimpleLaunchFlag_None
				)
			;
			if (MergeBaseResult.m_ExitCode != 0)
				co_return true;
		}

		co_return false;
	}

	TCFuture<TCVector<CLocalFileChange>> fg_GetLocalFileChanges(CGitLaunches _GitLaunches, CRepository _Repo, bool _bIncludeUntracked)
	{
		TCVector<CStr> Params = {"status", "-s"};

		if (!_bIncludeUntracked)
			Params.f_Insert("-uno");

		auto Result = co_await _GitLaunches.f_Launch(_Repo, Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

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

		auto Result = co_await _GitLaunches.f_Launch(_Repo, Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

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
		auto Result = co_await _GitLaunches.f_Launch(_Repo, {"remote"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

		if (Result.m_ExitCode)
			co_return DMibErrorInstance(Result.f_GetErrorOut().f_Trim());

		TCVector<CStr> Remotes;

		for (auto &Line : Result.f_GetStdOut().f_SplitLine<true>())
			Remotes.f_Insert(Line);

		co_return fg_Move(Remotes);
	}

	TCFuture<TCMap<CStr, CRemote>> fg_GetPushRemotes(CGitLaunches _GitLaunches, CRepository _Repo, TCVector<CStr> _Remotes)
	{
		auto Result = co_await _GitLaunches.f_Launch(_Repo, {"remote"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

		if (Result.m_ExitCode)
			co_return DMibErrorInstance(Result.f_GetErrorOut().f_Trim());

		TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> UrlResults;

		for (auto &Remote : Result.f_GetStdOut().f_SplitLine<true>())
			_GitLaunches.f_Launch(_Repo, {"remote", "get-url", Remote, "--push"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None) > UrlResults[Remote];

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
		auto LaunchResult = co_await _GitLaunches.f_Launch(_Repo, {"log", "{}..{}"_f << _From << _To, "--oneline", "--"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

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
		auto LogResult = co_await _GitLaunches.f_Launch(_Repo, {"log", "{}..{}"_f << _From << _To, "--pretty=raw", "--"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
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

	bool fg_BranchExists(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Branch)
	{
		if (CFile::fs_FileExists("{}/refs/heads/{}"_f << _DynamicInfo.m_CommonDir << _Branch))
			return true;

		CStr PackedRefs;
		{
			CStr PackedRefFile = "{}/packed-refs"_f << _DynamicInfo.m_CommonDir;
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

	bool fg_RemoteBranchExists(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Branch)
	{
		return !fsg_ResolveRefHash(_DynamicInfo.m_CommonDir, "refs/remotes/origin/{}"_f << _Branch).f_IsEmpty();
	}

	CStr fg_GetBranchHash(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Branch)
	{
		return fsg_ResolveRefHash(_DynamicInfo.m_CommonDir, "refs/heads/{}"_f << _Branch);
	}

	CStr fg_GetRemoteBranchHash(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Remote, CStr const &_Branch)
	{
		return fsg_ResolveRefHash(_DynamicInfo.m_CommonDir, "refs/remotes/{}/{}"_f << _Remote << _Branch);
	}

	CStr fg_GetBranch(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo)
	{
		CStr HeadRef = CFile::fs_ReadStringFromFile(_DynamicInfo.m_DataDir + "/HEAD", true).f_TrimRight("\n");
		if (!HeadRef.f_StartsWith("ref: "))
			return {};

		CStr Branch;
		(CStr::CParse("ref: refs/heads/{}") >> Branch).f_Parse(HeadRef);
		return Branch;
	}

	void fg_DetectGitBranchInfo(CStr const &_DataDir, CStr const &_CommonDir, CStr &o_Branch, CStr &o_DefaultBranch)
	{
		CStr HeadRef = CFile::fs_ReadStringFromFile(_DataDir + "/HEAD", true).f_TrimRight("\n");
		if (HeadRef.f_StartsWith("ref: "))
			(CStr::CParse("ref: refs/heads/{}") >> o_Branch).f_Parse(HeadRef);

		// Determine default branch from origin/HEAD
		CStr OriginHeadFile = _CommonDir + "/refs/remotes/origin/HEAD";
		if (CFile::fs_FileExists(OriginHeadFile))
		{
			CStr OriginHeadRef = CFile::fs_ReadStringFromFile(OriginHeadFile, true).f_TrimRight("\n");
			if (OriginHeadRef.f_StartsWith("ref: "))
				(CStr::CParse("ref: refs/remotes/origin/{}") >> o_DefaultBranch).f_Parse(OriginHeadRef);
		}

		// Best-effort local fallback when origin/HEAD is unavailable. Git does not
		// store the authoritative default branch locally in the general case, so we
		// only recognize the common main/master layouts here and otherwise fall back
		// to the current branch as default branch.
		//
		// This fallback makes the current branch indistinguishable from the default
		// branch, which means fg_GetExpectedBranch() will not propagate it as a
		// feature branch and mib unbranch becomes a no-op. This is expected to only
		// happen during bootstrap of a new project that does not yet have an
		// upstream, and is intentionally left unresolved: once the remote exists
		// and origin/HEAD is populated the heuristic is no longer needed.
		if (o_DefaultBranch.f_IsEmpty())
		{
			bool bHasMain = !fsg_ResolveRefHash(_CommonDir, "refs/heads/main").f_IsEmpty();
			bool bHasMaster = !fsg_ResolveRefHash(_CommonDir, "refs/heads/master").f_IsEmpty();

			if (o_Branch == "main" && bHasMain)
				o_DefaultBranch = "main";
			else if (o_Branch == "master" && bHasMaster)
				o_DefaultBranch = "master";
			else if (bHasMain && !bHasMaster)
				o_DefaultBranch = "main";
			else if (bHasMaster && !bHasMain)
				o_DefaultBranch = "master";
			else
				o_DefaultBranch = o_Branch;
		}
	}

	CStr fg_GetExpectedBranch(CRepository const &_Repo, CStr const &_MainRepoBranch, CStr const &_MainRepoDefaultBranch)
	{
		if (_MainRepoBranch.f_IsEmpty())
			return _Repo.m_OriginProperties.m_DefaultBranch;

		if (_MainRepoBranch == _MainRepoDefaultBranch)
			return _Repo.m_OriginProperties.m_DefaultBranch;

		// If we can't determine the default branch, assume we're on a feature branch
		return _MainRepoBranch;
	}

	CStr fg_HandleRepositoryActionToString(EHandleRepositoryAction _Action, EOutputType &o_OutputType)
	{
		o_OutputType = EOutputType_Warning;
		switch (_Action)
		{
		case EHandleRepositoryAction_None:
			return "(No recommendation)";
		case EHandleRepositoryAction_Leave:
			return "(Leave as is)";
		case EHandleRepositoryAction_ManualResolve:
			o_OutputType = EOutputType_Error;
			return "(Resolve manually)";
		case EHandleRepositoryAction_Reset:
			return "reset";
		case EHandleRepositoryAction_Rebase:
			return "rebase";
		case EHandleRepositoryAction_Auto:
		default:
			return "internal error";
		}
	}

	CMainRepoBranchInfo fg_GetMainRepoBranchInfo(CStr const &_BaseDir, TCVector<TCMap<CStr, CReposLocation>> const &_ReposOrdered)
	{
		CMainRepoBranchInfo Info;

		for (auto &Repos : _ReposOrdered)
		{
			for (auto &RepoLocation : Repos)
			{
				for (auto &Repo : RepoLocation.m_Repositories)
				{
					if (fg_AreGitPathsSame(Repo.m_Location, _BaseDir))
					{
						auto DynamicInfo = fg_GetRepositoryDynamicInfo(Repo);
						Info.m_Branch = fg_GetBranch(Repo, DynamicInfo);
						Info.m_DefaultBranch = Repo.m_OriginProperties.m_DefaultBranch;
						return Info;
					}
				}
			}
		}

		if (CFile::fs_FileExists(_BaseDir + "/.git", EFileAttrib_Directory | EFileAttrib_File))
		{
			auto DataDir = fg_GetGitDataDir(_BaseDir, CFilePosition{});
			CStr CommonDir = fg_GetGitCommonDir(DataDir, CFilePosition{});
			fg_DetectGitBranchInfo(DataDir, CommonDir, Info.m_Branch, Info.m_DefaultBranch);
		}

		return Info;
	}

	CStr fg_GetRemoteHead(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Remote)
	{
		CStr File = "{}/refs/remotes/{}/HEAD"_f << _DynamicInfo.m_CommonDir << _Remote;

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

		CGitLaunches Launches{_BuildSystem.f_GetGitLaunchOptions("update-remotes"), "Fetching remotes" + _ExtraMessage};

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

					auto DynamicInfo = fg_GetRepositoryDynamicInfo(Repo);

					for (auto &Remote : Remotes)
					{
						auto &RemoteName = Remote.f_Name();
						(*pRemoteHeadBranches)[RemoteName].m_LocalBranch = fg_GetRemoteHead(Repo, DynamicInfo, RemoteName);

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
							+ Launches.f_Launch
							(
								Repo
								, {"for-each-ref", "--format=%(upstream:short)", "refs/heads/{}"_f << Repo.m_OriginProperties.m_DefaultBranch}
								, {}
								, CProcessLaunchActor::ESimpleLaunchFlag_None
							)
						).f_Wrap()
					;

					co_await _BuildSystem.f_CheckCancelled();

					co_await (fg_Move(FetchResult) | g_Unwrap);
					co_await ((co_await (fg_Move(RemoteHeadResults) | g_Unwrap)) | g_Unwrap);

					TCFutureVector<void> SetHeadResults;

					CStr ExpectedTracking = "origin/{}"_f << Repo.m_OriginProperties.m_DefaultBranch;
					CStr CurrentTracking;
					if (TrackingResult)
					{
						if (TrackingResult->m_ExitCode != 0)
							Launches.f_Output(EOutputType_Warning, Repo, "Failed to query default branch tracking branch: {}"_f << TrackingResult->f_GetCombinedOut().f_Trim());
						else
							CurrentTracking = TrackingResult->f_GetStdOut().f_Trim();
					}
					else
						Launches.f_Output(EOutputType_Warning, Repo, "Failed to query default branch tracking branch: {}"_f << TrackingResult.f_GetExceptionStr());

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

		CStr VersionStr = (co_await _Launches.f_Launch(CStr(""), {"--version"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode)).f_GetStdOut().f_Trim();

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
