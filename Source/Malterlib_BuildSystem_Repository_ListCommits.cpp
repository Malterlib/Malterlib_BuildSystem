// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"
#include "Malterlib_BuildSystem_Repository_PerforceHelpers.h"

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

		// Auto-detect from/to changelists when both are default.
		// Returns {from, to} where to="" means workspace.
		// Checks if workspace files differ from the latest submitted version:
		//   - If changes: from=latest CL, to="" (workspace)
		//   - If no changes: from=second-latest CL, to=latest CL
		TCFuture<TCTuple<CStr, CStr>> fg_PerforceAutoDetectChangelists(TCVector<CStr> _ConfigFilePaths, CStr _BaseDir)
		{
			auto BlockingActorCheckout = fg_BlockingActor();
			co_return co_await
				(
					g_Dispatch(BlockingActorCheckout) / [_ConfigFilePaths, _BaseDir]() -> TCTuple<CStr, CStr>
					{
						CPerforceClientThrow Client;
						auto DepotPaths = fg_PerforceConnectAndResolveDepotPaths(_ConfigFilePaths, _BaseDir, Client);

						// DepotPaths is aligned 1:1 with _ConfigFilePaths and may contain
						// empty entries for brand-new files; filter them out before asking
						// Perforce for revisions.
						TCVector<CStr> TrackedDepotPaths;
						for (auto &DepotPath : DepotPaths)
						{
							if (!DepotPath.f_IsEmpty())
								TrackedDepotPaths.f_Insert(DepotPath);
						}

						uint32 HighestCL = 0;
						uint32 SecondHighestCL = 0;
						if (!TrackedDepotPaths.f_IsEmpty())
						{
							auto Revisions = Client.f_GetFileRevisions(TrackedDepotPaths);
							for (auto &File : Revisions.m_Files)
							{
								for (auto &Rev : File.m_Revisions)
								{
									if (Rev.m_ChangeList > 0)
									{
										uint32 CL = (uint32)Rev.m_ChangeList;
										if (CL > HighestCL)
										{
											SecondHighestCL = HighestCL;
											HighestCL = CL;
										}
										else if (CL > SecondHighestCL && CL != HighestCL)
											SecondHighestCL = CL;
									}
								}
							}
						}

						// Check if any workspace file differs from the latest submitted
						// version. New files (empty depot path) always count as a change.
						bool bWorkspaceHasChanges = false;
						for (umint i = 0; i < _ConfigFilePaths.f_GetLen(); ++i)
						{
							if (DepotPaths[i].f_IsEmpty())
							{
								if (CFile::fs_FileExists(_ConfigFilePaths[i]))
								{
									bWorkspaceHasChanges = true;
									break;
								}
								continue;
							}

							CStr SubmittedContent;
							if (!Client.f_NoThrow().f_GetTextFileContents("{}@{}"_f << DepotPaths[i] << HighestCL, SubmittedContent))
								continue;

							CStr WorkspaceContent = CFile::fs_ReadStringFromFile(_ConfigFilePaths[i]);
							if (WorkspaceContent != SubmittedContent)
							{
								bWorkspaceHasChanges = true;
								break;
							}
						}

						if (bWorkspaceHasChanges)
							return {CStr::fs_ToStr(HighestCL), CStr()};
						else if (SecondHighestCL > 0)
							return {CStr::fs_ToStr(SecondHighestCL), CStr::fs_ToStr(HighestCL)};
						else
							return {CStr::fs_ToStr(HighestCL), CStr()};
					}
				)
			;
		}


		// Collect Perforce changelists into AllChangelists, deduplicating by CL ID.
		// Check for workspace files that differ from head but aren't opened in any changelist.
		bool fg_PerforceHasUnreconciledChanges(CPerforceClientThrow &_Client, TCVector<CStr> const &_ConfigFilePaths)
		{
			for (auto &ConfigFile : _ConfigFilePaths)
			{
				CStr DepotPath;
				bool bHasMapping = _Client.f_NoThrow().f_GetDepotPath(ConfigFile, DepotPath);

				// Files already opened in any pending CL surface through the
				// CL-based log collection above, so they're not "unreconciled".
				if (bHasMapping)
				{
					TCVector<CStr> Opened;
					_Client.f_NoThrow().f_GetOpened(DepotPath, CStr(), Opened);
					if (!Opened.f_IsEmpty())
						continue;
				}

				CStr HeadContent;
				bool bHasHeadRevision = bHasMapping && _Client.f_NoThrow().f_GetTextFileContents(DepotPath, HeadContent);

				if (!bHasHeadRevision)
				{
					// No submitted revision (and not currently opened): a
					// brand-new config file that exists locally is an
					// unreconciled workspace addition.
					if (CFile::fs_FileExists(ConfigFile))
						return true;
					continue;
				}

				CStr WorkspaceContent = CFile::fs_ReadStringFromFile(ConfigFile);
				if (WorkspaceContent != HeadContent)
					return true;
			}
			return false;
		}

		// Convert Perforce changelists to CLogEntryFull entries.
		// Get changelists between from and to. Empty _ToCL means up to #head + pending + unreconciled.
		TCFuture<TCVector<CLogEntryFull>> fg_PerforceGetLogEntries(TCVector<CStr> _ConfigFilePaths, CStr _FromCL, CStr _ToCL, CStr _BaseDir)
		{
			auto BlockingActorCheckout = fg_BlockingActor();
			co_return co_await
				(
					g_Dispatch(BlockingActorCheckout) / [_ConfigFilePaths, _FromCL, _ToCL, _BaseDir]() -> TCVector<CLogEntryFull>
					{
						CPerforceClientThrow Client;
						fg_PerforceConnectAndResolveDepotPaths(_ConfigFilePaths, _BaseDir, Client);

						uint32 FromChangelist = _FromCL.f_ToInt(uint32(0));
						bool bToIsWorkspace = _ToCL.f_IsEmpty();
						CStr ToSpec = bToIsWorkspace ? CStr("#head") : ("@" + _ToCL);

						TCSet<uint32> SeenCLs;
						TCVector<CPerforceClient::CChangeList> AllChangelists;
						for (auto &ConfigFile : _ConfigFilePaths)
						{
							CStr DepotPath;
							if (!Client.f_NoThrow().f_GetDepotPath(ConfigFile, DepotPath))
								continue;

							// Submitted changelists in range
							TCVector<CPerforceClient::CChangeList> CLs;
							Client.f_NoThrow().f_GetChangelists("{}@{},{}"_f << DepotPath << (FromChangelist + 1) << ToSpec, CLs, false);

							for (auto &CL : CLs)
							{
								if (SeenCLs(CL.m_ChangeID).f_WasCreated())
									AllChangelists.f_Insert(CL);
							}

							if (!bToIsWorkspace)
								continue;

							// Pending changelists with this file open in this workspace
							TCVector<CPerforceClient::CChangeList> PendingCLs;
							Client.f_NoThrow().f_GetChangelists(ConfigFile, PendingCLs, false, CStr(), "pending");

							for (auto &CL : PendingCLs)
							{
								if (SeenCLs(CL.m_ChangeID).f_WasCreated())
									AllChangelists.f_Insert(CL);
							}

							// Default changelist (CL 0) is not returned by p4 changes
							TCVector<CStr> Opened;
							if (Client.f_NoThrow().f_GetOpened(ConfigFile, CStr(), Opened) && !Opened.f_IsEmpty())
							{
								if (SeenCLs(0u).f_WasCreated())
								{
									CPerforceClient::CChangeList DefaultCL;
									DefaultCL.m_ChangeID = 0;
									DefaultCL.m_Status = "pending";
									DefaultCL.m_Description = "Default changelist";
									Client.f_NoThrow().f_GetUserName(DefaultCL.m_User);
									AllChangelists.f_Insert(DefaultCL);
								}
							}
						}

						TCVector<CLogEntryFull> Entries;
						for (auto &CL : AllChangelists)
						{
							CLogEntryFull Entry;
							Entry.m_Commit = CL.m_Status == "pending" ? ("pending:" + CStr::fs_ToStr(CL.m_ChangeID)) : CStr::fs_ToStr(CL.m_ChangeID);
							Entry.m_Author = CL.m_User;
							// CL.m_Date is 0 for synthesized entries (e.g. the
							// default changelist) that have no real timestamp;
							// leave the dates as the default invalid CTime in
							// that case so the renderer doesn't show 1970.
							if (CL.m_Date != 0)
							{
								Entry.m_AuthorDate = CTimeConvert::fs_FromUnixSeconds(CL.m_Date);
								Entry.m_CommitterDate = Entry.m_AuthorDate;
							}
							Entry.m_Committer = CL.m_User;
							Entry.m_Message = CL.m_Description.f_Trim();
							if (!Entry.m_Message.f_IsEmpty())
								Entry.m_FirstLine = Entry.m_Message.f_SplitLine().f_GetFirst();
							Entries.f_Insert(fg_Move(Entry));
						}

						if (bToIsWorkspace && fg_PerforceHasUnreconciledChanges(Client, _ConfigFilePaths))
						{
							CLogEntryFull Entry;
							Entry.m_Commit = "unreconciled";
							Entry.m_Author = Client.f_GetUser();
							Entry.m_Message = "Unreconciled workspace changes";
							Entry.m_FirstLine = Entry.m_Message;
							Entries.f_InsertFirst(fg_Move(Entry));
						}

						return Entries;
					}
				)
			;
		}

		struct CPerforceResolvedRefs
		{
			CStr m_From;
			CStr m_To;
			CStr m_FromDisplay;
			CStr m_ToDisplay;
		};

		// Resolve Perforce from/to references, replacing git defaults and auto-detecting changelists.
		TCFuture<CPerforceResolvedRefs> fg_PerforceResolveRefs
			(
				CStr _From
				, CStr _To
				, TCSet<CStr> _PerforceRootConfigFiles
				, CStr _BaseDir
			)
		{
			CStr PerforceFrom = _From;
			CStr PerforceTo = _To;

			// Replace git defaults with empty (auto-detect) for Perforce
			if (PerforceFrom == "origin/master")
				PerforceFrom = "";
			if (PerforceTo == "HEAD")
				PerforceTo = "";

			// Strip leading @ from explicit changelist references
			if (PerforceFrom.f_StartsWith("@"))
				PerforceFrom = PerforceFrom.f_Extract(1);
			if (PerforceTo.f_StartsWith("@"))
				PerforceTo = PerforceTo.f_Extract(1);

			// Auto-detect default from/to changelists
			bool bFromDefault = PerforceFrom.f_IsEmpty();
			bool bToDefault = PerforceTo.f_IsEmpty();
			if (bFromDefault)
			{
				TCVector<CStr> ConfigFilePaths;
				for (auto &ConfigFile : _PerforceRootConfigFiles)
					ConfigFilePaths.f_Insert(ConfigFile);

				if (bToDefault)
				{
					auto [AutoFrom, AutoTo] = co_await fg_PerforceAutoDetectChangelists(ConfigFilePaths, _BaseDir);
					PerforceFrom = AutoFrom;
					PerforceTo = AutoTo;
				}
				else
					PerforceFrom = co_await fg_PerforceGetLatestChangelist(ConfigFilePaths, _BaseDir);
			}

			CPerforceResolvedRefs Result;
			Result.m_From = PerforceFrom;
			Result.m_To = PerforceTo;
			Result.m_FromDisplay = "@" + PerforceFrom;
			Result.m_ToDisplay = PerforceTo.f_IsEmpty() ? CStr("workspace") : ("@" + PerforceTo);
			co_return Result;
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

		bool bIsPerforceRoot = !CFile::fs_FileExists(f_GetBaseDir() + "/.git", EFileAttrib_Directory | EFileAttrib_File) && CPerforceClient::fs_HasP4Config(f_GetBaseDir());

		// Perforce root should always appear in the output
		if (bIsPerforceRoot)
			ShowRepos[f_GetBaseDir()];

		TCSharedPointer<CFilteredRepos> pFilteredRepositories = fg_Construct(co_await fg_GetFilteredRepos(CRepoFilter(), *this, mp_Data, EGetRepoFlag::mc_IncludeRepoCommit));
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

		CGitLaunches Launches{f_GetGitLaunchOptions("list-commits"), "Listing Commits"};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		TCVector<TCAsyncResult<void>> LaunchResults;

		TCMap<CStr, CRepository *> RepositoryByLocation;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCFutureVector<void> Results;
			for (auto *pRepo : Repos)
				RepositoryByLocation[pRepo->m_Location] = pRepo;
		}

		TCSet<CStr> PerforceRootConfigFiles;
		if (bIsPerforceRoot)
			PerforceRootConfigFiles = fg_CollectPerforceRootConfigFiles(RepositoryByLocation, f_GetBaseDir());

		// Insert a sentinel for the base dir so config file ownership resolution
		// can find it as the owner of Perforce-tracked config files
		if (bIsPerforceRoot && !RepositoryByLocation.f_FindEqual(f_GetBaseDir()))
			RepositoryByLocation[f_GetBaseDir()] = nullptr;

		CPerforceResolvedRefs PerforceRefs;
		if (bIsPerforceRoot)
			PerforceRefs = co_await fg_PerforceResolveRefs(_From, _To, PerforceRootConfigFiles, f_GetBaseDir());

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

		if (bIsPerforceRoot)
		{
			pState->m_StartCommits[f_GetBaseDir()] = PerforceRefs.m_From;
			pState->m_EndCommits[f_GetBaseDir()] = PerforceRefs.m_To;
		}
		else
		{
			pState->m_StartCommits[f_GetBaseDir()] = _From;
			pState->m_EndCommits[f_GetBaseDir()] = _To;
		}

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
					if (!pRepository)
						continue;

					auto &Repo = *pRepository;

					if (State.m_StartCommits.f_FindEqual(Repo.m_Location))
					{
						DCheck(State.m_EndCommits.f_FindEqual(Repo.m_Location));
						continue;
					}
					else
						bAllFinished = false;

					CStr ConfigDirectory = CFile::fs_GetPath(Repo.m_ConfigFile);
					CStr OwnerLocation;
					auto *pOwner = CBuildSystem::fs_FindContainingPath(RepositoryByLocation, ConfigDirectory, OwnerLocation);
					if (!pOwner)
						continue;

					auto *pStartCommit = State.m_StartCommits.f_FindEqual(OwnerLocation);
					if (!pStartCommit)
						continue;

					auto *pEndCommit = State.m_EndCommits.f_FindEqual(OwnerLocation);
					if (!pEndCommit)
						continue;

					auto ConfigFile = Repo.m_ConfigFile;

					if (!State.m_StartedGitShow(ConfigFile).f_WasCreated())
					{
						if (State.m_PendingGitShow.f_FindEqual(ConfigFile))
							continue;

						auto pStartConfigFile = State.m_StartConfigFiles.f_FindEqual(ConfigFile);

						CStr RelativePath = CFile::fs_MakePathRelative(ConfigFile, OwnerLocation);

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

					CStr RelativePath = CFile::fs_MakePathRelative(ConfigFile, OwnerLocation);

					bDoneSomething = true;

					if (bIsPerforceRoot && PerforceRootConfigFiles.f_FindEqual(ConfigFile))
					{
						auto [StartContent, EndContent] = co_await fg_PerforceGetConfigFileContents(ConfigFile, *pStartCommit, *pEndCommit);
						auto &State = *pState;

						{
							auto CaptureScope = co_await (g_CaptureExceptions % "Exception parsing config files");

							if (!StartContent.f_IsEmpty())
								State.m_StartConfigFiles[ConfigFile] = CStateHandler::fs_ParseConfigFile(StartContent, ConfigFile);
							if (!EndContent.f_IsEmpty())
								State.m_EndConfigFiles[ConfigFile] = CStateHandler::fs_ParseConfigFile(EndContent, ConfigFile);
						}
					}
					else
					{
						DCheck(*pOwner);
						auto &Owner = **pOwner;
						State.m_PendingGitShow[ConfigFile];

						auto [StartResult, EndResult] = co_await
							(
								Launches.f_Launch(Owner, {"show", "{}:{}"_f << *pStartCommit << RelativePath}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None)
								+ Launches.f_Launch(Owner, {"show", "{}:{}"_f << *pEndCommit << RelativePath}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None)
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

		// Handle Perforce root log entries separately (base dir has no CRepository object)
		if (bIsPerforceRoot)
		{
			auto *pStartCommit = State.m_StartCommits.f_FindEqual(f_GetBaseDir());
			auto *pEndCommit = State.m_EndCommits.f_FindEqual(f_GetBaseDir());

			if (pStartCommit && !pStartCommit->f_IsEmpty())
			{
				TCVector<CStr> ConfigFilePaths;
				for (auto &ConfigFile : PerforceRootConfigFiles)
					ConfigFilePaths.f_Insert(ConfigFile);

				CStr ToCL = pEndCommit ? *pEndCommit : CStr();

				fg_PerforceGetLogEntries(ConfigFilePaths, *pStartCommit, ToCL, f_GetBaseDir()) > CommitsResults[f_GetBaseDir()];
			}
			else
			{
				NoStartCommits[f_GetBaseDir()];
				TCFuture(TCVector<CLogEntryFull>()) > CommitsResults[f_GetBaseDir()];
			}

			TCFuture(TCVector<CLogEntryFull>()) > ReverseCommitsResults[f_GetBaseDir()];
		}

		for (auto &pRepository : RepositoryByLocation)
		{
			if (!pRepository)
				continue;

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

				TCFuture(TCVector<CLogEntryFull>()) > CommitsResults[Location];
				TCFuture(TCVector<CLogEntryFull>()) > ReverseCommitsResults[Location];
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
				auto fFilterEntries = [](TCVector<CLogEntryFull> &o_Entries, umint _MaxEntries, CStr const &_Message)
					{
						if (o_Entries.f_GetLen() <= _MaxEntries)
							return;

						umint nDeleted = o_Entries.f_GetLen() - _MaxEntries;
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
						umint SectionLen = pParse - _String.f_GetStr();
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
				if (pRepository && *pRepository)
					RelativePath = "~" + (*pRepository)->m_Identity;
			}

			CTableRenderHelper &TableRenderer = TableRenderers.f_Insert(_pCommandLine->f_TableRenderer());
			TableRenderer.f_SetPrefix(_Prefix);
			{
				TCVector<CStr> Headings;
				TCVector<TCTuple<int32, uint32>> MaxColumnWidths;
				for (auto &Column : Columns)
				{
					umint iColumn = Headings.f_GetLen();
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

			// The Perforce root uses the global PerforceRoot.RepoCommit config
			// (it has no %Repository and its RepositoryByLocation slot is a
			// nullptr sentinel), so route it through the same helper that
			// repo-commit uses when writing the outermost changelist. Every
			// other repo keeps its per-%Repository m_RepoCommitOptions.
			CStr CommitMessageHeader;
			if (bIsPerforceRoot && Repo == f_GetBaseDir())
			{
				if (auto RootOptions = fg_GetPerforceRootRepoCommitOptions(*this, mp_Data))
					CommitMessageHeader = RootOptions->m_MessageHeader;
			}
			else if (auto *pRepo = RepositoryByLocation.f_FindEqual(Repo); pRepo && *pRepo)
			{
				if ((*pRepo)->m_RepoCommitOptions)
					CommitMessageHeader = (*pRepo)->m_RepoCommitOptions->m_MessageHeader;
			}

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

				// Limit repo-commit messages to first line since the per-repo
				// breakdown is already shown separately and the extra lines are redundant
				if (Message.f_StartsWith(CommitMessageHeader ? CommitMessageHeader : gc_Str<"Update repositories">.m_Str))
					Message = LogEntry.m_FirstLine;

				Message = CStr::fs_Join(fCleanupLines(Message), "\n");

				ChangelogEntries[LogEntry.m_AuthorDate] += Message;

				RowValues.f_Insert(fColorSection(Message));

				TableRenderer.f_AddRowVector(RowValues);
			}

			if (RelativePath == ".")
			{
				if (bIsPerforceRoot)
				{
					CStr Description;
					Description += "{3}From {2}{}{1}\n"_f << PerforceRefs.m_FromDisplay << Colors.f_Default() << Colors.f_Foreground256(246) << Colors.f_Default();
					Description += "{3}To   {2}{}{1}"_f << PerforceRefs.m_ToDisplay << Colors.f_Default() << Colors.f_Foreground256(246) << Colors.f_Default();

					TableRenderer.f_AddDescription(Description);
				}
				else
				{
					auto *pRepo = AllReposByRelativePath.f_FindEqual(RelativePath);
					CStr FromHash = "Unknown";
					CStr ToHash = "Unknown";

					if (pRepo)
					{
						auto [FromResult, ToResult] = co_await
							(
								Launches.f_Launch(**pRepo, {"rev-parse", "{}^0"_f << _From}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None)
								+ Launches.f_Launch(**pRepo, {"rev-parse", "{}^0"_f << _To}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None)
							)
						;

						if (FromResult.m_ExitCode == 0)
							FromHash = FromResult.f_GetStdOut().f_Trim();
						else
							FromHash = CStr::fs_Join(FromResult.f_GetErrorOut().f_Trim().f_SplitLine(), " ").f_Replace("\t", " ");

						if (ToResult.m_ExitCode == 0)
							ToHash = ToResult.f_GetStdOut().f_Trim();
						else
							ToHash = CStr::fs_Join(ToResult.f_GetErrorOut().f_Trim().f_SplitLine(), " ").f_Replace("\t", " ");
					}

					umint MaxLen = fg_Max(_From.f_GetLen(), _To.f_GetLen());

					CStr Description;
					Description += "{5}From {3}{a-,sj*,sf }  {4}{}\n"_f << _From << MaxLen << FromHash << Colors.f_ToPush() << Colors.f_Foreground256(246) << Colors.f_Default();
					Description += "{5}To   {3}{a-,sj*,sf }  {4}{}"_f << _To << MaxLen << ToHash << Colors.f_ToPush() << Colors.f_Foreground256(246) << Colors.f_Default();

					TableRenderer.f_AddDescription(Description);
				}
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
				umint nLines = Lines.f_GetLen();
				CStr ToOutput = "{} "_f << DateStr;
				for (umint i = 0; i < nLines; ++i)
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
