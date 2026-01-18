// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/CommandLine/AnsiEncodingParse>
#include <Mib/CommandLine/TableRenderer>
#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
		TCVector<CUStr> fg_MergeLines(TCVector<CUStr> const &_Lines)
		{
			TCVector<CUStr> Lines;

			bool bWasEmpty = false;
			for (auto &Line : _Lines)
			{
				if (Lines.f_IsEmpty() || Line.f_Trim().f_IsEmpty() || fg_CharUpperCase(Line[0]) == Line[0] || bWasEmpty)
				{
					bWasEmpty = Line.f_Trim().f_IsEmpty();
					Lines.f_Insert(Line);
					continue;
				}

				Lines.f_GetLast() += " ";
				Lines.f_GetLast() += Line;
			}

			return Lines;
		}
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_ListCommits
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, CStr const &_From
			, CStr const &_To
			, ERepoListCommitsFlag _Flags
			, TCVector<CWildcardColumn> const &_WildcardColumns
			, CStr const &_Prefix
			, uint32 _MaxCommitsMainRepo
			, uint32 _MaxCommits
			, uint32 _MaxMessageWidth
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		TCSet<CStr> ShowRepos;
		{
			auto Repos = (co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None)).f_GetAllRepos();
			for (auto Repo : Repos)
				ShowRepos[fg_Get<0>(Repo).m_Location];
		}

		TCSharedPointer<CFilteredRepos> pFilteredRepositories = fg_Construct(co_await fg_GetFilteredRepos(CRepoFilter(), *this, mp_Data, EGetRepoFlag::mc_None));
		auto &FilteredRepositories = *pFilteredRepositories;

		TCLinkedList<CRepository> AllRepos;
		TCMap<CStr, CRepository *> AllReposByRelativePath;
		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			for (auto *pRepo : Repos)
			{
				auto &Repo = AllRepos.f_Insert(*pRepo);
				CStr RelativePath = CFile::fs_MakePathRelative(pRepo->m_Location, f_GetBaseDir());
				if (RelativePath == "")
					RelativePath = ".";

				AllReposByRelativePath[RelativePath] = &Repo;
			}
		}

		if (_Flags & ERepoListCommitsFlag_UpdateRemotes)
			co_await fg_UpdateRemotes(*this, FilteredRepositories, " (Disable with --local) ");

		CGitLaunches Launches{f_GetBaseDir(), "Listing Commits", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CStateHandler StateHandler{f_GetBaseDir(), mp_OutputDir, mp_AnsiFlags, mp_fOutputConsole};

		CColors Colors(mp_AnsiFlags);

		TCVector<TCAsyncResult<void>> LaunchResults;

		TCMap<CStr, CRepository *> RepositoryByLocation;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCFutureVector<void> Results;
			for (auto *pRepo : Repos)
				RepositoryByLocation[pRepo->m_Location] = pRepo;
		}

		struct CState
		{
			TCMap<CStr, CStr> m_StartCommits;
			TCMap<CStr, CStr> m_EndCommits;

			TCSet<CStr> m_StartedGitShow;
			TCSet<CStr> m_PendingGitShow;
			TCMap<CStr, CConfigFile> m_StartConfigFiles;
			TCMap<CStr, CConfigFile> m_EndConfigFiles;
		};

		TCSharedPointer<CState> pState = fg_Construct();

		pState->m_StartCommits[f_GetBaseDir()] = _From;
		pState->m_EndCommits[f_GetBaseDir()] = _To;

		while (true)
		{
			auto &State = *pState;
			bool bDoneSomething = false;
			bool bAllFinished = true;
			bool bResolved = true;
			while (bResolved)
			{
				bResolved = false;
				bAllFinished = true;
				for (auto &pRepository : RepositoryByLocation)
				{
					auto &Repo = *pRepository;

					if (State.m_StartCommits.f_FindEqual(Repo.m_Location))
					{
						DCheck(State.m_EndCommits.f_FindEqual(Repo.m_Location));
						continue;
					}
					else
						bAllFinished = false;

					CStr ConfigDirectory = CFile::fs_GetPath(Repo.m_ConfigFile);
					auto *pOwner = RepositoryByLocation.f_FindLargestLessThanEqual(ConfigDirectory);
					if (!pOwner)
						continue;

					auto &RepositoryPath = RepositoryByLocation.fs_GetKey(pOwner);

					if (!ConfigDirectory.f_StartsWith(RepositoryPath))
					{
						State.m_EndCommits[Repo.m_Location];
						State.m_StartCommits[Repo.m_Location];
						bResolved = true;
						bDoneSomething = true;
						continue;
					}

					auto &Owner = **pOwner;

					auto *pStartCommit = State.m_StartCommits.f_FindEqual(Owner.m_Location);
					if (!pStartCommit)
						continue;

					auto *pEndCommit = State.m_EndCommits.f_FindEqual(Owner.m_Location);
					if (!pEndCommit)
						continue;

					auto ConfigFile = Repo.m_ConfigFile;

					if (!State.m_StartedGitShow(ConfigFile).f_WasCreated())
					{
						if (State.m_PendingGitShow.f_FindEqual(ConfigFile))
							continue;

						auto pStartConfigFile = State.m_StartConfigFiles.f_FindEqual(ConfigFile);

						CStr RelativePath = CFile::fs_MakePathRelative(ConfigFile, Owner.m_Location);

						if (pStartConfigFile)
						{
							auto pStartHash = pStartConfigFile->f_GetConfig(Repo, f_GetBaseDir());
							if (pStartHash)
								State.m_StartCommits[Repo.m_Location] = pStartHash->m_Hash;
							else
								State.m_StartCommits[Repo.m_Location];
						}
						else
							State.m_StartCommits[Repo.m_Location];

						auto pEndConfigFile = State.m_EndConfigFiles.f_FindEqual(ConfigFile);
						if (pEndConfigFile)
						{
							auto pEndHash = pEndConfigFile->f_GetConfig(Repo, f_GetBaseDir());
							if (pEndHash)
								State.m_EndCommits[Repo.m_Location] = pEndHash->m_Hash;
							else
								State.m_EndCommits[Repo.m_Location];
						}
						else
							State.m_EndCommits[Repo.m_Location];

						bResolved = true;

						continue;
					}

					CStr RelativePath = CFile::fs_MakePathRelative(ConfigFile, Owner.m_Location);

					bDoneSomething = true;

					State.m_PendingGitShow[ConfigFile];

					auto [StartResult, EndResult] = co_await
						(
							Launches.f_Launch(Owner, {"show", "{}:{}"_f << *pStartCommit << RelativePath})
							+ Launches.f_Launch(Owner, {"show", "{}:{}"_f << *pEndCommit << RelativePath})
						)
					;
					auto &State = *pState;

					State.m_PendingGitShow.f_Remove(ConfigFile);

					{
						auto CaptureScope = co_await (g_CaptureExceptions % "Exception parsing config files");

						if (StartResult.m_ExitCode == 0)
							State.m_StartConfigFiles[ConfigFile] = CStateHandler::fs_ParseConfigFile(StartResult.f_GetStdOut(), ConfigFile);
						if (EndResult.m_ExitCode == 0)
							State.m_EndConfigFiles[ConfigFile] = CStateHandler::fs_ParseConfigFile(EndResult.f_GetStdOut(), ConfigFile);
					}
				}
			}

			if (bAllFinished)
				break;
			else if (!bDoneSomething && State.m_PendingGitShow.f_IsEmpty())
			{
				co_return DMibErrorInstance("Failed to resolve dependency graph");
			}
		}

		auto &State = *pState;

		TCFutureMap<CStr, TCVector<CLogEntryFull>> CommitsResults;
		TCFutureMap<CStr, TCVector<CLogEntryFull>> ReverseCommitsResults;

		TCSet<CStr> NoCommits;
		TCSet<CStr> NoStartCommits;
		TCSet<CStr> NoEndCommits;

		for (auto &pRepository : RepositoryByLocation)
		{
			auto &Location = pRepository->m_Location;
			auto *pStartCommit = State.m_StartCommits.f_FindEqual(Location);
			auto *pEndCommit = State.m_EndCommits.f_FindEqual(Location);
			bool bStartEmpty = !pStartCommit || pStartCommit->f_IsEmpty();
			bool bEndEmpty = !pEndCommit || pEndCommit->f_IsEmpty();

			if (bStartEmpty || bEndEmpty)
			{
				if (bStartEmpty && bEndEmpty)
					NoCommits[Location];
				else if (bStartEmpty)
					NoStartCommits[Location];
				else
					NoEndCommits[Location];

				{
					TCPromiseFuturePair<TCVector<CLogEntryFull>> Result;
					Result.m_Promise.f_SetResult(TCVector<CLogEntryFull>());
					fg_Move(Result.m_Future) > CommitsResults[Location];
				}

				{
					TCPromiseFuturePair<TCVector<CLogEntryFull>> Result;
					Result.m_Promise.f_SetResult(TCVector<CLogEntryFull>());
					fg_Move(Result.m_Future) > ReverseCommitsResults[Location];
				}
				continue;
			}

			fg_GetLogEntriesFull(Launches, *pRepository, *pStartCommit, *pEndCommit) > CommitsResults[Location];
			fg_GetLogEntriesFull(Launches, *pRepository, *pEndCommit, *pStartCommit) > ReverseCommitsResults[Location];
		}

		auto LogEntriesPerRepo = co_await fg_AllDone(CommitsResults);
		auto ReverseLogEntriesPerRepo = co_await fg_AllDone(ReverseCommitsResults);

		struct CWildcardColumn
		{
			CStr m_Name;
			CStr m_Wildcard;
			uint32 m_MaxWidth = 30;
			bool m_bIncludeMatch = true;
		};

		TCVector<CWildcardColumn> WildcardColumns;
		for (auto &Wildcard : _WildcardColumns)
		{
			CStr Name = Wildcard.m_Name;

			CStr ParsedName;

			uint32 MaxWidth = 30;
			aint nParsed;
			uint32 bIncludeMatch = true;

			(CStr::CParse("{}({},{})") >> ParsedName >> MaxWidth >> bIncludeMatch).f_Parse(Name, nParsed);

			if (nParsed != 3)
			{
				(CStr::CParse("{}({})") >> ParsedName >> MaxWidth).f_Parse(Name, nParsed);

				if (nParsed != 2)
					ParsedName = Name;
			}

			auto &NewColumn = WildcardColumns.f_Insert();
			NewColumn.m_Name = ParsedName;
			NewColumn.m_MaxWidth = MaxWidth;
			NewColumn.m_bIncludeMatch = bIncludeMatch;
			NewColumn.m_Wildcard = Wildcard.m_Wildcard;
		}

		TCVector<CStr> Columns;
		Columns.f_Insert("Commit");
		Columns.f_Insert("Author/Committer");
		Columns.f_Insert("Author/Commit time");
		for (auto &Wildcard : WildcardColumns)
			Columns.f_Insert(Wildcard.m_Name);
		Columns.f_Insert(gc_ConstString_Message.m_String);

		TCMap<CStr, uint32> MaxColumnWidth;
		MaxColumnWidth["Commit"] = 41;
		MaxColumnWidth["Author/Committer"] = 28;
		MaxColumnWidth["Author/Commit time"] = 20;
		for (auto &Wildcard : WildcardColumns)
			MaxColumnWidth[Wildcard.m_Name] = Wildcard.m_MaxWidth;
		if (_MaxMessageWidth > 0)
			MaxColumnWidth[gc_ConstString_Message.m_String] = _MaxMessageWidth;

		struct CLogEntry : public CLogEntryFull
		{
			bool m_bReverse = false;
		};

		auto fGetLogEntries = [&](TCVector<CLogEntryFull> _Entries, TCVector<CLogEntryFull> _ReverseEntries, CStr const &_RelativePath) -> TCVector<CLogEntry>
			{
				auto fFilterEntries = [](TCVector<CLogEntryFull> &o_Entries, mint _MaxEntries, CStr const &_Message)
					{
						if (o_Entries.f_GetLen() <= _MaxEntries)
							return;

						mint nDeleted = o_Entries.f_GetLen() - _MaxEntries;
						o_Entries.f_SetLen(_MaxEntries);
						auto &DummyLogEntry = o_Entries.f_Insert();
						DummyLogEntry.m_Commit = CStr::CFormat(_Message) << nDeleted << (nDeleted + _MaxEntries);
					}
				;

				if (_RelativePath.f_IsEmpty())
				{
					fFilterEntries(_Entries, _MaxCommitsMainRepo, "{} log entries were not shown. Specify --max-commits-main={} to see all.");
					fFilterEntries(_ReverseEntries, _MaxCommitsMainRepo, "{} reverse log entries were not shown. Specify --max-commits-main={} to see all.");
				}
				else
				{
					fFilterEntries(_Entries, _MaxCommits, "{} log entries were not shown. Specify --max-commits={} to see all.");
					fFilterEntries(_ReverseEntries, _MaxCommits, "{} reverse log entries were not shown. Specify --max-commits={} to see all.");
				}


				TCVector<CLogEntry> Entries;
				for (auto &Entry : _Entries)
					Entries.f_Insert({Entry, false});
				for (auto &Entry : _ReverseEntries)
					Entries.f_Insert({Entry, true});

				return Entries;
			}
		;

		TCVector<TCTuple<CStr, TCVector<CLogEntry>>> LogEntriesPerRepoSorted;
		for (auto &LogEntriesResult : LogEntriesPerRepo)
		{
			CStr Repo = LogEntriesPerRepo.fs_GetKey(LogEntriesResult);
			CStr RelativePath = CFile::fs_MakePathRelative(Repo, f_GetBaseDir());
			if (!RelativePath.f_StartsWith("External/"))
				LogEntriesPerRepoSorted.f_Insert({Repo, fGetLogEntries(LogEntriesResult, ReverseLogEntriesPerRepo[Repo], RelativePath)});
		}

		for (auto &LogEntriesResult : LogEntriesPerRepo)
		{
			CStr Repo = LogEntriesPerRepo.fs_GetKey(LogEntriesResult);
			CStr RelativePath = CFile::fs_MakePathRelative(Repo, f_GetBaseDir());
			if (RelativePath.f_StartsWith("External/"))
				LogEntriesPerRepoSorted.f_Insert({Repo, fGetLogEntries(LogEntriesResult, ReverseLogEntriesPerRepo[Repo], RelativePath)});
		}

		TCMap<NTime::CTime, CStr> ChangelogEntries;

		auto fColorSection = [&](CUStr const &_String, ch8 const *_pColor = "") -> CUStr
			{
				ch32 const *pParse = _String;

				if (*pParse == '[')
				{
					while (*pParse && *pParse != ']')
						++pParse;
					if (*pParse == ']')
					{
						++pParse;
						mint SectionLen = pParse - _String.f_GetStr();
						return str_utf32("{2}{}{3}{4}{}"_f)
							<< _String.f_Left(SectionLen)
							<< _String.f_Extract(SectionLen)
							<< Colors.f_Foreground256(39)
							<< Colors.f_Default()
							<< _pColor
						;
					}
				}
				return _String;
			}
		;

		TCVector<CTableRenderHelper> TableRenderers;

		for (auto & [Repo, LogEntriesSource] : LogEntriesPerRepoSorted)
		{
			auto LogEntries = LogEntriesSource;

			if (!ShowRepos.f_FindEqual(Repo))
				continue;

			if (NoCommits.f_FindEqual(Repo))
			{
				auto &DummyLogEntry = LogEntries.f_Insert();
				DummyLogEntry.m_Commit = "Could not resolve commits, config repo deleted?";
				DummyLogEntry.m_Message = gc_ConstString_Error;
			}
			else if (NoStartCommits.f_FindEqual(Repo))
			{
				auto &DummyLogEntry = LogEntries.f_Insert();
				DummyLogEntry.m_Commit = "No start commit was found, new repo?";
				DummyLogEntry.m_Message = gc_ConstString_Error;
			}
			else if (NoEndCommits.f_FindEqual(Repo))
			{
				auto &DummyLogEntry = LogEntries.f_Insert();
				DummyLogEntry.m_Commit = "No end commit was found, repo deleted?";
				DummyLogEntry.m_Message = gc_ConstString_Error;
			}
			else if (LogEntries.f_IsEmpty())
				continue;

			CStr RelativePath = CFile::fs_MakePathRelative(Repo, f_GetBaseDir());
			if (RelativePath == "")
				RelativePath = ".";
			else if (!Repo.f_StartsWith(f_GetBaseDir()))
			{
				auto pRepository = RepositoryByLocation.f_FindEqual(Repo);
				if (pRepository)
					RelativePath = "~" + (*pRepository)->m_Identity;
			}

			CTableRenderHelper &TableRenderer = TableRenderers.f_Insert(_pCommandLine->f_TableRenderer());
			TableRenderer.f_SetPrefix(_Prefix);
			{
				TCVector<CStr> Headings;
				TCVector<TCTuple<int32, uint32>> MaxColumnWidths;
				for (auto &Column : Columns)
				{
					mint iColumn = Headings.f_GetLen();
					Headings.f_Insert(Column);
					if (auto *pMaxWidth = MaxColumnWidth.f_FindEqual(Column))
						MaxColumnWidths.f_Insert({iColumn, *pMaxWidth});
				}

				TableRenderer.f_AddHeadingsVector(Headings);

				for (auto &Column : MaxColumnWidths)
					TableRenderer.f_SetMaxColumnWidth(fg_Get<0>(Column), fg_Get<1>(Column));
			}

			CStr CommitterColor = Colors.f_Foreground256(244);

			auto fStripEmail = [](CStr const &_String) -> CStr
				{
					aint iChar = _String.f_FindCharReverse('<');

					if (iChar >= 1)
						return _String.f_Left(iChar - 1);

					return _String;
				}
			;

			for (auto &LogEntry : LogEntries)
			{
				CStr CommitColor = Colors.f_Foreground256(11);
				CStr CommitColorWarning = Colors.f_Foreground256(11) + Colors.f_Bold();
				if (LogEntry.m_bReverse)
				{
					CommitColor = Colors.f_Foreground256(9);
					CommitColorWarning = Colors.f_Foreground256(9) + Colors.f_Bold();
				}

				TCVector<CStr> RowValues;

				if (LogEntry.m_AuthorDate.f_IsValid())
				{
					if (LogEntry.m_bReverse && !Colors.f_Color())
						RowValues.f_Insert("{1}-{}{2}"_f << LogEntry.m_Commit << CommitColor << Colors.f_Default());
					else
						RowValues.f_Insert("{1}{}{2}"_f << LogEntry.m_Commit << CommitColor << Colors.f_Default());
				}
				else
				{
					ChangelogEntries[LogEntry.m_AuthorDate] += "{}:\n{}\n"_f << Repo << LogEntry.m_Commit;

					if (LogEntry.m_Message == gc_ConstString_Error.m_String)
						RowValues.f_Insert("{1}{}{2}"_f << LogEntry.m_Commit << Colors.f_StatusError() << Colors.f_Default());
					else
						RowValues.f_Insert("{1}{}{2}"_f << LogEntry.m_Commit << CommitColorWarning << Colors.f_Default());
				}

				RowValues.f_Insert
					(
						"{}\n{2}{}{3}"_f
						<< fStripEmail(LogEntry.m_Author)
						<< fStripEmail(LogEntry.m_Committer)
						<< CommitterColor
						<< Colors.f_Default()
					)
				;
				RowValues.f_Insert
					(
						"{tc6}\n{2}{tc6}{3}"_f
						<< LogEntry.m_AuthorDate
						<< LogEntry.m_CommitterDate
						<< CommitterColor
						<< Colors.f_Default()
					)
				;

				auto fCleanupLines = [](CStr const &_Value)
					{
						return fg_MergeLines(_Value.f_SplitLine());
					}
				;

				CStr Message = LogEntry.m_Message;

				for (auto &Wildcard : WildcardColumns)
				{
					CStr NewMessage;
					TCVector<CStr> ColumnValue;
					for (auto &Line : Message.f_SplitLine())
					{
						ch8 const *pLine = Line.f_GetStr();
						if (NStr::fg_StrMatchWildcardParse(pLine, Wildcard.m_Wildcard.f_GetStr()) & NStr::EMatchWildcardResult_PatternExhausted)
						{
							if (Wildcard.m_bIncludeMatch)
								ColumnValue.f_Insert(fCleanupLines(Line));
							else
								ColumnValue.f_Insert(fCleanupLines(pLine));
							continue;
						}
						fg_AddStrSep(NewMessage, Line, "\n");
					}

					Message = NewMessage.f_Trim();

					RowValues.f_Insert(fColorSection(CStr::fs_Join(ColumnValue, "\n")));
				}

				Message = CStr::fs_Join(fCleanupLines(Message), "\n");

				ChangelogEntries[LogEntry.m_AuthorDate] += Message;

				RowValues.f_Insert(fColorSection(Message));

				TableRenderer.f_AddRowVector(RowValues);
			}

			if (RelativePath == ".")
			{
				auto *pRepo = AllReposByRelativePath.f_FindEqual(RelativePath);
				CStr FromHash = "Unknown";
				CStr ToHash = "Unknown";

				if (pRepo)
				{
					auto [FromResult, ToResult] = co_await
						(
							Launches.f_Launch(**pRepo, {"rev-parse", "{}^0"_f << _From})
							+ Launches.f_Launch(**pRepo, {"rev-parse", "{}^0"_f << _To})
						)
					;

					if (FromResult.m_ExitCode == 0)
						FromHash = FromResult.f_GetStdOut().f_Trim();
					else
						FromHash = CStr::fs_Join(FromResult.f_GetErrorOut().f_Trim().f_SplitLine(), " ").f_Replace("\t", " ");

					if (FromResult.m_ExitCode == 0)
						ToHash = ToResult.f_GetStdOut().f_Trim();
					else
						ToHash = CStr::fs_Join(ToResult.f_GetErrorOut().f_Trim().f_SplitLine(), " ").f_Replace("\t", " ");
				}

				mint MaxLen = fg_Max(_From.f_GetLen(), _To.f_GetLen());

				CStr Description;
				Description += "{5}From {3}{a-,sj*,sf }  {4}{}\n"_f << _From << MaxLen << FromHash << Colors.f_ToPush() << Colors.f_Foreground256(246) << Colors.f_Default();
				Description += "{5}To   {3}{a-,sj*,sf }  {4}{}"_f << _To << MaxLen << ToHash << Colors.f_ToPush() << Colors.f_Foreground256(246) << Colors.f_Default();

				TableRenderer.f_AddDescription(Description);
			}
			else
				TableRenderer.f_AddDescription("{1}{2}{}{1}"_f << RelativePath << Colors.f_Default() << Colors.f_Bold());
		}

		co_await fg_Move(DestroyLaunchs);

		if (_Flags & ERepoListCommitsFlag_Changelog)
		{
			for (auto &Entry : ChangelogEntries)
			{
				auto &Date = ChangelogEntries.fs_GetKey(Entry);
				auto Lines = Entry.f_SplitLine();
				CStr DateStr = "{tc6}"_f << Date;
				mint nLines = Lines.f_GetLen();
				CStr ToOutput = "{} "_f << DateStr;
				for (mint i = 0; i < nLines; ++i)
				{
					if (Lines[i] == "Copied from Perforce")
						break;

					if (i != 0)
					{
						ToOutput += "{sj*}"_f << "" << (DateStr.f_GetLen() + 1);
						ToOutput += "{}\n"_f << Lines[i];
					}
					else
						ToOutput += "{}\n"_f << fColorSection(Lines[i]);

					if (i != 0 && i == nLines - 1)
						ToOutput += "\n";
				}
				f_OutputConsole(ToOutput);
			}
		}
		else
		{
			if (!(_Flags & ERepoListCommitsFlag_Compact))
			{
				CTableRenderHelper TempRenderer(nullptr, CTableRenderHelper::EOption_None, EAnsiEncodingFlag_None, 0);
				for (auto &TableRenderer : TableRenderers)
					TempRenderer.f_MergeColumnWidths(TableRenderer);

				for (auto &TableRenderer : TableRenderers)
					TableRenderer.f_MergeColumnWidths(TempRenderer);
			}

			for (auto &TableRenderer : TableRenderers)
				TableRenderer.f_Output();
		}

		co_return ERetry_None;
	}
}
