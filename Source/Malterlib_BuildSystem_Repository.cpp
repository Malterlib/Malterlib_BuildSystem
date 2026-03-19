// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Encoding/EJson>
#include <Mib/Git/LfsReleaseStore>
#include <Mib/Git/Helpers/ConfigParser>
#include <Mib/Process/ProcessLaunch>
#include <Mib/Process/ProcessLaunchActor>

CStr fg_ReconcileHelp(EAnsiEncodingFlag _AnsiFlags)
{
	NMib::NBuildSystem::NRepository::CColors Colors(_AnsiFlags);

	return R"---(
Changes in sub-repositories needs to be reconciled.

Choose how you want to reconcile changes:

Accept recommended actions   : {0}./mib update_repos '--reconcile=*:auto'{1}
Rebase all                   : {0}./mib update_repos '--reconcile=*:rebase'{1}
Reset all                    : {0}./mib update_repos '--reconcile=*:reset'{1}
No action, leave as is       : {0}./mib update_repos '--reconcile=*:leave'{1}

To choose separate action for different repositories you can specify wildcards. The last matching wildcard wins:
{0}./mib update_repos '--reconcile=*:auto,External/*:reset'{1}

To force the action even for repositories that you have not yet seen
{0}./mib update_repos '--reconcile=*:auto --reconcile-force'{1}

To show current status without reconciling use:
{0}./mib status --skip-update{1}
)---"_f /**/
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

Leave removed repositories on disk    : {0}./mib update_repos '--reconcile-removed=*:leave'{1}
Delete removed repositories from disk : {0}./mib update_repos '--reconcile-removed=*:delete'{1}

{2}Warning:{1} Specifying {0}delete{1} for reconcile removed will delete the repository and any unpushed work permanently.

To choose separate action for different repositories you can specify wildcards. The last matching wildcard wins:
{0}./mib update_repos '--reconcile-removed=*:leave,External/*:delete'{1}

To force the action even for repositories that you have not yet seen the recommended action for
{0}./mib update_repos '--reconcile-remove=*:delete' --reconcile-force{1}

To show current status without reconciling use:
{0}./mib status --skip-update{1}
)---"_f /**/
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
			if (_Name == "origin" || _Name.f_StartsWith("origin/"))
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

		EAnsiEncodingFlag CStateHandler::f_AnsiFlags() const
		{
			return mp_AnsiFlags;
		}

		TCFuture<CActorSubscription> CStateHandler::f_SequenceConfigChanges(CStr const &_Path)
		{
			DLock(mp_GitConfigSequencersLock);
			return (*mp_GitConfigSequencers(_Path, CStr("Git Config for: "_f << _Path))).f_Sequence();
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
					if (CFile::fs_FileExists(GitDir, EFileAttrib_Directory))
					{
						GitRoot = CurrentPath;
						if (_GitIgnoreType == EGitIgnoreType::mc_GitInfoExclude)
						{
							CStr InfoDir = GitDir / "info";
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

				// Fall back to .gitignore if no .git directory found
				if (IgnoreFile.f_IsEmpty())
				{
					IgnoreFile = CFile::fs_GetPath(_FileName) / ".gitignore";
					GitRoot.f_Clear();
				}
			}
			else
				IgnoreFile = CFile::fs_GetPath(_FileName) / ".gitignore";

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

		void fg_OutputRepositoryInfo(EOutputType _OutputType, CStr const &_Info, CStateHandler &o_StateHandler, CStr const &_RepoName, umint _MaxRepoWidth)
		{
			CColors Colors(o_StateHandler.f_AnsiFlags());
			CStr RepoColor = Colors.f_StatusNormal();
			switch (_OutputType)
			{
			case EOutputType_Normal: RepoColor = Colors.f_StatusNormal(); break;
			case EOutputType_Warning: RepoColor = Colors.f_StatusWarning(); break;
			case EOutputType_Error: RepoColor = Colors.f_StatusError(); break;
			}

			CStr RepoName = "{sj*,a-}"_f << _RepoName << _MaxRepoWidth;

			CStr ReplacedRepo = RepoName.f_Replace("/", "{}{}/{}"_f << Colors.f_Default() << Colors.f_Foreground256(250) << RepoColor ^ 1);
			{
				DMibLock(o_StateHandler.f_ConsoleOutputLock());
				o_StateHandler.f_ConsoleOutput
					(
						"{}{}{}   {}\n"_f
						<< RepoColor
						<< ReplacedRepo
						<< Colors.f_Default()
						<< _Info
					)
				;
			}
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
			)
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			CColors Colors(o_StateHandler.f_AnsiFlags());

			CStr Location = _ReposDirectory + "/" + _Repo.f_GetName();
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

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params);

					CStr StdErr = Return.f_GetErrorOut().f_Trim();
					if (_bErrorOnStdErr && !StdErr.f_IsEmpty())
						DMibError("Failed to ask git question {vs}: {}"_f << _Params << Return.f_GetCombinedOut());

					co_return Return.m_ExitCode == 0;
				}
			;
			auto fLaunchGitNonEmpty = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir) -> TCUnsafeFuture<bool>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params);

					if (Return.m_ExitCode)
						co_return false;

					co_return !Return.f_GetStdOut().f_IsEmpty();
				}
			;
			auto fTryLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir) -> TCUnsafeFuture<CStr>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params);


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

			auto fOutputInfo = [&](EOutputType _OutputType, CStr const &_Info)
				{
					CStr RepoName;
					if (_Repo.m_Location.f_StartsWith(BaseDir))
						RepoName = CFile::fs_MakePathRelative(_Repo.m_Location, BaseDir);
					else
						RepoName = _Repo.m_Location;
					fg_OutputRepositoryInfo(_OutputType, _Info, o_StateHandler, RepoName, _MaxRepoWidth);
				}
			;

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
					DMibError("Aborting, root repository needs to exists");
				}
				else
					bCloneNew = true;
			}

			auto fGetCloneConfigParams = [&]() -> TCVector<CStr>
				{
					TCVector<CStr> Params;

					auto GlobalMibExecutable = CFile::fs_GetUserHomeDirectory() / (".Malterlib/bin/mib" + CFile::mc_ExecutableExtension);

					if (_Repo.m_OriginProperties.m_bLfsReleaseStore)
					{
						Params.f_Insert({"--config", "lfs.customtransfer.malterlib-release.path={}"_f << GlobalMibExecutable});
						Params.f_Insert({"--config", "lfs.customtransfer.malterlib-release.args=lfs-release-store"});
						Params.f_Insert({"--config", "lfs.customtransfer.malterlib-release.concurrent=true"});
						Params.f_Insert({"--config", "lfs.{}.standalonetransferagent=malterlib-release"_f << _Repo.m_OriginProperties.m_URL});
						Params.f_Insert({"--config", "remote.origin.fetch=^refs/heads/lfs"});
						Params.f_Insert({"--config", "remote.origin.fetch=^refs/tags/lfs/*"});
						Params.f_Insert({"--no-tags"});
						Params.f_Insert({"--config", "remote.origin.malterlib-lfs-setup=true"});
					}

					for (auto &FetchSpec : _Repo.m_OriginProperties.m_ExtraFetchSpecs)
						Params.f_Insert({"--config", "remote.origin.fetch={}"_f << FetchSpec});

					if (_Repo.m_UserName)
						Params.f_Insert({"--config", "user.name={}"_f << _Repo.m_UserName});

					if (_Repo.m_UserEmail)
						Params.f_Insert({"--config", "user.email={}"_f << _Repo.m_UserEmail});

					return Params;
				}
			;

			if (bCloneNew)
			{
				if (ConfigHash.f_IsEmpty())
					fOutputInfo(EOutputType_Normal, "Adding external repository");
				else
					fOutputInfo(EOutputType_Normal, "Adding external repository at commit {}"_f << ConfigHash);

				CStr GitRoot = fg_GetGitRoot(Location);
				if (GitRoot.f_IsEmpty() || !_Repo.m_bSubmodule)
				{
					auto Result = co_await fg_CallSafe
						(
							[&]() -> TCUnsafeFuture<void>
							{
								co_await ECoroutineFlag_CaptureMalterlibExceptions;

								TCVector<CStr> CloneParams{"clone"};

								CloneParams.f_Insert(fGetCloneConfigParams());
								CloneParams.f_Insert({"-n", _Repo.m_OriginProperties.m_URL, Location});

								co_await fLaunchGit(CloneParams, "");

								if (_Repo.m_OriginProperties.m_bLfsReleaseStore)
								{
									auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

									co_await fLaunchGit({"update-ref", "-d", "refs/remotes/origin/lfs"}, Location);
									co_await fLaunchGit({"config", "--local", "--unset", "remote.origin.tagOpt"}, Location);
									co_await fLaunchGit({"fetch"}, Location);
								}

								TCVector<CStr> Params = {"checkout", "-B", _Repo.m_OriginProperties.m_DefaultBranch};

								if (!ConfigHash.f_IsEmpty())
									Params.f_Insert(ConfigHash);

								co_await fLaunchGit(Params, Location);

								bChanged = true;

								co_await fLaunchGit({"branch", "-u", "origin/{}"_f << _Repo.m_OriginProperties.m_DefaultBranch}, Location);

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
						CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to clone repository: {}", Result.f_GetExceptionStr()));
				}
				else
				{
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
						CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to add submodule repository: {}", Result.f_GetExceptionStr()));
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
							if (CFile::fs_FileExists(CurrentPath + "/.git", EFileAttrib_Directory))
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

							auto fNeedUpdate = [&]
								{
									CStr ConfigFile = ConfigRepoRoot / ".git/config";
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

								co_await fLaunchGit({"config", "core.excludesFile", ExpectedExcludeFile}, ConfigRepoRoot);
							}
						}
					}
				}
			}

			bool bForceReset = fg_GetSys()->f_GetEnvironmentVariable("MalterlibRepositoryHardReset", "") == gc_ConstString_true.m_String;

			auto GitConfig = fg_GetGitConfig(Location, _Repo.m_Position);
			auto &CurrentRemotes = GitConfig.m_Remotes;
			auto WantedRemotes = _Repo.m_Remotes;
			auto &OriginRemote = WantedRemotes["origin"];
			OriginRemote.m_Properties = _Repo.m_OriginProperties;

			if (_Repo.m_UserName && GitConfig.m_UserName != _Repo.m_UserName)
			{
				auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

				fOutputInfo(EOutputType_Normal, "Changing user name '{}' -> '{}'"_f << GitConfig.m_UserName << _Repo.m_UserName);
				co_await fLaunchGit({"config", "--local", "user.name", _Repo.m_UserName}, Location);
			}

			if (_Repo.m_UserEmail && GitConfig.m_UserEmail != _Repo.m_UserEmail)
			{
				auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

				fOutputInfo(EOutputType_Normal, "Changing user email '{}' -> '{}'"_f << GitConfig.m_UserEmail << _Repo.m_UserEmail);
				co_await fLaunchGit({"config", "--local", "user.email", _Repo.m_UserEmail}, Location);
			}

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

			if (!WantedRemotes.f_IsEmpty())
			{
				for (auto iRemote = WantedRemotes.f_GetIterator(); iRemote; ++iRemote)
				{
					auto &RemoteName = iRemote.f_GetKey();
					auto &Remote = *iRemote;
					auto pCurrentRemote = CurrentRemotes.f_FindEqual(RemoteName);
					if (pCurrentRemote)
					{
						if (pCurrentRemote->m_Url != Remote.m_Properties.m_URL)
						{
							fOutputInfo(EOutputType_Normal, "Changing remote URL '{}={}'"_f << RemoteName << Remote.m_Properties.m_URL);
							co_await fLaunchGit({"remote", "set-url", RemoteName, Remote.m_Properties.m_URL}, Location);
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
								co_await fLaunchGit({"config", "--local", "--add", "remote.{}.fetch"_f << RemoteName, "^refs/heads/lfs"}, Location);
								co_await fLaunchGit({"config", "--local", "--add", "remote.{}.fetch"_f << RemoteName, "^refs/tags/lfs/*"}, Location);
								co_await fLaunchGit({"config", "--local", "remote.{}.malterlib-lfs-setup"_f << RemoteName, "true"}, Location);
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

						WantedFetchSpecs.f_Insert("+refs/heads/*:refs/remotes/{}/*"_f << RemoteName);

						if (Remote.m_Properties.m_bLfsReleaseStore)
						{
							WantedFetchSpecs.f_Insert(gc_Str<"^refs/heads/lfs">);
							WantedFetchSpecs.f_Insert(gc_Str<"^refs/tags/lfs/*">);
						}

						for (auto &FetchSpec : Remote.m_Properties.m_ExtraFetchSpecs)
							WantedFetchSpecs.f_Insert(FetchSpec);

						if (pCurrentRemote->m_Fetch != WantedFetchSpecs)
						{
							fOutputInfo(EOutputType_Normal, "Updating fetch specs for remote '{}'"_f << RemoteName);

							CStr RemoteConfigName = "remote.{}.fetch"_f << RemoteName;

							auto Subscription = co_await o_StateHandler.f_SequenceConfigChanges(Location);

							co_await fLaunchGit({"config", "--local", "--unset-all", RemoteConfigName}, Location);

							for (auto &FetchSpec : WantedFetchSpecs)
								co_await fLaunchGit({"config", "--local", "--add", RemoteConfigName, FetchSpec}, Location);
						}

						continue;
					}
					fOutputInfo(EOutputType_Normal, "Adding remote '{}={}'"_f << RemoteName << Remote.m_Properties.m_URL);
					co_await fLaunchGit({"remote", "add", RemoteName, Remote.m_Properties.m_URL}, Location);

					if (Remote.m_Properties.m_bLfsReleaseStore)
					{
						co_await fSetupLfsReleaseStorage();

						co_await fLaunchGit({"config", "--local", "lfs.{}.standalonetransferagent"_f << Remote.m_Properties.m_URL, "malterlib-release"}, Location);
						co_await fLaunchGit({"config", "--local", "--add", "remote.{}.fetch"_f << RemoteName, "^refs/heads/lfs"}, Location);
						co_await fLaunchGit({"config", "--local", "--add", "remote.{}.fetch"_f << RemoteName, "^refs/tags/lfs/*"}, Location);
						co_await fLaunchGit({"config", "--local", "remote.{}.malterlib-lfs-setup"_f << RemoteName, "true"}, Location);
					}

					for (auto &FetchSpec : Remote.m_Properties.m_ExtraFetchSpecs)
						co_await fLaunchGit({"config", "--local", "--add", "remote.{}.fetch"_f << RemoteName, FetchSpec}, Location);

					if (_BuildSystem.f_GetGenerateOptions().m_bForceUpdateRemotes)
						co_await fLaunchGit({"fetch", "--tags", "--force", RemoteName}, Location);
					else
						co_await fLaunchGit({"fetch", "--tags", RemoteName}, Location);
				}

				if (_BuildSystem.f_ApplyRepoPolicy())
				{
					for (auto &Remote : WantedRemotes)
					{
						if (!Remote.m_Properties.m_bApplyPolicy)
							continue;

						EApplyPolicyFlag Flags = EApplyPolicyFlag::mc_None;

						if (_BuildSystem.f_ApplyRepoPolicyPretend() || Remote.m_Properties.m_bApplyPolicyPretend)
							Flags |= EApplyPolicyFlag::mc_Pretend;

						if (_BuildSystem.f_ApplyRepoPolicyCreateMissing())
							Flags |= EApplyPolicyFlag::mc_CreateMissing;

						co_await fg_ApplyPolicies(Remote.m_Properties.m_URL, Location, Remote.m_Properties.m_Policy, Flags, fOutputInfo);
					}
				}

				if (_BuildSystem.f_UpdateLfsReleaseIndexes())
				{
					for (auto iRemote = WantedRemotes.f_GetIterator(); iRemote; ++iRemote)
					{
						auto &RemoteName = iRemote.f_GetKey();
						auto &Remote = *iRemote;

						if (!Remote.m_Properties.m_bLfsReleaseStore)
							continue;

						{
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
									, RemoteName
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
					for (auto iRemote = CurrentRemotes.f_GetIterator(); iRemote; ++iRemote)
					{
						auto &RemoteName = iRemote.f_GetKey();
						if (WantedRemotes.f_FindEqual(RemoteName))
							continue;
						fOutputInfo(EOutputType_Normal, "Removing remote '{}'"_f << RemoteName);
						co_await fLaunchGit({"remote", "remove", RemoteName}, Location);
					}
				}
			}

			if (bIsRoot)
				co_return false;

			CStr CurrentHash = o_StateHandler.f_GetHash(_Repo.m_StateFile, Location, _Repo.m_Identity, true);
			CStr HeadHash = fg_GetGitHeadHash(Location, _Repo.m_Position);

			if
				(
					!ConfigHash.f_IsEmpty()
					&&
					(
						(HeadHash != ConfigHash && CurrentHash != ConfigHash)
						|| bForceReset
					)
					&& !fg_IsSubmodule(Location)
				)
			{
				bool bPassException = false;
				auto Result = co_await fg_CallSafe
					(
						[&]() -> TCUnsafeFuture<void>
						{
							co_await ECoroutineFlag_CaptureMalterlibExceptions;

							if (HeadHash != ConfigHash)
							{
								if (!(co_await fLaunchGitQuestion({"cat-file", "-e", "{}^{{commit}"_f << ConfigHash}, Location, false)))
								{
									TCVector<CStr> FetchParams = {"fetch", "--all", "--prune", "--tags"};
									if (_BuildSystem.f_GetGenerateOptions().m_bForceUpdateRemotes)
										FetchParams.f_Insert("--force");

									auto GitVersion = co_await fg_GetGitVersion(_Launches);
									if (bForceReset && GitVersion >= CGitVersion{2, 17})
										FetchParams.f_Insert("--prune-tags");

									auto Result = co_await fLaunchGit(FetchParams, Location, fg_FetchEnvironment(_BuildSystem)).f_Wrap();
									if (!Result)
									{
										if (!co_await fLaunchGitQuestion({"cat-file", "-e", "{}^{{commit}"_f << ConfigHash}, Location, false))
											co_return fg_Move(Result.f_GetException());

										fOutputInfo(EOutputType_Error, "Not all remotes were fetched: {}"_f << Result.f_GetExceptionStr());
									}
								}
							}

							if (bForceReset)
							{
								if (HeadHash != ConfigHash || co_await fLaunchGitNonEmpty({"status", "--porcelain"}, Location))
								{
									fOutputInfo(EOutputType_Warning, "Force Resetting to '{}'"_f << ConfigHash);
									co_await fLaunchGit({"checkout", "-f", "-B", _Repo.m_OriginProperties.m_DefaultBranch, ConfigHash}, Location);
									co_await fLaunchGit({"branch", "-u", "origin/{}"_f << _Repo.m_OriginProperties.m_DefaultBranch}, Location);
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
									if (co_await fLaunchGitQuestion({"merge-base", "--is-ancestor", HeadHash, ConfigHash}, Location, true))
									{
										if (!(co_await fLaunchGit({"status", "--porcelain"}, Location)).f_IsEmpty())
											RecommendedAction = EHandleRepositoryAction_Rebase;
										else
											RecommendedAction = EHandleRepositoryAction_Reset;
										break;
									}

									if (co_await fLaunchGitQuestion({"merge-base", "--is-ancestor", ConfigHash, HeadHash}, Location, true))
									{
										RecommendedAction = EHandleRepositoryAction_None;
										break;
									}

									if (!(co_await fLaunchGit({"status", "--porcelain"}, Location)).f_IsEmpty())
									{
										RecommendedAction = EHandleRepositoryAction_ManualResolve;
										break;
									}

									bool bIsOnRemote = false;

									if
										(
											co_await fLaunchGitNonEmpty({"branch", "-r", "--contains", HeadHash}, Location)
											|| co_await fLaunchGitNonEmpty({"tag", "--contains", HeadHash}, Location)
										)
									{
										bIsOnRemote = true;
									}

									if (bIsOnRemote)
										RecommendedAction = EHandleRepositoryAction_Reset;
									else
										RecommendedAction = EHandleRepositoryAction_ManualResolve;

									auto *pRepo = _AllRepositories.f_FindLargestLessThanEqual(_Repo.m_ConfigFile);
									if (!pRepo || !_Repo.m_ConfigFile.f_StartsWith((*pRepo)->m_Location))
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
												RecommendedAction = EHandleRepositoryAction_None;
												break;
											}
											bFoundConfig = true;
										}
										if (Commit == HeadHash)
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
									if ((co_await fLaunchGit({"rev-parse", "--abbrev-ref", "HEAD"}, Location)).f_Trim() == "HEAD")
									{
										fOutputInfo(EOutputType_Warning, "Resetting to {} and recovering from detached head"_f << ConfigHash);
										co_await fLaunchGit({"checkout", "-f", "-B", _Repo.m_OriginProperties.m_DefaultBranch, ConfigHash}, Location); // Recover from detached head
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
									fOutputInfo(EOutputType_Normal, "Rebasing on top of {}"_f << ConfigHash);

									TCSet<CStr> AllConfigFiles;
									for (auto &pRepository : _AllRepositories)
										AllConfigFiles[pRepository->m_ConfigFile];

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
								}
								else if (Action == EHandleRepositoryAction_ManualResolve)
								{
									fOutputInfo(EOutputType_Error, "Manual reconcile against {} needed"_f << ConfigHash);
									bPassException = true;
									DMibError("Manual reconcile needed");
								}
								else if (Action == EHandleRepositoryAction_Leave)
									;
								else if (_ReconcileAction != EHandleRepositoryAction_Auto)
								{
									CStr ActionStr;

									EOutputType OutputType = EOutputType_Warning;
									switch (RecommendedAction)
									{
									case EHandleRepositoryAction_None:
										ActionStr = "(Leave as is)";
										break;
									case EHandleRepositoryAction_ManualResolve:
										ActionStr = "(Resolve manually)";
										OutputType = EOutputType_Error;
										break;
									case EHandleRepositoryAction_Reset:
										ActionStr = "reset";
										break;
									case EHandleRepositoryAction_Rebase:
										ActionStr = "rebase";
										break;
									case EHandleRepositoryAction_Auto:
									default:
										ActionStr = "internal error";
										break;
									}

									fOutputInfo
										(
											OutputType
											, "{}{}{} recommended for {}{}{} -> {}{}{}"_f
											<< Colors.f_RepositoryName() << ActionStr << Colors.f_Default()
											<< Colors.f_ToPush() << HeadHash << Colors.f_Default()
											<< Colors.f_ToPush() << ConfigHash << Colors.f_Default()
										)
									;
									bPassException = true;
									DMibError(fg_ReconcileHelp(o_StateHandler.f_AnsiFlags()));
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
					CBuildSystem::fs_ThrowError(_Repo.m_Position, "Failed to reconcile hash '{}': {}"_f << ConfigHash << ErrorString);
				}
			}

			CStr GitHeadHash = fg_GetGitHeadHash(Location, _Repo.m_Position);
			o_StateHandler.f_SetHash(_Repo.m_StateFile, Location, GitHeadHash, _Repo.m_Identity, true);
			o_StateHandler.f_SetHash(_Repo.m_ConfigFile, Location, GitHeadHash, _Repo.m_Identity, false);
			o_StateHandler.f_AddGitIgnore(_Repo.m_StateFile, _BuildSystem, EGitIgnoreType::mc_GitIgnore);

			co_return bChanged;
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
					Repo.m_Location = Location;
					Repo.m_ConfigFile = ConfigFile;
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
						if (Repo.m_Remotes.f_FindEqual(Name))
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
					Repo.m_Position = ChildEntityData.m_Position;

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

						auto *pDependency = RepoRoots.f_FindLargestLessThanEqual(_Repo.m_ConfigFile);
						if (pDependency)
						{
							auto &DependencyRoot = RepoRoots.fs_GetKey(*pDependency);
							if (_Repo.m_ConfigFile.f_StartsWith(DependencyRoot))
							{
								if (!AddedRepos.f_FindEqual(DependencyRoot))
								{
									LeftFilePos = _Repo.m_Position;
									return;
								}
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

		if (mp_GenerateOptions.m_bSkipUpdate)
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

		CStateHandler StateHandler{f_GetBaseDir(), mp_OutputDir, mp_AnsiFlags, mp_fOutputConsole};

		CColors Colors(mp_AnsiFlags);

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
				return fGetReconcileActionByName(_Repo.f_GetIdentifierName(f_GetBaseDir(), f_GetBaseDir()));
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
					if (Repo.m_Location.f_StartsWith(f_GetBaseDir()))
					{
						RepoName = CFile::fs_MakePathRelative(Repo.m_Location, f_GetBaseDir());
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

		CGitLaunches Launches{f_GetBaseDir(), "Check repository status", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

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
											)
										)
									{
										bChanged.f_Exchange(true);
										CStr RelativePath = CFile::fs_MakePathRelative(_Repo.m_Location, f_GetBaseDir());
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

		auto MergedFiles = StateHandler.f_GetMergedFiles();
		for (auto &File : MergedFiles)
		{
			CStr FileName = MergedFiles.fs_GetKey(File);
			CStr BasePath = CFile::fs_GetPath(FileName);

			CStr FileContents;
			if (File.m_bIsStateFile)
			{
				CRegistry Registry;

				for (auto iConfig = File.m_Configs.f_GetIterator(); iConfig; ++iConfig)
				{
					auto &Config = *iConfig;
					if (Config.m_bExternalPath)
						Registry.f_SetValueNoPath("~" + iConfig.f_GetKey(), Config.m_Hash);
					else
						Registry.f_SetValueNoPath(CFile::fs_MakePathRelative(iConfig.f_GetKey(), BasePath), Config.m_Hash);
				}

				FileContents = Registry.f_GenerateStr().f_Replace(DMibNewLine, File.m_LineEndings);
			}
			else
			{
				CJsonSorted StateJson;

				for (auto iConfig = File.m_Configs.f_GetIterator(); iConfig; ++iConfig)
				{
					auto &Config = *iConfig;
					if (Config.m_bExternalPath)
						StateJson["~" + iConfig.f_GetKey()]["Hash"] = Config.m_Hash;
					else
						StateJson[CFile::fs_MakePathRelative(iConfig.f_GetKey(), BasePath)]["Hash"] = Config.m_Hash;
				}

				FileContents = StateJson.f_ToString().f_Replace("\n", File.m_LineEndings);
			}

			bool bWasCreated = false;
			if (!f_AddGeneratedFile(FileName, FileContents, "", bWasCreated))
				fs_ThrowError(CFilePosition{}, CStr::CFormat("File '{}' already generated with other contents") << FileName);

			if (bWasCreated)
			{
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(FileContents), false);
				f_WriteFile(FileData, FileName);
			}
		}

		{
			bool bForceReset = fg_GetSys()->f_GetEnvironmentVariable("MalterlibRepositoryHardReset", "") == gc_ConstString_true.m_String;
			bool bLastSeenActionNeeded = false;

			for (auto &LastSeen : LastSeenRepositories)
			{
				CStr FullRepoPath = f_GetBaseDir() / LastSeen;
				if (!SeenRepositories.f_FindEqual(LastSeen) && !ExcludeFromSeenRepositories.f_FindEqual(LastSeen) && CFile::fs_FileExists(FullRepoPath, EFileAttrib_Directory))
				{
					EHandleRepositoryRemovedAction Action = fGetReconcileRemovedActionByName(LastSeen);

					if (bForceReset || Action == EHandleRepositoryRemovedAction_Delete)
					{
						fg_OutputRepositoryInfo(EOutputType_Warning, "Deleting repository permanently from disk: {}"_f << FullRepoPath, StateHandler, LastSeen, MaxRepoWidth);
						CFile::fs_DeleteDirectoryRecursive(FullRepoPath, true);
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
					co_return DMibErrorInstance(fg_ReconcileRemovedHelp(mp_AnsiFlags));
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
