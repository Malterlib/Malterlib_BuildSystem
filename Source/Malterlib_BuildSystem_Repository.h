// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem.h"
#include <Mib/Process/ProcessLaunch>
#include <Mib/Process/ProcessLaunchActor>
#include <Mib/Encoding/EJSON>

namespace NMib::NBuildSystem::NRepository
{
	#define DColor_Reset "\x1B[0m"
	#define DColor_Bold "\x1B[1m"
	#define DColor_Reverse "\x1B[7m"

	#define DColor_256(d_Color) "\x1B[38;5;" #d_Color "m"

	struct CColors
	{
		static constexpr ch8 const mc_Default[] = DColor_Reset;

		static constexpr ch8 const mc_StatusNormal[] = DColor_Reset DColor_256(118);
		static constexpr ch8 const mc_StatusWarning[] = DColor_Reset DColor_256(207);
		static constexpr ch8 const mc_StatusError[] = DColor_Reset DColor_Bold DColor_256(198);

		static constexpr ch8 const mc_RepositoryName[] = DColor_Reset DColor_256(221);
		static constexpr ch8 const mc_BranchName[] = DColor_Reset;

		static constexpr ch8 const mc_ToCommit[] = DColor_Reset DColor_256(46);
		static constexpr ch8 const mc_ToPush[] = DColor_Reset DColor_256(32);
		static constexpr ch8 const mc_ToPull[] = DColor_Reset DColor_256(9);
	};

	enum EOutputType
	{
		EOutputType_Normal
		, EOutputType_Warning
		, EOutputType_Error
	};

	struct CRepository
	{
		CStr const &f_GetName() const
		{
			return TCMap<CStr, CRepository>::fs_GetKey(*this);
		}

		CStr m_Location;
		CStr m_ConfigFile;
		CStr m_StateFile;
		CStr m_URL;
		CStr m_DefaultBranch;
		CStr m_DefaultUpstreamBranch;
		CStr m_Submodule;
		CStr m_SubmoduleName;
		CStr m_Type;
		TCSet<CStr> m_Tags;
		TCMap<CStr, CStr> m_Remotes;
		CFilePosition m_Position;
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
	};

	struct CConfigFile
	{
		TCMap<CStr, CRepositoryConfig> m_Configs;
		CStr m_LineEndings = "\n";
	};

	struct CStateHandler
	{
		void f_SetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Hash);
		CStr f_GetHash(CStr const &_FileName, CStr const &_RepoPath);
		TCMap<CStr, CConfigFile> const &f_GetNewFiles();
		TCMap<CStr, CConfigFile> f_GetMergedFiles();
		void f_AddGitIgnore(CStr const &_FileName, CBuildSystem const &_BuildSystem);

	private:
		CConfigFile const &fp_GetConfigFile(CStr const &_FileName);

		CMutual mp_Lock;
		TCMap<CStr, CConfigFile> mp_ConfigFiles;
		TCMap<CStr, CConfigFile> mp_NewConfigFiles;
		TCSet<CStr> mp_GitIgnores;
	};

	enum EGitLaunchesOutputFlag
	{
		EGitLaunchesOutputFlag_None = 0
		, EGitLaunchesOutputFlag_DeferOutput = DBit(0)
	};
	struct CGitLaunches
	{
		CGitLaunches(CStr const &_BaseDir, EGitLaunchesOutputFlag _Flags = EGitLaunchesOutputFlag_DeferOutput);

		void f_MeasureRepos(TCVector<TCVector<CRepository *>> const &_FilteredRepositories);

		TCContinuation<CProcessLaunchActor::CSimpleLaunchResult> f_Launch(CRepository const &_Repo, TCVector<CStr> const &_Params) const;
		TCContinuation<void> f_Launch
			(
			 	CRepository const &_Repo
			 	, TCVector<CStr> const &_Params
			 	, TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> &&_fHandleResult
			 	, CStr const &_Prefix = {}
			) const
		;
		TCContinuation<CProcessLaunchActor::CSimpleLaunchResult> f_OpenDocument(CStr const &_Application, CStr const &_Document) const;

		struct CDeferredOutput
		{
			TCVector<CStr> m_Lines;
			EOutputType m_OutputType = EOutputType_Normal;
		};

		struct CState
		{
			CState(CStr const &_BaseDir, EGitLaunchesOutputFlag _Flags);
			~CState();

			CMutual m_Lock;
			CStr m_BaseDir;
			TCActor<> m_OutputActor = fg_ConcurrentActor();
			TCVector<TCActor<CProcessLaunchActor>> m_Launches;
			TCMap<CStr, CStr> m_RepoNames;
			mint m_LongestRepo = 0;
			mint m_nRepos = 0;
			TCAtomic<mint> m_nDoneRepos = 0;
			EGitLaunchesOutputFlag m_OutputFlags;

			CMutual m_DeferredOutputLock;
			TCMap<CStr, TCVector<CDeferredOutput>> m_DeferredOutput;
		};

		void f_Output(EOutputType _OutputType, CRepository const &_Repo, CStr const &_Output, CStr const &_Prefix = {}) const;
		void f_Output(EOutputType _OutputType, CStr const &_Section, CStr const &_Output) const;
		void f_RepoDone(mint _nDone = 1) const;

		TCSharedPointer<CState> m_pState;
	};

	struct CFilteredRepos
	{
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

	CStr fg_GetGitRoot(CStr const &_Directory);
	CStr fg_GetGitDataDir(CStr const &_GitRoot, CFilePosition const &_Position);
	CStr fg_GetGitHeadHash(CStr const &_GitRoot, CFilePosition const &_Position);
	TCMap<CStr, CStr> fg_GetGitRemotes(CStr const &_GitRoot, CFilePosition const &_Position);
	bool fg_IsSubmodule(CStr const &_GitRoot);
	bool fg_HandleRepository(CStr const &_ReposDirectory, CRepository const &_Repo, CStateHandler &o_StateHandler, CBuildSystem const &_BuildSystem);
	TCVector<TCMap<CStr, CReposLocation>> fg_GetRepos(CBuildSystem &_BuildSystem, CBuildSystemData &_Data);

	TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> fg_LogAllFunctor();

	TCContinuation<bool> fg_RepoIsChanged(CGitLaunches const &_GitLaunches, CRepository const &_Repo);
	TCContinuation<TCVector<CLocalFileChange>> fg_GetLocalFileChanges(CGitLaunches const &_GitLaunches, CRepository const &_Repo, bool _bIncludeUntracked);
	TCContinuation<CGitBranches> fg_GetBranches(CGitLaunches const &_GitLaunches, CRepository const &_Repo, bool _bRemote);
	TCContinuation<TCVector<CStr>> fg_GetRemotes(CGitLaunches const &_GitLaunches, CRepository const &_Repo);
	TCContinuation<TCVector<CLogEntry>> fg_GetLogEntries(CGitLaunches const &_GitLaunches, CRepository const &_Repo, CStr const &_From, CStr const &_To);

	bool fg_BranchExists(CRepository const &_Repo, CStr const &_Branch);
	NStr::CStr fg_GetBranch(CRepository const &_Repo);
	CFilteredRepos fg_GetFilteredRepos(CBuildSystem::CRepoFilter const &_Filter, CBuildSystem &_BuildSystem, CBuildSystemData &_Data);
}
