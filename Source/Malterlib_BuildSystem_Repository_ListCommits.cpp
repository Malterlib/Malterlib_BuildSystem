// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
		mint fg_VisibleStrLen(CUStr const &_String)
		{
			// Combining Diacritical Marks (0300–036F), since version 1.0, with modifications in subsequent versions down to 4.1
			// Combining Diacritical Marks Extended (1AB0–1AFF), version 7.0
			// Combining Diacritical Marks Supplement (1DC0–1DFF), versions 4.1 to 5.2
			// Combining Diacritical Marks for Symbols (20D0–20FF), since version 1.0, with modifications in subsequent versions down to 5.1
			// Combining Half Marks (FE20–FE2F), versions 1.0, with modifications in subsequent versions down to 8.0

			ch32 const *pParse = _String;

			mint Len = 0;

			while (*pParse)
			{
				ch32 Char = *pParse;
				if
					(
					 	!
					 	(
						  	(Char >= 0x0300 && Char <= 0x036F)
						 	|| (Char >= 0x1AB0 && Char <= 0x1AFF)
						 	|| (Char >= 0x1DC0 && Char <= 0x1DFF)
						 	|| (Char >= 0x20D0 && Char <= 0x20FF)
						 	|| (Char >= 0xFE20 && Char <= 0xFE2F)
						)
					)
				{
					++Len;
				}
				++pParse;
			}

			return Len;
		}

		TCVector<CUStr> fg_LineBreak(CUStr const &_String, mint _Length)
		{
			// Combining Diacritical Marks (0300–036F), since version 1.0, with modifications in subsequent versions down to 4.1
			// Combining Diacritical Marks Extended (1AB0–1AFF), version 7.0
			// Combining Diacritical Marks Supplement (1DC0–1DFF), versions 4.1 to 5.2
			// Combining Diacritical Marks for Symbols (20D0–20FF), since version 1.0, with modifications in subsequent versions down to 5.1
			// Combining Half Marks (FE20–FE2F), versions 1.0, with modifications in subsequent versions down to 8.0

			ch32 const *pParse = _String;
			ch32 const *pParseStart = pParse;
			ch32 const *pLastWord = nullptr;

			mint Len = 0;

			TCVector<CUStr> Output;

			while (*pParse)
			{
				ch32 Char = *pParse;
				++Len;
				++pParse;
				while
					(
						(Char >= 0x0300 && Char <= 0x036F)
						|| (Char >= 0x1AB0 && Char <= 0x1AFF)
						|| (Char >= 0x1DC0 && Char <= 0x1DFF)
						|| (Char >= 0x20D0 && Char <= 0x20FF)
						|| (Char >= 0xFE20 && Char <= 0xFE2F)
					)
				{
					++pParse;
					Char = *pParse;
				}
				if (Len == _Length)
				{
					if (pLastWord)
					{
						Output.f_Insert(CUStr(pParseStart, pLastWord - pParseStart));
						pParseStart = pLastWord;
						while (fg_CharIsWhiteSpace(*pParseStart))
							++pParseStart;
						pParse = pParseStart;
					}
					else
					{
						Output.f_Insert(CUStr(pParseStart, pParse - pParseStart));
						pParseStart = pParse;
					}
					pLastWord = nullptr;
					Len = 0;
				}
				if (fg_CharIsWhiteSpace(Char))
					pLastWord = pParse;
			}
			if (pParseStart != pParse)
				Output.f_Insert(CUStr(pParseStart, pParse - pParseStart));

			return Output;
		}
	}
	
	void CBuildSystem::fp_Repository_ListCommits
		(
		 	CRepoFilter const &_Filter
		 	, CStr const &_From
		 	, CStr const &_To
		 	, ERepoListCommitsFlag _Flags
		 	, TCVector<CWildcardColumn> const &_WildcardColumns
		)
	{
		TCSharedPointer<CFilteredRepos> pFilteredRepositories = fg_Construct(fg_GetFilteredRepos(_Filter, *this, mp_Data));
		auto &FilteredRepositories = *pFilteredRepositories;

		TCVector<CRepository> AllRepos;
		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			for (auto *pRepo : Repos)
				AllRepos.f_Insert(*pRepo);
		}

		if (_Flags & ERepoListCommitsFlag_UpdateRemotes)
		{
			CGitLaunches Launches{mp_BaseDir, "Fetching remotes"};
			Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

			CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

			TCActorResultVector<void> Results;

			for (auto Repo : AllRepos)
			{
				TCContinuation<void> Continuation;
				Launches.f_Launch(Repo, {"fetch", "--all", "-q"}, fg_LogAllFunctor()) > [=](TCAsyncResult<void> &&_Result)
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

		CStateHandler StateHandler;

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

						auto *pOwner = RepositoryByLocation.f_FindLargestLessThanEqual(CFile::fs_GetPath(Repo.m_ConfigFile));
						if (!pOwner)
							continue;

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
							auto pStartConfigFile = State.m_StartConfigFiles.f_FindEqual(ConfigFile);
							if (!pStartConfigFile)
								continue;

							auto pEndConfigFile = State.m_EndConfigFiles.f_FindEqual(ConfigFile);
							if (!pEndConfigFile)
								continue;

							auto pStartHash = pStartConfigFile->m_Configs.f_FindEqual(Repo.m_Location);
							if (!pStartHash)
							{
								fReportError("No start config hash found for: {}"_f << Repo.m_Location);
								continue;
							}

							auto pEndHash = pEndConfigFile->m_Configs.f_FindEqual(Repo.m_Location);
							if (!pEndHash)
							{
								fReportError("No end config hash found for: {}"_f << Repo.m_Location);
								continue;
							}

							CStr RelativePath = CFile::fs_MakePathRelative(ConfigFile, Owner.m_Location);

							State.m_StartCommits[Repo.m_Location] = pStartHash->m_Hash;
							State.m_EndCommits[Repo.m_Location] = pEndHash->m_Hash;

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

								if (_StartResult->m_ExitCode)
								{
									fReportError("Get start config file failed: {}"_f << _StartResult->f_GetCombinedOut());
									return;
								}

								if (_EndResult->m_ExitCode)
								{
									fReportError("Get end config file failed: {}"_f << _EndResult->f_GetCombinedOut());
									return;
								}

								try
								{
									State.m_StartConfigFiles[ConfigFile] = CStateHandler::fs_ParseConfigFile(_StartResult->f_GetStdOut(), ConfigFile);
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

		fResolveCommits(fResolveCommits);

		ResolveContinuation.f_Dispatch().f_CallSync();

		auto &State = *pState;

		TCActorResultMap<CStr, TCVector<CLogEntryFull>> CommitsResults;

		for (auto &pRepository : RepositoryByLocation)
		{
			auto &Location = pRepository->m_Location;
			auto *pStartCommit = State.m_StartCommits.f_FindEqual(Location);
			auto *pEndCommit = State.m_EndCommits.f_FindEqual(Location);
			if (!pStartCommit)
			{
				DConOut("{}: Missing start commit\n", Location);
				continue;
			}
			if (!pEndCommit)
			{
				DConOut("{}: Missing end commit\n", Location);
				continue;
			}
			fg_GetLogEntriesFull(Launches, *pRepository, *pStartCommit, *pEndCommit) > CommitsResults.f_AddResult(Location);
		}

		auto LogEntriesPerRepo = CommitsResults.f_GetResults().f_CallSync();


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
		MaxColumnWidth["Card"] = 40;
		for (auto &Wildcard : WildcardColumns)
			MaxColumnWidth[Wildcard.m_Name] = Wildcard.m_MaxWidth;
		MaxColumnWidth["Message"] = 40;

		auto fColor = [&](ch8 const *pColor)
			{
				if (!bColor)
					return "";
				return pColor;
			}
		;

		for (auto &LogEntries : LogEntriesPerRepo)
		{
			CStr Repo = LogEntriesPerRepo.fs_GetKey(LogEntries);

			if (LogEntries->f_IsEmpty())
				continue;

			CStr RelativePath = CFile::fs_MakePathRelative(Repo, mp_BaseDir);
			if (RelativePath == "")
				RelativePath = ".";

			TCMap<CStr, zuint32> MaxLengths;

			for (auto &Column : Columns)
			{
				if (bCompact)
					MaxLengths[Column] = fg_VisibleStrLen(Column);
				else
					MaxLengths[Column] = MaxColumnWidth[Column];
			}

			auto fMeasureOutput = [&](CStr const &_Column, CUStr const &_String, ch8 const *_pColor = "")
				{
					MaxLengths[_Column] = fg_Max(MaxLengths[_Column], fg_VisibleStrLen(_String));
				}
			;

			ch8 const *pBorderColor = DColor_256(240);
			ch8 const *pHeadingColor = DColor_Bold;
			ch8 const *pCommitterColor = DColor_256(244);

			CUStr ToOutput;
			auto fRealOutput = [&](CStr const &_Column, CUStr const &_String, ch8 const *_pColor = "")
				{
					mint NeededLen = MaxLengths[_Column] + (_String.f_GetLen() - fg_VisibleStrLen(_String));
					ToOutput += CUStr::CFormat(str_utf32("{2}|{3} {4}{sz*,sf ,a-}{3} ")) << _String << NeededLen << fColor(pBorderColor) << fColor(CColors::mc_Default) << fColor(_pColor);
				}
			;

			auto fStripEmail = [](CStr const &_String)
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

					ToOutput += fColor(CColors::mc_Default);
				}
			;

			struct COutputEntry
			{
				CStr m_String;
				ch8 const *m_pColor = nullptr;
			};

			auto fOutputAll = [&](auto &&_fOutput, bool _bReal) -> void
				{
					for (auto &LogEntry : *LogEntries)
					{
						TCMap<CStr, TCVector<COutputEntry>> ColumnOutput;
						mint TallestColumn = 0;

						auto fAddColumnOutput = [&](CStr const &_Column, CStr const &_Value, ch8 const *_pColor = "")
							{
								auto &Output = ColumnOutput[_Column];
								for (auto &LongLine : _Value.f_SplitLine())
								{
									for (auto &Line : fg_LineBreak(LongLine, MaxColumnWidth[_Column]))
									{
										auto &Entry = Output.f_Insert();
										Entry.m_String = Line;
										Entry.m_pColor = _pColor;
									}
								}
								TallestColumn = fg_Max(TallestColumn, Output.f_GetLen());
							}
						;

						fAddColumnOutput("Commit", LogEntry.m_Commit, DColor_256(3));
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
								ToOutput += str_utf32("{}|{}\n"_f) << fColor(pBorderColor) << fColor(CColors::mc_Default);
						}
						if (_bReal && (&LogEntry != &(*LogEntries).f_GetLast()))
							fOutputDivider(EDivider_Middle);
					}
				}
			;
			fOutputAll(fMeasureOutput, false);

			fOutputDivider(EDivider_Top);

			for (auto &Column : Columns)
			{
				ToOutput += str_utf32("{2}|{3} {4}{sz*,sf ,a-}{3} "_f)
					<< Column
					<< MaxLengths[Column]
					<< fColor(pBorderColor)
					<< fColor(CColors::mc_Default)
					<< fColor(pHeadingColor)
				;
			}
			ToOutput += str_utf32("{}|{}\n"_f) << fColor(pBorderColor) << fColor(CColors::mc_Default);

			fOutputDivider(EDivider_Middle);

			fOutputAll(fRealOutput, true);

			fOutputDivider(EDivider_Bottom);
			ToOutput += "\n\n";

			DConOutRaw(CStr{(str_utf32("{}/¯{sz*,sf¯}¯\\{}\n"_f) << fColor(pBorderColor) << "" << RelativePath.f_GetLen() << fColor(CColors::mc_Default)).f_GetStr()});
			DConOut2("{1}|{2} {3}{}{2} {1}|{2}\n", RelativePath, fColor(pBorderColor), fColor(CColors::mc_Default), fColor(pHeadingColor));
			DConOutRaw(CStr{(str_utf32("{2}|{3} {sz*,sf } {2}|{3}\n"_f) << "" << RelativePath.f_GetLen() << fColor(pBorderColor) << fColor(CColors::mc_Default)).f_GetStr()});
			DConOutRaw(ToOutput);
		}
	}
}
