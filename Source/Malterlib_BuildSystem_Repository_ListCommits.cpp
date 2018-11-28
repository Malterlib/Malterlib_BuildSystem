// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

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

		TCVector<CUStr> fg_LineBreak(CUStr const &_String, mint _Length)
		{
			if (_String.f_Trim().f_IsEmpty())
				return {""};

			// Combining Diacritical Marks (0300–036F), since version 1.0, with modifications in subsequent versions down to 4.1
			// Combining Diacritical Marks Extended (1AB0–1AFF), version 7.0
			// Combining Diacritical Marks Supplement (1DC0–1DFF), versions 4.1 to 5.2
			// Combining Diacritical Marks for Symbols (20D0–20FF), since version 1.0, with modifications in subsequent versions down to 5.1
			// Combining Half Marks (FE20–FE2F), versions 1.0, with modifications in subsequent versions down to 8.0

			ch32 const *pParse = _String;
			ch32 const *pParseStart = pParse;
			ch32 const *pLastWord = nullptr;

			mint Len = 0;
			mint MaxLen = _Length;

			TCVector<CUStr> Output;

			auto fOutputLine = [&](ch32 const *_pStart, mint _Len)
				{
					Output.f_Insert(CUStr{_pStart, _Len});
				}
			;

			while (*pParse)
			{
				ch32 Char = *pParse;
				++pParse;
				++Len;
				while
					(
						(Char >= 0x0300 && Char <= 0x036F)
						|| (Char >= 0x1AB0 && Char <= 0x1AFF)
						|| (Char >= 0x1DC0 && Char <= 0x1DFF)
						|| (Char >= 0x20D0 && Char <= 0x20FF)
						|| (Char >= 0xFE20 && Char <= 0xFE2F)
					)
				{
					Char = *pParse;
					if (!Char)
						break;
					++pParse;
				}
				if (Len == MaxLen)
				{
					if (pLastWord)
					{
						fOutputLine(pParseStart, pLastWord - pParseStart);
						pParseStart = pLastWord;
						while (fg_CharIsWhiteSpace(*pParseStart))
							++pParseStart;
						pParse = pParseStart;
					}
					else
					{
						fOutputLine(pParseStart, pParse - pParseStart);
						pParseStart = pParse;
					}

					pLastWord = nullptr;
					Len = 0;
				}
				if (fg_CharIsWhiteSpace(Char))
					pLastWord = pParse;
			}

			if (pParseStart != pParse)
				fOutputLine(pParseStart, pParse - pParseStart);

			return Output;
		}
	}
	
	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_ListCommits
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
		)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;
		
		TCSharedPointer<CFilteredRepos> pFilteredRepositories = fg_Construct(fg_GetFilteredRepos(_Filter, *this, mp_Data));
		auto &FilteredRepositories = *pFilteredRepositories;

		TCLinkedList<CRepository> AllRepos;
		TCMap<CStr, CRepository *> AllReposByRelativePath;
		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			for (auto *pRepo : Repos)
			{
				auto &Repo = AllRepos.f_Insert(*pRepo);
				CStr RelativePath = CFile::fs_MakePathRelative(pRepo->m_Location, mp_BaseDir);
				if (RelativePath == "")
					RelativePath = ".";

				AllReposByRelativePath[RelativePath] = &Repo;
			}
		}

		if (_Flags & ERepoListCommitsFlag_UpdateRemotes)
		{
			CGitLaunches Launches{mp_BaseDir, "Fetching remotes (Disable with --local) "};
			Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

			CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

			TCActorResultVector<void> Results;

			for (auto Repo : AllRepos)
			{
				TCContinuation<void> Continuation;
				Launches.f_Launch(Repo, {"fetch", "--all", "--prune", "-q"}, fg_LogAllFunctor()) > [=](TCAsyncResult<void> &&_Result)
					{
						Continuation.f_SetResult(_Result);
						Launches.f_RepoDone();
					}
				;

				Continuation.f_Dispatch() > Results.f_AddResult();
			}

			auto SyncResults = Results.f_GetResults().f_CallSync();
			for (auto &Result : SyncResults)
				Result.f_Access();
		}

		CGitLaunches Launches{mp_BaseDir, "Listing Commits"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		CStateHandler StateHandler{mp_BaseDir, mp_OutputDir};

		bool bColor = _Flags & ERepoListCommitsFlag_Color;
		bool bCompact = _Flags & ERepoListCommitsFlag_Compact;

		TCVector<TCAsyncResult<void>> LaunchResults;

		TCMap<CStr, CRepository *> RepositoryByLocation;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCActorResultVector<void> Results;
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

		pState->m_StartCommits[mp_BaseDir] = _From;
		pState->m_EndCommits[mp_BaseDir] = _To;

		TCContinuation<> ResolveContinuation;

		auto fReportError = [ResolveContinuation](CStr const &_Error)
			{
				if (!ResolveContinuation.f_IsSet())
					ResolveContinuation.f_SetException(DErrorInstance(_Error));
			}
		;

		auto fResolveCommits = [=, pFilteredRepositories = pFilteredRepositories](auto &&_fResolveCommits) -> void
			{
				auto &State = *pState;
				bool bDoneSomething = false;
				bool bAllFinished = true;
				bool bResolved = true;
				static int iResolve = 0;
				++iResolve;
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
								auto pStartHash = pStartConfigFile->f_GetConfig(Repo, mp_BaseDir);
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
								auto pEndHash = pEndConfigFile->f_GetConfig(Repo, mp_BaseDir);
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

						Launches.f_Launch(Owner, {"show", "{}:{}"_f << *pStartCommit << RelativePath})
							+ Launches.f_Launch(Owner, {"show", "{}:{}"_f << *pEndCommit << RelativePath})
							> [=](TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult> &&_StartResult, TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult> &&_EndResult)
							{
								auto &State = *pState;

								State.m_PendingGitShow.f_Remove(ConfigFile);

								if (!_StartResult)
								{
									if (!ResolveContinuation.f_IsSet())
										ResolveContinuation.f_SetException(_StartResult);
									return;
								}

								if (!_EndResult)
								{
									if (!ResolveContinuation.f_IsSet())
										ResolveContinuation.f_SetException(_EndResult);
									return;
								}

								try
								{
									if (_StartResult->m_ExitCode == 0)
										State.m_StartConfigFiles[ConfigFile] = CStateHandler::fs_ParseConfigFile(_StartResult->f_GetStdOut(), ConfigFile);
									if (_EndResult->m_ExitCode == 0)
										State.m_EndConfigFiles[ConfigFile] = CStateHandler::fs_ParseConfigFile(_EndResult->f_GetStdOut(), ConfigFile);
								}
								catch (NException::CException const &_Exception)
								{
									fReportError("Excption parsing config files: {}"_f << _Exception);
									return;
								}

								_fResolveCommits(_fResolveCommits);
							}
						;
					}
				}

				if (bAllFinished)
				{
					if (!ResolveContinuation.f_IsSet())
						ResolveContinuation.f_SetResult();
				}
				else if (!bDoneSomething && State.m_PendingGitShow.f_IsEmpty())
				{
					fReportError("Failed to resolve dependency graph");
				}
			}
		;

		(
		 	g_Dispatch(Launches.m_pState->m_OutputActor) > [=]
			{
				fResolveCommits(fResolveCommits);
			}
		).f_CallSync();

		ResolveContinuation.f_Dispatch().f_CallSync();

		auto &State = *pState;

		TCActorResultMap<CStr, TCVector<CLogEntryFull>> CommitsResults;
		TCActorResultMap<CStr, TCVector<CLogEntryFull>> ReverseCommitsResults;

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
					TCContinuation<TCVector<CLogEntryFull>> Result;
					Result.f_SetResult(fg_Default());
					Result > CommitsResults.f_AddResult(Location);
				}

				{
					TCContinuation<TCVector<CLogEntryFull>> Result;
					Result.f_SetResult(fg_Default());
					Result > ReverseCommitsResults.f_AddResult(Location);
				}
				continue;
			}

			fg_GetLogEntriesFull(Launches, *pRepository, *pStartCommit, *pEndCommit) > CommitsResults.f_AddResult(Location);
			fg_GetLogEntriesFull(Launches, *pRepository, *pEndCommit, *pStartCommit) > ReverseCommitsResults.f_AddResult(Location);
		}

		auto LogEntriesPerRepo = CommitsResults.f_GetResults().f_CallSync();
		auto ReverseLogEntriesPerRepo = ReverseCommitsResults.f_GetResults().f_CallSync();

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
		Columns.f_Insert("Message");

		TCMap<CStr, uint32> MaxColumnWidth;
		MaxColumnWidth["Commit"] = 41;
		MaxColumnWidth["Author/Committer"] = 28;
		MaxColumnWidth["Author/Commit time"] = 20;
		for (auto &Wildcard : WildcardColumns)
			MaxColumnWidth[Wildcard.m_Name] = Wildcard.m_MaxWidth;
		MaxColumnWidth["Message"] = 60;

		auto fColor = [&](ch8 const *pColor)
			{
				if (!bColor)
					return "";
				return pColor;
			}
		;

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
			CStr RelativePath = CFile::fs_MakePathRelative(Repo, mp_BaseDir);
			if (!RelativePath.f_StartsWith("External/"))
				LogEntriesPerRepoSorted.f_Insert({Repo, fGetLogEntries(*LogEntriesResult, *ReverseLogEntriesPerRepo[Repo], RelativePath)});
		}

		for (auto &LogEntriesResult : LogEntriesPerRepo)
		{
			CStr Repo = LogEntriesPerRepo.fs_GetKey(LogEntriesResult);
			CStr RelativePath = CFile::fs_MakePathRelative(Repo, mp_BaseDir);
			if (RelativePath.f_StartsWith("External/"))
				LogEntriesPerRepoSorted.f_Insert({Repo, fGetLogEntries(*LogEntriesResult, *ReverseLogEntriesPerRepo[Repo], RelativePath)});
		}

		for (auto & [Repo, LogEntriesSource] : LogEntriesPerRepoSorted)
		{
			auto LogEntries = LogEntriesSource;

			if (NoCommits.f_FindEqual(Repo))
			{
				auto &DummyLogEntry = LogEntries.f_Insert();
				DummyLogEntry.m_Commit = "Could not resolve commits, config repo deleted?";
				DummyLogEntry.m_Message = "Error";
			}
			else if (NoStartCommits.f_FindEqual(Repo))
			{
				auto &DummyLogEntry = LogEntries.f_Insert();
				DummyLogEntry.m_Commit = "No start commit was found, new repo?";
				DummyLogEntry.m_Message = "Error";
			}
			else if (NoEndCommits.f_FindEqual(Repo))
			{
				auto &DummyLogEntry = LogEntries.f_Insert();
				DummyLogEntry.m_Commit = "No end commit was found, repo deleted?";
				DummyLogEntry.m_Message = "Error";
			}
			else if (LogEntries.f_IsEmpty())
				continue;

			CStr RelativePath = CFile::fs_MakePathRelative(Repo, mp_BaseDir);
			if (RelativePath == "")
				RelativePath = ".";
			else if (!Repo.f_StartsWith(mp_BaseDir))
			{
				auto pRepository = RepositoryByLocation.f_FindEqual(Repo);
				if (pRepository)
					RelativePath = "~" + (*pRepository)->m_Identity;
			}

			TCMap<CStr, zuint32> MaxLengths;

			for (auto &Column : Columns)
			{
				if (bCompact)
					MaxLengths[Column] = CAnsiEncoding::fs_RenderedStrLen(Column);
				else
					MaxLengths[Column] = MaxColumnWidth[Column];
			}

			auto fMeasureOutput = [&](CStr const &_Column, CStr const &_String, ch8 const *_pColor = "")
				{
					MaxLengths[_Column] = fg_Max(MaxLengths[_Column], CAnsiEncoding::fs_RenderedStrLen(_String));
				}
			;

			ch8 const *pBorderColor = DAnsiColor_256(240);
			ch8 const *pHeadingColor = DAnsiColor_Bold;
			ch8 const *pCommitterColor = DAnsiColor_256(244);

			CUStr ToOutput;
			auto fRealOutput = [&](CStr const &_Column, CStr const &_String, ch8 const *_pColor = "")
				{
					CUStr UnicodeString = _String;
					mint StringLen = UnicodeString.f_GetLen();
					mint RenderedLen = CAnsiEncoding::fs_RenderedStrLen(_String);
					mint NeededLen = MaxLengths[_Column] + (StringLen - RenderedLen);

					CUStr PaddedString = CUStr::CFormat(str_utf32("{sz*,sf ,a-}")) << UnicodeString << NeededLen;

					ch32 const *pParse = PaddedString;

					if (*pParse == '[')
					{
						while (*pParse && *pParse != ']')
							++pParse;
						if (*pParse == ']')
						{
							++pParse;
							mint SectionLen = pParse - PaddedString.f_GetStr();
							PaddedString = str_utf32("{2}{}{3}{4}{}"_f)
								<< PaddedString.f_Left(SectionLen)
								<< PaddedString.f_Extract(SectionLen)
								<< fColor(DAnsiColor_256(39))
								<< fColor(CColors::ms_Default)
								<< fColor(_pColor)
							;
						}
					}

					ToOutput += CUStr::CFormat(str_utf32("{1}|{2} {3}{}{2} ")) << PaddedString << fColor(pBorderColor) << fColor(CColors::ms_Default) << fColor(_pColor);
				}
			;

			auto fStripEmail = [](CStr const &_String) -> CStr
				{
					aint iChar = _String.f_FindCharReverse('<');

					if (iChar >= 1)
						return _String.f_Left(iChar - 1);

					return _String;
				}
			;

			enum EDivider
			{
				EDivider_Top
				, EDivider_Middle
				, EDivider_Bottom
			};

			auto fOutputDivider = [&](EDivider _Type)
				{
					ToOutput += _Prefix;
					ToOutput += fColor(pBorderColor);

					switch (_Type)
					{
					case EDivider_Top:
						{
							bool bFirst = true;
							for (auto &Column : Columns)
							{
								ToOutput += str_utf32("|¯{sz*,sf¯}¯"_f) << "" << MaxLengths[Column];

								bFirst = false;
							}
							ToOutput += "\\\n";
							break;
						}
					case EDivider_Middle:
						{
							bool bFirst = true;
							for (auto &Column : Columns)
							{
								ToOutput += str_utf32("|-{sz*,sf-}-"_f) << "" << MaxLengths[Column];

								bFirst = false;
							}
							ToOutput += "|\n";
							break;
						}
					case EDivider_Bottom:
						{
							bool bFirst = true;
							for (auto &Column : Columns)
							{
								if (bFirst)
									ToOutput += str_utf32("\\_{sz*,sf_}_"_f) << "" << MaxLengths[Column];
								else
									ToOutput += str_utf32("|_{sz*,sf_}_"_f) << "" << MaxLengths[Column];

								bFirst = false;
							}
							ToOutput += "/\n";
							break;
						}
					}

					ToOutput += fColor(CColors::ms_Default);
				}
			;

			struct COutputEntry
			{
				CStr m_String;
				ch8 const *m_pColor = nullptr;
			};

			auto fOutputAll = [&](auto &&_fOutput, bool _bReal) -> void
				{
					for (auto &LogEntry : LogEntries)
					{
						TCMap<CStr, TCVector<COutputEntry>> ColumnOutput;
						mint TallestColumn = 0;

						auto fAddColumnOutput = [&](CStr const &_Column, CStr const &_Value, ch8 const *_pColor = "")
							{
								auto &Output = ColumnOutput[_Column];
								bool bWasMultiple = false;
								auto Lines = fg_MergeLines(_Value.f_SplitLine());
								for (auto &LongLine : Lines)
								{
									if (bWasMultiple && !LongLine.f_Trim().f_IsEmpty())
										Output.f_Insert().m_pColor = _pColor;

									auto NewLines = fg_LineBreak(LongLine, MaxColumnWidth[_Column]);
									bWasMultiple = NewLines.f_GetLen() > 1;

									for (auto &Line : NewLines)
									{
										auto &Entry = Output.f_Insert();
										Entry.m_String = Line;
										Entry.m_pColor = _pColor;
									}
								}
								TallestColumn = fg_Max(TallestColumn, Output.f_GetLen());
							}
						;

						ch8 const *pCommitColor = DAnsiColor_256(11);
						ch8 const *pCommitColorWarning = DAnsiColor_256(11) DAnsiColor_Bold;
						if (LogEntry.m_bReverse)
						{
							pCommitColor = DAnsiColor_256(9);
							pCommitColorWarning = DAnsiColor_256(9) DAnsiColor_Bold;
						}

						if (LogEntry.m_AuthorDate.f_IsValid())
						{
							if (LogEntry.m_bReverse && !bColor)
								fAddColumnOutput("Commit", "-{}"_f << LogEntry.m_Commit, pCommitColor);
							else
								fAddColumnOutput("Commit", LogEntry.m_Commit, pCommitColor);
						}
						else
						{
							if (LogEntry.m_Message == "Error")
								fAddColumnOutput("Commit", LogEntry.m_Commit, CColors::ms_StatusError);
							else
								fAddColumnOutput("Commit", LogEntry.m_Commit, pCommitColorWarning);
						}

						fAddColumnOutput("Author/Committer", fStripEmail(LogEntry.m_Author));
						fAddColumnOutput("Author/Committer", fStripEmail(LogEntry.m_Committer), pCommitterColor);
						fAddColumnOutput("Author/Commit time", "{tc6}"_f << LogEntry.m_AuthorDate);
						fAddColumnOutput("Author/Commit time", "{tc6}"_f << LogEntry.m_CommitterDate, pCommitterColor);
						CStr Message = LogEntry.m_Message;

						for (auto &Wildcard : WildcardColumns)
						{
							auto &Column = Wildcard.m_Name;
							CStr NewMessage;
							for (auto &Line : Message.f_SplitLine())
							{
								ch8 const *pLine = Line.f_GetStr();
								if (NStr::fg_StrMatchWildcardParse(pLine, Wildcard.m_Wildcard.f_GetStr()) & NStr::EMatchWildcardResult_PatternExhausted)
								{
									if (Wildcard.m_bIncludeMatch)
										fAddColumnOutput(Column, Line);
									else
										fAddColumnOutput(Column, pLine);
									continue;
								}
								fg_AddStrSep(NewMessage, Line, "\n");
							}
							Message = NewMessage.f_Trim();
						}

						fAddColumnOutput("Message", Message);

						for (mint i = 0; i < TallestColumn; ++i)
						{
							if (_bReal)
								ToOutput += _Prefix;
							for (auto &Column : Columns)
							{
								auto &Output = ColumnOutput[Column];
								if (!Output.f_IsPosValid(i))
								{
									_fOutput(Column, "");
									continue;
								}

								_fOutput(Column, Output[i].m_String, Output[i].m_pColor);
							}
							if (_bReal)
								ToOutput += str_utf32("{}|{}\n"_f) << fColor(pBorderColor) << fColor(CColors::ms_Default);
						}
						if (_bReal && (&LogEntry != &LogEntries.f_GetLast()))
							fOutputDivider(EDivider_Middle);
					}
				}
			;
			fOutputAll(fMeasureOutput, false);

			fOutputDivider(EDivider_Top);

			ToOutput += _Prefix;
			for (auto &Column : Columns)
			{
				ToOutput += str_utf32("{2}|{3} {4}{sz*,sf ,a-}{3} "_f)
					<< Column
					<< MaxLengths[Column]
					<< fColor(pBorderColor)
					<< fColor(CColors::ms_Default)
					<< fColor(pHeadingColor)
				;
			}
			ToOutput += str_utf32("{}|{}\n"_f) << fColor(pBorderColor) << fColor(CColors::ms_Default);

			fOutputDivider(EDivider_Middle);

			fOutputAll(fRealOutput, true);

			fOutputDivider(EDivider_Bottom);
			ToOutput += str_utf32("{0}\n{0}\n"_f) << _Prefix;

			TCVector<TCTuple<CStr, CStr, ch8 const *>> HeadingLines;

			if (RelativePath == ".")
			{
				auto *pRepo = AllReposByRelativePath.f_FindEqual(RelativePath);
				CStr FromHash = "Unknown";
				CStr ToHash = "Unknown";

				if (pRepo)
				{
					auto [FromResult, ToResult] = (Launches.f_Launch(**pRepo, {"rev-parse", "{}^0"_f << _From}) + Launches.f_Launch(**pRepo, {"rev-parse", "{}^0"_f << _To})).f_CallSync();

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

				HeadingLines.f_Insert
					(
						{
							"From {a-,sj*,sf }  {}"_f << _From << MaxLen << FromHash
							, "From {3}{a-,sj*,sf }  {4}{}"_f << _From << MaxLen << FromHash << fColor(CColors::ms_ToPush) << fColor(DAnsiColor_256(246))
							, CColors::ms_Default
						}
					)
				;
				HeadingLines.f_Insert
					(
						{
							"To   {a-,sj*,sf }  {}"_f << _To << MaxLen << ToHash
							, "To   {3}{a-,sj*,sf }  {4}{}"_f << _To << MaxLen << ToHash << fColor(CColors::ms_ToPush) << fColor(DAnsiColor_256(246))
							, CColors::ms_Default
						}
					)
				;
			}
			else
				HeadingLines.f_Insert({RelativePath, RelativePath, nullptr});

			for (auto & [Line, ColoredLine, pColor] : HeadingLines)
				fMeasureOutput("**HEADING**", Line);

			mint ColumnWidth = MaxLengths["**HEADING**"];

			DConOutRaw(CStr{(str_utf32("{}{}/¯{sz*,sf¯}¯\\{}\n"_f) << _Prefix << fColor(pBorderColor) << "" << ColumnWidth << fColor(CColors::ms_Default)).f_GetStr()});
			for (auto & [Line, ColoredLine, pColor] : HeadingLines)
			{
				mint NeededLen = ColumnWidth + (ColoredLine.f_GetLen() - CAnsiEncoding::fs_RenderedStrLen(Line));
				CUStr PaddedString = CUStr::CFormat(str_utf32("{sz*,sf ,a-}")) << ColoredLine << NeededLen;

				DConOut2
					(
					 	"{}{2}|{3} {4}{a-,sf }{3} {2}|{3}\n"
					 	, _Prefix
					 	, PaddedString
					 	, fColor(pBorderColor)
					 	, fColor(CColors::ms_Default)
					 	, fColor(pColor ? pColor : pHeadingColor)
					)
				;
			}
			DConOutRaw(CStr{(str_utf32("{}{3}|{4} {sz*,sf } {3}|{4}\n"_f) << _Prefix << "" << ColumnWidth << fColor(pBorderColor) << fColor(CColors::ms_Default)).f_GetStr()});
			DConOutRaw(ToOutput);

		}
		
		return ERetry_None;
	}
}
