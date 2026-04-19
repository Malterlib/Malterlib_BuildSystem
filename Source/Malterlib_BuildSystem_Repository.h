// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem.h"
#include <Mib/Process/ProcessLaunch>
#include <Mib/Process/ProcessLaunchActor>
#include <Mib/Encoding/EJson>
#include <Mib/Concurrency/ActorSequencerActor>
#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Time/Stopwatch>

namespace NMib::NBuildSystem::NRepository
{
	struct CColors : public CAnsiEncoding
	{
		CColors(EAnsiEncodingFlag _AnsiFlags);

		NStr::CStr f_RepositoryName() const;
		NStr::CStr f_ChangedBranchName(CStr const &_Name) const;
		NStr::CStr f_BranchName() const;

		NStr::CStr f_ToCommit() const;
		NStr::CStr f_ToPush() const;
		NStr::CStr f_ToPull() const;
	};

	enum EOutputType
	{
		EOutputType_Normal
		, EOutputType_Warning
		, EOutputType_Error
	};

	struct CRepoEditor
	{
		CStr m_Application;
		CStr m_WorkingDir;
		TCVector<CStr> m_Params;
		fp32 m_Sleep = 0.0;
		bool m_bOpenSequential = false;
	};

	struct CReleasePackage
	{
		struct CPackage
		{
			CStr m_PackageName;
			TCVector<CStr> m_Files;
			TCVector<CStr> m_CompressArguments;
			bool m_bCompress = false;
		};

		CStr m_ReleaseName;
		CStr m_Description;
		CStr m_SourceReference;
		TCVector<CPackage> m_Packages;
		bool m_bMakeLatest = true;
	};

	struct CRemoteProperties
	{
		CStr m_URL;
		CStr m_DefaultBranch;
		CEJsonSorted m_Policy;
		TCOptional<CReleasePackage> m_ReleasePackage;
		TCVector<CStr> m_ExtraFetchSpecs;
		bool m_bLfsReleaseStore = false;
		bool m_bApplyPolicy = false;
		bool m_bApplyPolicyPretend = false;
		bool m_bTagPreviousOnForcePush = true;
	};

	struct CRemote
	{
		CStr const &f_Name() const
		{
			return TCMap<CStr, CRemote>::fs_GetKey(*this);
		}

		CRemoteProperties m_Properties;
		bool m_bCanPush = true;
	};

	enum class EGitIgnoreType : uint32
	{
		mc_GitIgnore,         // Use .gitignore in parent directory
		mc_GitInfoExclude,    // Use .git/info/exclude
		mc_CoreExcludesFile   // Use core.excludesFile (BuildSystem/.localgitignore)
	};

	struct CHooksConfig
	{
		TCMap<CStr, TCVector<CStr>> m_Hooks;
		TCVector<CStr> m_HelperFiles;
	};

	struct CRepoCommitOptions
	{
		CStr m_MessageHeader;
		CStr m_TransformScript;
	};

	struct CRepository
	{
		CRepository(CStr const &_Name)
			: m_Name(_Name)
		{
		}

		CRepository(CRepository const &) = default;
		CRepository(CRepository &&) = default;
		CRepository &operator = (CRepository const &) = default;
		CRepository &operator = (CRepository &&) = default;

		CStr const &f_GetName() const
		{
			return m_Name;
		}

		CStr f_GetIdentifierName(CStr const &_BasePath, CStr const &_Root) const;

		CStr m_Name;
		CStr m_Identity;
		CStr m_Location;
		CStr m_ConfigFile;
		CStr m_StateFile;
		CStr m_DefaultUpstreamBranch;
		CStr m_SubmoduleName;
		CStr m_Type;
		CStr m_UserName;
		CStr m_UserEmail;
		TCSet<CStr> m_Tags;
		TCMap<CStr, CRemote> m_Remotes;
		TCSet<CStr> m_ProtectedBranches;
		TCSet<CStr> m_ProtectedTags;
		CFilePosition m_Position;

		CRemoteProperties m_OriginProperties;

		bool m_bSubmodule = false;
		bool m_bExcludeFromSeen = false;
		bool m_bBootstrapSource = false;
		bool m_bUpdateSubmodules = false;
		EGitIgnoreType m_GitIgnoreType = EGitIgnoreType::mc_GitIgnore;
		CEJsonSorted m_License;
		bool m_bCheckLicense = false;
		TCOptional<CHooksConfig> m_HookConfig;
		TCOptional<CRepoCommitOptions> m_RepoCommitOptions;
	};

	struct CRepositoryDynamicInfo
	{
		CStr m_DataDir;
		CStr m_CommonDir;
	};

	struct CReposLocation
	{
		CStr const &f_GetPath() const
		{
			return TCMap<CStr, CReposLocation>::fs_GetKey(*this);
		}

		TCMap<CStr, CRepository> m_Repositories;
	};

	struct CRepositoryConfig
	{
		CStr m_Hash;
		bool m_bExternalPath = false;
	};

	struct CConfigFile
	{
		CRepositoryConfig const *f_GetConfig(CRepository const &_Repo, CStr const &_BasePath);

		TCMap<CStr, CRepositoryConfig> m_Configs;
		CStr m_LineEndings = "\n";
		bool m_bIsStateFile = false;
	};

	struct CMainRepoInfo
	{
		CStr m_Location;
		CStr m_DataDir;
		CStr m_CommonDir;
		CStr m_Branch;
		CStr m_DefaultBranch;
		bool m_bIsWorktree = false;
		bool m_bIsValid = false;
	};

	struct CMainRepoBranchInfo
	{
		CStr m_Branch;
		CStr m_DefaultBranch;
	};

	struct CStateHandler
	{
		CStateHandler
			(
				CStr const &_BasePath
				, CStr const &_OutputDir
				, EAnsiEncodingFlag _AnsiFlags
				, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			)
		;

		void f_SetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Hash, CStr const &_Identifier, bool _bIsStateFile);
		CStr f_GetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Identifier, bool _bIsStateFile);
		TCMap<CStr, CConfigFile> const &f_GetNewFiles();
		TCMap<CStr, CConfigFile> f_GetMergedFiles();
		bool f_AddGitIgnore(CStr const &_FileName, CBuildSystem const &_BuildSystem, EGitIgnoreType _GitIgnoreType);
		static CConfigFile fs_ParseConfigFile(CStr const &_Contents, CStr const &_FileName);
		CLowLevelRecursiveLock &f_ConsoleOutputLock();
		void f_ConsoleOutput(CStr const &_Output, bool _bError = false);
		TCSet<CStr> f_GetLastSeenRepositories();
		EAnsiEncodingFlag f_AnsiFlags() const;
		TCFuture<CActorSubscription> f_SequenceConfigChanges(CStr const &_Path);
		bool f_UpdateCoreExcludesFileLocation(CStr const &_Path);
		void f_IncrementBranchCreated(CStr const &_FromBranch, CStr const &_ToBranch, CStr const &_Repository);
		void f_IncrementBranchSwitched(CStr const &_FromBranch, CStr const &_ToBranch, CStr const &_Repository);
		void f_OutputBranchSwitchSummary(umint _MaxRepoWidth);

	private:
		struct CBranchTransition
		{
			CStr m_FromBranch;
			CStr m_ToBranch;
			bool m_bCreated = false;

			auto operator<=>(CBranchTransition const &_Other) const = default;
		};

		CConfigFile const &fp_GetConfigFile(CStr const &_FileName, bool _bIsStateFile);

		CStr const mp_BasePath;
		CStr const mp_OutputDir;
		CLowLevelRecursiveLock mp_Lock;
		TCMap<CStr, CConfigFile> mp_ConfigFiles;
		TCMap<CStr, CConfigFile> mp_NewConfigFiles;
		TCSet<CStr> mp_GitIgnores;
		TCMap<CStr, CStr> mp_LastSeenRepositories;
		CLowLevelRecursiveLock mp_ConsoleOutputLock;
		EAnsiEncodingFlag mp_AnsiFlags;
		NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> mp_fOutputConsole;

		CLowLevelRecursiveLock mp_GitConfigSequencersLock;
		TCMap<CStr, CSequencer> mp_GitConfigSequencers;

		CLowLevelRecursiveLock mp_CoreExcludesFileLocationLock;
		TCSet<CStr> mp_CoreExcludesFileLocationUpdated;

		CLowLevelRecursiveLock mp_BranchTransitionsLock;
		TCMap<CBranchTransition, TCVector<CStr>> mp_BranchTransitions;
	};

	struct CGitLaunches
	{
		CGitLaunches(CBuildSystem::CGitLaunchOptions const &_Options, CStr const &_ProgressDescription);
		CGitLaunches(CGitLaunches const &_Other);
		CGitLaunches(CGitLaunches &&_Other) = default;

		TCUnsafeFuture<CAsyncDestroyAwaiter> f_Init();

		void f_CheckInit() const;
		void f_SetNumRepos(umint _nRepos, bool _bReport = true);
		void f_SetProgressDelay(fp64 _Delay);
		void f_MeasureRepos(TCVector<TCVector<CRepository *>> const &_FilteredRepositories, bool _bReport = true);
		void f_MeasureRepos(TCVector<TCMap<CStr, CReposLocation>> const &_Repositories, bool _bReport = true);

		TCUnsafeFuture<CProcessLaunchActor::CSimpleLaunchResult> f_Launch
			(
				CRepository const &_Repo
				, TCVector<CStr> const &_Params
				, TCMap<CStr, CStr> const &_Environment
				, CProcessLaunchActor::ESimpleLaunchFlag _Flags
				, CStr const &_Application = "git"
			) const
		;
		TCUnsafeFuture<CProcessLaunchActor::CSimpleLaunchResult> f_Launch
			(
				CStr const &_Directory
				, TCVector<CStr> const &_Params
				, TCMap<CStr, CStr> const &_Environment
				, CProcessLaunchActor::ESimpleLaunchFlag _Flags
				, CStr const &_Application = "git"
			) const
		;
		TCUnsafeFuture<void> f_Launch
			(
				CRepository _Repo
				, TCVector<CStr> _Params
				, TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> _fHandleResult
				, CStr _Prefix = {}
				, TCMap<CStr, CStr> _Environment = {}
				, CStr _Application = "git"
			) const
		;
		TCUnsafeFuture<CProcessLaunchActor::CSimpleLaunchResult> f_OpenRepoEditor(CRepoEditor _Editor, CStr _Repo) const;

		struct CDeferredOutput
		{
			TCVector<CStr> m_Lines;
			EOutputType m_OutputType = EOutputType_Normal;
		};

		static uint32 fs_MaxProcesses();

		using CLaunchSequencer = TCSequencer<CProcessLaunchActor::CSimpleLaunchResult>;

		struct COwner;
		struct CState
		{
			CState(CBuildSystem::CGitLaunchOptions const &_Options, CStr const &_ProgressDescription);
			~CState();

			void f_OutputState() const;
			void f_ConsoleOutput(CStr const &_Output, bool _bError = false) const;


			CIntrusiveRefCount m_RefCount;

			CMutual m_Lock;
			CStr m_BaseDir;
			CStr m_InvocationCommand;
			TCActor<> m_OutputActor{fg_Construct()};
			TCMap<umint, TCActor<CProcessLaunchActor>> m_Launches;
			TCMap<umint, zbool> m_LaunchesAborted;
			TCMap<CStr, CStr> m_RepoNames;
			TCAtomic<umint> m_nDoneRepos = 0;
			CStr m_ProgressDescription;
			CStopwatch m_Stopwatch{true};
			fp64 m_ProgressDelay = 0.0;

			CMutual m_DeferredOutputLock;
			TCMap<CStr, TCVector<CDeferredOutput>> m_DeferredOutput;
			TCVector<TCSet<CStr>> m_OutputOrder;

			CLaunchSequencer m_LaunchSequencer{"GitLaunches State LaunchSequencer", fg_Clamp(NSys::fg_Thread_GetVirtualCores()*2u, 32u, fs_MaxProcesses())};

			mutable CMutual m_ConsoleOutputLock;
			mutable fp64 m_LastProgressOutputTime = 0.0;
			mutable bool m_bOutputStatusDone = false;

			umint m_LaunchID = 0;
			umint m_LongestRepo = 0;
			TCAtomic<umint> m_nRepos = 0;
			EAnsiEncodingFlag m_AnsiFlags;
			uint32 m_TerminalWidth = 0;
			bool m_bShowProgress = true;
			NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> m_fOutputConsole;

			NStorage::TCSharedPointer<TCAtomic<bool>> m_pCancelled;

			CActorSubscription m_CheckAbortTimer;

			bool m_bInited = false;
		};

		struct COwner
		{
			COwner(TCSharedPointer<CState> const&_pState);
			~COwner();

			TCSharedPointer<CState> m_pState;
		};

		CStr f_GetRepoName(CRepository const &_Repo) const;
		void f_Output(EOutputType _OutputType, CRepository const &_Repo, CStr const &_Output, CStr const &_Prefix = {}) const;
		void f_Output(EOutputType _OutputType, CStr const &_Section, CStr const &_Output) const;
		void f_RepoDone(umint _nDone = 1) const;
		COnScopeExitShared f_RepoDoneScope() const;
		void f_SetOutputOrder(TCVector<TCSet<CStr>> const &_OutputOrder) const;

		TCSharedPointer<CState> m_pState;
		TCSharedPointer<COwner> m_pOwner;

	private:
		TCUnsafeFuture<CProcessLaunchActor::CSimpleLaunchResult> fp_Launch(CProcessLaunchActor::CSimpleLaunch _Launch) const;
	};

	struct CFilteredRepos
	{
		TCVector<TCTuple<CRepository, umint>> f_GetAllRepos() const;

		TCVector<TCMap<CStr, CReposLocation>> m_ReposOrdered;
		TCVector<TCVector<CRepository *>> m_FilteredRepositories;
	};

	struct CBranchSettings
	{
		struct CBranch
		{
			CStr const &f_GetType() const
			{
				return TCMap<CStr, CBranch>::fs_GetKey(*this);
			}

			CStr m_Name;
			bool m_bOnlyChanged = true;
		};

		CBranchSettings(CStr const &_OutputDir);

		void f_WriteSettings();
		void f_ReadSettings();
		void f_RemoveBranch(CStr const &_Type);
		void f_SetBranch(CStr const &_Type, CStr const &_Branch, bool _bOnlyChanged);

		TCMap<CStr, CBranch> m_Branches;

		CStr m_OutputDir;
		bool m_bDirty = false;
	};

	struct CLocalFileChange
	{
		CStr m_ChangeType;
		CStr m_File;
	};

	struct CGitBranches
	{
	public:
		TCSet<CStr> m_Branches;
		CStr m_Current;
	};

	struct CLogEntry
	{
		CStr m_Hash;
		CStr m_Description;
		bool m_bUnresolved = false;
	};

	struct CLogEntryFull
	{
		CStr m_Commit;
		CStr m_Tree;
		CStr m_Parent;
		CStr m_Author;
		CTime m_AuthorDate;
		CStr m_Committer;
		CTime m_CommitterDate;
		CStr m_FirstLine;
		CStr m_Message;
	};

	struct CGitConfig
	{
		struct CRemote
		{
			CStr m_Url;
			CStr m_TagOptions;
			TCVector<CStr> m_Fetch;
			bool m_bMalterlibLfsSetup = false;
		};

		struct CLfsOptions
		{
			CStr m_Url;
			CStr m_TagOptions;
			TCVector<CStr> m_Fetch;
		};

		struct CLfsCustomTransfer
		{
			CStr m_Arguments;
			CStr m_Path;
			bool m_bConcurrent = false;
		};

		CLfsCustomTransfer m_MalterlibCustomTransfer;
		TCMap<CStr, CRemote> m_Remotes;
		TCMap<CStr, CStr> m_CustomLfsTransferAgents;
		CStr m_UserName;
		CStr m_UserEmail;
		bool m_bWorktreeConfig = false;
	};

	enum EFilterRepoFlag
	{
		EFilterRepoFlag_None = 0
		, EFilterRepoFlag_OnlyTracked = DBit(0)
	};

	struct CGitVersion
	{
		auto operator <=> (CGitVersion const &_Right) const noexcept = default;

		uint32 m_Major = 0;
		uint32 m_Minor = 0;
		uint32 m_Patch = 0;
	};

	enum class EApplyPolicyFlag
	{
		mc_None = 0
		, mc_Pretend = DMibBit(0)
		, mc_CreateMissing = DMibBit(1)
	};

	enum class EGetRepoFlag
	{
		mc_None = 0
		, mc_IncludePolicy = DMibBit(0)
		, mc_IncludeReleasePackage = DMibBit(1)
		, mc_IncludeLicense = DMibBit(2)
		, mc_IncludeRepoCommit = DMibBit(3)
	};

	TCUnsafeFuture<CGitVersion> fg_GetGitVersion(CGitLaunches &_Launches);
	CStr fg_GetGitRoot(CStr const &_Directory);
	CStr fg_GetGitDataDir(CStr const &_GitRoot, CFilePosition const &_Position);
	CStr fg_GetGitCommonDir(CStr const &_DataDir, CFilePosition const &_Position);
	CStr fg_GetGitHeadHash(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo);
	CRepositoryDynamicInfo fg_GetRepositoryDynamicInfo(CRepository const &_Repo);

	CGitConfig fg_GetGitConfig(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, bool _bIsWorktree);
	bool fg_IsSubmodule(CStr const &_DataDir);
	bool fg_IsWorktree(CStr const &_DataDir);
	bool fg_AreGitPathsSame(CStr const &_PathA, CStr const &_PathB);
	TCUnsafeFuture<TCVector<CStr>> fg_ListWorktreePaths(CGitLaunches &_Launches, CStr const &_RepoDir);
	TCUnsafeFuture<CStr> fg_FindSubRepoInWorktrees(CGitLaunches &_Launches, CStr const &_BaseDir, CStr const &_RelativeRepoPath);
	TCUnsafeFuture<void> fg_TransferGitDirMainToWorktree(CGitLaunches &_Launches, CStr const &_MainSubRepoDir, CStr const &_TargetWorktreeSubRepoDir);
	TCUnsafeFuture<void> fg_TransferGitDirWorktreeToMain(CGitLaunches &_Launches, CStr const &_WorktreeSubRepoDir, CStr const &_MainSubRepoDir, CStr const &_WorktreeName);
	TCVector<TCMap<CStr, CReposLocation>> fg_GetRepos(CBuildSystem &_BuildSystem, CBuildSystemData &_Data, EGetRepoFlag _Flags);
	CRepoEditor fg_GetRepoEditor(CBuildSystem &_BuildSystem, CBuildSystemData &_Data);
	NStorage::TCOptional<CRepoCommitOptions> fg_GetPerforceRootRepoCommitOptions(CBuildSystem &_BuildSystem, CBuildSystemData &_Data);

	TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> fg_LogAllFunctor();

	TCFuture<bool> fg_RepoIsChanged(CGitLaunches _GitLaunches, CRepository _Repo, CRepositoryDynamicInfo _DynamicInfo, EFilterRepoFlag _Flags, bool _bIncludePull, CStr _MainRepoBranch, CStr _MainRepoDefaultBranch);
	TCFuture<TCVector<CLocalFileChange>> fg_GetLocalFileChanges(CGitLaunches _GitLaunches, CRepository _Repo, bool _bIncludeUntracked);
	TCFuture<CGitBranches> fg_GetBranches(CGitLaunches _GitLaunches, CRepository _Repo, bool _bRemote);
	TCFuture<TCVector<CStr>> fg_GetRemotes(CGitLaunches _GitLaunches, CRepository _Repo);
	TCFuture<TCMap<CStr, CRemote>> fg_GetPushRemotes(CGitLaunches _GitLaunches, CRepository _Repo, TCVector<CStr> _Remotes);
	TCFuture<TCVector<CLogEntry>> fg_GetLogEntries(CGitLaunches _GitLaunches, CRepository _Repo, CStr _From, CStr _To, bool _bReportBadRevision = true);
	TCFuture<TCVector<CLogEntryFull>> fg_GetLogEntriesFull(CGitLaunches _GitLaunches, CRepository _Repo, CStr _From, CStr _To);

	bool fg_BranchExists(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Branch);
	bool fg_RemoteBranchExists(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Branch);
	CStr fg_GetBranchHash(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Branch);
	CStr fg_GetRemoteBranchHash(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Remote, CStr const &_Branch);
	CStr fg_GetBranch(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo);
	void fg_DetectGitBranchInfo(CStr const &_DataDir, CStr const &_CommonDir, CStr &o_Branch, CStr &o_DefaultBranch);
	CStr fg_GetExpectedBranch(CRepository const &_Repo, CStr const &_MainRepoBranch, CStr const &_MainRepoDefaultBranch);
	CMainRepoBranchInfo fg_GetMainRepoBranchInfo(CStr const &_BaseDir, TCVector<TCMap<CStr, CReposLocation>> const &_ReposOrdered);
	CStr fg_GetRemoteHead(CRepository const &_Repo, CRepositoryDynamicInfo const &_DynamicInfo, CStr const &_Remote);
	CStr fg_HandleRepositoryActionToString(EHandleRepositoryAction _Action, EOutputType &o_OutputType);

	TCUnsafeFuture<void> fg_UpdateRemotes(CBuildSystem &_BuildSystem, CFilteredRepos const &_FilteredRepositories, CStr const &_ExtraMessage = {});
	TCMap<CStr, CStr> fg_FetchEnvironment(CBuildSystem const &_BuildSystem);

	TCUnsafeFuture<CFilteredRepos> fg_GetFilteredRepos
		(
			CBuildSystem::CRepoFilter const &_Filter
			, CBuildSystem &_BuildSystem
			, CBuildSystemData &_Data
			, EGetRepoFlag _GetRepoFlags
			, EFilterRepoFlag _Flags = EFilterRepoFlag_None
		)
	;

	TCFuture<void> fg_ApplyPolicies(CStr _Url, CStr _RepoDir, CEJsonSorted _Policy, EApplyPolicyFlag _Flags, TCFunction<void (EOutputType _OutputType, CStr const &_String)> _fOutputInfo);

	template <typename tf_FOutput>
	void fg_OutputRepositoryInfo
		(
			EOutputType _OutputType
			, CStr const &_Info
			, EAnsiEncodingFlag _AnsiFlags
			, CStr const &_RepoName
			, umint _MaxRepoWidth
			, tf_FOutput const &_fOutput
		)
	;
}

#include "Malterlib_BuildSystem_Repository.hpp"
