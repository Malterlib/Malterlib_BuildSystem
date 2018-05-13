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

			for (auto &Line : PackedRefs.f_SplitLine())
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

	TCMap<CStr, CStr> fg_GetGitRemotes(CStr const &_GitRoot, CFilePosition const &_Position)
	{
		CStr GitDirectory = fg_GetGitDataDir(_GitRoot, _Position);

		CStr Config = CFile::fs_ReadStringFromFile(GitDirectory + "/config", true);

		TCMap<CStr, CStr> Remotes;

		auto pParse = Config.f_GetStr();
		CStr LastRemote;
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
			else if (fg_StrStartsWith(pParse, "url =") && !LastRemote.f_IsEmpty())
			{
				pParse += 5;
				fg_ParseWhiteSpace(pParse);
				auto pStart = pParse;
				fg_ParseToEndOfLine(pParse);
				CStr URL(pStart, pParse - pStart);
				Remotes[LastRemote] = URL;
			}
			else if (*pParse == '[')
				LastRemote.f_Clear();
			fg_ParseToEndOfLine(pParse);
			fg_ParseEndOfLine(pParse);
		}

		return Remotes;
	}

	bool fg_IsSubmodule(CStr const &_GitRoot)
	{
		CStr GitDirectory = _GitRoot + "/.git";
		return CFile::fs_FileExists(GitDirectory, EFileAttrib_File);
	}

	TCContinuation<bool> fg_RepoIsChanged(CGitLaunches const &_GitLaunches, CRepository const &_Repo)
	{
		CStr GitDirectory = fg_GetGitDataDir(_Repo.m_Location, _Repo.m_Position);

		CStr HeadRef = CFile::fs_ReadStringFromFile(GitDirectory + "/HEAD", true).f_TrimRight("\n");
		if (HeadRef.f_StartsWith("ref: "))
		{
			if (HeadRef != ("ref: refs/heads/{}"_f << _Repo.m_DefaultBranch).f_GetStr())
				return fg_Explicit(true);
		}
		else
			return fg_Explicit(true);

		TCContinuation<bool> Continuation;

		_GitLaunches.f_Launch(_Repo, {"status", "-s"}) > Continuation / [Continuation](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				Continuation.f_SetResult(!_Result.m_Output.f_IsEmpty());
			}
		;

		return Continuation;
	}

	TCContinuation<TCVector<CLocalFileChange>> fg_GetLocalFileChanges(CGitLaunches const &_GitLaunches, CRepository const &_Repo, bool _bIncludeUntracked)
	{
		TCContinuation<TCVector<CLocalFileChange>> Continuation;

		TCVector<CStr> Params = {"status", "-s"};

		if (!_bIncludeUntracked)
			Params.f_Insert("-uno");

		_GitLaunches.f_Launch(_Repo, Params) > Continuation / [Continuation](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					Continuation.f_SetException(DMibErrorInstance(_Result.f_GetErrorOut()));
					return;
				}

				TCVector<CLocalFileChange> Changes;

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine())
				{
					auto &Change = Changes.f_Insert();
					Change.m_ChangeType = Line.f_Left(2).f_Trim();
					if (Change.m_ChangeType == "??")
						Change.m_ChangeType = "?";
					Change.m_File = Line.f_Extract(3);
				}

				Continuation.f_SetResult(fg_Move(Changes));
			}
		;

		return Continuation;
	}

	TCContinuation<CGitBranches> fg_GetBranches(CGitLaunches const &_GitLaunches, CRepository const &_Repo, bool _bRemote)
	{
		TCContinuation<CGitBranches> Continuation;

		TCVector<CStr> Params = {"branch"};
		if (_bRemote)
			Params.f_Insert("-r");

		_GitLaunches.f_Launch(_Repo, Params) > Continuation / [Continuation](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					Continuation.f_SetException(DMibErrorInstance(_Result.f_GetErrorOut()));
					return;
				}

				CGitBranches GitBranches;

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine())
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

				Continuation.f_SetResult(fg_Move(GitBranches));
			}
		;

		return Continuation;
	}

	TCContinuation<TCVector<CStr>> fg_GetRemotes(CGitLaunches const &_GitLaunches, CRepository const &_Repo)
	{
		TCContinuation<TCVector<CStr>> Continuation;

		_GitLaunches.f_Launch(_Repo, {"remote"}) > Continuation / [Continuation](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					Continuation.f_SetException(DMibErrorInstance(_Result.f_GetErrorOut()));
					return;
				}

				TCVector<CStr> Remotes;

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine())
				{
					if (Line.f_IsEmpty())
						continue;

					Remotes.f_Insert(Line);
				}

				Continuation.f_SetResult(fg_Move(Remotes));
			}
		;

		return Continuation;
	}

	TCContinuation<TCVector<CLogEntry>> fg_GetLogEntries(CGitLaunches const &_GitLaunches, CRepository const &_Repo, CStr const &_From, CStr const &_To, bool _bReportBadRevision)
	{
		TCContinuation<TCVector<CLogEntry>> Continuation;

		_GitLaunches.f_Launch(_Repo, {"log", "{}..{}"_f << _From << _To, "--oneline", "--"})
			> Continuation / [Continuation, _bReportBadRevision](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					CStr Output = _Result.f_GetCombinedOut().f_Trim();
					if (!_bReportBadRevision && Output.f_StartsWith("fatal: bad revision "))
					{
						TCVector<CLogEntry> LogEntries;
						auto &DummyEntry = LogEntries.f_Insert();
						DummyEntry.m_Description = Output;
						Continuation.f_SetResult(fg_Move(LogEntries));
						return;
					}
					if (Output.f_IsEmpty())
						Output = "Error status from git: {}"_f << _Result.m_ExitCode;
					Continuation.f_SetException(DMibErrorInstance(Output));
					return;
				}

				TCVector<CLogEntry> LogEntries;

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine())
				{
					if (Line.f_IsEmpty())
						continue;

					auto &LogEntry = LogEntries.f_Insert();
					LogEntry.m_Description = Line;
					LogEntry.m_Hash = fg_GetStrSep(LogEntry.m_Description, " ");
				}

				Continuation.f_SetResult(fg_Move(LogEntries));
			}
		;

		return Continuation;
	}

	TCContinuation<TCVector<CLogEntryFull>> fg_GetLogEntriesFull(CGitLaunches const &_GitLaunches, CRepository const &_Repo, CStr const &_From, CStr const &_To)
	{
		TCContinuation<TCVector<CLogEntryFull>> Continuation;

		_GitLaunches.f_Launch(_Repo, {"log", "{}..{}"_f << _From << _To, "--pretty=raw", "--"})
			> Continuation / [Continuation](CProcessLaunchActor::CSimpleLaunchResult &&_Result)
			{
				if (_Result.m_ExitCode)
				{
					Continuation.f_SetException(DMibErrorInstance(_Result.f_GetErrorOut().f_Trim()));
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

				for (auto &Line : _Result.f_GetStdOut().f_SplitLine())
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

				Continuation.f_SetResult(fg_Move(LogEntries));
			}
		;

		return Continuation;
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

		for (auto &Line : PackedRefs.f_SplitLine())
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

	NStr::CStr fg_GetBranch(CRepository const &_Repo)
	{
		CStr GitDirectory = fg_GetGitDataDir(_Repo.m_Location, _Repo.m_Position);

		CStr HeadRef = CFile::fs_ReadStringFromFile(GitDirectory + "/HEAD", true).f_TrimRight("\n");
		if (!HeadRef.f_StartsWith("ref: "))
			return {};

		CStr Branch;
		(CStr::CParse("ref: refs/heads/{}") >> Branch).f_Parse(HeadRef);
		return Branch;
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
