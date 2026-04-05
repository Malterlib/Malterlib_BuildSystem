// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"
#include "Malterlib_BuildSystem_Repository_PerforceHelpers.h"

#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem
{
	using namespace NRepository;
	using namespace NStr;

	namespace
	{
		struct CChildCommitInfo
		{
			TCVector<CStr> m_ForwardCommits;
			umint m_nReverseCommits = 0;
			CStr m_AddedHash;
		};

		// Accumulated commits for a repo: display name → commit info
		using CAccumulatedCommits = TCMap<CStr, CChildCommitInfo>;

		struct CCommitAnalysis
		{
			CStr m_Hash;
			CStr m_Subject;
			TCVector<CStr> m_ConfigFilesChanged;
			bool m_bHasNonConfigChanges = false;
		};

		struct CIndexBackup
		{
			CStr m_DataDir;
			CStr m_IndexFile;
			CStr m_BackupFile;
			CStr m_HeadHash;
			CStr m_StashHash;
			// Config files that were untracked when the backup was
			// taken. These are present in the backup stash as tracked
			// additions (because `fg_BackupRepositoryIndex` does `git
			// add` on them before `git stash create`); the restore path
			// must delete them from the working tree before
			// `git stash apply`, otherwise their still-on-disk untracked
			// copies would make the apply fail with "untracked working
			// tree file would be overwritten".
			TCVector<CStr> m_UntrackedConfigFiles;
			bool m_bHadIndexFile = false;
		};

		TCFuture<bool> fg_PathExistsAtCommit(CGitLaunches _Launches, CRepository _Repo, CStr _CommitHash, CStr _Path)
		{
			auto PathResult = co_await _Launches.f_Launch
				(
					_Repo
					, {"cat-file", "-e", "{}:{}"_f << _CommitHash << _Path}
					, {}
					, CProcessLaunchActor::ESimpleLaunchFlag_None
				)
			;
			co_return PathResult.m_ExitCode == 0;
		}

		bool fg_HasActiveGitOperation(CStr const &_GitDataDir)
		{
			return
				CFile::fs_FileExists(_GitDataDir + "/rebase-merge", EFileAttrib_Directory)   // rebase
				|| CFile::fs_FileExists(_GitDataDir + "/rebase-apply", EFileAttrib_Directory) // rebase or git am
				|| CFile::fs_FileExists(_GitDataDir + "/MERGE_HEAD", EFileAttrib_File)       // merge
				|| CFile::fs_FileExists(_GitDataDir + "/CHERRY_PICK_HEAD", EFileAttrib_File) // single cherry-pick stop
				|| CFile::fs_FileExists(_GitDataDir + "/REVERT_HEAD", EFileAttrib_File)      // single revert stop
				|| CFile::fs_FileExists(_GitDataDir + "/sequencer", EFileAttrib_Directory)   // multi-step cherry-pick/revert
				|| CFile::fs_FileExists(_GitDataDir + "/BISECT_LOG", EFileAttrib_File)       // bisect
				|| CFile::fs_FileExists(_GitDataDir + "/BISECT_START", EFileAttrib_File)     // bisect
			;
		}

		TCFuture<void> fg_RestoreDesiredConfigContents
			(
				CRepository _Repo
				, TCSet<CStr> _AllConfigRelPaths
				, TCMap<CStr, CStr> _DesiredConfigContents
			)
		{
			auto BlockingActorCheckout = fg_BlockingActor();
			co_await
				(
					g_Dispatch(BlockingActorCheckout) / [_Repo, _AllConfigRelPaths, _DesiredConfigContents]()
					{
						for (auto &Path : _AllConfigRelPaths)
						{
							CStr FullPath = _Repo.m_Location / Path;
							if (auto *pDesiredContents = _DesiredConfigContents.f_FindEqual(Path))
							{
								CFile::fs_CreateDirectoryForFile(FullPath);
								CFile::fs_WriteStringToFile(FullPath, *pDesiredContents, false);
							}
							else if (CFile::fs_FileExists(FullPath))
								CFile::fs_DeleteFile(FullPath);
						}
					}
				)
			;

			co_return {};
		}

		TCFuture<void> fg_RestoreRepositoryIndexFile(CIndexBackup _IndexBackup)
		{
			if (_IndexBackup.m_IndexFile.f_IsEmpty())
				co_return {};

			auto BlockingActorCheckout = fg_BlockingActor();
			co_await
				(
					g_Dispatch(BlockingActorCheckout) / [_IndexBackup]()
					{
						if (CFile::fs_FileExists(_IndexBackup.m_IndexFile, EFileAttrib_File))
							CFile::fs_DeleteFile(_IndexBackup.m_IndexFile);

						if (_IndexBackup.m_bHadIndexFile)
							CFile::fs_CopyFile(_IndexBackup.m_BackupFile, _IndexBackup.m_IndexFile);

						if (CFile::fs_FileExists(_IndexBackup.m_BackupFile, EFileAttrib_File))
							CFile::fs_DeleteFile(_IndexBackup.m_BackupFile);
					}
				)
			;

			co_return {};
		}

		TCFuture<CIndexBackup> fg_BackupRepositoryIndex
			(
				CGitLaunches _Launches
				, CRepository _Repo
				, CStr _OutputDir
				, TCVector<CStr> _UntrackedConfigFiles
			)
		{
			auto BlockingActorCheckout = fg_BlockingActor();
			CIndexBackup IndexBackup = co_await
				(
					g_Dispatch(BlockingActorCheckout) / [_Repo, _OutputDir]() -> CIndexBackup
					{
						CIndexBackup IndexBackup;
						auto DynamicInfo = fg_GetRepositoryDynamicInfo(_Repo);
						IndexBackup.m_DataDir = DynamicInfo.m_DataDir;
						IndexBackup.m_IndexFile = DynamicInfo.m_DataDir + "/index";
						IndexBackup.m_BackupFile = _OutputDir / "Temp" / ("repo-commit-index-backup-{}"_f << fg_FastRandomID());
						IndexBackup.m_bHadIndexFile = CFile::fs_FileExists(IndexBackup.m_IndexFile, EFileAttrib_File);

						if (IndexBackup.m_bHadIndexFile)
						{
							CFile::fs_CreateDirectoryForFile(IndexBackup.m_BackupFile);
							CFile::fs_CopyFile(IndexBackup.m_IndexFile, IndexBackup.m_BackupFile);
						}

						return IndexBackup;
					}
				)
			;

			auto CleanupIndexBackup = co_await fg_AsyncDestroy
				(
					[&]() -> TCFuture<void>
					{
						// Safe: ECoroutineFlag_CaptureExceptions itself does not suspend, so
						// this still runs before the cleanup coroutine's first real suspension.
						co_await ECoroutineFlag_CaptureExceptions;
						co_await fg_RestoreRepositoryIndexFile(fg_Move(IndexBackup));
						co_return {};
					}
				)
			;

			auto HeadResult = co_await _Launches.f_Launch(_Repo, {"rev-parse", "--verify", "HEAD"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			if (HeadResult.m_ExitCode != 0)
				co_return DMibErrorInstance("Failed to get repository HEAD hash: {}"_f << HeadResult.f_GetCombinedOut().f_Trim());
			IndexBackup.m_HeadHash = HeadResult.f_GetStdOut().f_Trim();

			// Stage untracked config files so `git stash create` can
			// capture their content as part of the backup. Without this
			// any file that was newly created on the user's side (never
			// tracked at HEAD) would not be part of the stash's tree,
			// and the recovery path would not be able to restore it
			// after the subsequent merge-base checkout overwrites the
			// working-tree copy. The caller clears the index again with
			// `git reset` before committing, so this staging is transient.
			//
			// fg_RestoreRepositoryIndex uses m_UntrackedConfigFiles to
			// delete these paths from the working tree before
			// `git stash apply`, so the apply doesn't fail with
			// "untracked working tree file would be overwritten" when
			// an early abort leaves the original untracked copy on disk.
			//
			// Known limitation: only managed config files get staged here,
			// so any OTHER untracked file in the repo is not captured in
			// the backup stash and also not moved out of the working tree.
			// `git rebase --autostash` below only stashes tracked
			// modifications, so an unrelated untracked file at a path a
			// replayed commit introduces can still abort the rebase with
			// "untracked working tree file would be overwritten". In that
			// case repo-commit fails — the cleanup path resets tracked
			// state to the original HEAD and restores the saved index —
			// and it is up to the user to remove (or `git add`) the
			// offending file before retrying.
			IndexBackup.m_UntrackedConfigFiles = _UntrackedConfigFiles;
			if (!_UntrackedConfigFiles.f_IsEmpty())
			{
				TCVector<CStr> AddParams = {"add", "--"};
				for (auto &Path : _UntrackedConfigFiles)
					AddParams.f_Insert(Path);

				auto AddResult = co_await _Launches.f_Launch(_Repo, AddParams, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
				if (AddResult.m_ExitCode != 0)
					co_return DMibErrorInstance("Failed to stage untracked config files for backup: {}"_f << AddResult.f_GetCombinedOut().f_Trim());
			}

			auto StashCreateResult = co_await _Launches.f_Launch
				(
					_Repo
					, {"stash", "create", "mib-repo-commit backup"}
					, {}
					, CProcessLaunchActor::ESimpleLaunchFlag_None
				)
			;
			if (StashCreateResult.m_ExitCode != 0)
				co_return DMibErrorInstance("Failed to create repository state backup: {}"_f << StashCreateResult.f_GetCombinedOut().f_Trim());
			IndexBackup.m_StashHash = StashCreateResult.f_GetStdOut().f_Trim();

			CleanupIndexBackup.f_Clear();
			co_return fg_Move(IndexBackup);
		}

		TCFuture<bool> fg_RestoreRepositoryIndex(CGitLaunches _Launches, CRepository _Repo, CIndexBackup _IndexBackup)
		{
			auto BlockingActorCheckout = fg_BlockingActor();
			bool bCanRestore = co_await
				(
					g_Dispatch(BlockingActorCheckout) / [_IndexBackup]() -> bool
					{
						return
							!fg_HasActiveGitOperation(_IndexBackup.m_DataDir)
							&& !CFile::fs_FileExists(_IndexBackup.m_IndexFile + ".lock", EFileAttrib_File)
						;
					}
				)
			;

			if (!bCanRestore)
				co_return false;

			auto ResetResult = co_await _Launches.f_Launch(_Repo, {"reset", "--hard", _IndexBackup.m_HeadHash}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			if (ResetResult.m_ExitCode != 0)
				co_return false;

			if (_IndexBackup.m_StashHash)
			{
				// Delete the originally-untracked config files from the
				// working tree so `git stash apply` can re-add them
				// without tripping on "untracked working tree file would
				// be overwritten". `git reset --hard` above only removes
				// files tracked at the target commit; the original
				// untracked copies (if any still exist) are still on
				// disk and have to be cleared explicitly. The stash
				// itself carries their content as tracked additions, so
				// dropping the on-disk copies first is lossless.
				if (!_IndexBackup.m_UntrackedConfigFiles.f_IsEmpty())
				{
					auto BlockingActorCheckout = fg_BlockingActor();
					co_await
						(
							g_Dispatch(BlockingActorCheckout) / [_Repo, _IndexBackup]()
							{
								for (auto &Path : _IndexBackup.m_UntrackedConfigFiles)
								{
									CStr FullPath = _Repo.m_Location / Path;
									if (CFile::fs_FileExists(FullPath))
										CFile::fs_DeleteFile(FullPath);
								}
							}
						)
					;
				}

				auto StashApplyResult = co_await _Launches.f_Launch(_Repo, {"stash", "apply", _IndexBackup.m_StashHash}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
				if (StashApplyResult.m_ExitCode != 0)
				{
					co_await _Launches.f_Launch(_Repo, {"reset", "--hard", _IndexBackup.m_HeadHash}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
					co_return false;
				}
			}

			co_await fg_RestoreRepositoryIndexFile(fg_Move(_IndexBackup));

			co_return true;
		}

		CStr fg_EvalMessageHeader(CStr const &_Header)
		{
			return _Header ? _Header : gc_Str<"Update repositories">.m_Str;
		}

		CStr fg_GenerateCommitMessage
			(
				CAccumulatedCommits const &_Commits
				, TCMap<CStr, umint> const &_DisplayNameStages
				, bool _bSkipCi
				, uint32 _MaxCommitsPerSection
				, CStr const &_MessageHeader
			)
		{
			CStr Message = fg_EvalMessageHeader(_MessageHeader);
			if (_bSkipCi)
				Message += " [skip ci]";

			// Sort sections by dependency order (shallowest first = lowest stage, e.g. Malterlib/Core before Malterlib/String),
			// then alphabetically within the same stage.
			struct CSectionOrder
			{
				auto operator <=> (CSectionOrder const &_Other) const
				{
					if (auto Cmp = m_Stage <=> _Other.m_Stage; Cmp != 0)
						return Cmp;
					return m_Name <=> _Other.m_Name;
				}

				bool operator==(CSectionOrder const &_Other) const = default;

				umint m_Stage = 0;
				CStr m_Name;
			};

			TCVector<CSectionOrder> SectionOrder;
			for (auto &Entry : _Commits)
			{
				auto &Name = _Commits.fs_GetKey(Entry);
				if (Entry.m_ForwardCommits.f_IsEmpty() && Entry.m_nReverseCommits == 0 && Entry.m_AddedHash.f_IsEmpty())
					continue;

				auto &Order = SectionOrder.f_Insert();
				Order.m_Name = Name;
				if (auto *pStage = _DisplayNameStages.f_FindEqual(Name))
					Order.m_Stage = *pStage;
			}

			SectionOrder.f_Sort();

			for (auto &Section : SectionOrder)
			{
				auto *pEntry = _Commits.f_FindEqual(Section.m_Name);
				if (!pEntry)
					continue;

				Message += "\n\n[";
				Message += Section.m_Name;
				Message += "]";
				if (!pEntry->m_AddedHash.f_IsEmpty())
					Message += "\n(added @ {})"_f << pEntry->m_AddedHash;
				if (pEntry->m_nReverseCommits > 0)
					Message += "\n({} removed)"_f << pEntry->m_nReverseCommits;

				uint32 nShown = 0;
				for (auto &Commit : pEntry->m_ForwardCommits)
				{
					if (nShown >= _MaxCommitsPerSection)
					{
						umint nRemaining = pEntry->m_ForwardCommits.f_GetLen() - nShown;
						Message += "\n... {} more commits"_f << nRemaining;
						break;
					}
					Message += "\n";
					Message += Commit;
					++nShown;
				}
			}

			return Message;
		}

		TCFuture<CStr> fg_RunTransformScript
			(
				CGitLaunches _Launches
				, CStr _WorkingDirectory
				, CStr _TransformScript
				, CStr _TempDir
				, CStr _Message
			)
		{
			CStr TempFile = _TempDir / "commit-message.tmp";
			NFile::CFile::fs_WriteStringToFile(TempFile, _Message, false);

			auto Result = co_await _Launches.f_Launch(_WorkingDirectory, {TempFile}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None, _TransformScript);
			if (Result.m_ExitCode != 0)
			{
				CStr ErrorMessage = "Transform script '{}' failed (exit code {}): {}"_f << _TransformScript << Result.m_ExitCode << Result.f_GetCombinedOut().f_Trim();
				_Launches.f_Output(EOutputType_Error, ".", ErrorMessage);
				co_return DMibErrorInstance(ErrorMessage);
			}

			CStr Transformed = CFile::fs_ReadStringFromFile(TempFile, true);
			co_return Transformed;
		}

		// Run the transform script and verify its output still starts with
		// the MessageHeader prefix. repo-commit entries are identified by
		// that prefix across tooling (list-commits, Perforce pending-CL
		// reuse, any prefix-based filter), so a script that rewrites the
		// subject would silently create duplicates on the next run. Fail
		// loudly here rather than surprising the user later.
		TCFuture<CStr> fg_RunAndVerifyTransformScript
			(
				CGitLaunches _Launches
				, CStr _OutputScope
				, CStr _WorkingDirectory
				, CStr _TransformScript
				, CStr _MessageHeader
				, CStr _TempDir
				, CStr _Message
			)
		{
			CStr Transformed = co_await fg_RunTransformScript(_Launches, _WorkingDirectory, _TransformScript, _TempDir, _Message);

			CStr const &ExpectedHeader = fg_EvalMessageHeader(_MessageHeader);
			if (!Transformed.f_StartsWith(ExpectedHeader))
			{
				CStr ErrorMessage = "Transform script '{}' must preserve the MessageHeader prefix '{}' on the first line."_f << _TransformScript << ExpectedHeader;
				_Launches.f_Output(EOutputType_Error, _OutputScope, ErrorMessage);
				co_return DMibErrorInstance(ErrorMessage);
			}

			co_return Transformed;
		}

		// Collect child commits by comparing start commits with current on-disk
		// config hashes.  Processes ALL config files passed in, so callers can
		// include the full subtree to get transitive commits in one call.
		TCFuture<CAccumulatedCommits> fg_CollectChildCommits
			(
				CGitLaunches _Launches
				, TCSet<CStr> _ConfigFiles
				, CStr _BaseDir
				, TCSharedPointer<TCMap<CStr, CStr> const> _pStartCommits
				, TCSharedPointer<TCMap<CStr, CRepository *> const> _pRepositoryByLocation
				, TCSharedPointer<TCMap<CStr, CRepository *> const> _pRepositoryByIdentity
				, TCSet<CStr> _PerforceRootConfigFilesAtStart
			)
		{
			CAccumulatedCommits RepoCommits;

			for (auto &ConfigFile : _ConfigFiles)
			{
				// "Added" detection only makes sense when we know the owning
				// repo's config existed at the start commit. If the owner is
				// itself a newly-added repo (no start commit resolved), the
				// resolver couldn't fetch its config at start, so missing
				// child entries here just mean "unknown", not "added".
				//
				// Removed repos are intentionally NOT inferred here. In practice
				// old entries usually remain in the .MRepo file rather than being
				// deleted, and conditional DSL can make a repo disappear from the
				// evaluated config without meaning "this branch removed it".
				// Reporting removals from the current config alone would therefore
				// be noisy and often wrong.
				bool bOwnerExistedAtStart = false;
				if (_PerforceRootConfigFilesAtStart.f_FindEqual(ConfigFile))
				{
					// Perforce-root-managed config files have no git owner in
					// _pRepositoryByLocation, so fs_FindContainingPath would
					// fail. Membership in this set already means the file
					// existed at the last submitted CL, so its "owner" (the
					// Perforce root at that CL) existed at start.
					bOwnerExistedAtStart = true;
				}
				else
				{
					CStr ConfigDirectory = CFile::fs_GetPath(ConfigFile);
					CStr OwnerLocation;
					auto *pOwner = CBuildSystem::fs_FindContainingPath(*_pRepositoryByLocation, ConfigDirectory, OwnerLocation);
					if (pOwner)
					{
						if (auto *pOwnerStartHash = _pStartCommits->f_FindEqual(OwnerLocation))
							bOwnerExistedAtStart = !pOwnerStartHash->f_IsEmpty();
					}
				}

				CConfigFile CurrentConfig;
				{
					auto CaptureScope = co_await (g_CaptureExceptions % "Exception parsing current config file");
					if (CFile::fs_FileExists(ConfigFile))
						CurrentConfig = CStateHandler::fs_ParseConfigFile(CFile::fs_ReadStringFromFile(ConfigFile, true), ConfigFile);
				}

				// Launch all log queries for this config file in parallel.
				//
				// No ChildLocation can appear twice in Queries, so the consumer
				// loop below is safe to write into the shared RepoCommits map
				// without deduplication. The invariant comes from the DSL:
				// fg_GetRepos (Malterlib_BuildSystem_Repository.cpp) rejects
				// duplicate %Repository locations ("Duplicate repository
				// location" / "Repository location already specified previously"),
				// and the repository key (m_Identity = entity key name) is
				// unique per entity. Each %Repository also has exactly one
				// m_ConfigFile, so a given physical repo only appears in one
				// .MRepo file even as we iterate _ConfigFiles in the outer
				// loop. Orphaned on-disk entries that no longer match DSL
				// state are filtered out by the pRepositoryByIdentity /
				// pRepositoryByLocation lookups further down. If the DSL ever
				// relaxes these uniqueness checks, add explicit ChildLocation
				// deduplication here — otherwise m_ForwardCommits would gain
				// duplicated entries.
				struct CChildLogQuery
				{
					CStr m_DisplayName;
					CStr m_ChildLocation;
				};

				TCVector<CChildLogQuery> Queries;
				TCFutureMap<CStr, TCVector<CLogEntry>> ForwardResults;
				TCFutureMap<CStr, TCVector<CLogEntry>> ReverseResults;

				for (auto &Config : CurrentConfig.m_Configs)
				{
					// External repos (stored under `~<identity>` in .MRepo) are
					// keyed by identity in m_Configs, not by filesystem path.
					// _pStartCommits and _pRepositoryByLocation are both keyed
					// by location, so translate identity back to location via
					// _pRepositoryByIdentity before looking anything up.
					auto &ConfigKey = CurrentConfig.m_Configs.fs_GetKey(Config);
					CStr ChildLocation;
					if (Config.m_bExternalPath)
					{
						auto *pExternalRepo = _pRepositoryByIdentity->f_FindEqual(ConfigKey);
						if (!pExternalRepo)
							continue;
						ChildLocation = (*pExternalRepo)->m_Location;
					}
					else
						ChildLocation = ConfigKey;

					CStr NewHash = Config.m_Hash;

					CStr OldHash;
					if (auto *pOldHash = _pStartCommits->f_FindEqual(ChildLocation))
						OldHash = *pOldHash;

					if (OldHash == NewHash || NewHash.f_IsEmpty())
						continue;

					auto *pChildRepo = _pRepositoryByLocation->f_FindEqual(ChildLocation);
					if (!pChildRepo)
						continue;

					CStr DisplayName = (*pChildRepo)->f_GetIdentifierName(_BaseDir, _BaseDir);
					if (DisplayName.f_IsEmpty())
						DisplayName = ".";

					if (OldHash.f_IsEmpty())
					{
						if (!bOwnerExistedAtStart)
							continue;

						// Repo is newly added on this branch; no range log is
						// possible, so just record the hash it was added at.
						auto &ChildCommitInfo = RepoCommits[DisplayName];
						ChildCommitInfo.m_AddedHash = NewHash.f_GetLen() > 12 ? NewHash.f_Slice(0, 12) : NewHash;
						continue;
					}

					auto &Query = Queries.f_Insert();
					Query.m_DisplayName = DisplayName;
					Query.m_ChildLocation = ChildLocation;

					fg_GetLogEntries(_Launches, **pChildRepo, OldHash, NewHash, false) > ForwardResults[ChildLocation];
					fg_GetLogEntries(_Launches, **pChildRepo, NewHash, OldHash, false) > ReverseResults[ChildLocation];
				}

				auto AllForward = co_await fg_AllDone(ForwardResults);
				auto AllReverse = co_await fg_AllDone(ReverseResults);

				for (auto &Query : Queries)
				{
					auto &ChildCommitInfo = RepoCommits[Query.m_DisplayName];

					if (auto *pForward = AllForward.f_FindEqual(Query.m_ChildLocation))
					{
						for (auto &Entry : *pForward)
							ChildCommitInfo.m_ForwardCommits.f_Insert(Entry.m_Description);
					}

					if (auto *pReverse = AllReverse.f_FindEqual(Query.m_ChildLocation))
					{
						// Skip placeholder entries produced when the range can't
						// be resolved locally (e.g. ref deleted from origin). The
						// forward path already surfaces the error text in the
						// commit body; counting it here would fabricate a bogus
						// "N removed" line.
						umint nCount = 0;
						for (auto &Entry : *pReverse)
						{
							if (!Entry.m_bUnresolved)
								++nCount;
						}
						ChildCommitInfo.m_nReverseCommits = nCount;
					}
				}
			}

			co_return fg_Move(RepoCommits);
		}

		// Analyze existing commits on the branch to determine which touch config files
		TCFuture<TCVector<CCommitAnalysis>> fg_AnalyzeCommits
			(
				CGitLaunches _Launches
				, CRepository _Repo
				, TCVector<CStr> _CommitHashes
				, TCSet<CStr> _AllConfigRelPaths
			)
		{
			// Launch subject and diff-tree queries for all commits in parallel
			TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> SubjectResults;
			TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> DiffResults;

			for (auto &Hash : _CommitHashes)
			{
				_Launches.f_Launch(_Repo, {"log", "-1", "--format=%s", Hash}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None) > SubjectResults[Hash];
				// Explicit two-tree form so merges are diffed against their
				// first parent (the rebased-history view). The default
				// `diff-tree <hash>` skips merges, and `-m` would report
				// per-parent diffs — so a merge whose final tree matches
				// the first parent (e.g. conflict resolved to ours) would
				// be falsely flagged as touching config and break the
				// `Already up to date` fast-path on every subsequent run.
				_Launches.f_Launch(_Repo, {"diff-tree", "--no-commit-id", "--name-only", "-r", "{}^1"_f << Hash, Hash}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None) > DiffResults[Hash];
			}

			auto AllSubjects = co_await fg_AllDone(SubjectResults);
			auto AllDiffs = co_await fg_AllDone(DiffResults);

			TCVector<CCommitAnalysis> Analyses;
			for (auto &Hash : _CommitHashes)
			{
				auto &Analysis = Analyses.f_Insert();
				Analysis.m_Hash = Hash;

				if (auto *pSubject = AllSubjects.f_FindEqual(Hash))
				{
					if (pSubject->m_ExitCode != 0)
						co_return DMibErrorInstance("Failed to get commit subject for {}: {}"_f << Hash << pSubject->f_GetCombinedOut().f_Trim());
					Analysis.m_Subject = pSubject->f_GetStdOut().f_Trim();
				}

				if (auto *pDiff = AllDiffs.f_FindEqual(Hash))
				{
					if (pDiff->m_ExitCode != 0)
						co_return DMibErrorInstance("Failed to get changed files for {}: {}"_f << Hash << pDiff->f_GetCombinedOut().f_Trim());

					for (auto &File : pDiff->f_GetStdOut().f_SplitLine<true>())
					{
						if (File.f_IsEmpty())
							continue;

						if (_AllConfigRelPaths.f_FindEqual(File))
							Analysis.m_ConfigFilesChanged.f_Insert(File);
						else
							Analysis.m_bHasNonConfigChanges = true;
					}
				}
			}

			co_return fg_Move(Analyses);
		}

		// Resolve `origin/<default-branch>`..HEAD merge-base and emit bootstrap
		// warnings when it can't be obtained. An empty return signals the
		// caller to proceed without rebase (commit-at-HEAD only) — supported
		// for bootstrapping a fresh root repository that has no upstream yet,
		// at the cost of idempotency on any branch that already carries
		// auto-generated commits we can't see.
		//
		// Templated over the launch target and output scope to serve both the
		// root-repo site (CStr path + CStr section name "." for output) and
		// the sub-repo site (CRepository for both), since `f_Launch` and
		// `f_Output` each have overloads for the two shapes.
		template <typename t_CLaunchTarget, typename t_COutputScope>
		TCFuture<CStr> fg_ResolveMergeBaseWithBootstrapFallback
			(
				CGitLaunches _Launches
				, t_CLaunchTarget _LaunchTarget
				, t_COutputScope _OutputScope
				, CStr _DefaultBranch
			)
		{
			auto Result = co_await _Launches.f_Launch
				(
					_LaunchTarget
					, {"merge-base", "origin/{}"_f << _DefaultBranch, "HEAD"}
					, {}
					, CProcessLaunchActor::ESimpleLaunchFlag_None
				)
			;

			CStr MergeBase;
			if (Result.m_ExitCode != 0)
			{
				_Launches.f_Output
					(
						EOutputType_Warning
						, _OutputScope
						, "git merge-base origin/{} HEAD failed (exit code {}): {}. Falling back to commit-at-HEAD without rebase;\n"
						"any prior auto-generated commits on this branch will not be squashed."_f
						<< _DefaultBranch << Result.m_ExitCode << Result.f_GetCombinedOut().f_Trim()
					)
				;
			}
			else
				MergeBase = Result.f_GetStdOut().f_Trim();

			if (MergeBase.f_IsEmpty())
			{
				_Launches.f_Output
					(
						EOutputType_Warning
						, _OutputScope
						, "No merge-base with 'origin/{}'. Proceeding in bootstrap mode: commit-at-HEAD without rebase.\n"
						"Ensure the branch tracks an upstream once available to restore idempotent behavior."_f
						<< _DefaultBranch
					)
				;
			}

			co_return MergeBase;
		}

		// Check changed config files and find merge-base concurrently
		struct CRepoPreflightResult
		{
			TCVector<CStr> m_ChangedConfigFiles;
			TCVector<CStr> m_UntrackedConfigFiles;
			CStr m_MergeBase;
			TCVector<CStr> m_CommitHashes;
		};

		TCFuture<CRepoPreflightResult> fg_RepoPreFlight
			(
				CGitLaunches _Launches
				, CRepository _Repo
				, TCSet<CStr> _ConfigFiles
				, TCSet<CStr> _AllConfigRelPaths
				, CStr _DefaultBranch
			)
		{
			CRepoPreflightResult Result;

			// Launch config file status checks and merge-base query concurrently
			TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> StatusResults;
			for (auto &ConfigFile : _ConfigFiles)
			{
				CStr RelPath = CFile::fs_MakePathRelative(ConfigFile, _Repo.m_Location);
				_Launches.f_Launch(_Repo, {"status", "--porcelain", "--", RelPath}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None) > StatusResults[RelPath];
			}

			TCFuture<CProcessLaunchActor::CSimpleLaunchResult> MergeBaseFuture;
			if (!_DefaultBranch.f_IsEmpty())
				MergeBaseFuture = _Launches.f_Launch(_Repo, {"merge-base", "origin/{}"_f << _DefaultBranch, "HEAD"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

			auto AllStatuses = co_await fg_AllDone(StatusResults);

			for (auto &Status : AllStatuses)
			{
				if (Status.m_ExitCode != 0)
					co_return DMibErrorInstance("Failed to query status for {}: {}"_f << AllStatuses.fs_GetKey(Status) << Status.f_GetCombinedOut().f_Trim());

				CStr StatusOutput = Status.f_GetStdOut().f_Trim();
				if (!StatusOutput.f_IsEmpty())
				{
					Result.m_ChangedConfigFiles.f_Insert(AllStatuses.fs_GetKey(Status));
					if (StatusOutput.f_StartsWith("??"))
						Result.m_UntrackedConfigFiles.f_Insert(AllStatuses.fs_GetKey(Status));
				}
			}

			if (MergeBaseFuture.f_IsValid())
			{
				auto MergeBaseResult = co_await fg_Move(MergeBaseFuture);
				if (MergeBaseResult.m_ExitCode == 0)
					Result.m_MergeBase = MergeBaseResult.f_GetStdOut().f_Trim();
			}

			// Get commit list if we have a merge-base. Merges are included so
			// fg_AnalyzeCommits can detect repo/config changes that only appear
			// on the branch through a merge (e.g. merging master back into the
			// feature branch); without that the rewrite path would skip such
			// branches entirely.
			if (!Result.m_MergeBase.f_IsEmpty())
			{
				auto RevListResult = co_await _Launches.f_Launch
					(
						_Repo
						, {"rev-list", "--reverse", "{}..HEAD"_f << Result.m_MergeBase}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;
				if (RevListResult.m_ExitCode != 0)
					co_return DMibErrorInstance("Failed to list commits since {}: {}"_f << Result.m_MergeBase << RevListResult.f_GetCombinedOut().f_Trim());

				for (auto &Line : RevListResult.f_GetStdOut().f_SplitLine<true>())
				{
					if (!Line.f_IsEmpty())
						Result.m_CommitHashes.f_Insert(Line);
				}
			}

			co_return fg_Move(Result);
		}

		// Resolve start commits by walking the dependency tree through .MRepo
		// config files at each stage. Seed _InitialStartCommits with the
		// starting repo's commit (e.g. root merge-base, or origin/{branch}
		// for a sub-repo) and this walks the full subtree from there.
		TCFuture<TCSharedPointer<TCMap<CStr, CStr> const>> fg_ResolveStartCommits
			(
				CGitLaunches _Launches
				, CStr _BaseDir
				, TCMap<CStr, CStr> _InitialStartCommits
				, TCVector<TCVector<CRepository *>> _RepoStages
				, TCMap<CStr, TCSet<CStr>> _ConfigFilesByRepo
				, TCMap<CStr, TCVector<CRepository *>> _ReposByConfigFile
			)
		{
			TCMap<CStr, CStr> StartCommits = fg_Move(_InitialStartCommits);

			// Walk dependency stages from shallowest to deepest
			for (auto &Repos : _RepoStages)
			{
				// Fetch config files at start commits for repos in this stage
				TCFutureMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> ConfigResults;

				for (auto *pRepo : Repos)
				{
					auto *pStart = StartCommits.f_FindEqual(pRepo->m_Location);
					if (!pStart || pStart->f_IsEmpty())
						continue;

					auto *pConfigs = _ConfigFilesByRepo.f_FindEqual(pRepo->m_Location);
					if (!pConfigs)
						continue;

					for (auto &ConfigFile : *pConfigs)
					{
						CStr RelPath = CFile::fs_MakePathRelative(ConfigFile, pRepo->m_Location);
						_Launches.f_Launch(*pRepo, {"show", "{}:{}"_f << *pStart << RelPath}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None) > ConfigResults[ConfigFile];
					}
				}

				auto AllResults = co_await fg_AllDone(ConfigResults);

				// Parse fetched configs and resolve child start commits
				for (auto &Result : AllResults)
				{
					if (Result.m_ExitCode != 0)
						continue;

					auto &ConfigFile = AllResults.fs_GetKey(Result);

					CConfigFile Config;
					{
						auto CaptureScope = co_await (g_CaptureExceptions % "Exception parsing config file at start commit");
						Config = CStateHandler::fs_ParseConfigFile(Result.f_GetStdOut(), ConfigFile);
					}

					auto *pTrackedRepos = _ReposByConfigFile.f_FindEqual(ConfigFile);
					if (!pTrackedRepos)
						continue;

					for (auto *pRepo : *pTrackedRepos)
					{
						if (StartCommits.f_FindEqual(pRepo->m_Location))
							continue;

						auto *pHash = Config.f_GetConfig(*pRepo, _BaseDir);
						if (pHash && !pHash->m_Hash.f_IsEmpty())
							StartCommits[pRepo->m_Location] = pHash->m_Hash;
					}
				}
			}

			co_return fg_Construct(fg_Move(StartCommits));
		}

		// Create the Update repositories commit and rebase it into position
		TCFuture<void> fg_CommitAndRebase
			(
				CGitLaunches _Launches
				, CRepository _Repo
				, CStr _OutputDir
				, TCSet<CStr> _AllConfigRelPaths
				, TCVector<CStr> _ChangedConfigFiles
				, TCVector<CStr> _UntrackedConfigFiles
				, CStr _RepoMergeBase
				, TCVector<CCommitAnalysis> _CommitAnalyses
				, CStr _CommitMessage
			)
		{
			bool bHasExistingConfigCommits = false;
			for (auto &Analysis : _CommitAnalyses)
			{
				if (!Analysis.m_ConfigFilesChanged.f_IsEmpty())
				{
					bHasExistingConfigCommits = true;
					break;
				}
			}

			if (_ChangedConfigFiles.f_IsEmpty() && !bHasExistingConfigCommits)
				co_return {};

			// Check if the existing state already matches what we'd produce
			if (_ChangedConfigFiles.f_IsEmpty() && !_RepoMergeBase.f_IsEmpty() && !_CommitAnalyses.f_IsEmpty())
			{
				auto &LastAnalysis = _CommitAnalyses.f_GetLast();
				bool bLastIsConfigOnly = !LastAnalysis.m_ConfigFilesChanged.f_IsEmpty() && !LastAnalysis.m_bHasNonConfigChanges;
				bool bNoOtherConfigCommits = true;
				bool bHasMixedCommits = false;
				for (umint i = 0; i < _CommitAnalyses.f_GetLen() - 1; ++i)
				{
					if (!_CommitAnalyses[i].m_ConfigFilesChanged.f_IsEmpty() && !_CommitAnalyses[i].m_bHasNonConfigChanges)
						bNoOtherConfigCommits = false;
					if (!_CommitAnalyses[i].m_ConfigFilesChanged.f_IsEmpty() && _CommitAnalyses[i].m_bHasNonConfigChanges)
						bHasMixedCommits = true;
				}

				if (bLastIsConfigOnly && bNoOtherConfigCommits && !bHasMixedCommits)
				{
					auto MsgResult = co_await _Launches.f_Launch
						(
							_Repo
							, {"log", "-1", "--format=%B", LastAnalysis.m_Hash}
							, {}
							, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode
						)
					;
					CStr ExistingMessage = MsgResult.f_GetStdOut().f_TrimRight("\n");
					if (ExistingMessage == _CommitMessage)
					{
						_Launches.f_Output(EOutputType_Normal, _Repo, "Already up to date");
						co_return {};
					}
				}
			}

			// Nothing else to do if we don't have work for either branch below.
			if ((_RepoMergeBase.f_IsEmpty() || _CommitAnalyses.f_IsEmpty()) && _ChangedConfigFiles.f_IsEmpty())
				co_return {};

			// Refuse to proceed if an existing git operation is in progress.
			// fg_RestoreRepositoryIndex uses the same marker files to refuse
			// restoration, so entering the temp-commit/rebase flow below with
			// MERGE_HEAD/rebase-merge/etc. already present could leave the
			// worktree stranded with extra commits and only the backup-file
			// warning to recover from.
			{
				auto BlockingActorCheckout = fg_BlockingActor();
				bool bActiveOperation = co_await
					(
						g_Dispatch(BlockingActorCheckout) / [_Repo]() -> bool
						{
							auto DynamicInfo = fg_GetRepositoryDynamicInfo(_Repo);
							return fg_HasActiveGitOperation(DynamicInfo.m_DataDir);
						}
					)
				;
				if (bActiveOperation)
				{
					CStr Message = "Refusing to commit: repository has an in-progress merge/rebase/cherry-pick/revert. Resolve it before running repo-commit.";
					_Launches.f_Output(EOutputType_Error, _Repo, Message);
					co_return DMibErrorInstance(Message);
				}
			}

			// Unborn branch special-case: if the repo has never had a commit,
			// `git rev-parse HEAD` and `git reset` (both used by the shared
			// backup path below) fail, so bypass the backup/restore dance and
			// just add+commit directly. Any pre-existing staged work the user
			// had will end up in the initial commit — accepted limitation for
			// this rare edge case, since there's no HEAD to diff against.
			{
				auto HeadCheck = co_await _Launches.f_Launch(_Repo, {"rev-parse", "--verify", "HEAD"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
				if (HeadCheck.m_ExitCode != 0)
				{
					if (_ChangedConfigFiles.f_IsEmpty())
						co_return {};

					// The explicit pathspec keeps repo-commit scoped to managed config
					// files only; when one of those tracked files is deleted, `git add
					// -- <path>` still stages that deletion, so `-A` would only risk
					// sweeping in unrelated changes.
					TCVector<CStr> AddParams = {"add", "--"};
					for (auto &File : _ChangedConfigFiles)
						AddParams.f_Insert(File);
					co_await _Launches.f_Launch(_Repo, AddParams, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);

					auto CommitResult = co_await _Launches.f_Launch(_Repo, {"commit", "-m", _CommitMessage}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
					if (CommitResult.m_ExitCode != 0)
					{
						CStr Message = "Failed to commit: {}"_f << CommitResult.f_GetCombinedOut().f_Trim();
						_Launches.f_Output(EOutputType_Error, _Repo, Message);
						co_return DMibErrorInstance(Message);
					}

					_Launches.f_Output(EOutputType_Normal, _Repo, "Updated commit");
					co_return {};
				}
			}

			// Back up the user's index/working tree once, up-front. Both the
			// merge-base rewrite path and the no-merge-base commit-at-HEAD
			// path need to clear pre-existing staged entries (so unrelated
			// user work isn't folded into the auto-generated commit) and
			// later restore them plus re-sync the just-committed config
			// paths; keeping that shared backup/restore bracket here avoids
			// duplicating it in both branches.
			auto IndexBackup = co_await fg_BackupRepositoryIndex(_Launches, _Repo, _OutputDir, _UntrackedConfigFiles);
			auto CleanupIndexBackup = co_await fg_AsyncDestroy
				(
					[&]() -> TCFuture<void>
					{
						// Copy all captured state into locals before the first real suspension.
						// ECoroutineFlag_CaptureExceptions itself does not suspend, but the
						// later restore await may, so the cleanup body must stop touching
						// captured references before that point.
						CStr BackupFile = IndexBackup.m_BackupFile;
						CStr StashHash = IndexBackup.m_StashHash;
						bool bHadIndexFile = IndexBackup.m_bHadIndexFile;
						auto IndexBackupMove = fg_Move(IndexBackup);
						CGitLaunches LocalLaunches = _Launches;
						CRepository LocalRepo = _Repo;

						co_await ECoroutineFlag_CaptureExceptions;
						bool bRestored = co_await fg_RestoreRepositoryIndex(LocalLaunches, LocalRepo, fg_Move(IndexBackupMove));
						if (!bRestored)
						{
							CStr StashSuffix;
							if (StashHash)
								StashSuffix = ", stash snapshot: {}"_f << StashHash;

							if (bHadIndexFile)
							{
								LocalLaunches.f_Output
									(
										EOutputType_Error
										, LocalRepo
										, "Failed to restore the original repository state. Index backup kept at {}{}"_f << BackupFile << StashSuffix
									)
								;
							}
							else
							{
								LocalLaunches.f_Output
									(
										EOutputType_Error
										, LocalRepo
										, "Failed to restore the original repository state{}"_f << StashSuffix
									)
								;
							}
						}
						co_return {};
					}
				)
			;

			// Clear pre-existing staged entries so they don't get folded
			// into the commit(s) below. Without this, user-staged work on
			// unrelated paths either lands in the dropped temp reset commit
			// (merge-base path) or in the "Update repositories" commit
			// itself (no-merge-base path) and is silently included in the
			// history. The stash taken in fg_BackupRepositoryIndex still
			// carries that state for the failure-restore path.
			co_await _Launches.f_Launch(_Repo, {"reset"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);

			// Create the "Update repositories" commit using the reset+revert trick
			if (!_RepoMergeBase.f_IsEmpty() && !_CommitAnalyses.f_IsEmpty())
			{
				// Save desired config file contents from working tree
				TCMap<CStr, CStr> DesiredConfigContents;
				{
					auto BlockingActorCheckout = fg_BlockingActor();
					co_await
						(
							g_Dispatch(BlockingActorCheckout) / [&]()
							{
								for (auto &Path : _AllConfigRelPaths)
								{
									CStr FullPath = _Repo.m_Location / Path;
									if (CFile::fs_FileExists(FullPath))
										DesiredConfigContents[Path] = CFile::fs_ReadStringFromFile(FullPath, true);
								}
							}
						)
					;
				}

				{
					TCVector<CStr> MergeBaseExistingConfigRelPaths;
					TCVector<CStr> MergeBaseMissingConfigRelPaths;
					for (auto &Path : _AllConfigRelPaths)
					{
						if (co_await fg_PathExistsAtCommit(_Launches, _Repo, _RepoMergeBase, Path))
							MergeBaseExistingConfigRelPaths.f_Insert(Path);
						else
							MergeBaseMissingConfigRelPaths.f_Insert(Path);
					}

					if (!MergeBaseExistingConfigRelPaths.f_IsEmpty())
					{
						TCVector<CStr> CheckoutParams = {"checkout", _RepoMergeBase, "--"};
						for (auto &Path : MergeBaseExistingConfigRelPaths)
							CheckoutParams.f_Insert(Path);
						co_await _Launches.f_Launch(_Repo, CheckoutParams, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);
					}

					if (!MergeBaseMissingConfigRelPaths.f_IsEmpty())
					{
						auto BlockingActorCheckout = fg_BlockingActor();
						co_await
							(
								g_Dispatch(BlockingActorCheckout) / [_Repo, MergeBaseMissingConfigRelPaths]()
								{
									for (auto &Path : MergeBaseMissingConfigRelPaths)
									{
										CStr FullPath = _Repo.m_Location / Path;
										if (CFile::fs_FileExists(FullPath))
											CFile::fs_DeleteFile(FullPath);
									}
								}
							)
						;
					}
				}

				// Commit the reset (diff: HEAD state → merge-base state)
				{
					TCVector<CStr> AddParams = {"add", "--all", "--"};
					for (auto &Path : _AllConfigRelPaths)
						AddParams.f_Insert(Path);
					co_await _Launches.f_Launch(_Repo, AddParams, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);
				}
				auto TempResetCommitResult = co_await _Launches.f_Launch
					(
						_Repo
						, {"commit", "--allow-empty", "-m", "mib-repo-commit: temp reset"}
						, {}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;
				if (TempResetCommitResult.m_ExitCode != 0)
				{
					co_await fg_RestoreDesiredConfigContents(_Repo, _AllConfigRelPaths, DesiredConfigContents);
					CStr Message = "Failed to commit temp reset: {}"_f << TempResetCommitResult.f_GetCombinedOut().f_Trim();
					_Launches.f_Output(EOutputType_Error, _Repo, Message);
					co_return DMibErrorInstance(Message);
				}

				auto TempResetResult = co_await _Launches.f_Launch(_Repo, {"rev-parse", "HEAD"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);
				CStr TempResetHash = TempResetResult.f_GetStdOut().f_Trim();

				// Restore desired config file contents
				co_await fg_RestoreDesiredConfigContents(_Repo, _AllConfigRelPaths, DesiredConfigContents);

				// Stage and commit the desired state (diff: merge-base state → desired state)
				{
					TCVector<CStr> AddParams = {"add", "--all", "--"};
					for (auto &Path : _AllConfigRelPaths)
						AddParams.f_Insert(Path);
					co_await _Launches.f_Launch(_Repo, AddParams, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);
				}
				auto UpdateCommitResult = co_await _Launches.f_Launch(_Repo, {"commit", "--allow-empty", "-m", _CommitMessage}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
				if (UpdateCommitResult.m_ExitCode != 0)
				{
					CStr Message = "Failed to commit: {}"_f << UpdateCommitResult.f_GetCombinedOut().f_Trim();
					_Launches.f_Output(EOutputType_Error, _Repo, Message);
					co_return DMibErrorInstance(Message);
				}

				// Scratch dir for the todo roundtrip and capture-editor script.
				CStr TempDir = _OutputDir / "Temp" / fg_FastRandomID();
				CFile::fs_CreateDirectory(TempDir);
				auto CleanupTempDir = co_await fg_AsyncDestroy
					(
						[&]() -> TCFuture<void>
						{
							// Safe: ECoroutineFlag_CaptureExceptions itself does not suspend, so
							// this still runs before the cleanup coroutine's first real suspension.
							co_await ECoroutineFlag_CaptureExceptions;
							CFile::fs_DeleteDirectoryRecursive(TempDir);
							co_return {};
						}
					)
				;

				// Capture the default --rebase-merges todo that git would run
				// with. GIT_SEQUENCE_EDITOR is passed through `sh -c`, so an
				// inline `sh -c 'cp "$1" <capture>; exit 1' _` one-liner works
				// cross-platform (including Git Bash on Windows) without
				// needing a script file or an executable bit. The non-zero
				// exit aborts the rebase before any history is rewritten. The
				// capture path is inserted via fg_StrEscapeBashSingleQuotes(),
				// which returns a complete single-quoted shell literal, so
				// spaces and embedded quotes in TempDir are handled correctly.
				// Letting git generate the todo means merge commits come back
				// as `merge -C <hash>` lines (with surrounding `label`/`reset`
				// markers) and we can preserve them by passing those lines
				// through unchanged when we rewrite the todo.
				CStr CapturedTodoFile = TempDir / "captured-todo.txt";

				auto CaptureResult = co_await _Launches.f_Launch
					(
						_Repo
						, {"rebase", "-i", "--rebase-merges", "--no-autosquash", "--autostash", "--empty=drop", _RepoMergeBase, "-X", "theirs"}
						, {{"GIT_SEQUENCE_EDITOR", "sh -c 'cp \"$1\" '{}'; exit 1' _"_f << fg_StrEscapeBashSingleQuotes(CapturedTodoFile)}}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;

				if (!CFile::fs_FileExists(CapturedTodoFile))
				{
					co_await _Launches.f_Launch(_Repo, {"rebase", "--abort"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
					CStr Message = "Failed to capture rebase todo — sequence editor did not run: {}"_f << CaptureResult.f_GetCombinedOut().f_Trim();
					_Launches.f_Output(EOutputType_Error, _Repo, Message);
					co_return DMibErrorInstance(Message);
				}

				// Rewrite the captured todo: each `pick <hash>` line is edited
				// based on the analysis of that commit; everything else
				// (`merge`, `label`, `reset`, `exec`, `break`, comments,
				// blanks) is passed through unchanged so the merge structure
				// git emitted is preserved. Hashes in the todo are
				// abbreviated; match by prefix against our full hashes.
				CStr CapturedTodo = NFile::CFile::fs_ReadStringFromFile(CapturedTodoFile, false);
				CStr Todo;

				// Strip listed config paths from the commit at HEAD by reverting
				// each file to its first-parent state (or removing it from
				// HEAD if the parent never had it), then amend if anything
				// actually changed. The per-file loop is needed because
				// `git checkout HEAD~1 -- <files>` is atomic: a single missing
				// path makes the whole command fail, and `_Files` may include
				// newly-added config files that don't exist in HEAD~1.
				// Each path is wrapped with fg_StrEscapeBashSingleQuotes so
				// metacharacters (spaces, quotes, `$`, backticks, backslashes)
				// in valid repository paths can't corrupt the `for f in …` list.
				auto fAppendStripExec = [&](auto const &_Files)
					{
						CStr ExecCmd = "exec for f in";
						for (auto &File : _Files)
							ExecCmd += " {}"_f << fg_StrEscapeBashSingleQuotes(File);
						ExecCmd += "; do if git cat-file -e \"HEAD~1:$f\" 2>/dev/null; "
							"then git checkout HEAD~1 -- \"$f\" 2>/dev/null; elif test -e \"$f\"; "
							"then git rm -f --quiet -- \"$f\" 2>/dev/null; "
							"fi; "
							"done; "
							"git diff --cached --quiet || git commit --amend --no-edit\n"
						;
						Todo += ExecCmd;
					}
				;

				auto fProcessPick = [&](CStr const &_Hash, CStr const &_Rest) -> bool
					{
						if (TempResetHash.f_StartsWith(_Hash))
						{
							Todo += "drop {}{}\n"_f << _Hash << _Rest;
							return true;
						}

						for (auto &Analysis : _CommitAnalyses)
						{
							if (!Analysis.m_Hash.f_StartsWith(_Hash))
								continue;

							if (!Analysis.m_ConfigFilesChanged.f_IsEmpty() && !Analysis.m_bHasNonConfigChanges)
							{
								Todo += "drop {}{}\n"_f << _Hash << _Rest;
								return true;
							}

							if (!Analysis.m_ConfigFilesChanged.f_IsEmpty() && Analysis.m_bHasNonConfigChanges)
							{
								Todo += "pick {}{}\n"_f << _Hash << _Rest;
								fAppendStripExec(Analysis.m_ConfigFilesChanged);
								return true;
							}

							return false;
						}

						return false;
					}
				;

				for (auto &Line : CapturedTodo.f_SplitLine<true>())
				{
					aint CommandEnd = Line.f_FindCharOffset(0, ' ');
					if (CommandEnd > 0)
					{
						CStr Command = Line.f_Slice(0, CommandEnd);
						if (Command == "pick" || Command == "p")
						{
							aint HashStart = CommandEnd + 1;
							aint HashEnd = Line.f_FindCharOffset(HashStart, ' ');
							if (HashEnd < 0)
								HashEnd = Line.f_GetLen();
							CStr Hash = Line.f_Slice(HashStart, HashEnd);
							CStr Rest = Line.f_Slice(HashEnd, Line.f_GetLen());
							if (!Hash.f_IsEmpty() && fProcessPick(Hash, Rest))
								continue;
						}
						else if (Command == "merge" || Command == "m")
						{
							// Strip config files from the replayed merge commit
							// so they stay at the merge-base state. Without this
							// a merge that resolved a config-file conflict
							// (against a parent whose config edits we just
							// dropped) would either reintroduce stale config
							// changes or hit a rebase conflict on the next
							// pick that expects a clean config slate.
							Todo += Line;
							Todo += "\n";
							fAppendStripExec(_AllConfigRelPaths);
							continue;
						}
					}
					Todo += Line;
					Todo += "\n";
				}

				CStr TodoFile = TempDir / "repo-commit-todo.txt";
				NFile::CFile::fs_WriteStringToFile(TodoFile, Todo, false);

				auto RebaseResult = co_await _Launches.f_Launch
					(
						_Repo
						, {"rebase", "-i", "--rebase-merges", "--no-autosquash", "--autostash", "--empty=drop", _RepoMergeBase, "-X", "theirs"}
						, {{"GIT_SEQUENCE_EDITOR", "cp {}"_f << fg_StrEscapeBashSingleQuotes(TodoFile)}}
						, CProcessLaunchActor::ESimpleLaunchFlag_None
					)
				;

				if (RebaseResult.m_ExitCode != 0)
				{
					auto RebaseAbortResult = co_await _Launches.f_Launch(_Repo, {"rebase", "--abort"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
					CStr Message;
					if (RebaseAbortResult.m_ExitCode != 0)
						Message = "Rebase failed: {}. Abort also failed: {}"_f << RebaseResult.f_GetCombinedOut().f_Trim() << RebaseAbortResult.f_GetCombinedOut().f_Trim();
					else
						Message = "Rebase failed, aborted: {}"_f << RebaseResult.f_GetCombinedOut().f_Trim();

					_Launches.f_Output(EOutputType_Error, _Repo, Message);
					co_return DMibErrorInstance(Message);
				}
			}
			else if (!_ChangedConfigFiles.f_IsEmpty())
			{
				// No merge-base (no remote) — commit at HEAD without rebase.
				// The explicit pathspec keeps repo-commit scoped to managed config
				// files only; when one of those tracked files is deleted, `git add
				// -- <path>` still stages that deletion, so `-A` would only risk
				// sweeping in unrelated changes.
				TCVector<CStr> AddParams = {"add", "--"};
				for (auto &File : _ChangedConfigFiles)
					AddParams.f_Insert(File);
				co_await _Launches.f_Launch(_Repo, AddParams, {}, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);

				auto CommitResult = co_await _Launches.f_Launch(_Repo, {"commit", "-m", _CommitMessage}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
				if (CommitResult.m_ExitCode != 0)
				{
					CStr Message = "Failed to commit: {}"_f << CommitResult.f_GetCombinedOut().f_Trim();
					_Launches.f_Output(EOutputType_Error, _Repo, Message);
					co_return DMibErrorInstance(Message);
				}
			}

			CleanupIndexBackup.f_Clear();

			// Restore the user's pre-existing staged entries for unrelated
			// paths (the earlier `git reset` kept them out of the temp
			// commits, and `--autostash` in the merge-base path only brings
			// working-tree changes back — it does not re-stage them).
			//
			// Managed config files are intentionally different: repo-commit
			// takes ownership of their final contents and absorbs them into
			// the generated commit. After restoring the full saved index we
			// therefore reset those config paths back to the new HEAD on
			// purpose, so no pre-existing staged .MRepo/.MSettings state is
			// kept around as a stale staged diff against the freshly written
			// update commit.
			bool bHadIndexFile = IndexBackup.m_bHadIndexFile;
			co_await fg_RestoreRepositoryIndexFile(fg_Move(IndexBackup));

			if (bHadIndexFile && !_AllConfigRelPaths.f_IsEmpty())
			{
				TCVector<CStr> ResetParams = {"reset", "--quiet", "HEAD", "--"};
				for (auto &Path : _AllConfigRelPaths)
					ResetParams.f_Insert(Path);
				co_await _Launches.f_Launch(_Repo, ResetParams, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			}

			_Launches.f_Output(EOutputType_Normal, _Repo, "Updated commit");
			co_return {};
		}

		// Filter _ConfigFilePaths to those that have a submitted depot revision
		// (i.e. were previously tracked in Perforce, regardless of content).
		// Used to distinguish "file existed at last submitted CL but happened
		// to be empty" from "file is newly added on this branch".
		TCFuture<TCSet<CStr>> fg_PerforceGetSubmittedConfigFiles(TCVector<CStr> _ConfigFilePaths, CStr _BaseDir)
		{
			auto BlockingActorCheckout = fg_BlockingActor();
			co_return co_await
				(
					g_Dispatch(BlockingActorCheckout) / [_ConfigFilePaths, _BaseDir]() -> TCSet<CStr>
					{
						CPerforceClientThrow Client;
						auto DepotPaths = fg_PerforceConnectAndResolveDepotPaths(_ConfigFilePaths, _BaseDir, Client);

						TCSet<CStr> Submitted;
						for (umint i = 0; i < _ConfigFilePaths.f_GetLen(); ++i)
						{
							if (!DepotPaths[i].f_IsEmpty())
								Submitted[_ConfigFilePaths[i]];
						}
						return Submitted;
					}
				)
			;
		}

		// Create or update a Perforce changelist with the given description for config files.
		// Does NOT submit the changelist. Moves files from other pending changelists if needed.
		TCFuture<void> fg_PerforceUpdateChangelist
			(
				CGitLaunches _Launches
				, TCVector<CStr> _ConfigFilePaths
				, CStr _BaseDir
				, CStr _CommitMessage
				, CStr _MessageHeader
			)
		{
			auto BlockingActorCheckout = fg_BlockingActor();
			co_await
				(
					g_Dispatch(BlockingActorCheckout) / [_Launches, _ConfigFilePaths, _BaseDir, _CommitMessage, _MessageHeader]()
					{
						CPerforceClientThrow Client;
						auto DepotPaths = fg_PerforceConnectAndResolveDepotPaths(_ConfigFilePaths, _BaseDir, Client);

						// Get the latest submitted CL to compare against. Brand-new files have no
						// depot path yet, so filter them out before querying revisions.
						TCVector<CStr> TrackedDepotPaths;
						for (auto &DepotPath : DepotPaths)
						{
							if (!DepotPath.f_IsEmpty())
								TrackedDepotPaths.f_Insert(DepotPath);
						}

						uint32 LatestCL = 0;
						if (!TrackedDepotPaths.f_IsEmpty())
						{
							auto Revisions = Client.f_GetFileRevisions(TrackedDepotPaths);
							for (auto &File : Revisions.m_Files)
							{
								for (auto &Rev : File.m_Revisions)
								{
									if (Rev.m_ChangeList > 0 && (uint32)Rev.m_ChangeList > LatestCL)
										LatestCL = (uint32)Rev.m_ChangeList;
								}
							}
						}

						// Compare workspace content against last submitted version. New files
						// (empty depot path) have no submitted version, so they're always
						// "changed" and need `p4 add` rather than `p4 edit`. Keep the
						// tracked-file arrays aligned 1:1 — ChangedConfigFiles contains
						// ONLY already-tracked client paths paired with ChangedDepotPaths;
						// brand-new files live in NewConfigFiles and are handled separately.
						// A second pass below augments ChangedConfigFiles with tracked
						// files that are opened in a pending CL other than UpdateReposCL
						// so they get reclaimed into the "Update repositories" CL.
						TCVector<CStr> ChangedConfigFiles;
						TCVector<CStr> ChangedDepotPaths;
						TCVector<CStr> NewConfigFiles;
						for (umint i = 0; i < _ConfigFilePaths.f_GetLen(); ++i)
						{
							if (DepotPaths[i].f_IsEmpty())
							{
								if (CFile::fs_FileExists(_ConfigFilePaths[i]))
									NewConfigFiles.f_Insert(_ConfigFilePaths[i]);
								continue;
							}

							// A managed config file that is missing from the workspace is
							// always an error: repo-commit will not delete config files
							// automatically, the user must restore the file or remove the
							// corresponding entry from the build configuration.
							if (!CFile::fs_FileExists(_ConfigFilePaths[i]))
							{
								CStr Message = "Config file '{}' is missing from the workspace. Restore the file before running repo-commit."_f << _ConfigFilePaths[i];
								_Launches.f_Output(EOutputType_Error, ".", Message);
								DMibError(Message);
							}

							CStr SubmittedContent;
							if (LatestCL > 0)
								Client.f_NoThrow().f_GetTextFileContents("{}@{}"_f << DepotPaths[i] << LatestCL, SubmittedContent);

							CStr WorkspaceContent = CFile::fs_ReadStringFromFile(_ConfigFilePaths[i]);
							if (WorkspaceContent != SubmittedContent)
							{
								ChangedConfigFiles.f_Insert(_ConfigFilePaths[i]);
								ChangedDepotPaths.f_Insert(DepotPaths[i]);
							}
						}

						// Find existing pending changelist matching the commit message header or create a new one.
						// Prefix matching is deliberate: the user submits the CL themselves, so reusing
						// any pending CL whose description starts with the configured header is acceptable.
						// If we reuse such a CL we intentionally replace its full description with the
						// newly generated message so the CL body stays aligned with the current config
						// state, even when the user originally created the CL by hand.
						//
						// Two passes: prefer an exact-description match first, so that
						// when multiple pending CLs share the header prefix we latch
						// onto the one already carrying our current generated body
						// instead of whichever prefix-matching CL Perforce happened
						// to return first.
						CStr const &ChangelistPrefix = fg_EvalMessageHeader(_MessageHeader);

						uint32 UpdateReposCL = 0;
						{
							CStr ClientName;
							Client.f_NoThrow().f_GetClientName(ClientName);

							TCVector<CPerforceClient::CChangeList> PendingCLs;
							Client.f_NoThrow().f_GetChangelists(CStr(), PendingCLs, false, ClientName, "pending");

							for (auto &CL : PendingCLs)
							{
								if (CL.m_Description.f_Trim() == _CommitMessage)
								{
									UpdateReposCL = CL.m_ChangeID;
									break;
								}
							}

							if (UpdateReposCL == 0)
							{
								for (auto &CL : PendingCLs)
								{
									if (CL.m_Description.f_Trim().f_StartsWith(ChangelistPrefix))
									{
										UpdateReposCL = CL.m_ChangeID;
										break;
									}
								}
							}
						}

						// Fetch the matched pending CL once so we can both identify
						// which config files are already in it (and therefore don't
						// need reclaiming) and later derive bExistingDescriptionMatches
						// / ForeignFilesInCL without a second Perforce query.
						CPerforceClient::CChangeList ExistingCLData;
						bool bHasExistingCLData = false;
						if (UpdateReposCL != 0)
							bHasExistingCLData = Client.f_NoThrow().f_GetChangelist(UpdateReposCL, ExistingCLData);

						TCSet<CStr> OurCLFiles;
						if (bHasExistingCLData)
						{
							for (auto &File : ExistingCLData.m_Files)
								OurCLFiles[File.m_Name];
						}

						// Reclaim tracked config files that the user has opened in a
						// pending CL other than UpdateReposCL, even when workspace
						// content matches the submitted version. Without this, such
						// files stay stranded in the foreign CL and a later submit of
						// the "Update repositories" CL would silently omit them. Files
						// already in our own CL (OurCLFiles) are skipped because the
						// later MoveToChangelist would be a no-op for them.
						{
							TCVector<CStr> OpenedList;
							Client.f_NoThrow().f_GetOpened(CStr(), CStr(), OpenedList);

							TCSet<CStr> OpenedElsewhere;
							for (auto &DepotPath : OpenedList)
							{
								if (!OurCLFiles.f_FindEqual(DepotPath))
									OpenedElsewhere[DepotPath];
							}

							TCSet<CStr> AlreadyIncluded;
							for (auto &DepotPath : ChangedDepotPaths)
								AlreadyIncluded[DepotPath];

							for (umint i = 0; i < _ConfigFilePaths.f_GetLen(); ++i)
							{
								if (DepotPaths[i].f_IsEmpty())
									continue;
								if (AlreadyIncluded.f_FindEqual(DepotPaths[i]))
									continue;
								if (!OpenedElsewhere.f_FindEqual(DepotPaths[i]))
									continue;

								ChangedConfigFiles.f_Insert(_ConfigFilePaths[i]);
								ChangedDepotPaths.f_Insert(DepotPaths[i]);
							}
						}

						// Build the set of depot paths that legitimately belong in the
						// "Update repositories" changelist. Both already-tracked config
						// files being edited and brand-new config files about to be
						// `p4 add`ed are computed here so we can recognise any OTHER
						// files the user may have parked in a reused CL and evict them.
						TCSet<CStr> ConfigDepotPathSet;
						for (auto &DepotPath : ChangedDepotPaths)
							ConfigDepotPathSet[DepotPath];
						for (auto &NewFile : NewConfigFiles)
						{
							CStr NewDepotPath;
							if (Client.f_NoThrow().f_GetDepotPath(NewFile, NewDepotPath) && !NewDepotPath.f_IsEmpty())
								ConfigDepotPathSet[NewDepotPath];
						}

						// Collect any non-config files a previous repo-commit run left in
						// the reused CL so we can evict them back to the default
						// changelist. Eviction only happens when the existing CL's
						// description EXACTLY matches what we'd generate — that's the
						// signal that we created it. If the description only matches
						// by prefix the user may have hand-crafted this CL with extra
						// files they want to keep, so leave it alone. Computed before
						// the "already up to date" short-circuit so that reruns still
						// clean up stale foreign entries when nothing else has changed.
						//
						// bExistingDescriptionMatches gates the short-circuits below:
						// a prefix-only match means the generated message is not what
						// the pending CL currently shows (e.g. the user changed
						// PerforceRoot.RepoCommit.MessageHeader / TransformScript, or
						// new sub-repo commits changed the body). In that case we
						// still reuse the same CL and update its description in the
						// fallthrough — we just can't claim "already up to date".
						bool bExistingDescriptionMatches = false;
						TCVector<CStr> ForeignFilesInCL;
						if (bHasExistingCLData && ExistingCLData.m_Description.f_Trim() == _CommitMessage)
						{
							bExistingDescriptionMatches = true;
							for (auto &File : ExistingCLData.m_Files)
							{
								if (!ConfigDepotPathSet.f_FindEqual(File.m_Name))
									ForeignFilesInCL.f_Insert(File.m_Name);
							}
						}

						// Nothing to change and nothing stale to evict — truly up to
						// date. Requires an exact-matching CL; otherwise we need to
						// refresh its description (or create a brand new CL when no
						// prefix match was found at all, e.g. after the user changes
						// PerforceRoot.RepoCommit.MessageHeader so the old pending CL
						// no longer matches the new header).
						if (bExistingDescriptionMatches && ChangedConfigFiles.f_IsEmpty() && NewConfigFiles.f_IsEmpty() && ForeignFilesInCL.f_IsEmpty())
						{
							_Launches.f_Output(EOutputType_Normal, ".", "Already up to date");
							return;
						}

						// Only foreign files to evict — do that and stop, without
						// creating or touching the description of any CL.
						if (bExistingDescriptionMatches && ChangedConfigFiles.f_IsEmpty() && NewConfigFiles.f_IsEmpty())
						{
							Client.f_NoThrow().f_MoveToChangelist(ForeignFilesInCL, 0);
							_Launches.f_Output(EOutputType_Normal, ".", "Evicted {} foreign file(s) from changelist {}"_f << ForeignFilesInCL.f_GetLen() << UpdateReposCL);
							return;
						}

						// Check if already up to date: existing changelist with matching description
						// and all changed files already open in it. New files have no depot path
						// yet, so we can only verify the already-tracked subset here; if any new
						// file exists we always fall through to add+open below.
						if (bExistingDescriptionMatches && NewConfigFiles.f_IsEmpty() && ForeignFilesInCL.f_IsEmpty())
						{
							CPerforceClient::CChangeList CLData;
							if (Client.f_NoThrow().f_GetChangelist(UpdateReposCL, CLData))
							{
								TCSet<CStr> FilesInCL;
								for (auto &File : CLData.m_Files)
									FilesInCL[File.m_Name];

								bool bAllFilesInCL = true;
								for (auto &DepotPath : ChangedDepotPaths)
								{
									if (!FilesInCL.f_FindEqual(DepotPath))
									{
										bAllFilesInCL = false;
										break;
									}
								}

								if (bAllFilesInCL)
								{
									_Launches.f_Output(EOutputType_Normal, ".", "Already up to date");
									return;
								}
							}
						}

						// Nothing to put in a new CL and no existing CL to refresh —
						// don't create an empty "Update repositories" changelist.
						// ChangedConfigFiles already covers tracked files opened in
						// any pending CL other than UpdateReposCL (see reclaim pass
						// above), so this only fires when there is truly nothing to
						// move: no content diffs, no new files, and nothing stranded
						// in a foreign pending CL.
						if (UpdateReposCL == 0 && ChangedConfigFiles.f_IsEmpty() && NewConfigFiles.f_IsEmpty())
						{
							_Launches.f_Output(EOutputType_Normal, ".", "Already up to date");
							return;
						}

						// Create a brand new CL when no prefix match was found, or
						// refresh the description of the prefix-matched CL to the
						// newly generated message (covering both no-op description
						// changes and genuine content updates). The CL itself is
						// not migrated: an older pending CL whose description used
						// a now-obsolete header stays visible in `p4 changes` with
						// its original description, but the reclaim pass above
						// pulls any config files it still held into ChangedDepotPaths
						// so they end up in this fresh CL. The old CL is left
						// empty for the user to discard (or repurpose) manually.
						if (UpdateReposCL == 0)
							Client.f_NoThrow().f_CreateChangelist(_CommitMessage, {}, {}, UpdateReposCL);
						else
							Client.f_NoThrow().f_SetChangelistDescription(UpdateReposCL, _CommitMessage);

						// Move foreign files out before we start adding our own, so
						// a later `f_MoveToChangelist` call for the config files
						// doesn't accidentally undo the eviction if depot paths
						// overlap. `p4 reopen -c 0` moves files to the default CL.
						if (!ForeignFilesInCL.f_IsEmpty())
							Client.f_NoThrow().f_MoveToChangelist(ForeignFilesInCL, 0);

						// Open already-tracked files for edit, then move all to our
						// changelist. ChangedConfigFiles and ChangedDepotPaths are
						// aligned 1:1 here. Brand-new files go through `p4 add`
						// below instead.
						//
						// `p4 edit` is a no-op for files already opened for edit (in
						// our CL or a foreign one — the subsequent f_MoveToChangelist
						// migrates them) and combines cleanly with pending
						// integrate/branch actions. For delete/add/move/* we let the
						// throwing variant surface the Perforce error: the user needs
						// to resolve that state by hand (typically `p4 revert`) before
						// repo-commit can take over, otherwise the generated CL would
						// carry the wrong action.
						for (umint i = 0; i < ChangedConfigFiles.f_GetLen(); ++i)
							Client.f_OpenForEdit(ChangedConfigFiles[i]);

						// `p4 add` a brand-new file. If it's already opened from a
						// previous run, skip the call so the throwing variant only
						// fires on real failures (file outside client view, denied
						// by protections, etc.) — a blanket f_NoThrow would silently
						// swallow those and leave the config file outside the CL.
						for (auto &NewFile : NewConfigFiles)
						{
							bool bAlreadyOpened = false;
							CStr NewDepotPath;
							if (Client.f_NoThrow().f_GetDepotPath(NewFile, NewDepotPath))
							{
								TCVector<CStr> Opened;
								Client.f_NoThrow().f_GetOpened(NewDepotPath, CStr(), Opened);
								bAlreadyOpened = !Opened.f_IsEmpty();
							}
							if (!bAlreadyOpened)
								Client.f_Add(NewFile);
						}

						if (!ChangedDepotPaths.f_IsEmpty())
							Client.f_MoveToChangelist(ChangedDepotPaths, UpdateReposCL);

						if (!NewConfigFiles.f_IsEmpty())
						{
							TCVector<CStr> NewDepotPaths;
							for (auto &NewFile : NewConfigFiles)
							{
								CStr NewDepotPath;
								if (Client.f_NoThrow().f_GetDepotPath(NewFile, NewDepotPath) && !NewDepotPath.f_IsEmpty())
									NewDepotPaths.f_Insert(NewDepotPath);
							}
							if (!NewDepotPaths.f_IsEmpty())
								Client.f_MoveToChangelist(NewDepotPaths, UpdateReposCL);
						}

						_Launches.f_Output(EOutputType_Normal, ".", "Updated changelist {}"_f << UpdateReposCL);
					}
				)
			;

			co_return {};
		}
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_CommitRepos
		(
			CGenerateOptions const &_GenerateOptions
			, ERepoCommitFlag _Flags
			, uint32 _MaxCommitsPerSection
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		// Phase 1: Preparation - update all config files with current HEAD hashes
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(CRepoFilter(), *this, mp_Data, EGetRepoFlag::mc_IncludeRepoCommit);
		auto const &FilteredRepos = FilteredRepositories.m_FilteredRepositories;

		auto ReposOrdered = fg_GetRepos(*this, mp_Data, EGetRepoFlag::mc_IncludeRepoCommit);
		auto MainBranchInfo = fg_GetMainRepoBranchInfo(f_GetBaseDir(), ReposOrdered);

		CGitLaunches Launches{f_GetGitLaunchOptions("repo-commit"), "Committing repos"};
		auto DestroyLaunches = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(FilteredRepos);

		// Lazy temp dir for transform scripts — created on first use
		CStr TransformTempDir;
		auto fGetTransformTempDir = [&]() -> CStr const &
			{
				if (TransformTempDir.f_IsEmpty())
				{
					TransformTempDir = mp_OutputDir / "Temp" / fg_FastRandomID();
					CFile::fs_CreateDirectory(TransformTempDir);
				}
				return TransformTempDir;
			}
		;
		auto CleanupTransformTempDir = co_await fg_AsyncDestroy
			(
				[&]() -> TCFuture<void>
				{
					// Safe: ECoroutineFlag_CaptureExceptions itself does not suspend, so
					// this still runs before the cleanup coroutine's first real suspension.
					co_await ECoroutineFlag_CaptureExceptions;
					if (!TransformTempDir.f_IsEmpty())
						CFile::fs_DeleteDirectoryRecursive(TransformTempDir);
					co_return {};
				}
			)
		;

		// Build map of all repositories by location and track dependency stage per display name.
		// Also build a parallel identity-keyed map: external repos (outside BaseDir) appear in
		// .MRepo files keyed by identity rather than path, and callers need to translate back
		// to locations when walking m_Configs.
		TCSharedPointer<TCMap<CStr, CRepository *>> pRepositoryByLocation = fg_Construct();
		TCSharedPointer<TCMap<CStr, CRepository *>> pRepositoryByIdentity = fg_Construct();
		TCMap<CStr, umint> DisplayNameStages;
		{
			umint Stage = 0;
			for (auto &Repos : FilteredRepos)
			{
				for (auto *pRepo : Repos)
				{
					(*pRepositoryByLocation)[pRepo->m_Location] = pRepo;
					if (!pRepo->m_Identity.f_IsEmpty())
						(*pRepositoryByIdentity)[pRepo->m_Identity] = pRepo;
					CStr DisplayName = pRepo->f_GetIdentifierName(f_GetBaseDir(), f_GetBaseDir());
					if (DisplayName.f_IsEmpty())
						DisplayName = ".";
					DisplayNameStages[DisplayName] = Stage;
				}
				++Stage;
			}
		}

		// Phase 2: Build ownership map - which config files belong to which repo
		TCMap<CStr, TCSet<CStr>> ConfigFilesByRepo;
		TCSet<CStr> OrphanedConfigFiles;
		for (auto &pRepo : *pRepositoryByLocation)
		{
			auto &Repo = *pRepo;
			if (Repo.m_ConfigFile.f_IsEmpty())
				continue;

			CStr ConfigDirectory = CFile::fs_GetPath(Repo.m_ConfigFile);
			CStr OwnerLocation;
			auto *pOwner = CBuildSystem::fs_FindContainingPath(*pRepositoryByLocation, ConfigDirectory, OwnerLocation);
			if (pOwner)
				ConfigFilesByRepo[(*pOwner)->m_Location][Repo.m_ConfigFile];
			else
				OrphanedConfigFiles[Repo.m_ConfigFile];
		}

		// Detect Perforce root and collect Perforce-managed config files
		bool bIsPerforceRoot = !CFile::fs_FileExists(f_GetBaseDir() + "/.git", EFileAttrib_Directory | EFileAttrib_File) && CPerforceClient::fs_HasP4Config(f_GetBaseDir());
		TCSet<CStr> PerforceRootConfigFiles;
		if (bIsPerforceRoot)
			PerforceRootConfigFiles = fg_CollectPerforceRootConfigFiles(*pRepositoryByLocation, f_GetBaseDir());

		// Every managed config file needs an owning %Repository ancestor.
		// Without one, fs_FindContainingPath returns no owner in Phase 2 and
		// ConfigFilesByRepo silently drops the entry, so Phase 4 never
		// produces the corresponding "Update repositories" commit. The most
		// common case is a root-level config file in a workspace that never
		// declared `%Repository "."`. Perforce roots have no `.git` and route
		// their root-level config files through Phase 5 via
		// fg_CollectPerforceRootConfigFiles, so the guard is gated on the git
		// path. Workspaces with all configs nested under declared child
		// %Repository entries produce no orphans and pass through.
		if (!bIsPerforceRoot && !OrphanedConfigFiles.f_IsEmpty())
		{
			CStr FilesList;
			for (auto &File : OrphanedConfigFiles)
			{
				if (!FilesList.f_IsEmpty())
					FilesList += ", ";
				FilesList += File;
			}
			CStr Message = "repo-commit cannot find an owning %Repository for these config files: {}. Declare a parent %Repository that contains them (e.g. %Repository \".\" at the workspace base directory '{}')."_f << FilesList << f_GetBaseDir();
			Launches.f_Output(EOutputType_Error, ".", Message);
			co_return DMibErrorInstance(Message);
		}

		// Phase 3: Resolve start commits by walking the dependency tree.
		// Only the root repo compares against origin — all children get their
		// start commits from the .MRepo config files at their parent's start commit.

		// Build reverse map: config file → repos tracked by it
		TCMap<CStr, TCVector<CRepository *>> ReposByConfigFile;
		for (auto &pRepo : *pRepositoryByLocation)
		{
			if (!pRepo->m_ConfigFile.f_IsEmpty())
				ReposByConfigFile[pRepo->m_ConfigFile].f_Insert(pRepo);
		}

		TCMap<CStr, CStr> RootSeed;
		// Config files that existed in Perforce at the last submitted CL.
		// Determined from depot presence, not content — an already-tracked
		// file that happened to be zero bytes at LatestCL is still "owner
		// existed at start". A config file newly added on this branch does
		// not claim ownership — the same tradeoff as the git path, which
		// prevents repos moved from a previously-tracked config file into a
		// new one from being misreported as `(added @ …)`.
		TCSet<CStr> PerforceRootConfigFilesAtStart;
		if (bIsPerforceRoot && !PerforceRootConfigFiles.f_IsEmpty())
		{
			// Perforce root: seed from last submitted config files
			TCVector<CStr> ConfigFilePaths;
			for (auto &ConfigFile : PerforceRootConfigFiles)
				ConfigFilePaths.f_Insert(ConfigFile);

			auto [LatestCL, SubmittedConfigFiles] = co_await
				(
					fg_PerforceGetLatestChangelist(ConfigFilePaths, f_GetBaseDir())
					+ fg_PerforceGetSubmittedConfigFiles(ConfigFilePaths, f_GetBaseDir())
				)
			;

			PerforceRootConfigFilesAtStart = fg_Move(SubmittedConfigFiles);

			if (!LatestCL.f_IsEmpty() && LatestCL != "0")
			{
				for (auto &ConfigFile : PerforceRootConfigFilesAtStart)
				{
					auto [SubmittedContent, Unused] = co_await fg_PerforceGetConfigFileContents(ConfigFile, LatestCL, LatestCL);

					if (!SubmittedContent.f_IsEmpty())
					{
						auto CaptureScope = co_await (g_CaptureExceptions % "Exception parsing Perforce config file");
						auto ParsedConfig = CStateHandler::fs_ParseConfigFile(SubmittedContent, ConfigFile);
						for (auto &Config : ParsedConfig.m_Configs)
						{
							// RootSeed feeds fg_ResolveStartCommits, which is keyed
							// by m_Location — translate identity keys back to
							// locations for external repos so they participate in
							// start-commit resolution instead of being silently
							// skipped by a location-based lookup.
							auto &ConfigKey = ParsedConfig.m_Configs.fs_GetKey(Config);
							CStr ChildLocation;
							if (Config.m_bExternalPath)
							{
								auto *pExternalRepo = pRepositoryByIdentity->f_FindEqual(ConfigKey);
								if (!pExternalRepo)
									continue;
								ChildLocation = (*pExternalRepo)->m_Location;
							}
							else
								ChildLocation = ConfigKey;

							if (!Config.m_Hash.f_IsEmpty())
								RootSeed[ChildLocation] = Config.m_Hash;
						}
					}
				}
			}
		}
		else if (CFile::fs_FileExists(f_GetBaseDir() + "/.git", EFileAttrib_Directory | EFileAttrib_File))
		{
			// Git root: seed from merge-base with origin/defaultBranch when
			// available. An empty result is tolerated (bootstrap mode); the
			// helper emits warnings explaining the fallback.
			CStr MergeBase = co_await fg_ResolveMergeBaseWithBootstrapFallback
				(
					Launches
					, f_GetBaseDir()
					, CStr(".")
					, MainBranchInfo.m_DefaultBranch
				)
			;

			if (!MergeBase.f_IsEmpty())
				RootSeed[f_GetBaseDir()] = MergeBase;
		}

		auto pStartCommits = co_await fg_ResolveStartCommits(Launches, f_GetBaseDir(), RootSeed, FilteredRepos, ConfigFilesByRepo, ReposByConfigFile);

		// Phase 4: Process stages depth-first
		for (auto &Repos : FilteredRepos.f_Reverse())
		{
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;
				auto OnExit = g_OnScopeExit / [&]
					{
						Launches.f_RepoDone();
					}
				;

				auto *pConfigFiles = ConfigFilesByRepo.f_FindEqual(Repo.m_Location);
				if (!pConfigFiles || pConfigFiles->f_IsEmpty())
					continue;

				// In a Perforce-root workspace the base directory has no `.git`
				// and is therefore never registered as a %Repository, so it
				// cannot appear in pRepositoryByLocation (and by extension
				// ConfigFilesByRepo). Its config files are instead collected
				// by fg_CollectPerforceRootConfigFiles and handled by phase 5.
				// Reaching this point with `Repo.m_Location == f_GetBaseDir()`
				// while `bIsPerforceRoot` is true would mean a misconfigured
				// workspace that declared a %Repository at the base of a P4
				// client; fg_RepoPreFlight would then run `git merge-base`
				// against a non-git directory and fail with a confusing error.
				DMibCheck(!(bIsPerforceRoot && Repo.m_Location == f_GetBaseDir()));

				// A managed config file that is missing from the workspace is
				// always an error: repo-commit will not delete config files
				// automatically, the user must restore the file or remove the
				// corresponding entry from the build configuration.
				for (auto &ConfigFile : *pConfigFiles)
				{
					if (!CFile::fs_FileExists(ConfigFile))
					{
						CStr Message = "Config file '{}' is missing from the workspace. Restore the file before running repo-commit."_f << ConfigFile;
						Launches.f_Output(EOutputType_Error, Repo, Message);
						co_return DMibErrorInstance(Message);
					}
				}

				TCSet<CStr> AllConfigRelPaths;
				for (auto &ConfigFile : *pConfigFiles)
					AllConfigRelPaths[CFile::fs_MakePathRelative(ConfigFile, Repo.m_Location)];

				CStr DefaultBranch = Repo.m_OriginProperties.m_DefaultBranch;
				if (DefaultBranch.f_IsEmpty())
				{
					CStr Message = "Repository '{}' has no Repository.DefaultBranch configured; cannot resolve merge-base for repo-commit."_f << Launches.f_GetRepoName(Repo);
					Launches.f_Output(EOutputType_Error, Repo, Message);
					co_return DMibErrorInstance(Message);
				}

				// For the root repo use the pre-resolved start commits from the
				// merge-base chain. For all other repos seed from the
				// merge-base with origin/{defaultBranch} so the nested section
				// describes commits introduced on this branch since the branch
				// point. Each resolution walks the full subtree so grandchild
				// commits use a consistent range.
				bool bIsRootRepo = Repo.m_Location == f_GetBaseDir();
				TCSharedPointer<TCMap<CStr, CStr> const> pRepoStartCommits;
				if (bIsRootRepo)
					pRepoStartCommits = pStartCommits;
				else
				{
					CStr MergeBase = co_await fg_ResolveMergeBaseWithBootstrapFallback(Launches, Repo, Repo, DefaultBranch);

					if (MergeBase.f_IsEmpty())
						pRepoStartCommits = fg_Construct<TCMap<CStr, CStr>>();
					else
					{
						TCMap<CStr, CStr> RepoSeed;
						RepoSeed[Repo.m_Location] = MergeBase;
						pRepoStartCommits = co_await fg_ResolveStartCommits(Launches, f_GetBaseDir(), RepoSeed, FilteredRepos, ConfigFilesByRepo, ReposByConfigFile);
					}
				}

				// Collect all config files in the subtree so fg_CollectChildCommits
				// picks up transitive children using the same coherent range.
				TCSet<CStr> SubtreeConfigFiles;
				for (auto &Entry : *pRepoStartCommits)
				{
					auto *pConfigs = ConfigFilesByRepo.f_FindEqual(pRepoStartCommits->fs_GetKey(Entry));
					if (pConfigs)
					{
						for (auto &ConfigFile : *pConfigs)
							SubtreeConfigFiles[ConfigFile];
					}
				}

				// Launch child commit collection and preflight concurrently
				auto [RepoCommits, Preflight] = co_await
					(
						fg_CollectChildCommits(Launches, SubtreeConfigFiles, f_GetBaseDir(), pRepoStartCommits, pRepositoryByLocation, pRepositoryByIdentity, TCSet<CStr>())
						+ fg_RepoPreFlight(Launches, Repo, *pConfigFiles, AllConfigRelPaths, DefaultBranch)
					)
				;

				// Analyze existing commits (parallel git queries inside)
				auto CommitAnalyses = co_await fg_AnalyzeCommits(Launches, Repo, Preflight.m_CommitHashes, AllConfigRelPaths);

				// Generate message and commit/rebase. `[skip ci]` is only
				// emitted on the root repository's commit. Nested git
				// sub-repos are intentionally allowed to run their own CI;
				// the user-facing `--skip-ci` flag is scoped to the
				// outer "Update repositories" commit that ties the whole
				// world together. (The Perforce path below is also a root
				// commit, so it forwards the flag unconditionally.)
				CStr CommitMessage = fg_GenerateCommitMessage
					(
						RepoCommits
						, DisplayNameStages
						, bIsRootRepo && (_Flags & ERepoCommitFlag_SkipCi)
						, _MaxCommitsPerSection
						, Repo.m_RepoCommitOptions ? Repo.m_RepoCommitOptions->m_MessageHeader : CStr()
					)
				;

				if (Repo.m_RepoCommitOptions && Repo.m_RepoCommitOptions->m_TransformScript)
				{
					CommitMessage = co_await fg_RunAndVerifyTransformScript
						(
							Launches
							, Launches.f_GetRepoName(Repo)
							, Repo.m_Location
							, Repo.m_RepoCommitOptions->m_TransformScript
							, Repo.m_RepoCommitOptions->m_MessageHeader
							, fGetTransformTempDir()
							, CommitMessage
						)
					;
				}

				co_await fg_CommitAndRebase
					(
						Launches
						, Repo
						, mp_OutputDir
						, AllConfigRelPaths
						, Preflight.m_ChangedConfigFiles
						, Preflight.m_UntrackedConfigFiles
						, Preflight.m_MergeBase
						, CommitAnalyses
						, CommitMessage
					)
				;
			}

			// After processing this stage, re-run fp_HandleRepositories to update
			// parent config files with the new HEAD hashes from just-committed repos
			{
				DMibLock(mp_GeneratedFilesLock);
				for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; )
				{
					iFile->m_Workspaces.f_Remove(CStr());
					if (iFile->m_Workspaces.f_IsEmpty())
					{
						iFile.f_Remove();
						continue;
					}
					++iFile;
				}
			}

			if (auto Retry = co_await fp_HandleRepositories(GenerateState.m_GeneratorValues))
				co_return Retry;
		}

		// Phase 5: Handle Perforce root config files
		if (bIsPerforceRoot && !PerforceRootConfigFiles.f_IsEmpty())
		{
			// Collect all subtree config files reachable from Perforce start commits
			TCSet<CStr> PerforceSubtreeConfigFiles = PerforceRootConfigFiles;
			for (auto &Entry : *pStartCommits)
			{
				auto *pConfigs = ConfigFilesByRepo.f_FindEqual(pStartCommits->fs_GetKey(Entry));
				if (pConfigs)
				{
					for (auto &ConfigFile : *pConfigs)
						PerforceSubtreeConfigFiles[ConfigFile];
				}
			}

			auto PerforceRootCommits = co_await fg_CollectChildCommits
				(
					Launches
					, PerforceSubtreeConfigFiles
					, f_GetBaseDir()
					, pStartCommits
					, pRepositoryByLocation
					, pRepositoryByIdentity
					, PerforceRootConfigFilesAtStart
				)
			;

			// The Perforce root is not a %Repository entity, so the per-repo
			// Repository.RepoCommit property cannot attach to it. Read the
			// global PerforceRoot.RepoCommit property off the root entity
			// via fg_GetPerforceRootRepoCommitOptions instead — the same
			// helper drives list-commits, so the header configured here is
			// also what list-commits uses to recognise repo-commit entries.
			CStr RootMessageHeader;
			CStr RootTransformScript;
			if (auto RootOptions = fg_GetPerforceRootRepoCommitOptions(*this, mp_Data))
			{
				RootMessageHeader = RootOptions->m_MessageHeader;
				RootTransformScript = RootOptions->m_TransformScript;
			}

			CStr CommitMessage = fg_GenerateCommitMessage(PerforceRootCommits, DisplayNameStages, _Flags & ERepoCommitFlag_SkipCi, _MaxCommitsPerSection, RootMessageHeader);

			if (!RootTransformScript.f_IsEmpty())
			{
				CommitMessage = co_await fg_RunAndVerifyTransformScript
					(
						Launches
						, "."
						, f_GetBaseDir()
						, RootTransformScript
						, RootMessageHeader
						, fGetTransformTempDir()
						, CommitMessage
					)
				;
			}

			TCVector<CStr> ConfigFilePaths;
			for (auto &ConfigFile : PerforceRootConfigFiles)
				ConfigFilePaths.f_Insert(ConfigFile);

			co_await fg_PerforceUpdateChangelist(Launches, ConfigFilePaths, f_GetBaseDir(), CommitMessage, RootMessageHeader);
		}

		co_return ERetry_None;
	}
}
