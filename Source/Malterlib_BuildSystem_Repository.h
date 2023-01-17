// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem.h"
#include <Mib/Process/ProcessLaunch>
#include <Mib/Process/ProcessLaunchActor>
#include <Mib/Encoding/EJSON>
#include <Mib/Concurrency/ActorSequencer>
#include <Mib/CommandLine/AnsiEncoding>

namespace NMib::NBuildSystem::NRepository
{
	struct CColors : public CAnsiEncoding
	{
		CColors(EAnsiEncodingFlag _AnsiFlags);

		NStr::CStr f_RepositoryName() const;
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

	struct CRemote
	{
		CStr const &f_Name() const
		{
			return TCMap<CStr, CRemote>::fs_GetKey(*this);
		}

		CStr m_URL;
		CStr m_DefaultBranch;
		bool m_bCanPush = true;
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
		CStr m_URL;
		CStr m_DefaultBranch;
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
		bool m_bSubmodule = false;
		bool m_bExcludeFromSeen = false;
		bool m_bUpdateSubmodules = false;
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
		TCMap<CStr, CRepositoryConfig> m_Configs;
		CStr m_LineEndings = "\n";

		CRepositoryConfig const *f_GetConfig(CRepository const &_Repo, CStr const &_BasePath);
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

		void f_SetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Hash, CStr const &_Identifier);
		CStr f_GetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Identifier);
		TCMap<CStr, CConfigFile> const &f_GetNewFiles();
		TCMap<CStr, CConfigFile> f_GetMergedFiles();
		void f_AddGitIgnore(CStr const &_FileName, CBuildSystem const &_BuildSystem);
		static CConfigFile fs_ParseConfigFile(CStr const &_Contents, CStr const &_FileName);
		CMutual &f_ConsoleOutputLock();
		void f_ConsoleOutput(CStr const &_Output, bool _bError = false);
		TCSet<CStr> f_GetLastSeenRepositories();
		EAnsiEncodingFlag f_AnsiFlags() const;

	private:
		CConfigFile const &fp_GetConfigFile(CStr const &_FileName);

		CStr const mp_BasePath;
		CStr const mp_OutputDir;
		CMutual mp_Lock;
		TCMap<CStr, CConfigFile> mp_ConfigFiles;
		TCMap<CStr, CConfigFile> mp_NewConfigFiles;
		TCSet<CStr> mp_GitIgnores;
		TCMap<CStr, CStr> mp_LastSeenRepositories;
		CMutual mp_ConsoleOutputLock;
		EAnsiEncodingFlag mp_AnsiFlags;
		NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> mp_fOutputConsole;
	};

	struct CGitLaunches
	{
		CGitLaunches
			(
				CStr const &_BaseDir
				, CStr const &_ProgressDescription
				, EAnsiEncodingFlag _AnsiFlags
				, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			)
		;

		void f_SetNumRepos(mint _nRepos, bool _bReport = true);
		void f_MeasureRepos(TCVector<TCVector<CRepository *>> const &_FilteredRepositories, bool _bReport = true);

		TCFuture<CProcessLaunchActor::CSimpleLaunchResult> f_Launch(CRepository const &_Repo, TCVector<CStr> const &_Params, TCMap<CStr, CStr> const &_Environment = {}) const;
		TCFuture<void> f_Launch
			(
			 	CRepository const &_Repo
			 	, TCVector<CStr> const &_Params
			 	, TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> &&_fHandleResult
			 	, CStr const &_Prefix = {}
			 	, TCMap<CStr, CStr> const &_Environment = {}
			) const
		;
		TCFuture<CProcessLaunchActor::CSimpleLaunchResult> f_OpenRepoEditor(CRepoEditor const &_Editor, CStr const &_Repo) const;

		struct CDeferredOutput
		{
			TCVector<CStr> m_Lines;
			EOutputType m_OutputType = EOutputType_Normal;
		};

		static uint32 fs_MaxProcesses();

		struct CState
		{
			CState
				(
					CStr const &_BaseDir
					, CStr const &_ProgressDescription
					, EAnsiEncodingFlag _AnsiFlags
					, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
				)
			;
			~CState();

			void f_OutputState() const;
			void f_ConsoleOutput(CStr const &_Output, bool _bError = false) const;

			CMutual m_Lock;
			CStr m_BaseDir;
			TCActor<> m_OutputActor = fg_ConcurrentActor();
			TCMap<mint, TCActor<CProcessLaunchActor>> m_Launches;
			TCMap<CStr, CStr> m_RepoNames;
			TCAtomic<mint> m_nDoneRepos = 0;
			CStr m_ProgressDescription;

			CMutual m_DeferredOutputLock;
			TCMap<CStr, TCVector<CDeferredOutput>> m_DeferredOutput;
			TCVector<TCSet<CStr>> m_OutputOrder;

			TCActorSequencer<CProcessLaunchActor::CSimpleLaunchResult> m_LaunchSequencer{fg_Clamp(NSys::fg_Thread_GetVirtualCores()*2u, 32u, fs_MaxProcesses())};

			mint m_LaunchID = 0;
			mint m_LongestRepo = 0;
			TCAtomic<mint> m_nRepos = 0;
			EAnsiEncodingFlag m_AnsiFlags;
			NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> m_fOutputConsole;
		};

		CStr f_GetRepoName(CRepository const &_Repo) const;
		void f_Output(EOutputType _OutputType, CRepository const &_Repo, CStr const &_Output, CStr const &_Prefix = {}) const;
		void f_Output(EOutputType _OutputType, CStr const &_Section, CStr const &_Output) const;
		void f_RepoDone(mint _nDone = 1) const;
		COnScopeExitShared f_RepoDoneScope() const;
		void f_SetOutputOrder(TCVector<TCSet<CStr>> const &_OutputOrder) const;

		TCSharedPointer<CState> m_pState;

	private:
		TCFuture<CProcessLaunchActor::CSimpleLaunchResult> fp_Launch(CProcessLaunchActor::CSimpleLaunch &&_Launch) const;
	};

	struct CFilteredRepos
	{
		TCVector<TCTuple<CRepository, mint>> f_GetAllRepos() const;

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
		TCMap<CStr, CStr> m_Remotes;
		CStr m_UserName;
		CStr m_UserEmail;
	};

	enum EFilterRepoFlag
	{
		EFilterRepoFlag_None = 0
		, EFilterRepoFlag_OnlyTracked = DBit(0)
		, EFilterRepoFlag_IncludePull = DBit(1)
	};

	struct CGitVersion
	{
		auto operator <=> (CGitVersion const &_Right) const = default;

		uint32 m_Major = 0;
		uint32 m_Minor = 0;
		uint32 m_Patch = 0;
	};

	CGitVersion fg_GetGitVersion();
	CStr fg_GetGitRoot(CStr const &_Directory);
	CStr fg_GetGitDataDir(CStr const &_GitRoot, CFilePosition const &_Position);
	CStr fg_GetGitHeadHash(CStr const &_GitRoot, CFilePosition const &_Position);
	CGitConfig fg_GetGitConfig(CStr const &_GitRoot, CFilePosition const &_Position);
	bool fg_IsSubmodule(CStr const &_GitRoot);
	bool fg_HandleRepository
		(
		 	CStr const &_ReposDirectory
		 	, CRepository const &_Repo
		 	, CStateHandler &o_StateHandler
		 	, CBuildSystem const &_BuildSystem
		)
	;
	TCVector<TCMap<CStr, CReposLocation>> fg_GetRepos(CBuildSystem &_BuildSystem, CBuildSystemData &_Data);
	CRepoEditor fg_GetRepoEditor(CBuildSystem &_BuildSystem, CBuildSystemData &_Data);

	TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> fg_LogAllFunctor();

	TCFuture<bool> fg_RepoIsChanged(CGitLaunches const &_GitLaunches, CRepository const &_Repo, EFilterRepoFlag _Flags);
	TCFuture<TCVector<CLocalFileChange>> fg_GetLocalFileChanges(CGitLaunches const &_GitLaunches, CRepository const &_Repo, bool _bIncludeUntracked);
	TCFuture<CGitBranches> fg_GetBranches(CGitLaunches const &_GitLaunches, CRepository const &_Repo, bool _bRemote);
	TCFuture<TCVector<CStr>> fg_GetRemotes(CGitLaunches const &_GitLaunches, CRepository const &_Repo);
	TCFuture<TCMap<CStr, CRemote>> fg_GetPushRemotes(CGitLaunches const &_GitLaunches, CRepository const &_Repo, TCVector<CStr> const &_Remotes);
	TCFuture<TCVector<CLogEntry>> fg_GetLogEntries(CGitLaunches const &_GitLaunches, CRepository const &_Repo, CStr const &_From, CStr const &_To, bool _bReportBadRevision = true);
	TCFuture<TCVector<CLogEntryFull>> fg_GetLogEntriesFull(CGitLaunches const &_GitLaunches, CRepository const &_Repo, CStr const &_From, CStr const &_To);

	bool fg_BranchExists(CRepository const &_Repo, CStr const &_Branch);
	CStr fg_GetBranch(CRepository const &_Repo);
	CStr fg_GetRemoteHead(CRepository const &_Repo, CStr const &_Remote);

	void fg_UpdateRemotes(CBuildSystem &_BuildSystem, CFilteredRepos const &_FilteredRepositories, CStr const &_ExtraMessage = {});
	TCFuture<void> fg_UpdateRemotesAsync(CBuildSystem &_BuildSystem, CFilteredRepos const &_FilteredRepositories, CStr const &_ExtraMessage = {});
	TCMap<CStr, CStr> fg_FetchEnvironment(CBuildSystem const &_BuildSystem);

	TCFuture<CFilteredRepos> fg_GetFilteredReposAsync(CBuildSystem::CRepoFilter const &_Filter, CBuildSystem &_BuildSystem, CBuildSystemData &_Data, EFilterRepoFlag _Flags = EFilterRepoFlag_None);
	CFilteredRepos fg_GetFilteredRepos(CBuildSystem::CRepoFilter const &_Filter, CBuildSystem &_BuildSystem, CBuildSystemData &_Data, EFilterRepoFlag _Flags = EFilterRepoFlag_None);
}
