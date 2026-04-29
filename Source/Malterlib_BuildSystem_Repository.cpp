// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Perforce/Wrapper>
#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Encoding/EJson>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/Git/LfsReleaseStore>
#include <Mib/Git/Helpers/ConfigParser>
#include <Mib/Git/Helpers/Credentials>
#include <Mib/Git/HostingProvider>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Process/ProcessLaunch>
#include <Mib/Process/ProcessLaunchActor>
#include <Mib/Web/HTTP/URL>

constexpr static ch8 const gc_pHookDispatcherScript[] =
	{
		#embed "Malterlib_BuildSystem_Repository_HookDispatcher.sh"
		, '\0'
	}
;

CStr fg_ReconcileHelp(EAnsiEncodingFlag _AnsiFlags)
{
	NMib::NBuildSystem::NRepository::CColors Colors(_AnsiFlags);

	return R"---(
Changes in sub-repositories needs to be reconciled.

Choose how you want to reconcile changes:

Accept recommended actions   : {0}./mib update-repos '--reconcile=*:auto'{1}
Rebase all                   : {0}./mib update-repos '--reconcile=*:rebase'{1}
Reset all                    : {0}./mib update-repos '--reconcile=*:reset'{1}
No action, leave as is       : {0}./mib update-repos '--reconcile=*:leave'{1}

To choose separate action for different repositories you can specify wildcards. The last matching wildcard wins:
{0}./mib update-repos '--reconcile=*:auto,External/*:reset'{1}

To force the action even for repositories that you have not yet seen
{0}./mib update-repos '--reconcile=*:auto --reconcile-force'{1}

To disable stashing of local changes when switching branches:
{0}./mib update-repos '--reconcile=*:auto --no-stash'{1}

To show current status without reconciling use:
{0}./mib status --skip-update{1}
)---"_f
		<< Colors.f_RepositoryName()
		<< Colors.f_Default()
	;
}


// The follow-up commands here intentionally omit --no-apply-policy-pretend even
// though --apply-policy defaults to pretend mode. The two-step UX is deliberate:
// --apply-policy can rewrite permissions, branch protection, GenericRules and
// ActionsSettings on every remote, and a copy-pasted command without a dry-run
// could mass-rewrite production policy. The first invocation runs in pretend
// mode and lists every change as "Would have: ...". The pretend-mode help line
// emitted at the end of the apply-policy block then surfaces the actual-apply
// flag in context, on the same screen as the proposed changes. The user opts
// in by re-running with --no-apply-policy-pretend.
CStr fg_ReconcileMissingHelp(EAnsiEncodingFlag _AnsiFlags)
{
	NMib::NBuildSystem::NRepository::CColors Colors(_AnsiFlags);

	return R"---(
Origin does not exist for one or more sub-repositories.

Choose how you want to recover:

Fork from a supported remote         : {0}./mib update-repos '--reconcile=*:fork-remote'{1}
Clone from a non-origin remote       : {0}./mib update-repos '--reconcile=*:clone-remote'{1}
Create missing repositories locally  : {0}./mib update-repos '--reconcile=*:init'{1}

To choose separate action for different repositories you can specify wildcards. The last matching wildcard wins:
{0}./mib update-repos '--reconcile=*:fork-remote,External/*:init'{1}

{0}init{1} and recovering from an empty pre-existing origin both require a working git identity
({0}user.name{1} and {0}user.email{1}). If your machine has no global or per-repo identity, run
{0}git config --global user.name "..."{1} and {0}git config --global user.email "..."{1} first.

Use {0}fork-remote{1} when a remote on a supported hosting provider (e.g. GitHub) has the
repository — this creates origin as a fork in one step. Follow up with
{0}./mib update-repos --apply-policy{1} if your repository has a policy configured (to apply
permissions, branch protection, etc.); {0}./mib push{1} only if the main repo is on a feature
branch.

Use {0}clone-remote{1} when a remote on an unsupported provider has it — requires
{0}./mib update-repos --apply-policy --apply-policy-create-missing{1} followed by {0}./mib push{1}
afterwards to create origin and establish upstream tracking.

Use {0}init{1} when no remote has it yet — this creates a repo with a placeholder initial
commit so {0}./mib push{1} has something to push; follow up with
{0}./mib update-repos --apply-policy --apply-policy-create-missing{1} then {0}./mib push{1}, and
use {0}git commit --amend{1} later if you want to replace the placeholder with real content.
)---"_f
		<< Colors.f_RepositoryName()
		<< Colors.f_Default()
	;
}


CStr fg_ReconcileRemovedHelp(EAnsiEncodingFlag _AnsiFlags)
{
	NMib::NBuildSystem::NRepository::CColors Colors(_AnsiFlags);

	return R"---(
Removed sub-repositories needs to be reconciled.

Choose how you want to reconcile changes:

Leave removed repositories on disk    : {0}./mib update-repos '--reconcile-removed=*:leave'{1}
Delete removed repositories from disk : {0}./mib update-repos '--reconcile-removed=*:delete'{1}

{2}Warning:{1} Specifying {0}delete{1} for reconcile removed will delete the repository and any unpushed work permanently.

To choose separate action for different repositories you can specify wildcards. The last matching wildcard wins:
{0}./mib update-repos '--reconcile-removed=*:leave,External/*:delete'{1}

To force the action even for repositories that you have not yet seen the recommended action for
{0}./mib update-repos '--reconcile-remove=*:delete' --reconcile-force{1}

To show current status without reconciling use:
{0}./mib status --skip-update{1}
)---"_f
		<< Colors.f_RepositoryName()
		<< Colors.f_Default()
		<< Colors.f_StatusError()
	;
}

namespace NMib::NBuildSystem
{
	namespace
	{
		constinit TCAggregate<CMutual> g_SubmoduleAddLock = {DAggregateInit};
	}

	namespace NRepository
	{
		DMibImpErrorClassImplement(CExceptionBuildSystemReconcileHelp);

		CRepository::CRepository(CStr const &_Name)
			: m_Name(_Name)
		{
		}

		CStr const &CRepository::f_GetName() const
		{
			return m_Name;
		}

		CStr const &CRemote::CCompare::operator () (CRemote const &_Node)
		{
			return _Node.m_Name;
		}

		CRemote::CRemote(CStr const &_Name)
			: m_Name(_Name)
		{
		}

		CRemote::CRemote(CRemote const &_Other)
			: m_Name(_Other.m_Name)
			, m_Properties(_Other.m_Properties)
			, m_bCanPush(_Other.m_bCanPush)
		{
		}

		CRemote::CRemote(CRemote &&_Other)
			: m_Name(fg_Move(fg_RemoveQualifiers(_Other.m_Name)))
			, m_Properties(fg_Move(_Other.m_Properties))
			, m_bCanPush(_Other.m_bCanPush)
		{
		}

		CRemotes &CRemotes::operator = (CRemotes const &_Other)
		{
			m_Remotes.f_Clear();
			m_OrderedRemotes = _Other.m_OrderedRemotes;

			for (auto &Remote : m_OrderedRemotes)
				m_Remotes.f_Insert(Remote);

			return *this;
		}

		CRemotes::CRemotes(CRemotes const &_Other)
			: m_OrderedRemotes(_Other.m_OrderedRemotes)
		{
			for (auto &Remote : m_OrderedRemotes)
				m_Remotes.f_Insert(Remote);
		}

		CRemote &CRemotes::operator [] (CStr const &_Name)
		{
			auto *pRemote = m_Remotes.f_FindEqual(_Name);
			if (pRemote)
				return *pRemote;

			auto &Remote = m_OrderedRemotes.f_Insert(fg_Construct(_Name));
			m_Remotes.f_Insert(Remote);
			return Remote;
		}

		CColors::CColors(EAnsiEncodingFlag _AnsiFlags)
			: CAnsiEncoding(_AnsiFlags)
		{
		}

		NStr::CStr CColors::f_RepositoryName() const
		{
			return f_Default() + f_Foreground256(221);
		}

		NStr::CStr CColors::f_ChangedBranchName(CStr const &_Name) const
		{
			if (_Name == gc_Str<"origin">.m_Str || _Name.f_StartsWith("origin/"))
				return f_Default() + f_Foreground256(221);
			else
				return f_Default() + f_Foreground256(214);
		}

		NStr::CStr CColors::f_BranchName() const
		{
			return f_Default();
		}

		NStr::CStr CColors::f_ToCommit() const
		{
			return f_Default() + f_Foreground256(46);
		}

		NStr::CStr CColors::f_ToPush() const
		{
			return f_Default() + f_Foreground256(32);
		}

		NStr::CStr CColors::f_ToPull() const
		{
			return f_Default() + f_Foreground256(9);
		}

		CStr CRepository::f_GetIdentifierName(CStr const &_BasePath, CStr const &_Root) const
		{
			if (!m_Location.f_StartsWith(_Root))
				return "~" + m_Identity;
			return CFile::fs_MakePathRelative(m_Location, _BasePath);
		}

		CRepositoryConfig const *CConfigFile::f_GetConfig(CRepository const &_Repo, CStr const &_BasePath)
		{
			CStr Identifier;
			if (!_Repo.m_Location.f_StartsWith(_BasePath))
				Identifier = _Repo.m_Identity;
			else
				Identifier = _Repo.m_Location;

			return m_Configs.f_FindEqual(Identifier);
		}

		CStateHandler::CStateHandler
			(
				CStr const &_BasePath
				, CStr const &_OutputDir
				, EAnsiEncodingFlag _AnsiFlags
				, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			)
			: mp_BasePath(_BasePath)
			, mp_OutputDir(_OutputDir)
			, mp_AnsiFlags(_AnsiFlags)
			, mp_fOutputConsole(_fOutputConsole)
		{
		}

		bool CStateHandler::fp_IsPerforceRoot()
		{
			DLock(mp_IsPerforceRootLock);
			if (!mp_bIsPerforceRoot)
				mp_bIsPerforceRoot = CPerforceClient::fs_HasP4Config(mp_BasePath);
			return *mp_bIsPerforceRoot;
		}

		EAnsiEncodingFlag CStateHandler::f_AnsiFlags() const
		{
			return mp_AnsiFlags;
		}

		TCFuture<CActorSubscription> CStateHandler::f_SequenceConfigChanges(CStr const &_Path)
		{
			DLock(mp_GitConfigSequencersLock);
			return (*mp_GitConfigSequencers(_Path, CStr("Git Config for: "_f << _Path))).f_Sequence();
		}

		TCFuture<CActorSubscription> CStateHandler::f_SequenceLfsReleaseIndexUpdates()
		{
			return mp_LfsReleaseIndexUpdateSequencer.f_Sequence();
		}

		bool CStateHandler::f_UpdateCoreExcludesFileLocation(CStr const &_Path)
		{
			DLock(mp_CoreExcludesFileLocationLock);
			return mp_CoreExcludesFileLocationUpdated(_Path).f_WasCreated();
		}

		CLowLevelRecursiveLock &CStateHandler::f_ConsoleOutputLock()
		{
			return mp_ConsoleOutputLock;
		}

		CLowLevelRecursiveLock &CStateHandler::f_OutputConsoleLock()
		{
			return mp_ConsoleOutputLock;
		}

		void CStateHandler::f_ConsoleOutput(CStr const &_Output, bool _bError)
		{
			if (mp_fOutputConsole)
				mp_fOutputConsole(_Output, _bError);
		}

		void CStateHandler::f_SetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Hash, CStr const &_Identifier, bool _bIsStateFile)
		{
			DLock(mp_Lock);

			bool bExternalPath = false;
			CStr Identifier = _RepoPath;
			if (!_RepoPath.f_StartsWith(mp_BasePath))
			{
				Identifier = _Identifier;
				bExternalPath = true;
			}

			// Make sure we read the old config file if it exists
			fp_GetConfigFile(_FileName, _bIsStateFile);

			auto &ConfigFile = mp_NewConfigFiles[_FileName];
			ConfigFile.m_bIsStateFile = _bIsStateFile;
			auto &Config = ConfigFile.m_Configs[Identifier];
			Config.m_Hash = _Hash;
			Config.m_bExternalPath = bExternalPath;

			if (auto *pFile = mp_ConfigFiles.f_FindEqual(_FileName))
				ConfigFile.m_LineEndings = pFile->m_LineEndings;
		}

		CStr CStateHandler::f_GetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Identifier, bool _bIsStateFile)
		{
			DLock(mp_Lock);
			auto &ConfigFile = fp_GetConfigFile(_FileName, _bIsStateFile);

			CStr Identifier = _RepoPath;
			if (!_RepoPath.f_StartsWith(mp_BasePath))
				Identifier = _Identifier;

			auto *pConfig = ConfigFile.m_Configs.f_FindEqual(Identifier);
			if (!pConfig)
				return {};
			return pConfig->m_Hash;
		}

		TCMap<CStr, CConfigFile> const &CStateHandler::f_GetNewFiles()
		{
			return mp_NewConfigFiles;
		}

		TCMap<CStr, CConfigFile> CStateHandler::f_GetMergedFiles()
		{
			TCMap<CStr, CConfigFile> Files;
			Files = mp_NewConfigFiles;
			for (auto &ConfigFile : mp_ConfigFiles)
			{
				auto *pConfigFile = Files.f_FindEqual(mp_ConfigFiles.fs_GetKey(ConfigFile));

				if (!pConfigFile)
					Files[mp_ConfigFiles.fs_GetKey(ConfigFile)] = ConfigFile;
				else
					pConfigFile->m_Configs += ConfigFile.m_Configs;
			}
			return Files;
		}

		bool CStateHandler::f_AddGitIgnore(CStr const &_FileName, CBuildSystem const &_BuildSystem, EGitIgnoreType _GitIgnoreType)
		{
			DLock(mp_Lock);
			if (!mp_GitIgnores(_FileName).f_WasCreated())
				return false;

			CStr IgnoreFile;
			CStr GitRoot;

			if (_GitIgnoreType == EGitIgnoreType::mc_GitInfoExclude || _GitIgnoreType == EGitIgnoreType::mc_CoreExcludesFile)
			{
				CStr CurrentPath = CFile::fs_GetPath(_FileName);
				while (!CurrentPath.f_IsEmpty())
				{
					CStr GitDir = CurrentPath / ".git";
					if (CFile::fs_FileExists(GitDir, EFileAttrib_Directory | EFileAttrib_File))
					{
						GitRoot = CurrentPath;
						if (_GitIgnoreType == EGitIgnoreType::mc_GitInfoExclude)
						{
							CStr GitCommonDir = fg_GetGitCommonDir(fg_GetGitDataDir(CurrentPath, CFilePosition{}), CFilePosition{});
							CStr InfoDir = GitCommonDir / "info";
							if (!CFile::fs_FileExists(InfoDir, EFileAttrib_Directory))
								CFile::fs_CreateDirectory(InfoDir);
							IgnoreFile = InfoDir / "exclude";
						}
						else
						{
							CStr BuildSystemDir = mp_BasePath / "BuildSystem";
							if (!CFile::fs_FileExists(BuildSystemDir, EFileAttrib_Directory))
								CFile::fs_CreateDirectory(BuildSystemDir);
							IgnoreFile = BuildSystemDir / ".localgitignore";
						}
						break;
					}

					CStr ParentPath = CFile::fs_GetPath(CurrentPath);
					if (ParentPath == CurrentPath)
						break;

					CurrentPath = ParentPath;
				}

				// Fall back to .gitignore/.p4ignore if no .git directory found
				if (IgnoreFile.f_IsEmpty())
				{
					IgnoreFile = CFile::fs_GetPath(_FileName) / (fp_IsPerforceRoot() ? ".p4ignore" : ".gitignore");
					GitRoot.f_Clear();
				}
			}
			else
			{
				bool bUseP4Ignore = fp_IsPerforceRoot();
				if (bUseP4Ignore)
				{
					// Check if the file is inside a child git repository
					CStr CurrentPath = CFile::fs_GetPath(_FileName);
					while (!CurrentPath.f_IsEmpty())
					{
						if (CFile::fs_FileExists(CurrentPath / ".git", EFileAttrib_Directory | EFileAttrib_File))
						{
							bUseP4Ignore = false;
							break;
						}

						CStr ParentPath = CFile::fs_GetPath(CurrentPath);
						if (ParentPath == CurrentPath)
							break;

						CurrentPath = ParentPath;
					}
				}
				IgnoreFile = CFile::fs_GetPath(_FileName) / (bUseP4Ignore ? ".p4ignore" : ".gitignore");
			}

			CStr IgnoreContents;
			if (CFile::fs_FileExists(IgnoreFile))
				IgnoreContents = CFile::fs_ReadStringFromFile(IgnoreFile, true);

			ch8 const *pParse = IgnoreContents;
			fg_ParseToEndOfLine(pParse);
			ch8 const *pLineEnd = "\n";
			if (*pParse == '\r')
				pLineEnd = "\r\n";

			CStr IgnoreLine;
			if (!GitRoot.f_IsEmpty() && (_GitIgnoreType == EGitIgnoreType::mc_GitInfoExclude || _GitIgnoreType == EGitIgnoreType::mc_CoreExcludesFile))
				IgnoreLine = "/{}{}"_f << CFile::fs_MakePathRelative(_FileName, GitRoot) << pLineEnd;
			else
				IgnoreLine = "/{}{}"_f << CFile::fs_GetFile(_FileName) << pLineEnd;

			if (IgnoreContents.f_Find(IgnoreLine) < 0)
			{
				IgnoreContents += IgnoreLine;
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(IgnoreContents), false);
				return _BuildSystem.f_WriteFile(FileData, IgnoreFile);
			}

			return false;
		}

		CConfigFile CStateHandler::fs_ParseConfigFile(CStr const &_Contents, CStr const &_FileName)
		{
			CConfigFile ConfigFile;
			{
				ch8 const *pParse = _Contents;
				fg_ParseToEndOfLine(pParse);
				if (*pParse == '\r')
					ConfigFile.m_LineEndings = "\r\n";
			}

			CStr BasePath = CFile::fs_GetPath(_FileName);

			if (_Contents.f_StartsWith("{"))
			{
				auto RegistryJson = CJsonSorted::fs_FromString(_Contents, _FileName);

				for (auto &Repo : fg_Const(RegistryJson).f_Object())
				{
					auto &RepoName = Repo.f_Name();

					if (RepoName.f_StartsWith("~"))
					{
						auto &Config = ConfigFile.m_Configs[RepoName.f_Extract(1)];
						Config.m_Hash = Repo.f_Value()["Hash"].f_String();
						Config.m_bExternalPath = true;
					}
					else
						ConfigFile.m_Configs[CFile::fs_GetExpandedPath(RepoName, BasePath)].m_Hash = Repo.f_Value()["Hash"].f_String();
				}
			}
			else
			{
				CRegistry Registry;
				{
					ch8 const *pParse = _Contents;
					fg_ParseToEndOfLine(pParse);
					if (*pParse == '\r')
						ConfigFile.m_LineEndings = "\r\n";
				}

				Registry.f_ParseStr(_Contents, _FileName);
				for (auto &Child : Registry.f_GetChildren())
				{
					if (Child.f_GetName().f_StartsWith("~"))
					{
						auto &Config = ConfigFile.m_Configs[Child.f_GetName().f_Extract(1)];
						Config.m_Hash = Child.f_GetThisValue();
						Config.m_bExternalPath = true;
					}
					else
						ConfigFile.m_Configs[CFile::fs_GetExpandedPath(Child.f_GetName(), BasePath)].m_Hash = Child.f_GetThisValue();
				}
			}

			return ConfigFile;
		}

		CConfigFile const &CStateHandler::fp_GetConfigFile(CStr const &_FileName, bool _bIsStateFile)
		{
			DLock(mp_Lock);
			if (auto *pFile = mp_ConfigFiles.f_FindEqual(_FileName))
				return *pFile;
			auto &ConfigFile = mp_ConfigFiles[_FileName];
			ConfigFile.m_bIsStateFile = _bIsStateFile;

			if (CFile::fs_FileExists(_FileName))
				ConfigFile = fs_ParseConfigFile(CFile::fs_ReadStringFromFile(_FileName, true), _FileName);

			return ConfigFile;
		}

		TCSet<CStr> CStateHandler::f_GetLastSeenRepositories()
		{
			CStr RepositoryStateFile = mp_OutputDir / "RepositoryState.json";

			if (!CFile::fs_FileExists(RepositoryStateFile))
				return {};

			CEJsonSorted StateFile = CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(RepositoryStateFile, true), RepositoryStateFile);

			TCSet<CStr> SeenRepositories;

			if (auto *pSeenReposJson = StateFile.f_GetMember("SeenRepositories", EEJsonType_Object))
			{
				for (auto &SeenJson : pSeenReposJson->f_Object())
				{
					SeenRepositories[SeenJson.f_Name()];
				}
			}

			return SeenRepositories;
		}

		void CStateHandler::f_IncrementBranchCreated(CStr const &_FromBranch, CStr const &_ToBranch, CStr const &_Repository)
		{
			DLock(mp_BranchTransitionsLock);
			mp_BranchTransitions[CBranchTransition{_FromBranch, _ToBranch, true}].f_Insert(_Repository);
		}

		void CStateHandler::f_IncrementBranchSwitched(CStr const &_FromBranch, CStr const &_ToBranch, CStr const &_Repository)
		{
			DLock(mp_BranchTransitionsLock);
			mp_BranchTransitions[CBranchTransition{_FromBranch, _ToBranch, false}].f_Insert(_Repository);
		}

		void CStateHandler::f_NotePretendPolicyOutput()
		{
			mp_bAnyPretendPolicyOutput.f_Store(true, NAtomic::gc_MemoryOrder_Relaxed);
		}

		void CStateHandler::f_OutputPretendPolicyReminder()
		{
			if (!mp_bAnyPretendPolicyOutput.f_Load(NAtomic::gc_MemoryOrder_Relaxed))
				return;

			CColors Colors(f_AnsiFlags());
			// Matches EColor_Option (0xffd700) from the CommandLine help renderer so
			// option names here look the same as in `--help` output.
			CStr OptionColor = Colors.f_ForegroundRGB(0xffd700);

			f_ConsoleOutput
				(
					"{}Pretend mode is on by default for {}--apply-policy{}.{} To actually apply the changes shown above, re-run with {}--no-apply-policy-pretend{}.\n"_f
					<< Colors.f_StatusWarning()
					<< OptionColor
					<< Colors.f_StatusWarning()
					<< Colors.f_Default()
					<< OptionColor
					<< Colors.f_Default()
				)
			;
		}

		void CStateHandler::f_OutputBranchSwitchSummary(umint _MaxRepoWidth)
		{
			CColors Colors(f_AnsiFlags());
			CStr RepoColor = Colors.f_StatusNormal();

			using namespace NStr;
			for (auto &TransitionEntry : mp_BranchTransitions.f_Entries())
			{
				auto Repositories = TransitionEntry.f_Value();
				auto const &Key = TransitionEntry.f_Key();
				umint nCount = Repositories.f_GetLen();
				CStr Action = Key.m_bCreated ? gc_Str<"Created and switched">.m_Str : gc_Str<"Switched">.m_Str;
				CStr SwitchDesc = "{} > {}"_f << Key.m_FromBranch << Key.m_ToBranch;
				CStr SwitchDescAligned = "{sl*,a-}"_f << SwitchDesc << _MaxRepoWidth;

				if (Repositories.f_GetLen() > 3)
				{
					Repositories.f_SetLen(3);
					Repositories.f_Insert("…");
				}

				CStr SwitchDescColored = SwitchDescAligned.f_Replace(" > ", CStr("{}{} > {}"_f << Colors.f_Default() << Colors.f_Foreground256(250) << RepoColor));

				auto fFormatRepository = [&](CStr const &_Repository) -> CStr
					{
						if (_Repository == "…")
							return "{}{}…{}"_f << Colors.f_Default() << Colors.f_Foreground256(250) << Colors.f_Default();

						CStr Repository = _Repository;
						auto Parts = Repository.f_Split("/");
						if (umint nParts = Parts.f_GetLen(); nParts > 4)
						{
							Repository = "{}/{}/{}…{}/{}/{}"_f
								<< Parts[0]
								<< Parts[1]
								<< Colors.f_Foreground256(250)
								<< RepoColor
								<< Parts[nParts - 2]
								<< Parts[nParts - 1]
							;
						}

						return "{}{}{}"_f
							<< RepoColor
							<< Repository.f_Replace("/", CStr("{}{}/{}"_f << Colors.f_Default() << Colors.f_Foreground256(250) << RepoColor))
							<< Colors.f_Default()
						;
					}
				;

				f_ConsoleOutput
					(
						"{}{}{}   {} {} {} {vs}\n"_f
						<< RepoColor
						<< SwitchDescColored
						<< Colors.f_Default()
						<< Action
						<< nCount
						<< (nCount == 1 ? "repository" : "repositories")
						<< Repositories.f_Map(fFormatRepository)
					)
				;
			}
		}

		void fg_OutputRepositoryInfo(EOutputType _OutputType, CStr const &_Info, CStateHandler &o_StateHandler, CStr const &_RepoName, umint _MaxRepoWidth)
		{
			fg_OutputRepositoryInfo
				(
					_OutputType
					, _Info
					, o_StateHandler.f_AnsiFlags()
					, _RepoName
					, _MaxRepoWidth
					, [&](CStr const &_Line)
					{
						DMibLock(o_StateHandler.f_ConsoleOutputLock());
						o_StateHandler.f_ConsoleOutput(_Line);
					}
				)
			;
		}

		TCUnsafeFuture<bool> DMibWorkaroundUBSanSectionErrors fg_HandleRepository
			(
				CGitLaunches &_Launches
				, CStr const &_ReposDirectory
				, CRepository const &_Repo
				, CStateHandler &o_StateHandler
				, CBuildSystem const &_BuildSystem
				, TCMap<CStr, CRepository const *> const &_AllRepositories
				, EHandleRepositoryAction _ReconcileAction
				, umint _MaxRepoWidth
				, CMainRepoInfo const &_MainRepoInfo
				, bool _bStash
			)
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			CColors Colors(o_StateHandler.f_AnsiFlags());

			CStr Location = _Repo.m_Location;
			DMibCheck(_Repo.m_Location == _ReposDirectory + "/" + _Repo.f_GetName());
			CStr BaseDir = _BuildSystem.f_GetBaseDir();
			CStr RepositoryIdentifier = _Repo.f_GetIdentifierName(BaseDir, BaseDir);

			bool bIsRoot = _Repo.m_Type == gc_ConstString_Root.m_String;

			CDisableExceptionTraceScope DisableTrace;

			bool bChanged = false;

			auto fLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir, TCMap<CStr, CStr> const &_Environment = {}) -> TCUnsafeFuture<CStr>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					TCVector<CStr> CommandLineParams{"-C", _WorkingDir};
					CommandLineParams.f_Insert(_Params);

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params, _Environment, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);
					co_return Return.f_GetStdOut();
				}
			;
			auto fLaunchGitQuestion = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir, bool _bErrorOnStdErr) -> TCUnsafeFuture<bool>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

					CStr StdErr = Return.f_GetErrorOut().f_Trim();
					if (_bErrorOnStdErr && !StdErr.f_IsEmpty())
						DMibError("Failed to ask git question {vs}: {}"_f << _Params << Return.f_GetCombinedOut());

					co_return Return.m_ExitCode == 0;
				}
			;
			auto fLaunchGitNonEmpty = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir) -> TCUnsafeFuture<bool>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);

					if (Return.m_ExitCode)
						co_return false;

					co_return !Return.f_GetStdOut().f_IsEmpty();
				}
			;
			auto fTryLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir) -> TCUnsafeFuture<CStr>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);


					if (Return.m_ExitCode == 0)
						co_return {};

					auto Output = Return.f_GetCombinedOut();

					if (!Output.f_IsEmpty())
						co_return Output;

					co_return "Unknown error";
				}
			;

			CStr ConfigHash;
			if (!bIsRoot)
				ConfigHash = o_StateHandler.f_GetHash(_Repo.m_ConfigFile, Location, _Repo.m_Identity, false);

			auto fOutputInfo = [&](EOutputType _OutputType, CStr const &_Info, bool _bEmptyRepo = false)
				{
					DMibLock(o_StateHandler.f_OutputConsoleLock());

					CStr RepoName;
					if (_Repo.m_Location.f_StartsWith(BaseDir))
						RepoName = CFile::fs_MakePathRelative(_Repo.m_Location, BaseDir);
					else
						RepoName = _Repo.m_Location;

					if (RepoName.f_IsEmpty())
						RepoName = gc_Str<".">.m_Str;

					fg_OutputRepositoryInfo(_OutputType, _Info, o_StateHandler, _bEmptyRepo ? "" : RepoName, _MaxRepoWidth);
					_Launches.f_RepoDone(0);
				}
			;

			auto fCloneFailureLooksTransient = [](CStr const &_Error) -> bool
				{
					return _Error.f_Find("RPC failed") >= 0
						|| _Error.f_Find("Recv failure") >= 0
						|| _Error.f_Find("Connection reset") >= 0
						|| _Error.f_Find("Operation timed out") >= 0
						|| _Error.f_Find("Failed to connect") >= 0
						|| _Error.f_Find("Could not resolve host") >= 0
						|| _Error.f_Find("transfer closed") >= 0
						|| _Error.f_Find("unexpected disconnect") >= 0
						|| _Error.f_Find("remote end hung up unexpectedly") >= 0
						|| _Error.f_Find("early EOF") >= 0
						|| _Error.f_Find("invalid index-pack output") >= 0
						|| _Error.f_Find("SSL_ERROR_SYSCALL") >= 0
						|| _Error.f_Find("HTTP/2 stream") >= 0
						|| _Error.f_Find("Smudge error") >= 0
						|| _Error.f_Find("Error downloading object") >= 0
						|| _Error.f_Find("smudge filter lfs failed") >= 0
						|| _Error.f_Find("external filter 'git-lfs filter-process' failed") >= 0
						|| (_Error.f_Find("Error downloading") >= 0 && _Error.f_Find("EOF") >= 0)
						|| _Error.f_Find("The requested URL returned error: 500") >= 0
						|| _Error.f_Find("The requested URL returned error: 502") >= 0
						|| _Error.f_Find("The requested URL returned error: 503") >= 0
						|| _Error.f_Find("The requested URL returned error: 504") >= 0
					;
				}
			;

			auto fLaunchGitCloneWithRetry = [&](TCVector<CStr> const &_CloneParams) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					constexpr uint32 c_MaxCloneAttempts = 6;
					for (uint32 iAttempt = 0; iAttempt < c_MaxCloneAttempts; ++iAttempt)
					{
						auto CloneResult = co_await fLaunchGit(_CloneParams, "").f_Wrap();
						if (CloneResult)
							co_return {};

						CStr Error = CloneResult.f_GetExceptionStr();
						if (!fCloneFailureLooksTransient(Error))
							co_return CloneResult.f_GetException();

						if (CFile::fs_FileExists(Location, EFileAttrib_Directory))
							CFile::fs_DeleteDirectoryRecursive(Location, true);

						if (iAttempt + 1 == c_MaxCloneAttempts)
							co_return CloneResult.f_GetException();

						fOutputInfo
							(
								EOutputType_Warning
								, "Git clone failed with a transient transport error; retrying attempt {}/{}\n{}"_f
								<< (iAttempt + 2)
								<< c_MaxCloneAttempts
								<< Error
							)
						;

						co_await fg_Timeout(fp64(1 << (iAttempt + 1)) * 2.0);
					}

					co_return {};
				}
			;

			auto fFetchIfCommitMissing = [&](CStr const &_Hash, CStr const &_RepoDir, TCVector<CStr> _ExtraFetchParams = {}) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					if (_Hash.f_IsEmpty() || co_await fLaunchGitQuestion({"cat-file", "-e", "{}^{{commit}"_f << _Hash}, _RepoDir, false))
						co_return {};

					TCVector<CStr> FetchParams = {"fetch", "--all", "--prune", "--tags"};
					if (_BuildSystem.f_GetGenerateOptions().m_bForceUpdateRemotes)
						FetchParams.f_Insert("--force");
					FetchParams.f_Insert(_ExtraFetchParams);

					auto FetchResult = co_await fLaunchGit(FetchParams, _RepoDir, fg_FetchEnvironment(_BuildSystem)).f_Wrap();
					if (!FetchResult)
					{
						if (!(co_await fLaunchGitQuestion({"cat-file", "-e", "{}^{{commit}"_f << _Hash}, _RepoDir, false)))
						{
							fOutputInfo(EOutputType_Error, "Commit {} not found after fetch failed: {}"_f << _Hash << FetchResult.f_GetExceptionStr());
							co_return DMibErrorInstance("Commit {} not found in repository '{}' and fetch failed: {}"_f << _Hash << _RepoDir << FetchResult.f_GetExceptionStr());
						}

						fOutputInfo(EOutputType_Error, "Not all remotes were fetched: {}"_f << FetchResult.f_GetExceptionStr());
					}
					else if (!(co_await fLaunchGitQuestion({"cat-file", "-e", "{}^{{commit}"_f << _Hash}, _RepoDir, false)))
					{
						fOutputInfo(EOutputType_Error, "Commit {} not found after fetching from all remotes"_f << _Hash);
						co_return DMibErrorInstance("Commit {} not found in repository '{}' after fetching from all remotes"_f << _Hash << _RepoDir);
					}

					co_return {};
				}
			;

			auto fGetStashPrefixForBranch = [&](CStr const &_Branch) -> CStr
				{
					return "mib-branch-switch:{}:{}"_f << RepositoryIdentifier << _Branch;
				}
			;

			auto fMakeUniqueStashName = [&](CStr const &_Branch) -> CStr
				{
					// A per-invocation random suffix is appended so that the failure-recovery
					// hook can pop *exactly* the stash this invocation created, and so that a
					// later successful branch switch never silently re-applies an orphaned
					// stash left behind by a previous failed run.
					return "{}:{}"_f << fGetStashPrefixForBranch(_Branch) << fg_FastRandomID();
				}
			;

			auto fStashIfNeeded = [&](CStr const &_Location, CStr const &_CurrentBranch, CStr const &_StashName) -> TCUnsafeFuture<bool>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					if ((co_await fLaunchGit({"status", "--porcelain"}, _Location)).f_Trim().f_IsEmpty())
						co_return false;
					fOutputInfo(EOutputType_Normal, "Stashing local changes on branch '{}'"_f << _CurrentBranch);
					co_await fLaunchGit({"stash", "push", "--include-untracked", "-m", _StashName}, _Location);
					co_return true;
				}
			;

			// Restore a mib-created stash when switching back to the branch those
			// changes belonged to. The prefix match (without the random suffix) is
			// intentional: when a branch switch stashes changes, and the user later
			// switches back, those changes should be restored regardless of which
			// invocation created the stash.
			//
			// The only scenario that leaves a truly orphaned stash is:
			//   stash -> checkout fails -> failure-recovery unstash also fails
			// In that case the stash is left behind and will be popped here on the
			// next successful switch to the same branch. This is acceptable because
			// the changes originated on that branch and the user was already warned
			// about the conflict during the failed recovery.
			auto fUnstashIfExists = [&](CStr const &_Location, CStr const &_Branch) -> TCUnsafeFuture<bool>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					CStr StashList = co_await fLaunchGit({"stash", "list"}, _Location);
					CStr StashPrefix = fGetStashPrefixForBranch(_Branch);
					for (auto &Line : StashList.f_SplitLine<true>())
					{
						// Match either the legacy format (line ends with the prefix) or the
						// new format (prefix followed by ":<random>"). The trailing colon
						// avoids matching `:foobar` when looking for `:foo`.
						auto PrefixPos = Line.f_Find(StashPrefix);
						if (PrefixPos < 0)
							continue;
						auto AfterPrefix = PrefixPos + StashPrefix.f_GetLen();
						if (AfterPrefix != Line.f_GetLen() && Line[AfterPrefix] != ':')
							continue;

						CStr StashRef;
						(CStr::CParse("{}:") >> StashRef).f_Parse(Line);
						if (!StashRef.f_IsEmpty())
						{
							fOutputInfo(EOutputType_Normal, "Restoring stashed changes for branch '{}'"_f << _Branch);
							if (auto Error = co_await fTryLaunchGit({"stash", "pop", StashRef}, _Location))
								fOutputInfo(EOutputType_Warning, "Stash pop conflict: {}"_f << Error);
							co_return true;
						}
						break;
					}
					co_return false;
				}
			;

			// Shared factory for the fg_AsyncDestroy cleanup hook used around every
			// fStashIfNeeded call site. If the intervening checkout/reset/rebase throws
			// before fUnstashIfExists runs, this pops the exact stash created by
			// _UniqueStashName so the user's local changes aren't silently orphaned
			// and re-applied on a later successful branch switch.
			auto fMakeStashRestoreHook = [&](CStr const &_Location, CStr const &_UniqueStashName, bool const &_bStashed)
				{
					return [&]() -> TCFuture<void>
						{
							// Snapshot everything into the coroutine frame before the first co_await.
							// After the first suspension, all captured references are unsafe.
							if (!_bStashed)
								co_return {};

							CGitLaunches LocalLaunches = _Launches;
							CStr LocalLocation = _Location;
							CStr LocalUniqueStashName = _UniqueStashName;
							EAnsiEncodingFlag LocalAnsiFlags = o_StateHandler.f_AnsiFlags();
							umint LocalMaxRepoWidth = _MaxRepoWidth;
							auto fLocalOutputConsole = _BuildSystem.f_OutputConsoleFunctor();
							CStr LocalRepoName;
							{
								if (_Repo.m_Location.f_StartsWith(BaseDir))
									LocalRepoName = CFile::fs_MakePathRelative(_Repo.m_Location, BaseDir);
								else
									LocalRepoName = _Repo.m_Location;
								if (LocalRepoName.f_IsEmpty())
									LocalRepoName = gc_Str<".">.m_Str;
							}

							auto fLocalOutputInfo = [&](EOutputType _OutputType, CStr const &_Info)
								{
									fg_OutputRepositoryInfo
										(
											_OutputType
											, _Info
											, LocalAnsiFlags
											, LocalRepoName
											, LocalMaxRepoWidth
											, [&](CStr const &_Line)
											{
												fLocalOutputConsole(_Line, false);
											}
										)
									;
								}
							;

							co_await ECoroutineFlag_CaptureExceptions;

							auto StashListResult = co_await LocalLaunches.f_Launch(LocalLocation, {"stash", "list"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
							if (StashListResult.m_ExitCode != 0)
							{
								fLocalOutputInfo(EOutputType_Warning, "Failed to list stashes while restoring: {}"_f << StashListResult.f_GetCombinedOut());
								co_return {};
							}

							for (auto &Line : StashListResult.f_GetStdOut().f_SplitLine<true>())
							{
								if (!Line.f_EndsWith(LocalUniqueStashName))
									continue;

								CStr StashRef;
								(CStr::CParse("{}:") >> StashRef).f_Parse(Line);
								if (StashRef.f_IsEmpty())
									break;

								fLocalOutputInfo(EOutputType_Normal, "Operation failed; restoring stashed changes");

								auto PopResult = co_await LocalLaunches.f_Launch(LocalLocation, {"stash", "pop", StashRef}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
								if (PopResult.m_ExitCode != 0)
									fLocalOutputInfo(EOutputType_Warning, "Stash pop conflict while restoring: {}"_f << PopResult.f_GetCombinedOut());
								break;
							}
							co_return {};
						}
					;
				}
			;

			// Set upstream tracking to origin/<branch> when the remote branch exists.
			// If the remote branch does not exist yet (e.g. a new local feature branch
			// that has not been pushed), leave the upstream unset to avoid accidentally
			// tracking the default branch which would cause git pull to merge the wrong
			// branch.
			auto fSetUpstreamTracking = [&](CStr const &_Branch, CStr const &_Location) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					co_await fTryLaunchGit({"branch", "-u", "origin/{}"_f << _Branch}, _Location);
					co_return {};
				}
			;
			auto fPopulateConfigHashFromBranchIfExists = [&](CStr const &_ConfigHash, CStr const &_RepoDir, CStr const &_Branch) -> TCUnsafeFuture<CStr>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					if (!_ConfigHash.f_IsEmpty() || _Branch.f_IsEmpty())
						co_return _ConfigHash;

					if (co_await fLaunchGitQuestion({"rev-parse", "--verify", "refs/heads/{}^{{commit}}"_f << _Branch}, _RepoDir, false))
						co_return (co_await fLaunchGit({"rev-parse", "refs/heads/{}"_f << _Branch}, _RepoDir)).f_Trim();

					if (co_await fLaunchGitQuestion({"rev-parse", "--verify", "refs/remotes/origin/{}^{{commit}}"_f << _Branch}, _RepoDir, false))
					{
						// Best-effort fallback when no config hash is available: use the locally cached
						// remote-tracking ref as a branch-specific source, but treat it as stale local
						// state rather than verified current origin state.
						co_return (co_await fLaunchGit({"rev-parse", "refs/remotes/origin/{}"_f << _Branch}, _RepoDir)).f_Trim();
					}

					co_return _ConfigHash;
				}
			;

			// CI/cache-warming mode: repository state is disposable and should be reset
			// back to the configured hashes instead of preserving local work.
			bool bForceReset = fg_GetSys()->f_GetEnvironmentVariable("MalterlibRepositoryHardReset", "") == gc_ConstString_true.m_String;

			bool bCloneNew = false;
			if (CFile::fs_FileExists(Location))
			{
				if (!bIsRoot)
				{
					auto Files = CFile::fs_FindFiles(Location + "/*");
					if (Files.f_IsEmpty())
					{
						fOutputInfo(EOutputType_Warning, "Removing empty repo directory");
						CFile::fs_DeleteDirectory(Location);
						bCloneNew = true;
					}
				}
			}
			else
			{
				if (bIsRoot)
				{
					fOutputInfo(EOutputType_Error, "Root repository does not exist at: {}"_f << Location);
					co_return DMibErrorInstance("Aborting, root repository needs to exists");
				}
				else
					bCloneNew = true;
			}

			// The 3 (key, value) pairs for the LFS custom transfer agent configuration.
			// Single source of truth — used by fGetCloneConfigParams (emitted as --config
			// args during clone) and by fInitRecovery (applied as `git config --local`
			// on a brand-new `git init`). fSetupLfsReleaseStorage keeps its own structured
			// conditional logic because it compares against parsed GitConfig state.
			auto fGetLfsCustomTransferConfigPairs = [&]() -> TCVector<TCTuple<CStr, CStr>>
				{
					auto GlobalMibExecutable = CFile::fs_GetUserHomeDirectory() / (".Malterlib/bin/mib" + CFile::mc_ExecutableExtension);
					TCVector<TCTuple<CStr, CStr>> Pairs;
					Pairs.f_Insert({gc_Str<"lfs.customtransfer.malterlib-release.path">.m_Str, GlobalMibExecutable});
					Pairs.f_Insert({gc_Str<"lfs.customtransfer.malterlib-release.args">.m_Str, gc_Str<"lfs-release-store">.m_Str});
					Pairs.f_Insert({gc_Str<"lfs.customtransfer.malterlib-release.concurrent">.m_Str, gc_Str<"true">.m_Str});
					return Pairs;
				}
			;

			// Per-remote fetch refspec exclusions that keep the LFS book-keeping refs
			// out of the normal fetch. Single source of truth used by fGetCloneConfigParams
			// (as --config args), fApplyRemoteLfsAndFetchSpecs (as git config --local --add),
			// and the existing remote-sync loop's fetch-spec comparison.
			auto fGetLfsFetchExclusions = [&]() -> TCVector<CStr>
				{
					TCVector<CStr> Specs;
					Specs.f_Insert(gc_Str<"^refs/heads/lfs">.m_Str);
					Specs.f_Insert(gc_Str<"^refs/tags/lfs/*">.m_Str);
					return Specs;
				}
			;

			// True iff any configured remote (origin or other) uses the LFS release store.
			// The `lfs.customtransfer.malterlib-release.*` keys are global to the repo —
			// they register the transfer agent — so we set them up when *any* remote might
			// reference it, not just the remote being cloned.
			auto fAnyRemoteHasLfsReleaseStore = [&]() -> bool
				{
					if (_Repo.m_OriginProperties.m_bLfsReleaseStore)
						return true;
					for (auto &Remote : _Repo.m_Remotes.m_OrderedRemotes)
					{
						if (Remote.m_Properties.m_bLfsReleaseStore)
							return true;
					}
					return false;
				}
			;

			// Per-remote clone config params. The caller uses `git clone -o <_RemoteName>`
			// to name the remote directly — so we emit config keys under
			// "remote.<_RemoteName>.*" rather than hardcoding "remote.origin.*".
			auto fGetCloneConfigParams = [&](CRemoteProperties const &_Properties, CStr const &_RemoteName) -> TCVector<CStr>
				{
					TCVector<CStr> Params;

					// Global LFS custom-transfer-agent config: set if any configured remote
					// uses the release store, even if the remote being cloned right now does
					// not — otherwise the later remote-sync loop's fSetupLfsReleaseStorage
					// would have to retroactively set these.
					if (fAnyRemoteHasLfsReleaseStore())
					{
						for (auto &Pair : fGetLfsCustomTransferConfigPairs())
							Params.f_Insert({"--config", "{}={}"_f << fg_Get<0>(Pair) << fg_Get<1>(Pair)});
					}

					// Per-remote LFS keys — only for the remote being cloned.
					if (_Properties.m_bLfsReleaseStore)
					{
						Params.f_Insert({"--config", "lfs.{}.standalonetransferagent=malterlib-release"_f << _Properties.m_URL});
						for (auto &Spec : fGetLfsFetchExclusions())
							Params.f_Insert({"--config", "remote.{}.fetch={}"_f << _RemoteName << Spec});
						Params.f_Insert({"--no-tags"});
						Params.f_Insert({"--config", "remote.{}.malterlib-lfs-setup=true"_f << _RemoteName});
					}

					for (auto &FetchSpec : _Properties.m_ExtraFetchSpecs)
						Params.f_Insert({"--config", "remote.{}.fetch={}"_f << _RemoteName << FetchSpec});

					if (_Repo.m_UserName)
						Params.f_Insert({"--config", "user.name={}"_f << _Repo.m_UserName});

					if (_Repo.m_UserEmail)
						Params.f_Insert({"--config", "user.email={}"_f << _Repo.m_UserEmail});

					return Params;
				}
			;

			// Force-English git output for stderr-stable probing. Merged into the launch
			// environment for git subprocesses in the recovery code paths.
			auto fEnglishGitEnvironment = [&]() -> TCMap<CStr, CStr>
				{
					auto Env = fg_FetchEnvironment(_BuildSystem);
					Env["LC_ALL"] = "C";
					Env["LANG"] = "C";
					return Env;
				}
			;

			// Delete `Location` only if local HEAD is unborn — i.e. clone/init/fixup ran far
			// enough to create .git/ but HEAD never pointed at a real commit. Established
			// repos (HEAD born) are left in place because deleting them would throw away
			// potentially large amounts of fetched content for a failure the next
			// `update-repos` run can usually continue from. Callers use this in
			// cleanup-on-error paths where leaving a half-initialized Location behind would
			// make the next run take the "repo already exists" branch and trip over the
			// broken state.
			auto fCleanupLocationIfHeadUnborn = [&]() -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					if (!CFile::fs_FileExists(Location))
						co_return {};
					if (co_await fLaunchGitQuestion({"rev-parse", "--verify", "HEAD"}, Location, false))
						co_return {};
					CFile::fs_DeleteDirectoryRecursive(Location);
					co_return {};
				}
			;

			enum class EOriginProbeResult
			{
				mc_Missing
				, mc_Exists
				, mc_Unknown
			};

			// Classifies `git ls-remote` stderr as indicating a genuinely missing repo.
			// LC_ALL=C / LANG=C force English output so these phrases are stable. Matches:
			//   - "remote: Repository not found." (emitted by GitHub/GitLab/Bitbucket HTTPS)
			//   - "fatal: repository 'URL' not found" (emitted by git itself for HTTP 404)
			// Used as the authoritative signal for UNSUPPORTED hosting providers, where no API
			// fallback is available to apply the authenticated-404 rule. For SUPPORTED providers
			// we never rely on stderr matching — the same message is emitted by GitHub for
			// private repos hidden from anonymous callers (auth masquerade), and the
			// authenticated-404 API rule is the only safe disambiguation.
			auto fLsRemoteStderrIndicatesMissing = [](CStr const &_StdErr) -> bool
				{
					return _StdErr.f_Find("Repository not found") >= 0
						|| _StdErr.f_Find("not found") >= 0
					;
				}
			;

			// Probe whether origin exists. Returns Missing / Exists / Unknown.
			//   1. `git ls-remote --exit-code <url>` with LC_ALL=C. Success + stdout → Exists.
			//   2. If the URL is on a supported hosting provider, fall through to the API with
			//      the authenticated-404 rule (only a 404 from a successfully-authenticated call
			//      counts as Missing; unauthenticated 404s can be private repos hidden from
			//      anonymous callers).
			//   3. Otherwise — unsupported provider — use ls-remote stderr as the authoritative
			//      signal. This is the whole reason we forced LC_ALL=C above. "Repository not
			//      found" / "... not found" → Missing; any other failure → Unknown. Intentional:
			//      self-hosted GitLab/Bitbucket-style servers that hide private repos behind a
			//      "not found" error can be misclassified as Missing here, but `auto` only
			//      *recommends* recovery — the user must explicitly pass --reconcile=*:init
			//      /clone-remote/fork-remote for anything destructive, and the original clone
			//      error is echoed via fOutputInfo so the auth cause is visible. Accepted in
			//      exchange for having auto-detection work at all on unsupported providers.
			//
			// Any throw from the API-fallback path (credential helper failing, hosting provider
			// crashing, DNS blowing up mid-probe) is absorbed and reported as Unknown, so one
			// flaky remote never aborts the missing-origin recovery loop.
			auto fProbeOriginExists = [&](CStr _OriginUrl) -> TCUnsafeFuture<EOriginProbeResult>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					// Step 1: ls-remote with forced English output. No --exit-code: we want
					// exit 0 to mean "successful talk with the remote" regardless of ref
					// count, so an empty-but-reachable repo classifies as Exists instead
					// of falling through to the API/stderr fallback.
					CStr LsStdErr;
					{
						auto Env = fEnglishGitEnvironment();
						auto Return = co_await _Launches.f_Launch(BaseDir, {"ls-remote", _OriginUrl}, Env, CProcessLaunchActor::ESimpleLaunchFlag_None);
						if (Return.m_ExitCode == 0)
							co_return EOriginProbeResult::mc_Exists;
						LsStdErr = Return.f_GetErrorOut();
					}

					CStr ProviderClass = fg_GetHostingProviderClassName(_OriginUrl);
					if (ProviderClass.f_IsEmpty())
					{
						// Step 3: unsupported provider — stderr match is our only signal.
						if (fLsRemoteStderrIndicatesMissing(LsStdErr))
							co_return EOriginProbeResult::mc_Missing;
						co_return EOriginProbeResult::mc_Unknown;
					}

					// Step 2: hosting provider API fallback (authenticated-404 only).
					// Exceptions from the credential helper or provider propagate to the caller,
					// which wraps via f_Wrap() and collects the exception into its error
					// collector so the user sees why the probe couldn't classify origin.
					NWeb::NHTTP::CURL ProbeUrl(_OriginUrl);
					CStr Slug = CStr::fs_Join(ProbeUrl.f_GetPath(), "/").f_RemoveSuffix(".git");

					auto HostingProvider = CGitHostingProvider::fs_CreateHostingProvider(ProviderClass);
					auto DestroyHostingProvider = co_await fg_AsyncDestroy(HostingProvider);

					CStr Token = co_await fg_GetGitCredentials(ProbeUrl, BaseDir);

					bool bAuthenticated = false;
					if (Token)
					{
						auto LoginResult = co_await HostingProvider(&CGitHostingProvider::f_Login, CEJsonSorted{"Token"_= Token}).f_Wrap();
						bAuthenticated = !!LoginResult;
					}

					auto Probe = co_await HostingProvider(&CGitHostingProvider::f_GetRepository, Slug).f_Wrap();
					if (Probe)
						co_return EOriginProbeResult::mc_Exists;

					// 404 from an authenticated probe means missing (we can see private repos).
					// Anything else (401, network error, 5xx) is ambiguous — rethrow the
					// provider exception so the caller's error collector can forward it.
					bool bMissing = false;
					NException::fg_VisitException<CGitHostingProviderException>
						(
							Probe.f_GetException()
							, [&](CGitHostingProviderException const &_Exception)
							{
								if (_Exception.f_GetSpecific().m_StatusCode == 404 && bAuthenticated)
									bMissing = true;
							}
						)
					;

					if (bMissing)
						co_return EOriginProbeResult::mc_Missing;

					co_return Probe.f_GetException();
				}
			;

			// Probe whether a configured non-origin remote is reachable as a clone source.
			// Returns true iff we're confident we can clone from the URL. Same structure as
			// fProbeOriginExists — ls-remote first, then hosting-provider API fallback on
			// supported providers. For unsupported providers, only a successful ls-remote
			// counts as reachable; any failure (missing, auth, network) returns false. We
			// don't need the stderr match here because both "definitely missing" and "maybe
			// unreachable" collapse to the same outcome (not reachable) — the stderr
			// classification would only matter if we exposed the reason, which we don't.
			//
			// Exceptions from the credential helper or provider propagate to the caller,
			// which wraps via f_Wrap() and collects them into its error collector so the
			// user can see why a remote was treated as unreachable.
			//
			// The API fallback intentionally accepts "repo exists per provider" as reachable
			// even when ls-remote failed. That surfaces real git errors from the subsequent
			// clone/fetch at the point of failure rather than pre-filtering remotes on a
			// transport heuristic we can't make reliable.
			//
			// Empty source repos (exist per provider, no refs yet) are treated the same way:
			// a user-error corner case that the explicit clone-remote/fork-remote recovery
			// will surface with a clear default-branch-missing error at checkout. Demoting
			// to init here would silently place a placeholder commit on origin, which is
			// the wrong action when the user was expecting content from the source.
			// Empty origin is a different scenario and is handled by
			// fHandleEmptyOriginAfterClone — origin is authoritative for our content, so
			// we create the first commit automatically there.
			auto fProbeRemoteReachability = [&](CStr _Url) -> TCUnsafeFuture<bool>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					{
						auto Env = fEnglishGitEnvironment();
						auto Return = co_await _Launches.f_Launch(BaseDir, {"ls-remote", _Url}, Env, CProcessLaunchActor::ESimpleLaunchFlag_None);
						if (Return.m_ExitCode == 0)
							co_return true;
					}

					CStr ProviderClass = fg_GetHostingProviderClassName(_Url);
					if (ProviderClass.f_IsEmpty())
						co_return false;

					NWeb::NHTTP::CURL ProbeUrl(_Url);
					CStr Slug = CStr::fs_Join(ProbeUrl.f_GetPath(), "/").f_RemoveSuffix(".git");

					auto HostingProvider = CGitHostingProvider::fs_CreateHostingProvider(ProviderClass);
					auto DestroyHostingProvider = co_await fg_AsyncDestroy(HostingProvider);

					CStr Token = co_await fg_GetGitCredentials(ProbeUrl, BaseDir);
					if (Token)
					{
						auto LoginResult = co_await HostingProvider(&CGitHostingProvider::f_Login, CEJsonSorted{"Token"_= Token}).f_Wrap();
						(void)LoginResult;
					}

					auto Probe = co_await HostingProvider(&CGitHostingProvider::f_GetRepository, Slug).f_Wrap();
					if (Probe)
						co_return true;

					// API call failed — rethrow so the caller can collect the exception.
					co_return Probe.f_GetException();
				}
			;

			// Apply per-remote LFS config (standalonetransferagent + fetch exclusions +
			// malterlib-lfs-setup marker) and any extra fetch specs. Caller owns the
			// `git remote add` (or rename) and any sequencing scope it wants to wrap.
			// Used by the recovery flows (init/clone-remote/fork-remote) and by the
			// existing remote-sync loop's "add new remote" branch.
			auto fApplyRemoteLfsAndFetchSpecs = [&](CStr _RemoteName, CRemoteProperties const &_Properties) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					if (_Properties.m_bLfsReleaseStore)
					{
						// Ensure the global mib binary exists at the path we're about to
						// reference via lfs.<URL>.standalonetransferagent. Idempotent —
						// guarded by mp_bGlobalMToolAlreadySetup.
						co_await _BuildSystem.f_SetupGlobalMTool();

						co_await fLaunchGit({"config", "--local", "lfs.{}.standalonetransferagent"_f << _Properties.m_URL, "malterlib-release"}, Location);
						for (auto &Spec : fGetLfsFetchExclusions())
							co_await fLaunchGit({"config", "--local", "--add", "remote.{}.fetch"_f << _RemoteName, Spec}, Location);
						co_await fLaunchGit({"config", "--local", "remote.{}.malterlib-lfs-setup"_f << _RemoteName, "true"}, Location);
					}

					for (auto &FetchSpec : _Properties.m_ExtraFetchSpecs)
						co_await fLaunchGit({"config", "--local", "--add", "remote.{}.fetch"_f << _RemoteName, FetchSpec}, Location);

					co_return {};
				}
			;

			// Common helper: add a remote with full LFS/fetch-spec setup and fetch.
			// Mirrors the per-remote block at the existing remote-sync loop, so that the later
			// remote-sync sees the remote as already-configured and takes the no-fetch branch.
			auto fAddRemoteAndFetch = [&](CStr _RemoteName, CRemoteProperties const &_Properties) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

					co_await fLaunchGit({"remote", "add", _RemoteName, _Properties.m_URL}, Location);
					co_await fApplyRemoteLfsAndFetchSpecs(_RemoteName, _Properties);

					if (_BuildSystem.f_GetGenerateOptions().m_bForceUpdateRemotes)
						co_await fLaunchGit({"fetch", "--tags", "--force", _RemoteName}, Location);
					else
						co_await fLaunchGit({"fetch", "--tags", _RemoteName}, Location);

					co_return {};
				}
			;

			// Variant that adds a remote without fetching (used by `init`, where no remote
			// has content yet, and by `clone-remote` for the missing origin).
			auto fAddRemoteNoFetch = [&](CStr _RemoteName, CRemoteProperties const &_Properties) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

					co_await fLaunchGit({"remote", "add", _RemoteName, _Properties.m_URL}, Location);
					co_await fApplyRemoteLfsAndFetchSpecs(_RemoteName, _Properties);

					co_return {};
				}
			;

			// `init` recovery: create empty local repo with remotes wired up + placeholder commit.
			// Does NOT fetch any remote — only chosen when no remote has content yet.
			auto fInitRecovery = [&](CStr _CloneExpectedBranch) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					if (!ConfigHash.f_IsEmpty())
					{
						CStr Message =
							"Cannot use 'init' reconcile action on repository '{}': a ConfigHash is pinned ({})\n"
							"but no remote has this commit yet. Use 'clone-remote' or 'fork-remote' against a\n"
							"reachable source remote, or remove the pinned hash from the config file."_f
							<< _Repo.f_GetName()
							<< ConfigHash
						;
						fOutputInfo(EOutputType_Error, Message);
						co_return DMibErrorInstance(Message);
					}

					// Wrap all subsequent steps so we can clean up the partially-initialized
					// directory if any step fails (otherwise next run sees a non-empty Location
					// and skips the new-clone path entirely, leaving the user stuck).
					auto InitResult = co_await fg_CallSafe
						(
							[&]() -> TCUnsafeFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								CFile::fs_CreateDirectory(Location);
								co_await fLaunchGit({"init", Location}, "");

								// Ensure the global mib binary exists before we write any config
								// that references it (customtransfer.malterlib-release.path below
								// and per-remote standalonetransferagent via fAddRemoteNoFetch).
								if (fAnyRemoteHasLfsReleaseStore())
									co_await _BuildSystem.f_SetupGlobalMTool();

								// Apply the same global config keys that fGetCloneConfigParams
								// would have set as --config args during a clone. Per-remote LFS
								// keys (standalonetransferagent + fetch exclusions) are handled
								// by fAddRemoteNoFetch via fApplyRemoteLfsAndFetchSpecs.
								{
									auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

									if (fAnyRemoteHasLfsReleaseStore())
									{
										for (auto &Pair : fGetLfsCustomTransferConfigPairs())
											co_await fLaunchGit({"config", "--local", fg_Get<0>(Pair), fg_Get<1>(Pair)}, Location);
									}

									if (_Repo.m_UserName)
										co_await fLaunchGit({"config", "--local", "user.name", _Repo.m_UserName}, Location);

									if (_Repo.m_UserEmail)
										co_await fLaunchGit({"config", "--local", "user.email", _Repo.m_UserEmail}, Location);
								}

								co_await fAddRemoteNoFetch(gc_Str<"origin">.m_Str, _Repo.m_OriginProperties);

								for (auto &Remote : _Repo.m_Remotes.m_OrderedRemotes)
								{
									if (Remote.m_Name == gc_Str<"origin">.m_Str)
										continue;
									co_await fAddRemoteNoFetch(Remote.m_Name, Remote.m_Properties);
								}

								// `-B` (capital) so the command is idempotent with init.defaultBranch.
								co_await fLaunchGit({"checkout", "-B", _CloneExpectedBranch}, Location);

								// Placeholder commit so HEAD is born — required for downstream
								// HEAD-dependent code in fg_HandleRepository, and gives ./mib push
								// something to push.
								co_await fLaunchGit({"commit", "--allow-empty", "-m", "Initial commit"}, Location);

								bChanged = true;

								co_return {};
							}
						)
						.f_Wrap()
					;

					if (!InitResult)
					{
						co_await fCleanupLocationIfHeadUnborn();
						fOutputInfo(EOutputType_Error, "Failed to init repository: {}"_f << InitResult.f_GetExceptionStr());
						co_return InitResult.f_GetException();
					}

					if (fg_IsSupportedHostingProvider(_Repo.m_OriginProperties.m_URL))
					{
						fOutputInfo
							(
								EOutputType_Warning
								, "Initialized empty repository with a placeholder initial commit. Origin is missing.\n"
								"To finish setup: (1) run './mib update-repos --apply-policy --apply-policy-create-missing'\n"
								"to create origin, (2) run './mib push' to populate origin and establish upstream tracking.\n"
								"To replace the placeholder commit with real content later, use 'git commit --amend'."
							)
						;
					}
					else
					{
						fOutputInfo
							(
								EOutputType_Warning
								, "Initialized empty repository with a placeholder initial commit. Origin is missing.\n"
								"To replace the placeholder commit with real content later, use 'git commit --amend'."
							)
						;
					}

					co_return {};
				}
			;

			// `clone-remote` recovery: clone from a non-origin source, rename the auto-created
			// origin to that source's configured name, then add the real origin (with no fetch
			// since it doesn't exist yet) plus all other configured remotes (with fetch).
			// The local branch ends with no upstream; user runs ./mib push to set it after
			// --apply-policy-create-missing has created origin.
			auto fCloneRemoteRecovery = [&](CStr _SourceName, CRemote const &_SourceRemote, CStr _CloneExpectedBranch) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					// If any configured remote uses the LFS release store, ensure the mib
					// binary exists before any operation that may invoke the transfer agent
					// (the post-clone `git fetch` below; later fetches added via fAddRemote*).
					if (fAnyRemoteHasLfsReleaseStore())
						co_await _BuildSystem.f_SetupGlobalMTool();

					// Wrap the clone-and-configure sequence so a failure can clean up a
					// half-initialized Location (only when HEAD is unborn — established repos
					// are preserved so the next `update-repos` can continue from partial state).
					auto CloneResult = co_await fg_CallSafe
						(
							[&]() -> TCUnsafeFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								// Clone from source URL using source-specific config (LFS, fetch specs).
								// `-o _SourceName` names the remote directly — avoids a post-clone
								// `git remote rename` (and the "Not updating non-default fetch refspec"
								// warning it emits for pure-negative refspecs like ^refs/heads/lfs).
								{
									TCVector<CStr> CloneParams{"clone"};
									CloneParams.f_Insert(fGetCloneConfigParams(_SourceRemote.m_Properties, _SourceName));
									CloneParams.f_Insert({"-o", _SourceName, "-n", _SourceRemote.m_Properties.m_URL, Location});
									co_await fLaunchGit(CloneParams, "");

									if (_SourceRemote.m_Properties.m_bLfsReleaseStore)
									{
										auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
										co_await fLaunchGit({"update-ref", "-d", "refs/remotes/{}/lfs"_f << _SourceName}, Location);
										co_await fLaunchGit({"config", "--local", "--unset", "remote.{}.tagOpt"_f << _SourceName}, Location);
										co_await fLaunchGit({"fetch"}, Location);
									}
								}

								// Add the real origin with no fetch (it doesn't exist yet). LFS keys
								// and extra fetch specs are applied by fAddRemoteNoFetch.
								co_await fAddRemoteNoFetch(gc_Str<"origin">.m_Str, _Repo.m_OriginProperties);

								// Add and fetch every other configured remote (skipping origin and the
								// renamed source).
								for (auto &Remote : _Repo.m_Remotes.m_OrderedRemotes)
								{
									if (Remote.m_Name == gc_Str<"origin">.m_Str || Remote.m_Name == _SourceName)
										continue;
									co_await fAddRemoteAndFetch(Remote.m_Name, Remote.m_Properties);
								}

								// Resolve source's default branch first via fetched ref, then fall back to
								// the configured value if the server didn't advertise HEAD.
								auto DynamicInfo = fg_GetRepositoryDynamicInfo(_Repo);
								CStr SourceDefaultBranch = fg_GetRemoteHead(_Repo, DynamicInfo, _SourceName);
								if (SourceDefaultBranch.f_IsEmpty())
									SourceDefaultBranch = _SourceRemote.m_Properties.m_DefaultBranch;

								if (SourceDefaultBranch.f_IsEmpty())
								{
									CStr Message =
										"Source remote '{}' at '{}' has refs but doesn't advertise a default branch (HEAD symref is unset).\n"
										"Fix this on the source's hosting provider, or set the source remote's m_DefaultBranch in the .MHeader\n"
										"to specify which branch to use."_f
										<< _SourceName
										<< _SourceRemote.m_Properties.m_URL
									;
									fOutputInfo(EOutputType_Error, Message);
									co_return DMibErrorInstance(Message);
								}

								CStr CheckoutSHA;
								if (!ConfigHash.f_IsEmpty())
								{
									// The config file pinned a specific commit; refuse to check
									// out a different one. If this source doesn't have the commit,
									// the user needs a different source (or to remove the pin).
									if (!(co_await fLaunchGitQuestion({"cat-file", "-e", "{}^{{commit}"_f << ConfigHash}, Location, false)))
									{
										CStr Message =
											"Source remote '{}' at '{}' does not contain the pinned ConfigHash {}.\n"
											"Make a source remote containing this commit reachable, or remove the pinned hash from the config file."_f
											<< _SourceName
											<< _SourceRemote.m_Properties.m_URL
											<< ConfigHash
										;
										fOutputInfo(EOutputType_Error, Message);
										co_return DMibErrorInstance(Message);
									}
									CheckoutSHA = ConfigHash;
								}
								else
								{
									// Intentional: seed _CloneExpectedBranch from the source's default-branch
									// tip, NOT from refs/remotes/<source>/<_CloneExpectedBranch> even if that
									// ref happens to exist. The main-repo branch name has no required
									// relationship to the source's branch namespace — a local `feature-x` may
									// be about to receive entirely different content than an upstream
									// `feature-x` that happens to share the name, and cross-matching would
									// silently seed recovery from the wrong history. Users who need a specific
									// source commit pin it via ConfigHash above.
									CheckoutSHA = (co_await fLaunchGit({"rev-parse", "refs/remotes/{}/{}"_f << _SourceName << SourceDefaultBranch}, Location)).f_Trim();
								}

								co_await fLaunchGit({"checkout", "-B", _CloneExpectedBranch, CheckoutSHA}, Location);

								// Explicitly clear upstream config (origin doesn't exist yet, source is the
								// only remote that could be tracked but we don't want that). `git branch
								// --unset-upstream` aborts non-zero when nothing is set, so unset the keys
								// directly via fTryLaunchGit which ignores non-zero exits.
								co_await fTryLaunchGit({"config", "--local", "--unset-all", "branch.{}.remote"_f << _CloneExpectedBranch}, Location);
								co_await fTryLaunchGit({"config", "--local", "--unset-all", "branch.{}.merge"_f << _CloneExpectedBranch}, Location);

								if (_Repo.m_bUpdateSubmodules)
									co_await fLaunchGit({"submodule", "update", "--init"}, Location);

								if (_Repo.m_bBootstrapSource)
									co_await _BuildSystem.f_SetupBootstrapMTool();

								bChanged = true;

								co_return {};
							}
						)
						.f_Wrap()
					;

					if (!CloneResult)
					{
						co_await fCleanupLocationIfHeadUnborn();
						co_return CloneResult.f_GetException();
					}

					if (fg_IsSupportedHostingProvider(_Repo.m_OriginProperties.m_URL))
					{
						fOutputInfo
							(
								EOutputType_Warning
								, "Cloned from '{}' — origin missing. Run './mib update-repos --apply-policy --apply-policy-create-missing'\n"
								"to create origin, then './mib push' to populate it and set upstream tracking."_f
								<< _SourceName
							)
						;
					}
					else
					{
						fOutputInfo(EOutputType_Warning, "Cloned from '{}' — origin missing."_f << _SourceName);
					}

					co_return {};
				}
			;

			// `fork-remote` recovery: fork the source via hosting provider into origin's
			// namespace, optionally rename the fork's default branch to match origin's, then
			// clone origin and add the source remote locally so we can resolve pinned hashes.
			auto fForkRemoteRecovery = [&](CStr _SourceName, CRemote const &_SourceRemote, CStr _CloneExpectedBranch) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					// Captured so success messages below can reference them without re-parsing.
					NWeb::NHTTP::CURL SourceUrlOuter(_SourceRemote.m_Properties.m_URL);
					CStr SourceSlugOuter = CStr::fs_Join(SourceUrlOuter.f_GetPath(), "/").f_RemoveSuffix(".git");
					CStr OriginDefaultOuter = _Repo.m_OriginProperties.m_DefaultBranch;

					// Wrap the fork + clone + configure sequence so a failure can clean up a
					// half-initialized Location (only when HEAD is unborn — established repos
					// are preserved so the next `update-repos` can continue from partial state).
					// The fork itself persists on the hosting provider across failures; on retry,
					// origin now exists and the regular clone path takes over.
					auto ForkResult = co_await fg_CallSafe
						(
							[&]() -> TCUnsafeFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								NWeb::NHTTP::CURL OriginUrl(_Repo.m_OriginProperties.m_URL);
								CStr OriginSlug = CStr::fs_Join(OriginUrl.f_GetPath(), "/").f_RemoveSuffix(".git");
								CStr TargetOrganization = CFile::fs_GetPath(OriginSlug);
								CStr TargetName = CFile::fs_GetFile(OriginSlug);

								NWeb::NHTTP::CURL SourceUrl(_SourceRemote.m_Properties.m_URL);
								CStr SourceSlug = CStr::fs_Join(SourceUrl.f_GetPath(), "/").f_RemoveSuffix(".git");

								CStr ProviderClass = fg_GetHostingProviderClassName(_Repo.m_OriginProperties.m_URL);
								DMibCheck(!ProviderClass.f_IsEmpty()); // Caller (dispatcher) verifies this precondition.

								auto HostingProvider = CGitHostingProvider::fs_CreateHostingProvider(ProviderClass);
								auto DestroyHostingProvider = co_await fg_AsyncDestroy(HostingProvider);

								// Use origin-URL credentials: the fork API writes into origin's namespace,
								// so we need write permission for that scope. With per-org or
								// credential.useHttpPath setups, source-scoped tokens may only be read-capable.
								CStr Token = co_await fg_GetGitCredentials(OriginUrl, BaseDir);
								if (Token)
									co_await HostingProvider(&CGitHostingProvider::f_Login, CEJsonSorted{"Token"_= Token});

								// Always default-branch-only — we configure source remote locally for any
								// pinned ConfigHash that lives on a non-default branch.
								//
								// Decide whether to pass `m_Organization`:
								//  - Target == authenticated user's login → omit (personal fork into self).
								//  - Target is an organization → set m_Organization (org fork).
								//  - Target is a different user → refuse; the provider can only fork
								//    into the authenticated user's own namespace or an org they can write to.
								CGitHostingProvider::CForkRepository ForkParams;
								ForkParams.m_Name = TargetName;
								ForkParams.m_bDefaultBranchOnly = true;
								{
									auto AuthUser = co_await HostingProvider(&CGitHostingProvider::f_GetAuthenticatedUser);
									// GitHub owner names (users and orgs) are case-insensitive on the server
									// but rendered in whatever casing the owner picked. A .MHeader URL of
									// `github.com/MyUser/repo` with an authenticated login reported as
									// `myuser` still identifies the same account, so compare case-insensitively
									// to avoid rejecting valid personal forks over URL-casing mismatches.
									if (AuthUser.m_Login.f_CmpNoCase(TargetOrganization) == 0)
									{
										// Personal fork into the authenticated user's own namespace.
									}
									else if (co_await HostingProvider(&CGitHostingProvider::f_IsOrganization, TargetOrganization))
										ForkParams.m_Organization = TargetOrganization;
									else
									{
										CStr Message =
											"Cannot fork into '{}': the target is a user account that is not the authenticated user '{}'.\n"
											"Forks can only be created in the authenticated user's own namespace or in an organization they can write to."_f
											<< TargetOrganization
											<< AuthUser.m_Login
										;
										fOutputInfo(EOutputType_Error, Message);
										co_return DMibErrorInstance(Message);
									}
								}

								auto Forked = co_await HostingProvider(&CGitHostingProvider::f_ForkRepository, SourceSlug, ForkParams);

								// GitHub creates the fork asynchronously — poll f_GetRepository until 200.
								{
									constexpr uint32 c_MaxPolls = 30;
									constexpr uint32 c_PollDelayMillis = 1000;
									bool bReady = false;
									for (uint32 iPoll = 0; iPoll < c_MaxPolls; ++iPoll)
									{
										auto Probe = co_await HostingProvider(&CGitHostingProvider::f_GetRepository, OriginSlug).f_Wrap();
										if (Probe)
										{
											bReady = true;
											Forked = *Probe;
											break;
										}
										co_await fg_Timeout(fp64(c_PollDelayMillis) / 1000.0);
									}

									if (!bReady)
									{
										CStr Message = "Fork was created but did not become available within {} seconds;\n"
											"re-run './mib update-repos' to continue."_f << c_MaxPolls
										;
										fOutputInfo(EOutputType_Error, Message);
										co_return DMibErrorInstance(Message);
									}
								}

								// Determine whether to rename the fork's default branch (use the discovered
								// value from f_GetRepository, fall back to configured if empty).
								CStr SourceDefault;
								if (Forked.m_DefaultBranch && !Forked.m_DefaultBranch->f_IsEmpty())
									SourceDefault = *Forked.m_DefaultBranch;
								else
									SourceDefault = _SourceRemote.m_Properties.m_DefaultBranch;

								CStr OriginDefault = _Repo.m_OriginProperties.m_DefaultBranch;

								if (!SourceDefault.f_IsEmpty() && !OriginDefault.f_IsEmpty() && SourceDefault != OriginDefault)
								{
									// GitHub's branch rename atomically renames the branch and updates the
									// default-branch pointer. We fork with m_bDefaultBranchOnly = true so
									// the fork starts with exactly one branch (the source's default), so
									// the target name cannot pre-exist and the rename cannot collide. If
									// the API ever returns an error here it's a real configuration
									// problem (or a GitHub-side invariant violation) and we want the
									// exception to surface; don't defensively swallow it.
									co_await HostingProvider(&CGitHostingProvider::f_RenameBranch, OriginSlug, SourceDefault, OriginDefault);
								}

								// If any configured remote uses the LFS release store, ensure the mib
								// binary exists before any operation that may invoke the transfer agent
								// (the post-clone `git fetch` below, and the later fAddRemote* calls).
								if (fAnyRemoteHasLfsReleaseStore())
									co_await _BuildSystem.f_SetupGlobalMTool();

								// Now clone origin (which exists). Use origin's own clone config.
								{
									TCVector<CStr> CloneParams{"clone"};
									CloneParams.f_Insert(fGetCloneConfigParams(_Repo.m_OriginProperties, gc_Str<"origin">.m_Str));
									CloneParams.f_Insert({"-n", _Repo.m_OriginProperties.m_URL, Location});
									co_await fLaunchGit(CloneParams, "");

									if (_Repo.m_OriginProperties.m_bLfsReleaseStore)
									{
										auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
										co_await fLaunchGit({"update-ref", "-d", "refs/remotes/origin/lfs"}, Location);
										co_await fLaunchGit({"config", "--local", "--unset", "remote.origin.tagOpt"}, Location);
										co_await fLaunchGit({"fetch"}, Location);
									}
								}

								// When a ConfigHash is pinned, fetch every configured non-origin remote
								// so the subsequent checkout can resolve the commit — origin is a
								// default-branch-only fork and may not contain the pin, and the hash
								// might live on any mirror, not just the source we forked from.
								// When no ConfigHash is pinned, skip the whole add-remote pass here
								// and let the outer WantedRemotes loop in fg_HandleRepository handle
								// it through the regular clone-path code. That preserves the "always
								// fetch added remotes" invariant without turning a broken
								// source/mirror transport into a fork-remote hard failure when the
								// fetch isn't recovery-critical.
								if (!ConfigHash.f_IsEmpty())
								{
									for (auto &Remote : _Repo.m_Remotes.m_OrderedRemotes)
									{
										if (Remote.m_Name == gc_Str<"origin">.m_Str)
											continue;
										co_await fAddRemoteAndFetch(Remote.m_Name, Remote.m_Properties);
									}
								}

								// Run the standard outer wrap (checkout + upstream + submodules + bootstrap).
								// Intentional: fPopulateConfigHashFromBranchIfExists looks only at local refs
								// and origin/<_CloneExpectedBranch>. Right after a default-branch-only fork,
								// origin has only the default branch, so when _CloneExpectedBranch names a
								// feature branch the result is empty and the checkout below seeds the new
								// local branch from origin's default-branch tip. We do NOT cross-match
								// against refs/remotes/<source>/<_CloneExpectedBranch> even when that ref
								// was fetched above: the main-repo branch name has no required relationship
								// to the source's branch namespace (same rationale as the clone-remote
								// checkout above). Users who need a specific source commit pin it via
								// ConfigHash.
								CStr CloneConfigHash = co_await fPopulateConfigHashFromBranchIfExists(ConfigHash, Location, _CloneExpectedBranch);

								// fPopulateConfigHashFromBranchIfExists returns a pinned ConfigHash
								// without verifying its presence. If the pin isn't in any of the
								// fetched remotes, refuse to proceed — `git checkout -B <branch> <hash>`
								// would error anyway, but the error is clearer here.
								if (!ConfigHash.f_IsEmpty() && !(co_await fLaunchGitQuestion({"cat-file", "-e", "{}^{{commit}"_f << ConfigHash}, Location, false)))
								{
									CStr Message =
										"Neither origin (forked from '{}') nor source remote '{}' contains the pinned ConfigHash {}.\n"
										"Make a source remote containing this commit reachable, or remove the pinned hash from the config file."_f
										<< SourceSlug
										<< _SourceName
										<< ConfigHash
									;
									fOutputInfo(EOutputType_Error, Message);
									co_return DMibErrorInstance(Message);
								}

								TCVector<CStr> CheckoutParams = {"checkout", "-B", _CloneExpectedBranch};
								if (!CloneConfigHash.f_IsEmpty())
									CheckoutParams.f_Insert(CloneConfigHash);

								co_await fLaunchGit(CheckoutParams, Location);

								bChanged = true;

								co_await fSetUpstreamTracking(_CloneExpectedBranch, Location);

								if (_Repo.m_bUpdateSubmodules)
									co_await fLaunchGit({"submodule", "update", "--init"}, Location);

								if (_Repo.m_bBootstrapSource)
									co_await _BuildSystem.f_SetupBootstrapMTool();

								co_return {};
							}
						)
						.f_Wrap()
					;

					if (!ForkResult)
					{
						co_await fCleanupLocationIfHeadUnborn();
						co_return ForkResult.f_GetException();
					}

					if (_CloneExpectedBranch == OriginDefaultOuter)
					{
						fOutputInfo
							(
								EOutputType_Warning
								, "Forked '{}' into origin via hosting provider. If this repository has a policy configured,\n"
								"run './mib update-repos --apply-policy' to apply permissions and branch protection."_f
								 << SourceSlugOuter
							)
						;
					}
					else
					{
						fOutputInfo
							(
								EOutputType_Warning
								, "Forked '{}' into origin via hosting provider. Feature branch '{}' was created locally but has\n"
								"no upstream — run './mib push' to establish tracking. If this repository has a policy\n"
								"configured, also run './mib update-repos --apply-policy' to apply permissions and branch protection.\n"_f
								<< SourceSlugOuter
								<< _CloneExpectedBranch
							)
						;
					}

					co_return {};
				}
			;

			// Handle the case where `git clone -n` succeeded but local HEAD is unborn.
			// Distinguish empty-origin (recover with placeholder commit) from broken-HEAD
			// origin (fail with actionable error).
			auto fHandleEmptyOriginAfterClone = [&](CStr _CloneExpectedBranch) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					// HEAD born → no work needed (most common case).
					if (co_await fLaunchGitQuestion({"rev-parse", "--verify", "HEAD"}, Location, false))
						co_return {};

					// HEAD unborn. Distinguish empty origin vs. broken-HEAD origin.
					bool bHasRemoteRefs = co_await fLaunchGitNonEmpty({"for-each-ref", "--count=1", "refs/remotes/origin/"}, Location);

					if (bHasRemoteRefs)
					{
						// Broken-HEAD: origin has refs but doesn't advertise a default branch.
						co_await fCleanupLocationIfHeadUnborn();
						CStr Message =
							"Origin '{}' has refs but doesn't advertise a default branch (HEAD symref is unset on the server).\n"
							"Fix this on the hosting provider — for GitHub, change or set the default branch in repository\n"
							"settings; for self-hosted git, run 'git symbolic-ref HEAD refs/heads/<branch>' on the bare\n"
							"repository. Then re-run './mib update-repos'."_f
							<< _Repo.m_OriginProperties.m_URL
						;
						fOutputInfo(EOutputType_Error, Message);
						co_return DMibErrorInstance(Message);
					}

					// Genuinely empty origin. Reject pinned ConfigHash up front (we can't satisfy it).
					if (!ConfigHash.f_IsEmpty())
					{
						co_await fCleanupLocationIfHeadUnborn();
						CStr Message =
							"Origin '{}' exists but is empty, and this repository pins ConfigHash {}. The pinned commit\n"
							"doesn't exist on origin. Either populate origin via './mib push' from another working clone,\n"
							"or remove the pinned hash from the config file."_f
							<< _Repo.m_OriginProperties.m_URL
							<< ConfigHash
						;
						fOutputInfo(EOutputType_Error, Message);
						co_return DMibErrorInstance(Message);
					}

					// Switch HEAD to expected branch + create placeholder empty commit.
					// Wrap with cleanup-on-failure: if commit fails (most commonly because
					// user.name/user.email is unconfigured), the cloned .git/ would otherwise
					// be left behind with an unborn HEAD. The next run sees a non-empty
					// Location, sets bCloneNew = false, and trips on fg_GetGitHeadHash later.
					// Removing Location restores bCloneNew = true so the user can re-run after
					// fixing their identity.
					//
					// Per-repo git identity (_Repo.m_UserName / _Repo.m_UserEmail from the
					// .MHeader) is already persisted in .git/config at this point: the clone
					// above ran through fGetCloneConfigParams which emits `--config user.name=`
					// and `--config user.email=` to `git clone`, so the commit below picks them
					// up from .git/config exactly like any later commit in the repo would.
					// The "please tell me who you are" failure mode only happens when the
					// .MHeader has no identity configured AND the user has no global git
					// identity either — same prerequisite as the init recovery path and
					// documented in fg_ReconcileMissingHelp.
					auto FixupResult = co_await fg_CallSafe
						(
							[&]() -> TCUnsafeFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;
								co_await fLaunchGit({"checkout", "-B", _CloneExpectedBranch}, Location);
								co_await fLaunchGit({"commit", "--allow-empty", "-m", "Initial commit"}, Location);
								co_return {};
							}
						)
						.f_Wrap()
					;

					if (!FixupResult)
					{
						co_await fCleanupLocationIfHeadUnborn();
						fOutputInfo(EOutputType_Error, "Failed to populate empty origin with a placeholder commit: {}"_f << FixupResult.f_GetExceptionStr());
						co_return FixupResult.f_GetException();
					}

					fOutputInfo
						(
							EOutputType_Warning
							, "Origin existed but was empty; populated with a placeholder initial commit. Run './mib push'\n"
							  "to push it and establish upstream tracking. Use 'git commit --amend' later to replace the\n"
							  "placeholder with real content."
						)
					;

					co_return {};
				}
			;

			// Main recovery dispatch when `git clone` of origin fails.
			auto fHandleMissingOrigin = [&](CExceptionPointer _pOriginalCloneError, CStr _CloneExpectedBranch) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					// MalterlibRepositoryHardReset preserves the pre-recovery "hard fail on
					// any clone error" behavior. Used by CI builds that want clone failures
					// to surface immediately rather than silently kicking off a recovery path.
					if (bForceReset)
						co_return _pOriginalCloneError;

					EHandleRepositoryAction ResolvedAction = _ReconcileAction;
					bool bExplicitRecovery =
						_ReconcileAction == EHandleRepositoryAction_Init
						|| _ReconcileAction == EHandleRepositoryAction_CloneRemote
						|| _ReconcileAction == EHandleRepositoryAction_ForkRemote
					;
					bool bAutoAction =
						_ReconcileAction == EHandleRepositoryAction_None
						|| _ReconcileAction == EHandleRepositoryAction_Auto
					;

					// Reject reconcile actions that make no sense for a missing origin.
					// `leave`/`reset`/`rebase`/manual-resolve assume a local repo exists and
					// has something to reconcile with the remote; here there's no local repo
					// yet and the remote is absent, so the action cannot apply. Mirrors the
					// worktree-reconcile code's unsupported-action branch.
					if (!bExplicitRecovery && !bAutoAction)
					{
						EOutputType ActionOutputType;
						CStr ActionStr = fg_HandleRepositoryActionToString(_ReconcileAction, ActionOutputType);
						fOutputInfo
							(
								EOutputType_Error
								, "Unsupported reconcile action '{}' for missing origin recovery, only init, clone-remote, and fork-remote are supported"_f
								<< ActionStr
							)
						;
						co_return DMibErrorInstanceBuildSystemReconcileHelp(fg_ReconcileMissingHelp(o_StateHandler.f_AnsiFlags()));
					}

					// Intentional: explicit --reconcile=*:init|clone-remote|fork-remote
					// bypasses fProbeOriginExists(). If the clone failed because origin exists
					// but is unreachable (auth, network, 5xx), the probe itself is equally
					// unreliable — the authenticated-404 rule can't distinguish "missing" from
					// "private repo I can't see" for unauthenticated callers, so treating it
					// as a block rather than a recommendation would incorrectly refuse legitimate
					// recovery in common setups. The user who passed the flag has opted in; we
					// still emit "Original clone error: ..." (via fOutputInfo below) so they
					// see the underlying reason and can interrupt or undo if they made a mistake.
					// Users who want unconditional hard failure on any clone error can set
					// MalterlibRepositoryHardReset=true (common in CI).
					//
					// Codex reviews have flagged this in multiple rounds; the trade-off is
					// deliberate — do not gate explicit recovery on the probe.
					// Collect probe errors as we go so we can surface them on hard-failure
					// paths. Probes throw on credential-helper / provider / network failures;
					// we wrap via f_Wrap() here so a single flaky remote doesn't abort the
					// loop, and collect the exception for later reporting.
					NException::CExceptionExceptionVectorData::CErrorCollector ProbeErrors;

					auto fEmitCollectedProbeErrors = [&](bool _bIndent)
						{
							if (!ProbeErrors.f_HasError())
								return;
							auto Bundled = fg_Move(ProbeErrors).f_GetException();

							CStr Error = "Probe errors while evaluating remotes:\n{}"_f << NException::fg_ExceptionString(Bundled);
							if (_bIndent)
								Error = ("   " + Error).f_Indent("      ", false);
							else
								Error = Error.f_Indent("   ", false);

							fOutputInfo(EOutputType_Error, Error, _bIndent);
						}
					;

					if (!bExplicitRecovery)
					{
						// Auto / default — probe origin to decide whether recovery applies.
						auto ProbeResult = co_await fProbeOriginExists(_Repo.m_OriginProperties.m_URL).f_Wrap();
						EOriginProbeResult Probe = EOriginProbeResult::mc_Unknown;
						if (ProbeResult)
							Probe = *ProbeResult;
						else
							ProbeErrors.f_AddError(ProbeResult.f_GetException());

						if (Probe != EOriginProbeResult::mc_Missing)
						{
							fEmitCollectedProbeErrors(false);
							co_return _pOriginalCloneError;
						}

						// Will compute recommendation below.
					}

					auto fOutputOriginalCloneError = [&](bool _bIndent)
						{
							CStr Error = "Original clone error: {}"_f << NException::fg_ExceptionString(_pOriginalCloneError);
							if (_bIndent)
								Error = ("   " + Error).f_Indent("      ", false);
							else
								Error = Error.f_Indent("   ", false);
							fOutputInfo(EOutputType_Warning, Error, _bIndent);
						}
					;

					// Build reachable lists (skip for explicit Init — no source needed).
					struct CSourceCandidate
					{
						CStr m_Name;
						CRemote const *m_pRemote = nullptr;
					};

					TCVector<CSourceCandidate> ReachableRemotes;
					TCVector<CSourceCandidate> ReachableForkableRemotes;

					CStr OriginProviderClass = fg_GetHostingProviderClassName(_Repo.m_OriginProperties.m_URL);

					if (ResolvedAction != EHandleRepositoryAction_Init)
					{
						for (auto &Remote : _Repo.m_Remotes.m_OrderedRemotes)
						{
							if (Remote.m_Name == gc_Str<"origin">.m_Str)
								continue;

							auto ReachResult = co_await fProbeRemoteReachability(Remote.m_Properties.m_URL).f_Wrap();
							bool bReachable = false;
							if (ReachResult)
								bReachable = *ReachResult;
							else
								ProbeErrors.f_AddError(ReachResult.f_GetException());

							if (!bReachable)
								continue;

							ReachableRemotes.f_Insert({Remote.m_Name, &Remote});

							CStr SourceProviderClass = fg_GetHostingProviderClassName(Remote.m_Properties.m_URL);
							if (!SourceProviderClass.f_IsEmpty() && SourceProviderClass == OriginProviderClass)
								ReachableForkableRemotes.f_Insert({Remote.m_Name, &Remote});
						}
					}

					// Compute Auto recommendation if needed. Intentional asymmetry with the
					// rest of --reconcile=*:auto: we recommend but do NOT apply for missing
					// origin. init / clone-remote / fork-remote are all destructive
					// (placeholder commit, clone from arbitrary source, mutate hosting
					// provider), so the user must explicitly opt into one. The recommendation
					// plus reconcile help text tells them which to pick; they re-run with
					// the explicit action.
					if (!bExplicitRecovery)
					{
						EHandleRepositoryAction Recommended;
						if (!ReachableForkableRemotes.f_IsEmpty())
							Recommended = EHandleRepositoryAction_ForkRemote;
						else if (!ReachableRemotes.f_IsEmpty())
							Recommended = EHandleRepositoryAction_CloneRemote;
						else if (ConfigHash.f_IsEmpty())
							Recommended = EHandleRepositoryAction_Init;
						else
						{
							// No reachable source + pinned ConfigHash: `init` can't satisfy
							// the pinned commit (fInitRecovery rejects it up front), and
							// there's no source to clone or fork from. Recommend
							// ManualResolve the same way the commit reconcile code does
							// when no automatic action applies.
							Recommended = EHandleRepositoryAction_ManualResolve;
						}

						EOutputType RecommendedOutputType;
						CStr RecommendedActionStr = "{}{}{}"_f << Colors.f_RepositoryName() << fg_HandleRepositoryActionToString(Recommended, RecommendedOutputType) << Colors.f_Default();

						CStr Message;
						if (Recommended == EHandleRepositoryAction_ManualResolve)
						{
							Message =
								"Origin '{}' does not exist.\n"
								"No reachable source remote has the pinned ConfigHash:\n"
								"   {}\n"
								"Resolve manually by performing one of:\n"
								"   * Make a source remote containing the pinned commit reachable.\n"
								"   * Remove the pinned hash from the config file."_f
								<< _Repo.m_OriginProperties.m_URL
								<< ConfigHash
							;
						}
						else
						{
							Message = "Origin does not exist: {} recommended for {}"_f << RecommendedActionStr << _Repo.m_OriginProperties.m_URL;
							if (Recommended == EHandleRepositoryAction_ForkRemote)
								Message += "\n   (source remote: {} {})"_f << ReachableForkableRemotes[0].m_Name << ReachableForkableRemotes[0].m_pRemote->m_Properties.m_URL;
							else if (Recommended == EHandleRepositoryAction_CloneRemote)
								Message += "\n   (source remote: {} {})"_f << ReachableRemotes[0].m_Name << ReachableRemotes[0].m_pRemote->m_Properties.m_URL;
						}

						{
							DMibLock(o_StateHandler.f_OutputConsoleLock());
							fOutputInfo(RecommendedOutputType, Message);
							fOutputOriginalCloneError(true);
							fEmitCollectedProbeErrors(true);
						}
						co_return DMibErrorInstanceBuildSystemReconcileHelp(fg_ReconcileMissingHelp(o_StateHandler.f_AnsiFlags()));
					}

					// Explicit recovery actions:
					if (ResolvedAction == EHandleRepositoryAction_Init)
					{
						fOutputOriginalCloneError(false);
						co_await fInitRecovery(_CloneExpectedBranch);
						co_return {};
					}

					if (ResolvedAction == EHandleRepositoryAction_CloneRemote)
					{
						if (ReachableRemotes.f_IsEmpty())
						{
							CStr Message = "Cannot use 'clone-remote' on '{}': no configured remote is reachable. Use 'init' instead."_f << _Repo.f_GetName();
							{
								DMibLock(o_StateHandler.f_OutputConsoleLock());
								fOutputInfo(EOutputType_Error, Message);
								fOutputOriginalCloneError(true);
								fEmitCollectedProbeErrors(true);
							}
							co_return DMibErrorInstance(Message);
						}

						fOutputOriginalCloneError(false);

						// Intentional: always pick the first reachable remote. If ConfigHash is pinned
						// and this source doesn't contain that commit, the downstream recovery fails
						// with a clear error rather than silently retrying later candidates. Retry-with-
						// cleanup across multiple reachable sources would require unwinding a partially
						// cloned .git/ and all configured remotes on each failed attempt, which is a
						// lot of complexity for a narrow corner case. Users can reorder _Repo.m_Remotes
						// in the .MHeader to put the authoritative source first, or drop the pin.
						auto &Pick = ReachableRemotes[0];
						co_await fCloneRemoteRecovery(Pick.m_Name, *Pick.m_pRemote, _CloneExpectedBranch);
						co_return {};
					}

					if (ResolvedAction == EHandleRepositoryAction_ForkRemote)
					{
						if (OriginProviderClass.f_IsEmpty())
						{
							CStr Message = "Cannot use 'fork-remote' on '{}': origin '{}' is not on a supported hosting provider.\n"
								"Use 'clone-remote' or 'init' instead."_f
								<< _Repo.f_GetName()
								<< _Repo.m_OriginProperties.m_URL
							;
							{
								DMibLock(o_StateHandler.f_OutputConsoleLock());
								fOutputInfo(EOutputType_Error, Message);
								fOutputOriginalCloneError(true);
								fEmitCollectedProbeErrors(true);
							}
							co_return DMibErrorInstance(Message);
						}
						if (ReachableForkableRemotes.f_IsEmpty())
						{
							CStr Message = "Cannot use 'fork-remote' on '{}': no configured remote is on the same supported hosting\n"
								"provider as origin. Use 'clone-remote' or 'init' instead."_f
								<< _Repo.f_GetName()
							;
							{
								DMibLock(o_StateHandler.f_OutputConsoleLock());
								fOutputInfo(EOutputType_Error, Message);
								fOutputOriginalCloneError(true);
								fEmitCollectedProbeErrors(true);
							}
							co_return DMibErrorInstance(Message);
						}

						fOutputOriginalCloneError(false);
						// Same first-reachable selection as clone-remote above — retry across
						// candidates when the first doesn't contain a pinned ConfigHash isn't
						// worth the rollback complexity. Users can reorder _Repo.m_Remotes.
						auto &Pick = ReachableForkableRemotes[0];
						co_await fForkRemoteRecovery(Pick.m_Name, *Pick.m_pRemote, _CloneExpectedBranch);
						co_return {};
					}

					// Should be unreachable.
					co_return _pOriginalCloneError;
				}
			;

			if (bCloneNew)
			{
				CStr GitRoot = fg_GetGitRoot(Location);
				if (GitRoot.f_IsEmpty() || !_Repo.m_bSubmodule)
				{
					bool bHandledByWorktree = false;
					if (_MainRepoInfo.m_bIsWorktree)
					{
						// We're in a worktree - check if the sub-repo exists in any root worktree
						CStr RelativePath = CFile::fs_MakePathRelative(Location, BaseDir);

						CStr MainSubRepoPath;
						{
							auto RootWorktrees = co_await fg_ListWorktreePaths(_Launches, BaseDir);
							for (auto &RootWorktree : RootWorktrees)
							{
								// Skip our own worktree
								if (fg_AreGitPathsSame(RootWorktree, BaseDir))
									continue;

								CStr CandidatePath = RootWorktree / RelativePath;
								if (CFile::fs_FileExists(CandidatePath + "/.git", EFileAttrib_Directory))
								{
									MainSubRepoPath = CandidatePath;
									break;
								}
							}
						}

						if (!MainSubRepoPath.f_IsEmpty())
						{
							if (ConfigHash.f_IsEmpty())
								fOutputInfo(EOutputType_Normal, "Adding external repository (worktree)");
							else
								fOutputInfo(EOutputType_Normal, "Adding external repository (worktree) at commit {} {}"_f << ConfigHash << MainSubRepoPath);

							auto Result = co_await fg_CallSafe
								(
									[&]() -> TCUnsafeFuture<void>
									{
										co_await ECoroutineFlag_CaptureMalterlibExceptions;

										co_await fFetchIfCommitMissing(ConfigHash, MainSubRepoPath);

										// Prune stale worktree entries before adding
										co_await fLaunchGit({"worktree", "prune"}, MainSubRepoPath);

										CStr WorktreeExpectedBranch = fg_GetExpectedBranch(_Repo, _MainRepoInfo.m_Branch, _MainRepoInfo.m_DefaultBranch);

										// Check if the branch already exists in the main sub-repo
										bool bBranchExists = !WorktreeExpectedBranch.f_IsEmpty()
											&& co_await fLaunchGitQuestion({"rev-parse", "--verify", "refs/heads/{}"_f << WorktreeExpectedBranch}, MainSubRepoPath, false)
										;
										CStr WorktreeConfigHash = ConfigHash;
										if (!bBranchExists)
											WorktreeConfigHash = co_await fPopulateConfigHashFromBranchIfExists(ConfigHash, MainSubRepoPath, WorktreeExpectedBranch);

										if (bBranchExists && !ConfigHash.f_IsEmpty())
										{
											// Branch exists - need to reconcile
											CStr ExistingHash = (co_await fLaunchGit({"rev-parse", WorktreeExpectedBranch}, MainSubRepoPath)).f_Trim();

											if (ExistingHash == ConfigHash)
											{
												// Already at the right commit, safe to use -B
												co_await fLaunchGit({"worktree", "add", "-B", WorktreeExpectedBranch, Location, ConfigHash}, MainSubRepoPath);
											}
											else
											{
												// Determine recommended action
												EHandleRepositoryAction RecommendedAction = EHandleRepositoryAction_ManualResolve;
												if (co_await fLaunchGitQuestion({"merge-base", "--is-ancestor", ExistingHash, ConfigHash}, MainSubRepoPath, true))
													RecommendedAction = EHandleRepositoryAction_Reset;

												EHandleRepositoryAction Action = bForceReset ? EHandleRepositoryAction_Reset : _ReconcileAction;

												// Auto resolves to the recommendation for actionable cases
												if (Action == EHandleRepositoryAction_Auto && RecommendedAction == EHandleRepositoryAction_Reset)
													Action = RecommendedAction;

												if (Action == EHandleRepositoryAction_Reset)
												{
													fOutputInfo(EOutputType_Warning, "Resetting worktree branch '{}' to {}"_f << WorktreeExpectedBranch << ConfigHash);
													co_await fLaunchGit({"worktree", "add", "-B", WorktreeExpectedBranch, Location, ConfigHash}, MainSubRepoPath);
												}
												else if (Action == EHandleRepositoryAction_Leave)
												{
													fOutputInfo(EOutputType_Normal, "Leaving worktree branch '{}' at existing commit"_f << WorktreeExpectedBranch);
													co_await fLaunchGit({"worktree", "add", "-B", WorktreeExpectedBranch, Location, ExistingHash}, MainSubRepoPath);
												}
												else if (Action == EHandleRepositoryAction_None || Action == EHandleRepositoryAction_Auto)
												{
													EOutputType RecommendedOutputType;
													CStr RecommendedActionStr = fg_HandleRepositoryActionToString(RecommendedAction, RecommendedOutputType);
													fOutputInfo
														(
															RecommendedOutputType
															, "Worktree branch '{}' already exists: {}{}{} recommended for {}{}{} -> {}{}{}"_f
															<< WorktreeExpectedBranch
															<< Colors.f_RepositoryName() << RecommendedActionStr << Colors.f_Default()
															<< Colors.f_ToPush() << ExistingHash << Colors.f_Default()
															<< Colors.f_ToPush() << ConfigHash << Colors.f_Default()
														)
													;
													co_return DMibErrorInstanceBuildSystemReconcileHelp(fg_ReconcileHelp(o_StateHandler.f_AnsiFlags()));
												}
												else
												{
													EOutputType ActionOutputType;
													CStr ActionStr = fg_HandleRepositoryActionToString(Action, ActionOutputType);
													fOutputInfo
														(
															EOutputType_Error
															, "Unsupported reconcile action '{}' for worktree creation of branch '{}', only reset and leave are supported"_f
															<< ActionStr << WorktreeExpectedBranch
														)
													;
													co_return DMibErrorInstanceBuildSystemReconcileHelp(fg_ReconcileHelp(o_StateHandler.f_AnsiFlags()));
												}
											}
										}
										else
										{
											TCVector<CStr> WorktreeParams{"worktree", "add"};

											if (!WorktreeExpectedBranch.f_IsEmpty() && !WorktreeConfigHash.f_IsEmpty())
												WorktreeParams.f_Insert({"-b", WorktreeExpectedBranch, Location, WorktreeConfigHash});
											else if (!WorktreeConfigHash.f_IsEmpty())
												WorktreeParams.f_Insert({Location, WorktreeConfigHash});
											else if (!WorktreeExpectedBranch.f_IsEmpty() && !bBranchExists)
											{
												// Use origin/<default> as explicit start-point to avoid
												// forking from a sibling worktree's current HEAD.
												// --no-track prevents git from auto-configuring origin/<default>
												// as the upstream for the new branch; fSetUpstreamTracking below
												// will set the correct upstream if origin/<branch> exists.
												CStr RemoteDefault = "origin/{}"_f << _Repo.m_OriginProperties.m_DefaultBranch;
												if (co_await fLaunchGitQuestion({"rev-parse", "--verify", RemoteDefault}, MainSubRepoPath, false))
													WorktreeParams.f_Insert({"--no-track", "-b", WorktreeExpectedBranch, Location, RemoteDefault});
												else
													WorktreeParams.f_Insert({"-b", WorktreeExpectedBranch, Location});
											}
											else if (!WorktreeExpectedBranch.f_IsEmpty())
												WorktreeParams.f_Insert({"-B", WorktreeExpectedBranch, Location, WorktreeExpectedBranch});
											else
												WorktreeParams.f_Insert({Location});

											co_await fLaunchGit(WorktreeParams, MainSubRepoPath);
										}

										bChanged = true;

										if (!WorktreeExpectedBranch.f_IsEmpty())
											co_await fSetUpstreamTracking(WorktreeExpectedBranch, Location);

										if (_Repo.m_bUpdateSubmodules)
											co_await fLaunchGit({"submodule", "update", "--init"}, Location);

										if (_Repo.m_bBootstrapSource)
											co_await _BuildSystem.f_SetupBootstrapMTool();

										co_return {};
									}
								)
								.f_Wrap()
							;
							if (!Result)
							{
								// Any error in the repository management takes precedence, and we don't want double report of the same error when retrying
								_BuildSystem.f_SetDisableOnDemandPositions();
								CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to add worktree for repository: {}", Result.f_GetExceptionStr()));
							}

							bHandledByWorktree = true;
						}
					}
					else if (_MainRepoInfo.m_bIsValid)
					{
						// We're in the main working tree - check if the sub-repo exists in any worktree
						CStr RelativePath = CFile::fs_MakePathRelative(Location, BaseDir);
						CStr WorktreeSubRepoPath = co_await fg_FindSubRepoInWorktrees(_Launches, BaseDir, RelativePath);

						if (!WorktreeSubRepoPath.f_IsEmpty())
						{
							// Sub-repo exists as standalone clone in a worktree - transfer .git/ here
							if (ConfigHash.f_IsEmpty())
								fOutputInfo(EOutputType_Normal, "Adding external repository (transfer from worktree)");
							else
								fOutputInfo(EOutputType_Normal, "Adding external repository (transfer from worktree) at commit {}"_f << ConfigHash);

							CStr WorktreeName = CFile::fs_GetFile(WorktreeSubRepoPath);

							auto Result = co_await fg_CallSafe
								(
									[&]() -> TCUnsafeFuture<void>
									{
										co_await ECoroutineFlag_CaptureMalterlibExceptions;

										CFile::fs_CreateDirectory(Location);
										co_await fg_TransferGitDirWorktreeToMain(_Launches, WorktreeSubRepoPath, Location, WorktreeName);

										co_await fFetchIfCommitMissing(ConfigHash, Location);

										CStr TransferExpectedBranch = fg_GetExpectedBranch(_Repo, _MainRepoInfo.m_Branch, _MainRepoInfo.m_DefaultBranch);
										CStr TransferConfigHash = co_await fPopulateConfigHashFromBranchIfExists(ConfigHash, Location, TransferExpectedBranch);

										TCVector<CStr> Params = {"checkout", "-B", TransferExpectedBranch};

										if (!TransferConfigHash.f_IsEmpty())
											Params.f_Insert(TransferConfigHash);

										co_await fLaunchGit(Params, Location);

										bChanged = true;

										co_await fSetUpstreamTracking(TransferExpectedBranch, Location);

										if (_Repo.m_bUpdateSubmodules)
											co_await fLaunchGit({"submodule", "update", "--init"}, Location);

										if (_Repo.m_bBootstrapSource)
											co_await _BuildSystem.f_SetupBootstrapMTool();

										co_return {};
									}
								)
								.f_Wrap()
							;
							if (!Result)
							{
								// Any error in the repository management takes precedence, and we don't want double report of the same error when retrying
								_BuildSystem.f_SetDisableOnDemandPositions();
								CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to transfer repository from worktree: {}", Result.f_GetExceptionStr()));
							}

							bHandledByWorktree = true;
						}
					}

					if (!bHandledByWorktree)
					{
						if (ConfigHash.f_IsEmpty())
							fOutputInfo(EOutputType_Normal, "Adding external repository (clone)");
						else
							fOutputInfo(EOutputType_Normal, "Adding external repository (clone) at commit {}"_f << ConfigHash);

						CStr CloneExpectedBranch = fg_GetExpectedBranch(_Repo, _MainRepoInfo.m_Branch, _MainRepoInfo.m_DefaultBranch);

						auto fRunOuterClonePath = [&]() -> TCUnsafeFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								CStr CloneConfigHash = co_await fPopulateConfigHashFromBranchIfExists(ConfigHash, Location, CloneExpectedBranch);

								// Safe in the clone-new path, and lets retries overwrite files left by a failed LFS smudge checkout.
								TCVector<CStr> Params = {"checkout", "-f", "-B", CloneExpectedBranch};

								if (!CloneConfigHash.f_IsEmpty())
									Params.f_Insert(CloneConfigHash);

								co_await fLaunchGit(Params, Location);

								bChanged = true;

								co_await fSetUpstreamTracking(CloneExpectedBranch, Location);

								if (_Repo.m_bUpdateSubmodules)
									co_await fLaunchGit({"submodule", "update", "--init"}, Location);

								if (_Repo.m_bBootstrapSource)
									co_await _BuildSystem.f_SetupBootstrapMTool();

								co_return {};
							}
						;

						auto fRunOuterClonePathWithRetry = [&]() -> TCUnsafeFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								constexpr uint32 c_MaxCloneSetupAttempts = 6;
								for (uint32 iAttempt = 0; iAttempt < c_MaxCloneSetupAttempts; ++iAttempt)
								{
									auto Result = co_await fRunOuterClonePath().f_Wrap();
									if (Result)
										co_return {};

									CStr Error = Result.f_GetExceptionStr();
									if (!fCloneFailureLooksTransient(Error) || iAttempt + 1 == c_MaxCloneSetupAttempts)
										co_return Result.f_GetException();

									fOutputInfo
										(
											EOutputType_Warning
											, "Git checkout/LFS setup failed with a transient transport error; retrying attempt {}/{}\n{}"_f
											<< (iAttempt + 2)
											<< c_MaxCloneSetupAttempts
											<< Error
										)
									;
									co_await fg_Timeout(fp64(1 << (iAttempt + 1)) * 2.0);
								}

								co_return {};
							}
						;

						// Inner wrap: just the clone + LFS-release-store config tweaks. If this
						// fails, origin may be missing and we want to consider recovery actions.
						auto InnerCloneResult = co_await fg_CallSafe
							(
								[&]() -> TCUnsafeFuture<void>
								{
									co_await ECoroutineFlag_CaptureMalterlibExceptions;

									TCVector<CStr> CloneParams{"clone"};

									CloneParams.f_Insert(fGetCloneConfigParams(_Repo.m_OriginProperties, gc_Str<"origin">.m_Str));
									CloneParams.f_Insert({"-n", _Repo.m_OriginProperties.m_URL, Location});

									co_await fLaunchGitCloneWithRetry(CloneParams);

									if (_Repo.m_OriginProperties.m_bLfsReleaseStore)
									{
										auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

										co_await fLaunchGit({"update-ref", "-d", "refs/remotes/origin/lfs"}, Location);
										co_await fLaunchGit({"config", "--local", "--unset", "remote.origin.tagOpt"}, Location);
										co_await fLaunchGit({"fetch"}, Location);
									}

									co_return {};
								}
							)
							.f_Wrap()
						;

						bool bSkipOuterWrap = false;

						if (!InnerCloneResult)
						{
							// Clone itself failed. Try recovery (init / clone-remote / fork-remote).
							auto pOriginalCloneError = InnerCloneResult.f_GetException();

							auto RecoveryResult = co_await fHandleMissingOrigin(pOriginalCloneError, CloneExpectedBranch).f_Wrap();

							if (!RecoveryResult)
							{
								// Any error in the repository management takes precedence, and we don't want double report of the same error when retrying
								_BuildSystem.f_SetDisableOnDemandPositions();
								CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to clone repository: {}", RecoveryResult.f_GetExceptionStr()));
							}

							// Recovery handled the full setup (init/clone-remote skip outer
							// wrap entirely; fork-remote sets up everything before invoking
							// the outer wrap itself if needed).
							bSkipOuterWrap = true;
						}
						else
						{
							// Clone succeeded. Check for unborn HEAD: empty origin (recover by
							// creating a placeholder commit) vs broken-HEAD origin (fail with
							// actionable error and clean up).
							auto FixupResult = co_await fHandleEmptyOriginAfterClone(CloneExpectedBranch).f_Wrap();

							if (!FixupResult)
							{
								// Any error in the repository management takes precedence, and we don't want double report of the same error when retrying
								_BuildSystem.f_SetDisableOnDemandPositions();
								CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to clone repository: {}", FixupResult.f_GetExceptionStr()));
							}
						}

						if (!bSkipOuterWrap)
						{
							// Outer wrap: checkout + upstream + submodules + bootstrap.
							// Failures propagate normally and do not trigger recovery — the
							// clone succeeded, so recovery against an existing .git/ would be
							// wrong and violate the bCloneNew-implies-empty invariant.
							auto OuterResult = co_await fRunOuterClonePathWithRetry().f_Wrap();

							if (!OuterResult)
							{
								// Any error in the repository management takes precedence, and we don't want double report of the same error when retrying
								_BuildSystem.f_SetDisableOnDemandPositions();
								CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to clone repository: {}", OuterResult.f_GetExceptionStr()));
							}
						}
					}
				}
				else
				{
					if (ConfigHash.f_IsEmpty())
						fOutputInfo(EOutputType_Normal, "Adding external repository (submodule)");
					else
						fOutputInfo(EOutputType_Normal, "Adding external repository (submodule) at commit {}"_f << ConfigHash);

					CStr RelativeLocation = CFile::fs_MakePathRelative(Location, GitRoot);
					auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
					auto Result = co_await fg_CallSafe
						(
							[&]() -> TCUnsafeFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;
								DLock(*g_SubmoduleAddLock); // Currently git has race issues with submodule adds
								co_await fLaunchGit
									(
										{
											"submodule"
											, "add"
											, "-b"
											, _Repo.m_OriginProperties.m_DefaultBranch
											, "--name"
											, _Repo.m_SubmoduleName
											, _Repo.m_OriginProperties.m_URL
											, RelativeLocation
										}
										, GitRoot
									)
								;
								co_await fLaunchGit({"config", "-f", ".gitmodules", fg_Format("submodule.{}.fetchRecurseSubmodules", _Repo.m_SubmoduleName), "on-demand"}, GitRoot);
								bChanged = true;

								co_return {};
							}
						)
						.f_Wrap()
					;
					if (!Result)
					{
						// Any error in the repository management takes precedence, and we don't want double report of the same error when retrying
						_BuildSystem.f_SetDisableOnDemandPositions();
						CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to add submodule repository: {}", Result.f_GetExceptionStr()));
					}
				}
			}
			else if (!bIsRoot)
			{
				CStr GitRoot = fg_GetGitRoot(Location);
				if (GitRoot.f_IsEmpty() || !_Repo.m_bSubmodule)
				{
					o_StateHandler.f_AddGitIgnore(Location, _BuildSystem, _Repo.m_GitIgnoreType);

					if (_Repo.m_GitIgnoreType == EGitIgnoreType::mc_CoreExcludesFile)
					{
						CStr ConfigRepoRoot;
						CStr CurrentPath = CFile::fs_GetPath(Location);
						while (!CurrentPath.f_IsEmpty())
						{
							if (CFile::fs_FileExists(CurrentPath + "/.git", EFileAttrib_Directory | EFileAttrib_File))
							{
								ConfigRepoRoot = CurrentPath;
								break;
							}

							CStr ParentPath = CFile::fs_GetPath(CurrentPath);
							if (ParentPath == CurrentPath)
								break;

							CurrentPath = ParentPath;
						}

						if (!ConfigRepoRoot.f_IsEmpty() && o_StateHandler.f_UpdateCoreExcludesFileLocation(ConfigRepoRoot))
						{
							auto ExpectedExcludeFile = BaseDir / "BuildSystem/.localgitignore";
							CStr GitDataDir = fg_GetGitDataDir(ConfigRepoRoot, _Repo.m_Position);
							// For the main checkout fg_IsWorktree returns false and we write to
							// the shared .git/config — this is correct because git reads
							// core.excludesFile from shared config for the main worktree.
							// Linked worktrees with extensions.worktreeConfig=true read their
							// own config.worktree which overrides the shared value, so the
							// main checkout's write does not affect them.
							bool bConfigRepoIsWorktree = fg_IsWorktree(GitDataDir);

							auto fNeedUpdate = [&]
								{
									CStr ConfigFile;
									if (bConfigRepoIsWorktree)
									{
										// With worktreeConfig, excludesFile is per-worktree in config.worktree
										ConfigFile = GitDataDir + "/config.worktree";
									}
									else
									{
										CStr GitCommonDir = fg_GetGitCommonDir(GitDataDir, _Repo.m_Position);
										ConfigFile = GitCommonDir + "/config";
									}
									if (CFile::fs_FileExists(ConfigFile))
									{
										CStr ConfigContents = CFile::fs_ReadStringFromFile(ConfigFile, true);
										auto Config = CGitConfigParser::fs_Parse(ConfigContents);

										CStr const *pCurrentExcludesFile = Config.f_GetValue("core", "excludesfile");
										if (pCurrentExcludesFile && *pCurrentExcludesFile == ExpectedExcludeFile)
											return false;
									}

									return true;
								}
							;

							if (fNeedUpdate())
							{
								auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(ConfigRepoRoot);

								if (bConfigRepoIsWorktree)
								{
									// Enable worktreeConfig extension and set excludesFile per-worktree
									co_await fLaunchGit({"config", "--local", "extensions.worktreeConfig", "true"}, ConfigRepoRoot);
									co_await fLaunchGit({"config", "--worktree", "core.excludesFile", ExpectedExcludeFile}, ConfigRepoRoot);
								}
								else
									co_await fLaunchGit({"config", "--local", "core.excludesFile", ExpectedExcludeFile}, ConfigRepoRoot);
							}
						}
					}
				}
			}

			auto DynamicInfo = fg_GetRepositoryDynamicInfo(_Repo);

			bool bLocationIsWorktree = fg_IsWorktree(DynamicInfo.m_DataDir);

			auto GitConfig = fg_GetGitConfig(_Repo, DynamicInfo, bLocationIsWorktree);
			auto &CurrentRemotes = GitConfig.m_Remotes;
			auto WantedRemotes = _Repo.m_Remotes;
			auto &OriginRemote = WantedRemotes[gc_Str<"origin">.m_Str];
			OriginRemote.m_Properties = _Repo.m_OriginProperties;

			CStr ConfigLocationParam = bLocationIsWorktree ? gc_Str<"--worktree">.m_Str : gc_Str<"--local">.m_Str;

			if (bLocationIsWorktree && !GitConfig.m_bWorktreeConfig)
			{
				auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
				co_await fLaunchGit({"config", "--local", "extensions.worktreeConfig", "true"}, Location);
			}

			if (_Repo.m_UserName && GitConfig.m_UserName != _Repo.m_UserName)
			{
				auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

				fOutputInfo(EOutputType_Normal, "Changing user name '{}' -> '{}'"_f << GitConfig.m_UserName << _Repo.m_UserName);
				co_await fLaunchGit({"config", ConfigLocationParam, "user.name", _Repo.m_UserName}, Location);
			}

			if (_Repo.m_UserEmail && GitConfig.m_UserEmail != _Repo.m_UserEmail)
			{
				auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

				fOutputInfo(EOutputType_Normal, "Changing user email '{}' -> '{}'"_f << GitConfig.m_UserEmail << _Repo.m_UserEmail);
				co_await fLaunchGit({"config", ConfigLocationParam, "user.email", _Repo.m_UserEmail}, Location);
			}

			// Manage git hooks for this worktree.
			// Hook scripts are stored per-worktree under malterlib/hooks/<type>/ (main)
			// or malterlib/worktrees/<name>/<type>/ (linked), with a shared dispatcher
			// script in hooks/<type> that routes to the correct worktree at runtime.
			// For linked worktrees, the worktree root directory is also an authoritative
			// marker after the initial `git worktree add` post-checkout. During that
			// initial hook, the dispatcher ignores the destination payload (which may be
			// stale from a removed worktree with the same id) and falls back to the
			// parent worktree hook payload, then the main worktree payload.
			auto fManageHooks = [&]() -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					CStr WorktreeId;
					if (bLocationIsWorktree)
						WorktreeId = CFile::fs_GetFile(DynamicInfo.m_DataDir);

					CStr HooksDir = DynamicInfo.m_CommonDir / "hooks";
					CStr MalterlibDir = HooksDir / "malterlib";
					CStr MalterlibMainHooksDir = MalterlibDir / "hooks";
					CStr MalterlibWorktreesDir = MalterlibDir / "worktrees";
					CStr WorktreeHooksDir = WorktreeId ? (MalterlibWorktreesDir / WorktreeId) : MalterlibMainHooksDir;
					// The main worktree uses ".hooks-main-hash" rather than ".hooks-hash-main"
					// so a linked worktree whose admin-dir basename is "main" cannot collide
					// with it — linked worktree files follow the ".hooks-hash-<id>" pattern.
					CStr HashFile = MalterlibDir / (WorktreeId ? (".hooks-hash-" + WorktreeId) : CStr(".hooks-main-hash"));

					auto fWorktreeSuffix = [&]() -> CStr
						{
							return WorktreeId ? CStr(" for linked worktree") : CStr();
						}
					;

					// Helper: collect all hook types still present across main and all
					// linked worktrees so we know which dispatcher scripts to keep.
					auto fCollectAllHookTypes = [&]() -> TCSet<CStr>
						{
							TCSet<CStr> HookTypes;
							if (CFile::fs_FileExists(MalterlibMainHooksDir, EFileAttrib_Directory))
							{
								for (auto &HookTypeDir : CFile::fs_FindFiles(MalterlibMainHooksDir / "*", EFileAttrib_Directory))
									HookTypes.f_Insert(CFile::fs_GetFile(HookTypeDir));
							}
							if (CFile::fs_FileExists(MalterlibWorktreesDir, EFileAttrib_Directory))
							{
								for (auto &WorktreeDir : CFile::fs_FindFiles(MalterlibWorktreesDir / "*", EFileAttrib_Directory))
								{
									for (auto &HookTypeDir : CFile::fs_FindFiles(WorktreeDir / "*", EFileAttrib_Directory))
										HookTypes.f_Insert(CFile::fs_GetFile(HookTypeDir));
								}
							}
							return HookTypes;
						}
					;

					// Helper: remove Malterlib-managed dispatcher scripts from hooks/
					// for hook types that are no longer used by any worktree.
					auto fCleanupDispatchers = [&](TCSet<CStr> const &_ActiveHookTypes)
						{
							for (auto &ExistingFile : CFile::fs_FindFiles(HooksDir / "*", EFileAttrib_File))
							{
								CStr FileName = CFile::fs_GetFile(ExistingFile);
								if (FileName.f_StartsWith("."))
									continue;
								if (!_ActiveHookTypes.f_FindEqual(FileName))
								{
									CStr ExistingContent = CFile::fs_ReadStringFromFile(ExistingFile, true);
									if (ExistingContent.f_Find("# Managed by Malterlib") >= 0)
									{
										CFile::fs_DeleteFile(ExistingFile);
										fOutputInfo(EOutputType_Normal, "Removed dispatcher hook '{}'"_f << FileName);
									}
								}
							}
						}
					;

					if (!_Repo.m_HookConfig || _Repo.m_HookConfig->m_Hooks.f_IsEmpty())
					{
						// Clean up this worktree's hooks if the Hooks property was removed.
						// Each worktree has its own directory so this is safe — it won't
						// affect hooks installed for other worktrees. For linked worktrees,
						// keep an empty worktree root directory as an authoritative marker;
						// otherwise future non-initial hooks would fall back to the main hooks.
						bool bWorktreeHooksDirExists = CFile::fs_FileExists(WorktreeHooksDir, EFileAttrib_Directory);
						bool bHasManagedHookDirs = false;
						if (bWorktreeHooksDirExists)
							bHasManagedHookDirs = !CFile::fs_FindFiles(WorktreeHooksDir / "*", EFileAttrib_Directory).f_IsEmpty();
						bool bHashFileExists = CFile::fs_FileExists(HashFile);
						bool bNeedsUpdate = bHasManagedHookDirs || bHashFileExists || (WorktreeId && !bWorktreeHooksDirExists) || (!WorktreeId && bWorktreeHooksDirExists);

						if (bNeedsUpdate)
						{
							// Sequence on HooksDir (shared across worktrees of the same repo)
							// rather than Location, since fCleanupDispatchers mutates the shared
							// hooks/<type> dispatcher files. A single build system run won't
							// have two worktrees of the same repo, so this only serializes
							// within-process work; cross-process races (two mib commands on
							// the same repo at once) are unsupported.
							auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(HooksDir);

							if (bWorktreeHooksDirExists)
								CFile::fs_DeleteDirectoryRecursive(WorktreeHooksDir);
							if (bHashFileExists)
								CFile::fs_DeleteFile(HashFile);

							if (WorktreeId)
							{
								CFile::fs_CreateDirectory(HooksDir);
								CFile::fs_CreateDirectory(MalterlibDir);
								CFile::fs_CreateDirectory(MalterlibWorktreesDir);
								CFile::fs_CreateDirectory(WorktreeHooksDir);
							}

							if (bHasManagedHookDirs || bHashFileExists || (!WorktreeId && bWorktreeHooksDirExists))
								fOutputInfo(EOutputType_Normal, "Removed managed hooks{}"_f << fWorktreeSuffix());

							fCleanupDispatchers(fCollectAllHookTypes());
						}
						co_return {};
					}

					auto &Hooks = _Repo.m_HookConfig->m_Hooks;

					// Compute hash from all hook source files + hook type names + worktree id
					// + embedded dispatcher script so dispatcher changes in a new mib build
					// trigger reinstall without needing --force-update-hooks.
					NCryptography::CHash_SHA256 HooksHash;
					{
						CStr HashDiscriminator = WorktreeId ? WorktreeId : CStr("@main");
						HooksHash.f_AddData(HashDiscriminator.f_GetStr(), HashDiscriminator.f_GetLen());
						HooksHash.f_AddData(gc_pHookDispatcherScript, sizeof(gc_pHookDispatcherScript) - 1);
					}

					for (auto iHook = Hooks.f_GetIterator(); iHook; ++iHook)
					{
						CStr const &HookType = TCMap<CStr, TCVector<CStr>>::fs_GetKey(*iHook);
						HooksHash.f_AddData(HookType.f_GetStr(), HookType.f_GetLen());
						for (auto &FilePath : *iHook)
						{
							if (!CFile::fs_FileExists(FilePath))
							{
								CStr Message = "Hook script '{}' configured for '{}' does not exist"_f << FilePath << HookType;
								fOutputInfo(EOutputType_Error, Message);
								co_return DMibErrorInstance(Message);
							}
							HooksHash.f_AddData(FilePath.f_GetStr(), FilePath.f_GetLen());
							CStr FileContents = CFile::fs_ReadStringFromFile(FilePath, true);
							HooksHash.f_AddData(FileContents.f_GetStr(), FileContents.f_GetLen());
						}
					}

					TCSet<CStr> SeenHelperBaseNames;
					for (auto &HelperPath : _Repo.m_HookConfig->m_HelperFiles)
					{
						if (!CFile::fs_FileExists(HelperPath))
						{
							CStr Message = "Hook helper file '{}' does not exist"_f << HelperPath;
							fOutputInfo(EOutputType_Error, Message);
							co_return DMibErrorInstance(Message);
						}

						CStr HelperBaseName = CFile::fs_GetFile(HelperPath);
						if
						(
							HelperBaseName.f_GetLen() >= 4
							&& HelperBaseName[3] == '_'
							&& HelperBaseName[0] >= '0' && HelperBaseName[0] <= '9'
							&& HelperBaseName[1] >= '0' && HelperBaseName[1] <= '9'
							&& HelperBaseName[2] >= '0' && HelperBaseName[2] <= '9'
						)
						{
							CStr Message = "Hook helper file '{}' uses reserved NNN_ prefix which would cause the dispatcher to execute it as a hook"_f << HelperPath;
							fOutputInfo(EOutputType_Error, Message);
							co_return DMibErrorInstance(Message);
						}

						if (SeenHelperBaseNames.f_FindEqual(HelperBaseName))
						{
							CStr Message = "Hook helper file '{}' collides with another helper sharing basename '{}'"_f << HelperPath << HelperBaseName;
							fOutputInfo(EOutputType_Error, Message);
							co_return DMibErrorInstance(Message);
						}

						SeenHelperBaseNames[HelperBaseName];
						HooksHash.f_AddData(HelperPath.f_GetStr(), HelperPath.f_GetLen());
						CStr FileContents = CFile::fs_ReadStringFromFile(HelperPath, true);
						HooksHash.f_AddData(FileContents.f_GetStr(), FileContents.f_GetLen());
						// Include the executable bit so `chmod +x helper.sh` (content
						// unchanged) triggers a reinstall — otherwise fs_CopyFileDiff would
						// see matching content and leave the destination's permissions stale.
						uint8 HelperExecFlag = (CFile::fs_GetAttributes(HelperPath) & EFileAttrib_Executable) ? 1 : 0;
						HooksHash.f_AddData(&HelperExecFlag, 1);
					}

					CStr NewHashString = HooksHash.f_GetDigest().f_GetString();

					bool bNeedsUpdate = true;
					if (!_BuildSystem.f_GetGenerateOptions().m_bForceUpdateHooks && CFile::fs_FileExists(HashFile))
					{
						CStr ExistingHash = CFile::fs_ReadStringFromFile(HashFile, true).f_Trim();
						if (ExistingHash == NewHashString)
						{
							// Guard only against third-party tools overwriting the dispatcher
							// at .git/hooks/<type> after we installed it (notably `git lfs
							// install`, which rewrites pre-push/post-checkout/post-commit/
							// post-merge). The source-only hash still matches in that case, so
							// without this check the configured hooks would silently stop
							// running until the user ran with --force-update-hooks.
							//
							// We deliberately do NOT enumerate the managed payload under
							// .git/hooks/malterlib/... Users who hand-delete files inside that
							// directory are expected to recover with --force-update-hooks;
							// probing every script/helper on the hot path would cost far more
							// than the rare recovery it would automate.
							bNeedsUpdate = false;
							for (auto iHook = Hooks.f_GetIterator(); iHook; ++iHook)
							{
								CStr const &HookType = TCMap<CStr, TCVector<CStr>>::fs_GetKey(*iHook);

								if (!CFile::fs_FileExists(WorktreeHooksDir / HookType, EFileAttrib_Directory))
								{
									bNeedsUpdate = true;
									break;
								}

								CStr WrapperPath = HooksDir / HookType;
								if (!CFile::fs_FileExists(WrapperPath))
								{
									bNeedsUpdate = true;
									break;
								}

								CStr WrapperContent = CFile::fs_ReadStringFromFile(WrapperPath, true);
								if (WrapperContent.f_Find("# Managed by Malterlib") < 0)
								{
									bNeedsUpdate = true;
									break;
								}
							}
						}
					}

					if (!bNeedsUpdate)
						co_return {};

					// Sequence on HooksDir (shared across worktrees of the same repo)
					// rather than Location, since the install path below mutates shared
					// hooks/<type> dispatcher files. A single build system run won't have
					// two worktrees of the same repo, so this only serializes within-process
					// work; cross-process races (two mib commands on the same repo at once)
					// are unsupported.
					auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(HooksDir);

					// Check if core.hooksPath is set (locally, globally, or system-wide),
					// which would cause git to ignore the default .git/hooks directory.
					// Note: this only runs when hooks need updating, not on every invocation,
					// as a tradeoff to avoid a process launch on the hot path.
					// Note: when the Repository.Hooks property is set, Malterlib takes full
					// ownership of the listed hook types in .git/hooks — any existing hook
					// scripts at those paths (including ones installed by third-party tools
					// such as Git LFS) are replaced by the Malterlib dispatcher. Tools that
					// rely on repo-local hooks must be installed globally instead — for LFS,
					// run `git lfs install --skip-repo` so its hooks are registered via the
					// global core.hooksPath-equivalent template/hooks mechanism rather than
					// written into each repo's .git/hooks directory. core.hooksPath itself
					// is also incompatible with LFS hooks for the same reason.
					{
						auto HooksPathResult = co_await _Launches.f_Launch(Location, {"config", "core.hooksPath"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
						if (HooksPathResult.m_ExitCode == 0)
						{
							CStr HooksPath = HooksPathResult.f_GetStdOut().f_Trim();
							if (HooksPath)
								fOutputInfo(EOutputType_Warning, "core.hooksPath is set to '{}', managed hooks in .git/hooks will be ignored by git"_f << HooksPath);
						}
					}

					CFile::fs_CreateDirectory(HooksDir);
					CFile::fs_CreateDirectory(MalterlibDir);
					if (WorktreeId)
						CFile::fs_CreateDirectory(MalterlibWorktreesDir);
					CFile::fs_CreateDirectory(WorktreeHooksDir);

					// Remove hook types no longer specified for this worktree
					if (CFile::fs_FileExists(WorktreeHooksDir, EFileAttrib_Directory))
					{
						for (auto &ExistingDir : CFile::fs_FindFiles(WorktreeHooksDir / "*", EFileAttrib_Directory))
						{
							CStr ExistingHookType = CFile::fs_GetFile(ExistingDir);
							if (!Hooks.f_FindEqual(ExistingHookType))
							{
								CFile::fs_DeleteDirectoryRecursive(ExistingDir);
								fOutputInfo(EOutputType_Normal, "Removed hook '{}'{}"_f << ExistingHookType << fWorktreeSuffix());
							}
						}
					}

					// Clean up dispatcher scripts for hook types no longer used
					auto AllHookTypes = fCollectAllHookTypes();
					for (auto iHook = Hooks.f_GetIterator(); iHook; ++iHook)
						AllHookTypes.f_Insert(TCMap<CStr, TCVector<CStr>>::fs_GetKey(*iHook));
					fCleanupDispatchers(AllHookTypes);

					// Install/update configured hooks for this worktree
					for (auto iHook = Hooks.f_GetIterator(); iHook; ++iHook)
					{
						CStr const &HookType = TCMap<CStr, TCVector<CStr>>::fs_GetKey(*iHook);
						CStr HookTypeDir = WorktreeHooksDir / HookType;

						// Remove stale files before repopulating so that renamed,
						// removed, or reordered entries don't leave behind executables
						// that the wildcard dispatcher would still invoke.
						if (CFile::fs_FileExists(HookTypeDir, EFileAttrib_Directory))
							CFile::fs_DeleteDirectoryRecursive(HookTypeDir);
						CFile::fs_CreateDirectory(HookTypeDir);

						bool bHookChanged = false;

						// Zero-pad the index to 3 digits so that shell glob ordering
						// (lexicographic) matches the configured order even beyond 9 entries.
						// The dispatcher only invokes files matching this NNN_* prefix so
						// helper files copied below (without the prefix) sit alongside the
						// hook scripts — reachable via "$(dirname "$0")/<name>" — but are
						// not themselves executed as hooks.
						umint nIndex = 0;
						for (auto &FilePath : *iHook)
						{
							CStr DestFile = HookTypeDir / fg_Format("{sj3,sf0}_{}", nIndex, CFile::fs_GetFile(FilePath));
							if (CFile::fs_CopyFileDiff(FilePath, DestFile, true))
								bHookChanged = true;
							auto Attribs = CFile::fs_GetAttributes(DestFile);
							if (!(Attribs & EFileAttrib_Executable))
								CFile::fs_SetAttributes(DestFile, Attribs | EFileAttrib_Executable);
							++nIndex;
						}

						// Copy helper files (shared scripts, fixtures, etc.) into the same
						// directory so hook scripts can source or invoke them by relative
						// path. They retain their original filenames, so the dispatcher's
						// NNN_* glob excludes them from execution.
						for (auto &HelperPath : _Repo.m_HookConfig->m_HelperFiles)
						{
							CStr DestFile = HookTypeDir / CFile::fs_GetFile(HelperPath);
							if (CFile::fs_CopyFileDiff(HelperPath, DestFile, true))
								bHookChanged = true;
						}

						// Generate the worktree-aware dispatcher script.
						// Git always runs hooks from $GIT_COMMON_DIR/hooks (see path.c common_list),
						// so the dispatcher detects the active worktree at runtime and dispatches
						// to the correct per-worktree hook scripts:
						//   Main worktree:   malterlib/hooks/<type>/
						//   Linked worktree: malterlib/worktrees/<name>/<type>/
						CStr WrapperPath = HooksDir / HookType;

						// Warn when replacing a pre-existing non-Malterlib hook. Preserving
						// the old hook is a non-goal: hook types listed under
						// Repository.Hooks are fully owned by Malterlib. Third-party tools
						// (e.g. Git LFS) that want to participate must register themselves
						// globally rather than writing into this repo's .git/hooks.
						if (CFile::fs_FileExists(WrapperPath))
						{
							CStr ExistingContent = CFile::fs_ReadStringFromFile(WrapperPath, true);
							if (ExistingContent.f_Find("# Managed by Malterlib") < 0)
								fOutputInfo(EOutputType_Warning, "Overwriting existing '{}' hook not managed by Malterlib"_f << HookType);
						}

						CStr Script = gc_pHookDispatcherScript;
						NContainer::CByteVector ScriptData;
						CFile::fs_WriteStringToVector(ScriptData, Script, false);
						if (CFile::fs_CopyFileDiff(ScriptData, WrapperPath, NTime::CTime::fs_NowUTC(), EFileAttrib_Executable))
							bHookChanged = true;

						if (bHookChanged)
							fOutputInfo(EOutputType_Normal, "Updated hook '{}'{}"_f << HookType << fWorktreeSuffix());
					}

					CFile::fs_WriteStringToFile(HashFile, NewHashString, false);
					co_return {};
				}
			;
			co_await fManageHooks();

			auto fSetupLfsReleaseStorage = [&, bDidSetup = false]() -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					if (bDidSetup)
						co_return {};

					co_await _BuildSystem.f_SetupGlobalMTool();

					auto &CustomTransfer = GitConfig.m_MalterlibCustomTransfer;
					auto GlobalMibExecutable = CFile::fs_GetUserHomeDirectory() / (".Malterlib/bin/mib" + CFile::mc_ExecutableExtension);

					bool bChangedConfig = false;

					if (CustomTransfer.m_Path != GlobalMibExecutable)
					{
						auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
						bChangedConfig = true;
						co_await fLaunchGit({"config", "--local", "lfs.customtransfer.malterlib-release.path", GlobalMibExecutable}, Location);
					}

					if (CustomTransfer.m_Arguments != "lfs-release-store")
					{
						auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
						bChangedConfig = true;
						co_await fLaunchGit({"config", "--local", "lfs.customtransfer.malterlib-release.args", "lfs-release-store"}, Location);
					}

					if (!CustomTransfer.m_bConcurrent)
					{
						auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
						bChangedConfig = true;
						co_await fLaunchGit({"config", "--local", "lfs.customtransfer.malterlib-release.concurrent", "true"}, Location);
					}

					if (bChangedConfig)
						fOutputInfo(EOutputType_Normal, "Setting up LFS custom transfer agent");

					co_return {};
				}
			;

			if (!WantedRemotes.m_OrderedRemotes.f_IsEmpty())
			{
				for (auto &Remote : WantedRemotes.m_OrderedRemotes)
				{
					auto pCurrentRemote = CurrentRemotes.f_FindEqual(Remote.m_Name);
					if (pCurrentRemote)
					{
						if (pCurrentRemote->m_Url != Remote.m_Properties.m_URL)
						{
							fOutputInfo(EOutputType_Normal, "Changing remote URL '{}={}'"_f << Remote.m_Name << Remote.m_Properties.m_URL);
							co_await fLaunchGit({"remote", "set-url", Remote.m_Name, Remote.m_Properties.m_URL}, Location);
						}

						if (Remote.m_Properties.m_bLfsReleaseStore)
						{
							co_await fSetupLfsReleaseStorage();

							auto *pTransferAgent = GitConfig.m_CustomLfsTransferAgents.f_FindEqual(Remote.m_Properties.m_URL);
							if (!pTransferAgent || *pTransferAgent != "malterlib-release")
							{
								fOutputInfo(EOutputType_Normal, "Adding Malterlib Release LFS transfer agent");

								auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
								co_await fLaunchGit({"config", "--local", "lfs.{}.standalonetransferagent"_f << Remote.m_Properties.m_URL, "malterlib-release"}, Location);
							}

							if (!pCurrentRemote->m_bMalterlibLfsSetup)
							{
								fOutputInfo(EOutputType_Normal, "Setting up LFS fetch exclusions");

								auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
								for (auto &Spec : fGetLfsFetchExclusions())
									co_await fLaunchGit({"config", "--local", "--add", "remote.{}.fetch"_f << Remote.m_Name, Spec}, Location);
								co_await fLaunchGit({"config", "--local", "remote.{}.malterlib-lfs-setup"_f << Remote.m_Name, "true"}, Location);
							}
						}
						else
						{
							auto *pTransferAgent = GitConfig.m_CustomLfsTransferAgents.f_FindEqual(Remote.m_Properties.m_URL);
							if (pTransferAgent && *pTransferAgent == "malterlib-release")
							{
								fOutputInfo(EOutputType_Normal, "Removing Malterlib Release LFS transfer agent");

								auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);
								co_await fLaunchGit({"config", "--local", "--unset", "lfs.{}.standalonetransferagent"_f << Remote.m_Properties.m_URL}, Location);
							}
						}

						TCVector<CStr> WantedFetchSpecs;

						WantedFetchSpecs.f_Insert("+refs/heads/*:refs/remotes/{}/*"_f << Remote.m_Name);

						if (Remote.m_Properties.m_bLfsReleaseStore)
						{
							for (auto &Spec : fGetLfsFetchExclusions())
								WantedFetchSpecs.f_Insert(Spec);
						}

						for (auto &FetchSpec : Remote.m_Properties.m_ExtraFetchSpecs)
							WantedFetchSpecs.f_Insert(FetchSpec);

						if (pCurrentRemote->m_Fetch != WantedFetchSpecs)
						{
							fOutputInfo(EOutputType_Normal, "Updating fetch specs for remote '{}'"_f << Remote.m_Name);

							CStr RemoteConfigName = "remote.{}.fetch"_f << Remote.m_Name;

							auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

							co_await fLaunchGit({"config", "--local", "--unset-all", RemoteConfigName}, Location);

							for (auto &FetchSpec : WantedFetchSpecs)
								co_await fLaunchGit({"config", "--local", "--add", RemoteConfigName, FetchSpec}, Location);
						}

						continue;
					}
					fOutputInfo(EOutputType_Normal, "Adding remote '{}={}'"_f << Remote.m_Name << Remote.m_Properties.m_URL);
					co_await fLaunchGit({"remote", "add", Remote.m_Name, Remote.m_Properties.m_URL}, Location);

					if (Remote.m_Properties.m_bLfsReleaseStore)
						co_await fSetupLfsReleaseStorage();

					co_await fApplyRemoteLfsAndFetchSpecs(Remote.m_Name, Remote.m_Properties);

					if (_BuildSystem.f_GetGenerateOptions().m_bForceUpdateRemotes)
						co_await fLaunchGit({"fetch", "--tags", "--force", Remote.m_Name}, Location);
					else
						co_await fLaunchGit({"fetch", "--tags", Remote.m_Name}, Location);
				}

				if (_BuildSystem.f_ApplyRepoPolicy())
				{
					auto fApplyPolicyOutputInfo = [&](EOutputType _OutputType, CStr const &_String)
						{
							if (_String.f_Find("Would have:") >= 0)
								o_StateHandler.f_NotePretendPolicyOutput();
							fOutputInfo(_OutputType, _String);
						}
					;

					for (auto &Remote : WantedRemotes.m_OrderedRemotes)
					{
						bool bWantApplyPolicy = Remote.m_Properties.m_bApplyPolicy;
						// Origin-only bypass: --apply-policy-create-missing should still create
						// origin even when the repo has no Policy configured. Restricting this
						// to origin avoids speculatively creating non-origin mirrors and avoids
						// "Unsupported hosting provider" errors on read-only mirrors.
						bool bWantCreateMissing = _BuildSystem.f_ApplyRepoPolicyCreateMissing() && Remote.m_Name == gc_Str<"origin">.m_Str;

						if (!bWantApplyPolicy && !bWantCreateMissing)
							continue;

						EApplyPolicyFlag Flags = EApplyPolicyFlag::mc_None;

						if (_BuildSystem.f_ApplyRepoPolicyPretend() || Remote.m_Properties.m_bApplyPolicyPretend)
							Flags |= EApplyPolicyFlag::mc_Pretend;

						if (bWantCreateMissing)
							Flags |= EApplyPolicyFlag::mc_CreateMissing;

						CEJsonSorted const EmptyPolicy(EJsonType_Object);
						auto const &Policy = bWantApplyPolicy ? Remote.m_Properties.m_Policy : EmptyPolicy;

						co_await fg_ApplyPolicies(Remote.m_Properties.m_URL, Location, Policy, Flags, fApplyPolicyOutputInfo);
					}
				}

				if (_BuildSystem.f_UpdateLfsReleaseIndexes())
				{
					for (auto &Remote : WantedRemotes.m_OrderedRemotes)
					{
						if (!Remote.m_Properties.m_bLfsReleaseStore)
							continue;

						{
							auto LfsReleaseIndexSequence = co_await o_StateHandler.f_SequenceLfsReleaseIndexUpdates();

							TCActor<NGit::CLfsReleaseStoreService> LfsService = fg_Construct(nullptr, Location);

							auto Destroy = co_await fg_AsyncDestroy(LfsService);

							NGit::CLfsReleaseStoreService::EUpdateReleaseIndexOption Options = NGit::CLfsReleaseStoreService::EUpdateReleaseIndexOption::mc_None;

							if (_BuildSystem.f_UpdateLfsReleaseIndexesPretend())
								Options |= NGit::CLfsReleaseStoreService::EUpdateReleaseIndexOption::mc_Pretend;

							if (_BuildSystem.f_UpdateLfsReleaseIndexesPruneOrphanedAssets())
								Options |= NGit::CLfsReleaseStoreService::EUpdateReleaseIndexOption::mc_PruneOrphanedAssets;

							co_await LfsService
								(
									&NGit::CLfsReleaseStoreService::f_UpdateReleaseIndex
									, Remote.m_Name
									, Options
									, [fOutputInfo](NStr::CStr const &_Output)
									{
										fOutputInfo(EOutputType_Normal, _Output.f_TrimRight());
									}
								)
							;
						}

						co_await g_AsyncDestroy;
					}
				}

				if (bForceReset)
				{
					for (auto &Remote : CurrentRemotes.f_Entries())
					{
						auto &RemoteName = Remote.f_Key();
						if (WantedRemotes.m_Remotes.f_FindEqual(RemoteName))
							continue;
						fOutputInfo(EOutputType_Normal, "Removing remote '{}'"_f << RemoteName);
						co_await fLaunchGit({"remote", "remove", RemoteName}, Location);
					}
				}
			}

			if (bIsRoot)
				co_return false;

			CStr CurrentHash = o_StateHandler.f_GetHash(_Repo.m_StateFile, Location, _Repo.m_Identity, true);
			CStr HeadHash = fg_GetGitHeadHash(_Repo, DynamicInfo);

			CStr ExpectedBranch = fg_GetExpectedBranch(_Repo, _MainRepoInfo.m_Branch, _MainRepoInfo.m_DefaultBranch);
			CStr CurrentBranch = fg_GetBranch(_Repo, DynamicInfo);
			bool bNeedBranchSwitch = !ExpectedBranch.f_IsEmpty() && CurrentBranch != ExpectedBranch;
			CStr ExpectedBranchConfigHash = ConfigHash;
			if (bNeedBranchSwitch)
				ExpectedBranchConfigHash = co_await fPopulateConfigHashFromBranchIfExists(ConfigHash, Location, ExpectedBranch);

			CStr TargetHash;
			bool bTargetBranchExists = false;
			if (bNeedBranchSwitch)
			{
				bTargetBranchExists = fg_BranchExists(_Repo, DynamicInfo, ExpectedBranch);
				if (bTargetBranchExists)
					TargetHash = fg_GetBranchHash(_Repo, DynamicInfo, ExpectedBranch);
				else
					TargetHash = ExpectedBranchConfigHash;
			}
			else
				TargetHash = HeadHash;

			bool bReconciliationHandled = false;
			if
				(
					!ConfigHash.f_IsEmpty()
					&&
					(
						(TargetHash != ConfigHash && (CurrentHash != ConfigHash || bNeedBranchSwitch))
						|| bForceReset
					)
					&& !fg_IsSubmodule(DynamicInfo.m_DataDir)
				)
			{
				bReconciliationHandled = true;
				bool bPassException = false;
				auto Result = co_await fg_CallSafe
					(
						[&]() -> TCUnsafeFuture<void>
						{
							co_await ECoroutineFlag_CaptureMalterlibExceptions;

							if (HeadHash != ConfigHash)
							{
								TCVector<CStr> ExtraFetchParams;
								auto GitVersion = co_await fg_GetGitVersion(_Launches);
								if (bForceReset && GitVersion >= CGitVersion{2, 17})
									ExtraFetchParams.f_Insert("--prune-tags");

								co_await fFetchIfCommitMissing(ConfigHash, Location, fg_Move(ExtraFetchParams));
							}

							if (bForceReset)
							{
								if (HeadHash != ConfigHash || co_await fLaunchGitNonEmpty({"status", "--porcelain"}, Location))
								{
									// bForceReset is the CI hard-reset path: do not stash or restore
									// local changes, because the requested mode is to discard them.
									fOutputInfo(EOutputType_Warning, "Force Resetting to '{}'"_f << ConfigHash);
									co_await fLaunchGit({"checkout", "-f", "-B", ExpectedBranch, ConfigHash}, Location);

									co_await fSetUpstreamTracking(ExpectedBranch, Location);
									co_await fLaunchGit({"clean", "-fd"}, Location);
									if (_Repo.m_bUpdateSubmodules)
										co_await fLaunchGit({"submodule", "update", "--init"}, Location);

									if (_Repo.m_bBootstrapSource)
										co_await _BuildSystem.f_SetupBootstrapMTool();

									// git remote set-head origin master
									bChanged = true;
								}
							}
							else
							{
								EHandleRepositoryAction RecommendedAction = EHandleRepositoryAction_None;

								do
								{
									if (co_await fLaunchGitQuestion({"merge-base", "--is-ancestor", TargetHash, ConfigHash}, Location, true))
									{
										if (!bNeedBranchSwitch && !(co_await fLaunchGit({"status", "--porcelain"}, Location)).f_IsEmpty())
											RecommendedAction = EHandleRepositoryAction_Rebase;
										else
											RecommendedAction = EHandleRepositoryAction_Reset;
										break;
									}

									if (co_await fLaunchGitQuestion({"merge-base", "--is-ancestor", ConfigHash, TargetHash}, Location, true))
									{
										RecommendedAction = EHandleRepositoryAction_Leave;
										break;
									}

									if (!bNeedBranchSwitch && !(co_await fLaunchGit({"status", "--porcelain"}, Location)).f_IsEmpty())
									{
										RecommendedAction = EHandleRepositoryAction_ManualResolve;
										break;
									}

									bool bIsOnRemote = false;

									if
										(
											co_await fLaunchGitNonEmpty({"branch", "-r", "--contains", TargetHash}, Location)
											|| co_await fLaunchGitNonEmpty({"tag", "--contains", TargetHash}, Location)
										)
									{
										bIsOnRemote = true;
									}

									if (bIsOnRemote)
										RecommendedAction = EHandleRepositoryAction_Reset;
									else
										RecommendedAction = EHandleRepositoryAction_ManualResolve;

									CStr OwnerLocation;
									auto *pRepo = CBuildSystem::fs_FindContainingPath(_AllRepositories, _Repo.m_ConfigFile, OwnerLocation);
									if (!pRepo)
									{
										//fOutputInfo(EOutputType_Warning, "Could not find containing repo for config file '{}'"_f << _Repo.m_ConfigFile);
										break;
									}

									auto &Repo = **pRepo;

									CStr ConfigDir = CFile::fs_GetPath(_Repo.m_ConfigFile);

									CStr RepoIdentifier = _Repo.f_GetIdentifierName(ConfigDir, BaseDir);

									CStr History = co_await fLaunchGit({"log", "-p", "origin/{}"_f << Repo.m_OriginProperties.m_DefaultBranch, "--", _Repo.m_ConfigFile}, Repo.m_Location);
									CStr FilteredHistory;
									CStr LookFor = "+{} "_f << RepoIdentifier;

									bool bFoundConfig = false;
									bool bFoundCurrent = false;

									for (auto &Line : History.f_SplitLine())
									{
										if (!Line.f_StartsWith(LookFor))
											continue;
										CStr Commit = Line.f_Extract(LookFor.f_GetLen());
										if (Commit == ConfigHash)
										{
											if (bFoundCurrent)
											{
												RecommendedAction = EHandleRepositoryAction_Leave;
												break;
											}
											bFoundConfig = true;
										}
										if (Commit == TargetHash)
										{
											if (bFoundConfig)
											{
												RecommendedAction = EHandleRepositoryAction_Reset; // Our current commit is in origin history, it should be safe to do a reset
												break;
											}
											bFoundCurrent = true;
										}
									}
								}
								while (false)
									;

								EHandleRepositoryAction Action = EHandleRepositoryAction_None;

								if (_ReconcileAction == EHandleRepositoryAction_Auto)
									Action = RecommendedAction;
								else if (_ReconcileAction != EHandleRepositoryAction_None)
									Action = _ReconcileAction;

								if (Action == EHandleRepositoryAction_Reset)
								{
									if (bNeedBranchSwitch)
									{
										CStr UniqueStashName = fMakeUniqueStashName(CurrentBranch);
										bool bStashed = false;
										auto RestoreOnFailure = co_await fg_AsyncDestroy(fMakeStashRestoreHook(Location, UniqueStashName, bStashed));

										if (_bStash)
											bStashed = co_await fStashIfNeeded(Location, CurrentBranch, UniqueStashName);

										fOutputInfo(EOutputType_Warning, "Switching to branch '{}' and resetting to {}"_f << ExpectedBranch << ConfigHash);
										co_await fLaunchGit({"checkout", "-B", ExpectedBranch, ConfigHash}, Location);
										RestoreOnFailure.f_Clear();

										co_await fSetUpstreamTracking(ExpectedBranch, Location);
										co_await fUnstashIfExists(Location, ExpectedBranch);
									}
									else if ((co_await fLaunchGit({"rev-parse", "--abbrev-ref", "HEAD"}, Location)).f_Trim() == "HEAD")
									{
										fOutputInfo(EOutputType_Warning, "Resetting to {} and recovering from detached head"_f << ConfigHash);
										co_await fLaunchGit({"checkout", "-f", "-B", ExpectedBranch, ConfigHash}, Location);
										co_await fSetUpstreamTracking(ExpectedBranch, Location);
									}
									else
									{
										fOutputInfo(EOutputType_Warning, "Resetting to {}"_f << ConfigHash);
										co_await fLaunchGit({"reset", "--hard", ConfigHash}, Location);
									}
									if (_Repo.m_bUpdateSubmodules)
										co_await fLaunchGit({"submodule", "update", "--init"}, Location);

									if (_Repo.m_bBootstrapSource)
										co_await _BuildSystem.f_SetupBootstrapMTool();
								}
								else if (Action == EHandleRepositoryAction_Rebase)
								{
									CStr UniqueStashName = fMakeUniqueStashName(CurrentBranch);
									bool bStashed = false;
									auto RestoreOnFailure = co_await fg_AsyncDestroy(fMakeStashRestoreHook(Location, UniqueStashName, bStashed));

									if (bNeedBranchSwitch)
									{
										if (_bStash)
											bStashed = co_await fStashIfNeeded(Location, CurrentBranch, UniqueStashName);

										fOutputInfo(EOutputType_Normal, "Switching to branch '{}' and rebasing on top of {}"_f << ExpectedBranch << ConfigHash);
										co_await fLaunchGit({"checkout", ExpectedBranch}, Location);
										RestoreOnFailure.f_Clear();
									}
									else
									{
										fOutputInfo(EOutputType_Normal, "Rebasing on top of {}"_f << ConfigHash);
										RestoreOnFailure.f_Clear();
									}

									TCSet<CStr> AllConfigFiles;
									for (auto &pRepository : _AllRepositories)
									{
										AllConfigFiles[pRepository->m_ConfigFile];
										for (auto &AdditionalFile : pRepository->m_AdditionalConfigFiles)
										{
											if (!AdditionalFile.f_IsEmpty())
												AllConfigFiles[AdditionalFile];
										}
									}

									auto fResolveConflicts = [&](CStr const &_ConflictingFiles) -> TCUnsafeFuture<bool>
										{
											co_await ECoroutineFlag_CaptureMalterlibExceptions;

											bool bAllResolved = true;
											for (auto &File : _ConflictingFiles.f_SplitLine<true>())
											{
												CStr FullPath = CFile::fs_AppendPath(_Repo.m_Location, File);

												if (AllConfigFiles.f_FindEqual(FullPath))
												{
													fOutputInfo(EOutputType_Warning, "Ignoring conflict in config file '{}'"_f << File);
													co_await fLaunchGit({"checkout", "--ours", "--", File}, Location);
													co_await fLaunchGit({"add", File}, Location);
												}
												else
													bAllResolved = false;
											}
											co_return bAllResolved;
										}
									;

									bool bAllResolved = true;
									if (auto RebaseError = co_await fTryLaunchGit({"rebase", "--autostash", ConfigHash}, Location))
									{
										while (bAllResolved)
										{
											CStr ConflictingFiles = co_await fLaunchGit({"diff", "--name-only", "--diff-filter=U"}, Location);
											if (ConflictingFiles.f_IsEmpty())
											{
												bAllResolved = false;
												break;
											}

											if (!(co_await fResolveConflicts(ConflictingFiles)))
												bAllResolved = false;

											if (bAllResolved)
											{
												if ((co_await fLaunchGit({"status", "--porcelain"}, Location)).f_IsEmpty())
												{
													if (!(co_await fTryLaunchGit({"rebase", "--skip"}, Location)))
														break;
												}
												else
												{
													if (!(co_await fTryLaunchGit({"rebase", "--continue"}, Location)))
														break;
												}
											}
										}

										if (!bAllResolved)
										{
											fOutputInfo(EOutputType_Error, "Failed to automatically resolve rebase conflicts:\n\n{}\n"_f << RebaseError.f_Trim());
											DMibError("Aborting, resolve conflicts manually");
										}
									}

									if (bAllResolved)
									{
										CStr ConflictingFiles = co_await fLaunchGit({"diff", "--name-only", "--diff-filter=U"}, Location);
										if (!ConflictingFiles.f_IsEmpty())
											co_await fResolveConflicts(ConflictingFiles);

										if (_Repo.m_bUpdateSubmodules)
											co_await fLaunchGit({"submodule", "update", "--init"}, Location);

										if (_Repo.m_bBootstrapSource)
											co_await _BuildSystem.f_SetupBootstrapMTool();
									}

									if (bNeedBranchSwitch)
										co_await fUnstashIfExists(Location, ExpectedBranch);
								}
								else if (Action == EHandleRepositoryAction_ManualResolve)
								{
									fOutputInfo(EOutputType_Error, "Manual reconcile against {} needed"_f << ConfigHash);
									bPassException = true;
									co_return DMibErrorInstance("Manual reconcile needed");
								}
								else if (Action == EHandleRepositoryAction_Leave)
								{
									if (bNeedBranchSwitch)
									{
										CStr UniqueStashName = fMakeUniqueStashName(CurrentBranch);
										bool bStashed = false;
										auto RestoreOnFailure = co_await fg_AsyncDestroy(fMakeStashRestoreHook(Location, UniqueStashName, bStashed));

										if (_bStash)
											bStashed = co_await fStashIfNeeded(Location, CurrentBranch, UniqueStashName);

										fOutputInfo(EOutputType_Normal, "Switching to branch '{}'"_f << ExpectedBranch);
										co_await fLaunchGit({"checkout", ExpectedBranch}, Location);
										RestoreOnFailure.f_Clear();

										co_await fUnstashIfExists(Location, ExpectedBranch);
									}
								}
								else if (_ReconcileAction != EHandleRepositoryAction_Auto)
								{
									EOutputType OutputType;
									CStr ActionStr = fg_HandleRepositoryActionToString(RecommendedAction, OutputType);

									if (bNeedBranchSwitch)
									{
										fOutputInfo
											(
												OutputType
												, "Branch switch to '{}': {}{}{} recommended for {}{}{} -> {}{}{}"_f
												<< ExpectedBranch
												<< Colors.f_RepositoryName() << ActionStr << Colors.f_Default()
												<< Colors.f_ToPush() << TargetHash << Colors.f_Default()
												<< Colors.f_ToPush() << ConfigHash << Colors.f_Default()
											)
										;
									}
									else
									{
										fOutputInfo
											(
												OutputType
												, "{}{}{} recommended for {}{}{} -> {}{}{}"_f
												<< Colors.f_RepositoryName() << ActionStr << Colors.f_Default()
												<< Colors.f_ToPush() << TargetHash << Colors.f_Default()
												<< Colors.f_ToPush() << ConfigHash << Colors.f_Default()
											)
										;
									}
									bPassException = true;
									co_return DMibErrorInstanceBuildSystemReconcileHelp(fg_ReconcileHelp(o_StateHandler.f_AnsiFlags()));
								}
								bChanged = true;
							}

							co_return {};
						}
					)
					.f_Wrap()
				;

				if (!Result)
				{
					if (bPassException)
						co_return fg_Move(Result).f_GetException();

					CStr ErrorString = Result.f_GetExceptionStr().f_Trim();

					fOutputInfo(EOutputType_Error, "Reconcile error: {}"_f << ErrorString);
					// Any error in the repository management takes precedence, and we don't want double report of the same error when retrying.
					// Parse errors where we are interested in detailed positions would come from property evaluation, not git commands.
					_BuildSystem.f_SetDisableOnDemandPositions();
					CBuildSystem::fs_ThrowError(_Repo.m_Position, "Failed to reconcile hash '{}': {}"_f << ConfigHash << ErrorString);
				}
			}

			if (!bCloneNew && !bIsRoot && bNeedBranchSwitch && !bReconciliationHandled && !fg_IsSubmodule(DynamicInfo.m_DataDir))
			{
				if (!bTargetBranchExists)
				{
					co_await fFetchIfCommitMissing(ExpectedBranchConfigHash, Location);

					CStr UniqueStashName = fMakeUniqueStashName(CurrentBranch);
					bool bStashed = false;
					auto RestoreOnFailure = co_await fg_AsyncDestroy(fMakeStashRestoreHook(Location, UniqueStashName, bStashed));

					if (_bStash)
						bStashed = co_await fStashIfNeeded(Location, CurrentBranch, UniqueStashName);

					o_StateHandler.f_IncrementBranchCreated(CurrentBranch, ExpectedBranch, RepositoryIdentifier);
					if (!ExpectedBranchConfigHash.f_IsEmpty())
						co_await fLaunchGit({"checkout", "-b", ExpectedBranch, ExpectedBranchConfigHash}, Location);
					else
						co_await fLaunchGit({"checkout", "-b", ExpectedBranch}, Location);
					RestoreOnFailure.f_Clear();

					co_await fSetUpstreamTracking(ExpectedBranch, Location);
					bool bUnstashed = co_await fUnstashIfExists(Location, ExpectedBranch);
					if ((!ExpectedBranchConfigHash.f_IsEmpty() && HeadHash != ExpectedBranchConfigHash) || bStashed || bUnstashed)
						bChanged = true;
				}
				else if (TargetHash == ConfigHash || ConfigHash.f_IsEmpty())
				{
					CStr UniqueStashName = fMakeUniqueStashName(CurrentBranch);
					bool bStashed = false;
					auto RestoreOnFailure = co_await fg_AsyncDestroy(fMakeStashRestoreHook(Location, UniqueStashName, bStashed));

					if (_bStash)
						bStashed = co_await fStashIfNeeded(Location, CurrentBranch, UniqueStashName);

					o_StateHandler.f_IncrementBranchSwitched(CurrentBranch, ExpectedBranch, RepositoryIdentifier);
					co_await fLaunchGit({"checkout", ExpectedBranch}, Location);
					RestoreOnFailure.f_Clear();

					bool bUnstashed = co_await fUnstashIfExists(Location, ExpectedBranch);
					if (HeadHash != TargetHash || bStashed || bUnstashed)
						bChanged = true;
				}
			}

			CStr GitHeadHash = fg_GetGitHeadHash(_Repo, DynamicInfo);
			o_StateHandler.f_SetHash(_Repo.m_StateFile, Location, GitHeadHash, _Repo.m_Identity, true);
			o_StateHandler.f_SetHash(_Repo.m_ConfigFile, Location, GitHeadHash, _Repo.m_Identity, false);
			o_StateHandler.f_AddGitIgnore(_Repo.m_StateFile, _BuildSystem, EGitIgnoreType::mc_GitIgnore);

			co_return bChanged;
		}

		// Reads the global PerforceRoot.RepoCommit DSL object off the root
		// entity. The Perforce root is not a %Repository, so both repo-commit
		// (when generating the outermost changelist) and list-commits (when
		// deciding whether to collapse a log entry to its first line) route
		// through this helper to stay aligned on the configured MessageHeader
		// and TransformScript. Returns an empty optional when the property is
		// unset so callers can distinguish "no user override" from "override
		// with empty fields".
		NStorage::TCOptional<CRepoCommitOptions> fg_GetPerforceRootRepoCommitOptions(CBuildSystem &_BuildSystem, CBuildSystemData &_Data)
		{
			auto const PerforceRootJson = _BuildSystem.f_EvaluateEntityPropertyObject(_Data.m_RootEntity, gc_ConstKey_PerforceRoot, CEJsonSorted()).f_Move();
			if (!PerforceRootJson.f_IsValid() || !PerforceRootJson.f_IsObject())
				return {};

			// `PerforceRoot` is an optional DSL object that contains an
			// optional `RepoCommit` sub-object. When the user writes
			// `PerforceRoot {}` (or no RepoCommit inside it) the nested member
			// is absent, and the const `operator[]` on TCJsonValue throws
			// `DMibError` for a missing member. Use `f_GetMember` so the
			// helper returns an empty optional in that case instead of aborting
			// repo-commit / list-commits with a hard error.
			auto *pRepoCommitJson = PerforceRootJson.f_GetMember(gc_ConstKey_Repository_RepoCommit.m_Name);
			if (!pRepoCommitJson || !pRepoCommitJson->f_IsObject())
				return {};

			// Both members are declared as `fg_Defaulted(g_String, "")` with
			// `, true` (optional) in fp_RegisterBuiltinVariables. The evaluator
			// always runs with EDoesValueConformToTypeFlag_CanApplyDefault, so
			// whenever the enclosing RepoCommit object is present any missing
			// sibling is materialized as the empty-string default before we
			// read it — the typed f_String() accessors are safe even for
			// one-field user configs.
			CRepoCommitOptions Options;
			Options.m_MessageHeader = (*pRepoCommitJson)[gc_ConstString_MessageHeader].f_String();
			Options.m_TransformScript = (*pRepoCommitJson)[gc_ConstString_TransformScript].f_String();
			return Options;
		}

		CRepoEditor fg_GetRepoEditor(CBuildSystem &_BuildSystem, CBuildSystemData &_Data)
		{
			CStr EditorString = _BuildSystem.f_EvaluateEntityPropertyString(_Data.m_RootEntity, gc_ConstKey_MalterlibRepositoryEditor);

			CRepoEditor Editor;
			Editor.m_bOpenSequential = _BuildSystem.f_EvaluateEntityPropertyBool(_Data.m_RootEntity, gc_ConstKey_MalterlibRepositoryEditorSequential, false);

			Editor.m_Sleep = _BuildSystem.f_EvaluateEntityPropertyFloat(_Data.m_RootEntity, gc_ConstKey_MalterlibRepositoryEditorSleep, fp64(0.0));

			Editor.m_WorkingDir = _BuildSystem.f_EvaluateEntityPropertyString(_Data.m_RootEntity, gc_ConstKey_MalterlibRepositoryEditorWorkingDir, CStr());
			Editor.m_Application = fg_GetStrSepEscaped(EditorString, " ");

			while (!EditorString.f_IsEmpty())
				Editor.m_Params.f_Insert(fg_GetStrSepEscaped(EditorString, " "));

			return Editor;
		}

		TCVector<TCMap<CStr, CReposLocation>> fg_GetRepos(CBuildSystem &_BuildSystem, CBuildSystemData &_Data, EGetRepoFlag _Flags)
		{
			bool bIncludePolicy = fg_IsSet(_Flags, EGetRepoFlag::mc_IncludePolicy);
			bool bIncludeReleasePackage = fg_IsSet(_Flags, EGetRepoFlag::mc_IncludeReleasePackage);
			bool bIncludeLicense = fg_IsSet(_Flags, EGetRepoFlag::mc_IncludeLicense);
			bool bIncludeRepoCommit = fg_IsSet(_Flags, EGetRepoFlag::mc_IncludeRepoCommit);

			TCMap<CStr, CReposLocation> Repos;

			TCMap<CStr, CFilePosition> RepoRoots;

			CColors Colors(_BuildSystem.f_AnsiFlags());

			for (auto &ChildEntity : _Data.m_RootEntity.m_ChildEntitiesOrdered)
			{
				if (ChildEntity.f_GetKey().m_Type != EEntityType_Repository)
					continue;

				auto &ChildEntityData = ChildEntity.f_Data();

				bool bFatalError = false;

				try
				{
					if (!_BuildSystem.f_EvalCondition(ChildEntity, ChildEntityData.m_Condition, ChildEntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
						continue;

					CBuildSystemPropertyInfo PropertyInfoLocation;
					CStr Location = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_Location, PropertyInfoLocation, CStr());

					if (Location.f_IsEmpty())
						_BuildSystem.fs_ThrowError(PropertyInfoLocation, ChildEntityData.m_Position, "You have to specify Repository.Location");

					CStr ReposDirectory = CFile::fs_GetPath(Location);

					CBuildSystemPropertyInfo PropertyInfoConfigFile;

					auto ConfigFile = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_ConfigFile, PropertyInfoConfigFile, CStr());
					auto AdditionalConfigFiles = _BuildSystem.f_EvaluateEntityPropertyStringArray(ChildEntity, gc_ConstKey_Repository_AdditionalConfigFiles, TCVector<CStr>());
					CBuildSystemPropertyInfo PropertyInfoStateFile;
					auto StateFile = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_StateFile, PropertyInfoStateFile, CStr());
					CBuildSystemPropertyInfo PropertyInfoURL;
					auto URL = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_URL, PropertyInfoURL, CStr());
					CBuildSystemPropertyInfo PropertyInfoDefaultBranch;
					auto DefaultBranch = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_DefaultBranch, PropertyInfoDefaultBranch, CStr());
					auto DefaultUpstreamBranch = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_DefaultUpstreamBranch, CStr());
					auto Tags = _BuildSystem.f_EvaluateEntityPropertyStringArray(ChildEntity, gc_ConstKey_Repository_Tags, TCVector<CStr>());
					auto bSubmodule = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_Submodule, false);
					CBuildSystemPropertyInfo PropertyInfoSubmoduleName;
					auto SubmoduleName = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_SubmoduleName, PropertyInfoSubmoduleName, CStr());
					auto Type = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_Type, CStr());
					auto UserName = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_UserName, CStr());
					auto UserEmail = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_UserEmail, CStr());
					auto ProtectedBranches = _BuildSystem.f_EvaluateEntityPropertyStringArray(ChildEntity, gc_ConstKey_Repository_ProtectedBranches, TCVector<CStr>());
					auto ProtectedTags = _BuildSystem.f_EvaluateEntityPropertyStringArray(ChildEntity, gc_ConstKey_Repository_ProtectedTags, TCVector<CStr>());
					auto bUpdateSubmodules = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_UpdateSubmodules, false);
					auto bExcludeFromSeen = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_ExcludeFromSeen, false);
					auto bLfsReleaseStore = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_LfsReleaseStore, false);
					auto bTagPreviousOnForcePush = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_TagPreviousOnForcePush, true);
					auto bBootstrapSource = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_BootstrapSource, false);
					auto GitIgnoreTypeStr = _BuildSystem.f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Repository_GitIgnoreType, CStr());
					auto ExtraFetchSpecs = _BuildSystem.f_EvaluateEntityPropertyStringArray(ChildEntity, gc_ConstKey_Repository_ExtraFetchSpecs, TCVector<CStr>());
					auto HooksJson = _BuildSystem.f_EvaluateEntityPropertyObject(ChildEntity, gc_ConstKey_Repository_Hooks, CEJsonSorted()).f_Move();

					TCVector<CStr> NoPushRemotes = _BuildSystem.f_EvaluateEntityPropertyStringArray(ChildEntity, gc_ConstKey_Repository_NoPushRemotes, TCVector<CStr>());

					CBuildSystemPropertyInfo PropertyInfoRemotes;

					auto Remotes = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, gc_ConstKey_Repository_Remotes, PropertyInfoRemotes);

					auto &ReposLocation = Repos[ReposDirectory];

					CStr RepoName = CFile::fs_GetFile(Location);

					auto RepoMap = ReposLocation.m_Repositories(RepoName, RepoName);

					if (!RepoMap.f_WasCreated())
						_BuildSystem.fs_ThrowError(PropertyInfoLocation, ChildEntityData.m_Position, fg_Format("Duplicate repository location: {}", Location));

					if (!RepoRoots(Location, ChildEntityData.m_Position).f_WasCreated())
					{
						CBuildSystemError Error;
						Error.m_Error = "Other specification";
						Error.m_Positions = CBuildSystemUniquePositions(RepoRoots[Location], "Repository root");

						CBuildSystemUniquePositions Positions;
						Positions.f_AddPosition(ChildEntityData.m_Position, gc_ConstString_Entity);

						if (PropertyInfoLocation.m_pPositions)
							Positions.f_AddPositions(*PropertyInfoLocation.m_pPositions);

						_BuildSystem.fs_ThrowError(Positions, "Repository location already specified specified previously", fg_CreateVector(Error));
					}

					auto &Repo = *RepoMap;
					Repo.m_Identity = ChildEntity.f_GetKeyName();
					Repo.m_Position = ChildEntityData.m_Position;
					Repo.m_Location = Location;
					Repo.m_ConfigFile = ConfigFile;
					Repo.m_AdditionalConfigFiles = fg_Move(AdditionalConfigFiles);
					Repo.m_StateFile = StateFile;
					Repo.m_DefaultUpstreamBranch = DefaultUpstreamBranch;
					Repo.m_Tags.f_AddContainer(Tags);
					Repo.m_bSubmodule = bSubmodule;
					Repo.m_SubmoduleName = SubmoduleName;
					Repo.m_Type = Type;
					Repo.m_UserName = UserName;
					Repo.m_UserEmail = UserEmail;
					Repo.m_ProtectedBranches.f_AddContainer(ProtectedBranches);
					Repo.m_ProtectedTags.f_AddContainer(ProtectedTags);
					Repo.m_bUpdateSubmodules = bUpdateSubmodules;
					Repo.m_bExcludeFromSeen = bExcludeFromSeen;
					Repo.m_bBootstrapSource = bBootstrapSource;

					if (GitIgnoreTypeStr == "GitInfoExclude")
						Repo.m_GitIgnoreType = EGitIgnoreType::mc_GitInfoExclude;
					else if (GitIgnoreTypeStr == "CoreExcludesFile")
						Repo.m_GitIgnoreType = EGitIgnoreType::mc_CoreExcludesFile;
					else
						Repo.m_GitIgnoreType = EGitIgnoreType::mc_GitIgnore; // Default

					if (HooksJson.f_IsValid() && HooksJson.f_IsObject())
					{
						auto &Config = *(Repo.m_HookConfig = CHooksConfig());
						for (auto &HookEntry : fg_Const(HooksJson).f_Object())
						{
							CStr HookType = HookEntry.f_Name();
							TCVector<CStr> HookFiles = HookEntry.f_Value().f_StringArray();
							if (!HookFiles.f_IsEmpty())
								Config.m_Hooks[HookType] = fg_Move(HookFiles);
						}
						Config.m_HelperFiles = _BuildSystem.f_EvaluateEntityPropertyStringArray(ChildEntity, gc_ConstKey_Repository_HookHelperFiles, TCVector<CStr>());
					}

					if (bIncludeRepoCommit)
					{
						auto RepoCommitJson = _BuildSystem.f_EvaluateEntityPropertyObject(ChildEntity, gc_ConstKey_Repository_RepoCommit, CEJsonSorted()).f_Move();
						if (RepoCommitJson.f_IsValid() && RepoCommitJson.f_IsObject())
						{
							CRepoCommitOptions Options;
							Options.m_MessageHeader = RepoCommitJson[gc_ConstString_MessageHeader].f_AsString();
							Options.m_TransformScript = RepoCommitJson[gc_ConstString_TransformScript].f_AsString();
							Repo.m_RepoCommitOptions = fg_Move(Options);
						}
					}

					Repo.m_OriginProperties.m_URL = URL;
					Repo.m_OriginProperties.m_DefaultBranch = DefaultBranch;
					Repo.m_OriginProperties.m_bLfsReleaseStore = bLfsReleaseStore;
					Repo.m_OriginProperties.m_bTagPreviousOnForcePush = bTagPreviousOnForcePush;

					if (bIncludePolicy)
					{
						Repo.m_OriginProperties.m_bApplyPolicy = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_ApplyPolicy, false);
						if (Repo.m_OriginProperties.m_bApplyPolicy)
						{
							Repo.m_OriginProperties.m_Policy = _BuildSystem.f_EvaluateEntityPropertyObject
								(
									ChildEntity
									, gc_ConstKey_Repository_Policy
									, CEJsonSorted(EJsonType_Object)
								)
								.f_Move()
							;
							Repo.m_OriginProperties.m_bApplyPolicyPretend = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_ApplyPolicyPretend, false);
						}
					}

					if (bIncludeLicense)
					{
						// Types and defaults for CheckLicense/License are defined in Core/Build/Shared_RepositoryLicense.MSettings
						Repo.m_bCheckLicense = _BuildSystem.f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Repository_CheckLicense, false);
						if (Repo.m_bCheckLicense)
						{
							Repo.m_License = _BuildSystem.f_EvaluateEntityPropertyObject
								(
									ChildEntity
									, gc_ConstKey_Repository_License
									, CEJsonSorted(EJsonType_Object)
								)
								.f_Move()
							;
						}
					}

					auto fParseReleasePackage = [](CEJsonSorted const &_Value) -> CReleasePackage
						{
							CReleasePackage ReleasePackage;
							ReleasePackage.m_ReleaseName = _Value["ReleaseName"].f_String();
							ReleasePackage.m_Description = _Value["Description"].f_String();
							ReleasePackage.m_SourceReference = _Value["SourceReference"].f_String();
							ReleasePackage.m_bMakeLatest = _Value["MakeLatest"].f_Boolean();

							for (auto &Package : _Value["Packages"].f_Array())
							{
								auto &OutPackage = ReleasePackage.m_Packages.f_Insert();
								OutPackage.m_PackageName = Package["PackageName"].f_String();
								OutPackage.m_Files = Package["Files"].f_StringArray();
								OutPackage.m_CompressArguments = Package["CompressArguments"].f_StringArray();
								OutPackage.m_bCompress = Package["Compress"].f_Boolean();
							}

							return ReleasePackage;
						}
					;

					if (bIncludeReleasePackage)
					{
						auto ReleasePackageJson = _BuildSystem.f_EvaluateEntityPropertyObject(ChildEntity, gc_ConstKey_Repository_ReleasePackage, CEJsonSorted()).f_Move();
						if (ReleasePackageJson.f_IsValid())
							Repo.m_OriginProperties.m_ReleasePackage = fParseReleasePackage(ReleasePackageJson);
					}

					for (auto &Remote : Remotes.f_Get().f_Array())
					{
						CStr Name = Remote[gc_ConstString_Name].f_String();
						if (Repo.m_Remotes.m_Remotes.f_FindEqual(Name))
							_BuildSystem.fs_ThrowError(PropertyInfoRemotes, ChildEntityData.m_Position, fg_Format("Same remote '{}' specified multiple times", Name));

						CStr URL = Remote[gc_ConstString_URL].f_String();

						auto &OutRemote = Repo.m_Remotes[Name];
						OutRemote.m_Properties.m_URL = URL;

						if (auto pValue = Remote.f_GetMember(gc_ConstString_Write))
							OutRemote.m_bCanPush = pValue->f_Boolean();

						if (auto pValue = Remote.f_GetMember(gc_ConstString_DefaultBranch))
							OutRemote.m_Properties.m_DefaultBranch = pValue->f_String();

						if (auto pValue = Remote.f_GetMember(gc_ConstString_LfsReleaseStore))
							OutRemote.m_Properties.m_bLfsReleaseStore = pValue->f_Boolean();

						if (auto pValue = Remote.f_GetMember(gc_ConstString_TagPreviousOnForcePush))
							OutRemote.m_Properties.m_bTagPreviousOnForcePush = pValue->f_Boolean();

						if (auto pValue = Remote.f_GetMember(gc_ConstString_ExtraFetchSpecs))
						{
							for (auto &FetchSpec : pValue->f_Array())
								OutRemote.m_Properties.m_ExtraFetchSpecs.f_Insert(FetchSpec.f_String());
						}

						for (auto &Wildcard : NoPushRemotes)
						{
							if (fg_StrMatchWildcard(Name.f_GetStr(), Wildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							{
								OutRemote.m_bCanPush = false;
								break;
							}
						}

						if (bIncludePolicy)
						{
							OutRemote.m_Properties.m_bApplyPolicy = Remote.f_GetMemberValue(gc_ConstKey_Repository_ApplyPolicy.m_Name, false).f_Boolean();
							if (OutRemote.m_Properties.m_bApplyPolicy)
							{
								OutRemote.m_Properties.m_Policy = Remote.f_GetMemberValue(gc_ConstKey_Repository_Policy.m_Name, CEJsonSorted(EJsonType_Object));
								OutRemote.m_Properties.m_bApplyPolicyPretend = Remote.f_GetMemberValue(gc_ConstKey_Repository_ApplyPolicyPretend.m_Name, false).f_Boolean();
							}
						}

						if (bIncludeReleasePackage)
						{
							auto ReleasePackageJson = Remote.f_GetMemberValue(gc_ConstKey_Repository_ReleasePackage.m_Name, CEJsonSorted());
							if (ReleasePackageJson.f_IsValid())
								OutRemote.m_Properties.m_ReleasePackage = fParseReleasePackage(ReleasePackageJson);
						}
					}

					if (Repo.m_OriginProperties.m_URL.f_IsEmpty())
						_BuildSystem.fs_ThrowError(PropertyInfoURL, ChildEntityData.m_Position, "You have to specify Repository.URL");
					if (Repo.m_ConfigFile.f_IsEmpty() && Repo.m_Type != gc_ConstString_Root.m_String)
					{
						bFatalError = true;
						_BuildSystem.fs_ThrowError(PropertyInfoConfigFile, ChildEntityData.m_Position, "You have to specify Repository.ConfigFile");
					}
					if (Repo.m_StateFile.f_IsEmpty() && Repo.m_Type != gc_ConstString_Root.m_String)
					{
						bFatalError = true;
						_BuildSystem.fs_ThrowError(PropertyInfoStateFile, ChildEntityData.m_Position, "You have to specify Repository.StateFile");
					}
					if (Repo.m_ConfigFile == Repo.m_StateFile && Repo.m_Type != gc_ConstString_Root.m_String)
					{
						bFatalError = true;

						CBuildSystemUniquePositions Positions;
						Positions.f_AddPosition(ChildEntityData.m_Position, gc_ConstString_Entity);
						if (PropertyInfoConfigFile.m_pPositions)
							Positions.f_AddPositions(*PropertyInfoConfigFile.m_pPositions);
						if (PropertyInfoStateFile.m_pPositions)
							Positions.f_AddPositions(*PropertyInfoStateFile.m_pPositions);

						_BuildSystem.fs_ThrowError(Positions, "You have to specify Repository.ConfigFile and Repository.StateFile must not be same file");
					}
					if (Repo.m_OriginProperties.m_DefaultBranch.f_IsEmpty())
						_BuildSystem.fs_ThrowError(PropertyInfoDefaultBranch, ChildEntityData.m_Position, "You have to specify Repository.DefaultBranch");
					if (Repo.m_bSubmodule && Repo.m_SubmoduleName.f_IsEmpty())
						_BuildSystem.fs_ThrowError(PropertyInfoSubmoduleName, ChildEntityData.m_Position, "You have to specify Repository.SubmoduleName");
				}
				catch (CException const &_Exception)
				{
					if (bFatalError)
						throw;

					_BuildSystem.f_OutputConsole
						(
							"{}Ignored exception trying to get repository {}{}:\n{}\n\n"_f
							<< Colors.f_StatusWarning()
							<< ChildEntity.f_GetKey().m_Name
							<< Colors.f_Default()
							<< _Exception.f_GetErrorStr().f_Indent(DMibPFileLineFormatIndent)
							, true
						)
					;
				}
			}

			TCVector<TCMap<CStr, CReposLocation>> ReposOrdered;

			TCSet<CStr> AddedRepos;

			while (true)
			{
				bool bDoneSomething = false;
				bool bAllAdded = true;
				TCSet<CStr> ToAddRepos;
				CFilePosition LeftFilePos;

				TCMap<CStr, CReposLocation> *pTheseRepos = nullptr;

				auto fAddRepo = [&](CReposLocation const &_RepoLocation, CRepository const &_Repo)
					{
						if (AddedRepos.f_FindEqual(_Repo.m_Location))
							return;

						bAllAdded = false;

						CStr DependencyRoot;
						auto *pDependency = CBuildSystem::fs_FindContainingPath(RepoRoots, _Repo.m_ConfigFile, DependencyRoot);
						if (pDependency)
						{
							if (!AddedRepos.f_FindEqual(DependencyRoot))
							{
								LeftFilePos = _Repo.m_Position;
								return;
							}
						}

						if (!pTheseRepos)
							pTheseRepos = &ReposOrdered.f_Insert();

						*((*pTheseRepos)[_RepoLocation.f_GetPath()].m_Repositories(_Repo.f_GetName(), _Repo.f_GetName())) = _Repo;
						ToAddRepos[_Repo.m_Location];
						bDoneSomething = true;
					}
				;

				for (auto &RepoLocation : Repos)
				{
					for (auto &Repo : RepoLocation.m_Repositories)
					{
						if (Repo.m_Type != gc_ConstString_Root.m_String)
							continue;
						fAddRepo(RepoLocation, Repo);
					}
				}

				if (!bDoneSomething)
				{
					for (auto &RepoLocation : Repos)
					{
						for (auto &Repo : RepoLocation.m_Repositories)
						{
							if (Repo.m_Type == gc_ConstString_Root.m_String)
								continue;
							fAddRepo(RepoLocation, Repo);
						}
					}
				}
				AddedRepos += ToAddRepos;

				if (bAllAdded)
					break;

				if (!bDoneSomething)
					_BuildSystem.fs_ThrowError(LeftFilePos, "Circular dependency in repositories");
			}

			return ReposOrdered;
		}

	}

	using namespace NRepository;

	void CBuildSystem::f_PopulateAllRepositories(CBuildSystemData &_BuildSystemData) const
	{
		CEJsonSorted Repositories = EJsonType_Array;

		for (auto &ChildEntity : _BuildSystemData.m_RootEntity.m_ChildEntitiesOrdered)
		{
			if (ChildEntity.f_GetKey().m_Type != EEntityType_Repository)
				continue;

			auto &ChildEntityData = ChildEntity.f_Data();

			if (!f_EvalCondition(ChildEntity, ChildEntityData.m_Condition, ChildEntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
				continue;

			Repositories.f_Insert(ChildEntity.f_GetPathForGetProperty());
		}

		f_AddExternalProperty
			(
				fg_RemoveQualifiers(_BuildSystemData.m_RootEntity)
				, gc_ConstKey_AllRepositories
				, fg_Move(Repositories)
			)
		;
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::fp_HandleRepositories(TCMap<CPropertyKey, CEJsonSorted> const &_Values)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		f_InitEntityForEvaluation(mp_Data.m_RootEntity, _Values);
		f_ExpandRepositoryEntities(mp_Data);

		if (mp_GenerateOptions.m_bSkipUpdate && !mp_bForceUpdate)
			co_return ERetry_None;

		TCVector<TCMap<CStr, CReposLocation>> ReposOrdered = fg_GetRepos(*this, mp_Data, mp_bApplyRepoPolicy ? EGetRepoFlag::mc_IncludePolicy : EGetRepoFlag::mc_None);

		TCMap<CStr, CRepository const *> AllRepositories;

		for (auto &Repos : ReposOrdered)
		{
			for (auto &ReposLocation : Repos)
			{
				for (auto &Repo : ReposLocation.m_Repositories)
					AllRepositories[Repo.m_Location] = &Repo;
			}
		}

		TCAtomic<bool> bChanged;
		TCAtomic<bool> bBinariesChange;

		auto BaseDir = f_GetBaseDir();

		CStateHandler StateHandler{BaseDir, mp_OutputDir, mp_AnsiFlags, mp_fOutputConsole};

		CColors Colors(mp_AnsiFlags);

		CMainRepoInfo MainRepoInfo;
		auto pMainRepo = AllRepositories.f_FindEqual(BaseDir);
		if (pMainRepo)
		{
			auto &MainRepo = **pMainRepo;
			MainRepoInfo.m_bIsValid = true;

			auto DynamicInfo = fg_GetRepositoryDynamicInfo(MainRepo);
			MainRepoInfo.m_DataDir = DynamicInfo.m_DataDir;
			MainRepoInfo.m_Location = MainRepo.m_Location;
			MainRepoInfo.m_CommonDir = DynamicInfo.m_CommonDir;
			MainRepoInfo.m_Branch = fg_GetBranch(MainRepo, DynamicInfo);
			MainRepoInfo.m_DefaultBranch = MainRepo.m_OriginProperties.m_DefaultBranch;
			MainRepoInfo.m_bIsWorktree = fg_IsWorktree(DynamicInfo.m_DataDir);
		}
		else if (CFile::fs_FileExists(BaseDir + "/.git", EFileAttrib_Directory | EFileAttrib_File))
		{
			// Base dir is a git directory but not part of the repositories
			MainRepoInfo.m_bIsValid = true;
			MainRepoInfo.m_Location = BaseDir;
			MainRepoInfo.m_DataDir = fg_GetGitDataDir(BaseDir, CFilePosition{});
			MainRepoInfo.m_CommonDir = fg_GetGitCommonDir(MainRepoInfo.m_DataDir, CFilePosition{});
			MainRepoInfo.m_bIsWorktree = fg_IsWorktree(MainRepoInfo.m_DataDir);

			fg_DetectGitBranchInfo(MainRepoInfo.m_DataDir, MainRepoInfo.m_CommonDir, MainRepoInfo.m_Branch, MainRepoInfo.m_DefaultBranch);
		}
		else
		{
			// Non-git root: read .malterlib-branch file
			// There is no git metadata here that tells us the workspace's real default branch.
			// Use "master" as the sentinel for the unbranched/default state because mib branch
			// and mib unbranch persist that state via .malterlib-branch using the same convention.
			// This value is only the main-workspace sentinel: individual repositories still map
			// that default state to their own Repository.DefaultBranch via fg_GetExpectedBranch().
			CStr BranchFile = BaseDir + "/.malterlib-branch";
			if (CFile::fs_FileExists(BranchFile))
			{
				MainRepoInfo.m_Branch = CFile::fs_ReadStringFromFile(BranchFile, true).f_Trim();
				if (MainRepoInfo.m_Branch.f_IsEmpty())
					MainRepoInfo.m_Branch = "master";
			}
			else
				MainRepoInfo.m_Branch = "master";

			MainRepoInfo.m_DefaultBranch = "master";
		}

		auto fGetReconcileActionByName = [&](CStr const &_RepoName)
			{
				if (mp_bNoReconcileOptions)
					return EHandleRepositoryAction_None;
				EHandleRepositoryAction Action = EHandleRepositoryAction_None;
				for (auto &WildcardAction : mp_GenerateOptions.m_ReconcileActions)
				{
					auto &Wildcard = mp_GenerateOptions.m_ReconcileActions.fs_GetKey(WildcardAction);
					if (fg_StrMatchWildcard(_RepoName.f_GetStr(), Wildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						Action = WildcardAction;
				}

				return Action;
			}
		;

		auto fGetReconcileRemovedActionByName = [&](CStr const &_RepoName)
			{
				if (mp_bNoReconcileOptions)
					return EHandleRepositoryRemovedAction_None;
				EHandleRepositoryRemovedAction Action = EHandleRepositoryRemovedAction_None;
				for (auto &WildcardAction : mp_GenerateOptions.m_ReconcileRemovedActions)
				{
					auto &Wildcard = mp_GenerateOptions.m_ReconcileRemovedActions.fs_GetKey(WildcardAction);
					if (fg_StrMatchWildcard(_RepoName.f_GetStr(), Wildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						Action = WildcardAction;
				}

				return Action;
			}
		;

		auto fGetReconcileAction = [&](CRepository const &_Repo)
			{
				return fGetReconcileActionByName(_Repo.f_GetIdentifierName(BaseDir, BaseDir));
			}
		;

		umint MaxRepoWidth = 0;

		auto LastSeenRepositories = StateHandler.f_GetLastSeenRepositories();
		for (auto &RepoName : LastSeenRepositories)
			MaxRepoWidth = fg_Max(MaxRepoWidth, (umint)RepoName.f_GetLen());

		TCSet<CStr> SeenRepositories;
		TCSet<CStr> ExcludeFromSeenRepositories;

		for (auto &Repos : ReposOrdered)
		{
			for (auto &Repos : Repos)
			{
				for (auto &Repo : Repos.m_Repositories)
				{
					CStr RepoName;
					if (Repo.m_Location.f_StartsWith(BaseDir))
					{
						RepoName = CFile::fs_MakePathRelative(Repo.m_Location, BaseDir);
						if (!Repo.m_bExcludeFromSeen)
							SeenRepositories[RepoName];
						else
							ExcludeFromSeenRepositories[RepoName];
					}
					else
						RepoName = Repo.m_Location;

					MaxRepoWidth = fg_Max(MaxRepoWidth, (umint)RepoName.f_GetLen());
				}
			}
		}

		mp_MaxRepoWidth = MaxRepoWidth;

		CGitLaunches Launches{f_GetGitLaunchOptions("handle-repos"), "Check repository status"};
		Launches.f_SetProgressDelay(1.0);
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		Launches.f_MeasureRepos(ReposOrdered, false);

		for (auto &Repos : ReposOrdered)
		{
			co_await fg_ParallelForEach
				(
					Repos
					, [&](auto &_Repos) mutable -> TCUnsafeFuture<void>
					{
						co_await ECoroutineFlag_CaptureMalterlibExceptions;
						co_await f_CheckCancelled();

						CStr ReposDirectory = _Repos.f_GetPath();
						CStr LockFileName = ReposDirectory + "/RepoLock.MRepoState";

						TCUniquePointer<CLockFile> pLockFile;
						{
							auto BlockingActorCheckout = fg_BlockingActor();
							pLockFile = co_await
								(
									g_Dispatch(BlockingActorCheckout) / [&]()
									{
										CFile::fs_CreateDirectory(ReposDirectory);
										StateHandler.f_AddGitIgnore(ReposDirectory + "/RepoLock.MRepoState", *this, EGitIgnoreType::mc_GitIgnore);
										TCUniquePointer<CLockFile> pLockFile = fg_Construct(LockFileName);

										if (mp_bDebugFileLocks)
											f_OutputConsole("{} File lock: {}\n"_f << pLockFile.f_Get() << LockFileName, true);

										pLockFile->f_LockWithException(5.0*60.0);

										return fg_Move(pLockFile);
									}
								)
							;
						}

						if (mp_bDebugFileLocks)
							f_OutputConsole("{} File locked: {}\n"_f << pLockFile.f_Get() << LockFileName, true);

						auto CleanupLock = g_BlockingActorSubscription
							/ [LockFileName, bDebugFileLocks = mp_bDebugFileLocks, pLockFile = fg_Move(pLockFile), fOutputConsole = mp_fOutputConsole]
							() mutable -> TCFuture<void>
							{
								try
								{
									pLockFile.f_Clear();
									if (bDebugFileLocks && fOutputConsole)
										fOutputConsole("{} File lock released: {}\n"_f << pLockFile.f_Get() << LockFileName, true);
								}
								catch (CExceptionFile const &)
								{
								}

								co_return {};
							}
						;

						co_await fg_ParallelForEach
							(
								_Repos.m_Repositories
								, [&](auto &_Repo) -> TCUnsafeFuture<void>
								{
									co_await ECoroutineFlag_CaptureMalterlibExceptions;
									co_await f_CheckCancelled();

									auto DoneScope = Launches.f_RepoDoneScope();

									if
										(
											co_await fg_HandleRepository
											(
												Launches
												, ReposDirectory
												, _Repo
												, StateHandler
												, *this
												, AllRepositories
												, fGetReconcileAction(_Repo)
												, MaxRepoWidth
												, MainRepoInfo
												, mp_GenerateOptions.m_bStash
											)
										)
									{
										bChanged.f_Exchange(true);
										CStr RelativePath = CFile::fs_MakePathRelative(_Repo.m_Location, BaseDir);
										if (RelativePath.f_StartsWith("Binaries/Malterlib/"))
											bBinariesChange.f_Exchange(true);
									}

									co_return {};
								}
								, mp_bSingleThreaded
							)
						;
						co_await f_CheckCancelled();

						co_return {};
					}
					, mp_bSingleThreaded
				)
			;

			co_await f_CheckCancelled();

			if (bChanged.f_Load())
				break;
		}

		StateHandler.f_OutputBranchSwitchSummary(MaxRepoWidth);
		StateHandler.f_OutputPretendPolicyReminder();

		auto MergedFiles = StateHandler.f_GetMergedFiles();

		// Mirror subsets of each ConfigFile that should be kept in lockstep with
		// selected repository entries. They never affect functionality (no ownership
		// lookups, no parsing); they exist so external systems like the GitHub
		// Actions cache can key off their content from a stable location.
		//
		// Indexed by the destination mirror path so multiple repositories that share
		// a target mirror (e.g. several nested .MRepo files all keyed off one cache
		// file at the workspace root) merge into a single output instead of racing
		// to f_AddGeneratedFile with conflicting subsets.
		TCMap<CStr, TCMap<CStr, TCSet<CStr>>> AdditionalConfigSourcesByMirror;
		for (auto &Repos : ReposOrdered)
		{
			for (auto &Repos : Repos)
			{
				for (auto &Repo : Repos.m_Repositories)
				{
					if (Repo.m_ConfigFile.f_IsEmpty())
						continue;

					CStr ConfigIdentifier = Repo.m_Location;
					if (!Repo.m_Location.f_StartsWith(BaseDir))
						ConfigIdentifier = Repo.m_Identity;

					for (auto &Additional : Repo.m_AdditionalConfigFiles)
					{
						if (Additional.f_IsEmpty())
							continue;
						AdditionalConfigSourcesByMirror[Additional][Repo.m_ConfigFile][ConfigIdentifier];
					}
				}
			}
		}

		auto fBuildConfigFileContents = [](CConfigFile const &_File, CStr const &_FileName) -> CStr
			{
				CStr BasePath = CFile::fs_GetPath(_FileName);

				if (_File.m_bIsStateFile)
				{
					CRegistry Registry;

					for (auto iConfig = _File.m_Configs.f_GetIterator(); iConfig; ++iConfig)
					{
						auto &Config = *iConfig;
						if (Config.m_bExternalPath)
							Registry.f_SetValueNoPath("~" + iConfig.f_GetKey(), Config.m_Hash);
						else
							Registry.f_SetValueNoPath(CFile::fs_MakePathRelative(iConfig.f_GetKey(), BasePath), Config.m_Hash);
					}

					return Registry.f_GenerateStr().f_Replace(DMibNewLine, _File.m_LineEndings);
				}

				CJsonSorted StateJson = EJsonType_Object;

				for (auto iConfig = _File.m_Configs.f_GetIterator(); iConfig; ++iConfig)
				{
					auto &Config = *iConfig;
					if (Config.m_bExternalPath)
						StateJson["~" + iConfig.f_GetKey()]["Hash"] = Config.m_Hash;
					else
						StateJson[CFile::fs_MakePathRelative(iConfig.f_GetKey(), BasePath)]["Hash"] = Config.m_Hash;
				}

				return StateJson.f_ToString().f_Replace("\n", _File.m_LineEndings);
			}
		;

		auto fWriteConfigFile = [&](CStr const &_FileName, CConfigFile const &_File)
			{
				CStr FileContents = fBuildConfigFileContents(_File, _FileName);

				bool bWasCreated = false;
				if (!f_AddGeneratedFile(_FileName, FileContents, "", bWasCreated))
					fs_ThrowError(CFilePosition{}, CStr::CFormat("File '{}' already generated with other contents") << _FileName);

				if (bWasCreated)
				{
					CByteVector FileData;
					CFile::fs_WriteStringToVector(FileData, CStr(FileContents), false);
					CFile::fs_CreateDirectory(CFile::fs_GetPath(_FileName));
					f_WriteFile(FileData, _FileName);
				}
			}
		;

		for (auto &File : MergedFiles)
			fWriteConfigFile(MergedFiles.fs_GetKey(File), File);

		for (auto &SourceMap : AdditionalConfigSourcesByMirror)
		{
			CStr AdditionalFileName = AdditionalConfigSourcesByMirror.fs_GetKey(SourceMap);

			CConfigFile MergedMirror;
			bool bLineEndingsSet = false;

			for (auto &IdentifierSet : SourceMap)
			{
				CStr SourceConfigFile = SourceMap.fs_GetKey(IdentifierSet);
				auto *pSourceFile = MergedFiles.f_FindEqual(SourceConfigFile);
				if (!pSourceFile || pSourceFile->m_bIsStateFile)
					continue;

				if (!bLineEndingsSet)
				{
					MergedMirror.m_LineEndings = pSourceFile->m_LineEndings;
					bLineEndingsSet = true;
				}

				for (auto &Identifier : IdentifierSet)
				{
					auto *pConfig = pSourceFile->m_Configs.f_FindEqual(Identifier);
					if (pConfig)
						MergedMirror.m_Configs[Identifier] = *pConfig;
				}
			}

			if (!MergedMirror.m_Configs.f_IsEmpty())
				fWriteConfigFile(AdditionalFileName, MergedMirror);
		}

		{
			bool bForceReset = fg_GetSys()->f_GetEnvironmentVariable("MalterlibRepositoryHardReset", "") == gc_ConstString_true.m_String;
			bool bLastSeenActionNeeded = false;

			for (auto &LastSeen : LastSeenRepositories)
			{
				CStr FullRepoPath = BaseDir / LastSeen;
				if (!SeenRepositories.f_FindEqual(LastSeen) && !ExcludeFromSeenRepositories.f_FindEqual(LastSeen) && CFile::fs_FileExists(FullRepoPath, EFileAttrib_Directory))
				{
					EHandleRepositoryRemovedAction Action = fGetReconcileRemovedActionByName(LastSeen);

					if (bForceReset || Action == EHandleRepositoryRemovedAction_Delete)
					{
						bool bHandled = false;

						if (CFile::fs_FileExists(FullRepoPath + "/.git", EFileAttrib_File))
						{
							CStr SubRepoDataDir = fg_GetGitDataDir(FullRepoPath, CFilePosition{});
							if (fg_IsWorktree(SubRepoDataDir))
							{
								// Sub-repo is a worktree - find its main repo and use git worktree remove
								fg_OutputRepositoryInfo(EOutputType_Warning, "Removing worktree: {}"_f << FullRepoPath, StateHandler, LastSeen, MaxRepoWidth);

								CStr SubRepoCommonDir = fg_GetGitCommonDir(SubRepoDataDir, CFilePosition{});
								CStr MainSubRepoPath = CFile::fs_GetPath(SubRepoCommonDir);
								co_await Launches.f_Launch
									(
										MainSubRepoPath
										, {"worktree", "remove", "--force", FullRepoPath}
										, {}
										, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode
									)
								;
								bHandled = true;
							}
						}
						else if (CFile::fs_FileExists(FullRepoPath + "/.git", EFileAttrib_Directory))
						{
							// Sub-repo has a .git directory - check for worktrees to preserve
							auto Worktrees = co_await fg_ListWorktreePaths(Launches, FullRepoPath);

							// Transfer .git/ only to a linked worktree; the main checkout itself has a .git directory.
							CStr TargetWorktree;
							for (auto &WorkTree : Worktrees)
							{
								if (!fg_AreGitPathsSame(WorkTree, FullRepoPath) && CFile::fs_FileExists(WorkTree + "/.git", EFileAttrib_File))
								{
									TargetWorktree = WorkTree;
									break;
								}
							}

							if (TargetWorktree)
							{
								fg_OutputRepositoryInfo
									(
										EOutputType_Warning
										, "Transferring repository to worktree before removal: {}"_f << TargetWorktree
										, StateHandler
										, LastSeen
										, MaxRepoWidth
									)
								;
								co_await fg_TransferGitDirMainToWorktree(Launches, FullRepoPath, TargetWorktree);
								bHandled = true;
							}
						}

						if (!bHandled)
						{
							fg_OutputRepositoryInfo(EOutputType_Warning, "Deleting repository permanently from disk: {}"_f << FullRepoPath, StateHandler, LastSeen, MaxRepoWidth);
							CFile::fs_DeleteDirectoryRecursive(FullRepoPath, true);
						}
					}
					else if (Action != EHandleRepositoryRemovedAction_Leave)
					{
						if (!SeenRepositories.f_IsEmpty())
							fg_OutputRepositoryInfo(EOutputType_Warning, "Repository has been {}removed{}"_f<< Colors.f_ToPush() << Colors.f_Default(), StateHandler, LastSeen, MaxRepoWidth);
						bLastSeenActionNeeded = true;
					}
				}
			}

			if (bLastSeenActionNeeded)
			{
				if (!SeenRepositories.f_IsEmpty())
					co_return DMibErrorInstanceBuildSystemReconcileHelp(fg_ReconcileRemovedHelp(mp_AnsiFlags));
			}
			else
			{
				CStr RepositoryStateFile = mp_OutputDir / "RepositoryState.json";

				CEJsonSorted StateFile;

				auto &SeenRepositoriesJson = StateFile["SeenRepositories"];
				SeenRepositoriesJson = EJsonType_Object;
				for (auto &LastSeen : SeenRepositories)
					SeenRepositoriesJson[LastSeen] = 1;

				CStr FileContents = StateFile.f_ToString();

				bool bWasCreated = false;
				if (!f_AddGeneratedFile(RepositoryStateFile, FileContents, "", bWasCreated))
					fs_ThrowError(CFilePosition{}, CStr::CFormat("File '{}' already generated with other contents") << RepositoryStateFile);

				if (bWasCreated)
				{
					CByteVector FileData;
					CFile::fs_WriteStringToVector(FileData, CStr(FileContents), false);
					CFile::fs_CreateDirectory(CFile::fs_GetPath(RepositoryStateFile));
					f_WriteFile(FileData, RepositoryStateFile);
				}
			}
		}

		if (bChanged.f_Load())
		{
			if (mp_GenerateOptions.m_bReconcileForce)
			{
				if (bBinariesChange.f_Load())
					co_return ERetry_Relaunch;
				else
					co_return ERetry_Again;
			}
			else
			{
				if (bBinariesChange.f_Load())
					co_return ERetry_Relaunch_NoReconcileOptions;
				else
					co_return ERetry_Again_NoReconcileOptions;
			}
		}

		co_return ERetry_None;
	}
}
