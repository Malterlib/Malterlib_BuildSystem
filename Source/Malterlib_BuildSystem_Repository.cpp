// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Repository.h"
#include <Mib/Process/ProcessLaunch>
#include <Mib/Process/ProcessLaunchActor>
#include <Mib/Encoding/EJSON>

static CStr g_ReconcileHelp = R"---(
Changes in sub-repositories needs to be reconciled.

Choose how you want to reconcile changes:

Accept recommended actions   : {0}./mib update_repos --reconcile=*:auto{1}
Rebase all                   : {0}./mib update_repos --reconcile=*:rebase{1}
Reset all                    : {0}./mib update_repos --reconcile=*:reset{1}
Reset all and delete removed : {0}./mib update_repos --reconcile=*:reset_delete{1}

To choose separate action for different repositories you can specify wildcards. The last matching wildcard wins:
{0}./mib update_repos --reconcile=*:auto,External/*:reset{1}

To show current status without reconciling use:
{0}./mib status --skip-update{1}
)---"_f /**/
	<< NMib::NBuildSystem::NRepository::CColors::ms_RepositoryName
	<< NMib::NBuildSystem::NRepository::CColors::ms_Default
;

static CStr g_ReconcileRemovedHelp = R"---(
Removed sub-repositories needs to be reconciled.

Choose how you want to reconcile changes:

Leave removed repositories on disk    : {0}./mib update_repos --reconcile=*:leave_removed{1}
Delete removed repositories from disk : {0}./mib update_repos --reconcile=*:delete_removed{1}

{2}Warning:{1} Specifying {0}reset_delete{1} or {0}delete_removed{1} for reconcile will delete the repository and any unpushed work permanently.

To choose separate action for different repositories you can specify wildcards. The last matching wildcard wins:
{0}./mib update_repos --reconcile=*:leave_removed,External/*:delete_removed{1}

To show current status without reconciling use:
{0}./mib status --skip-update{1}
)---"_f /**/
	<< NMib::NBuildSystem::NRepository::CColors::ms_RepositoryName
	<< NMib::NBuildSystem::NRepository::CColors::ms_Default
	<< NMib::NBuildSystem::NRepository::CColors::ms_StatusError
;

namespace NMib::NBuildSystem
{
	namespace
	{
		TCAggregate<CMutual> g_SubmoduleAddLock = {DAggregateInit};
	}

	namespace NRepository
	{
		ch8 const CColors::ms_RepositoryName[] = DAnsiColor_Reset DAnsiColor_256(221);
		ch8 const CColors::ms_BranchName[] = DAnsiColor_Reset;

		ch8 const CColors::ms_ToCommit[] = DAnsiColor_Reset DAnsiColor_256(46);
		ch8 const CColors::ms_ToPush[] = DAnsiColor_Reset DAnsiColor_256(32);
		ch8 const CColors::ms_ToPull[] = DAnsiColor_Reset DAnsiColor_256(9);

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

		CStateHandler::CStateHandler(CStr const &_BasePath, CStr const &_OutputDir)
			: mp_BasePath(_BasePath)
			, mp_OutputDir(_OutputDir)
		{
		}

		CMutual &CStateHandler::f_ConsoleOutputLock()
		{
			return mp_ConsoleOutputLock;
		}

		void CStateHandler::f_SetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Hash, CStr const &_Identifier)
		{
			DLock(mp_Lock);

			bool bExternalPath = false;
			CStr Identifier = _RepoPath;
			if (!_RepoPath.f_StartsWith(mp_BasePath))
			{
				Identifier = _Identifier;
				bExternalPath = true;
			}

			auto &ConfigFile = mp_NewConfigFiles[_FileName];
			auto &Config = ConfigFile.m_Configs[Identifier];
			Config.m_Hash = _Hash;
			Config.m_bExternalPath = bExternalPath;

			if (auto *pFile = mp_ConfigFiles.f_FindEqual(_FileName))
				ConfigFile.m_LineEndings = pFile->m_LineEndings;
		}

		CStr CStateHandler::f_GetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Identifier)
		{
			DLock(mp_Lock);
			auto &ConfigFile = fp_GetConfigFile(_FileName);

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

		void CStateHandler::f_AddGitIgnore(CStr const &_FileName, CBuildSystem const &_BuildSystem)
		{
			DLock(mp_Lock);
			if (!mp_GitIgnores(_FileName).f_WasCreated())
				return;
			CStr GitIgnoreFile = CFile::fs_GetPath(_FileName) + "/.gitignore";
			CStr IgnoreContents;
			if (CFile::fs_FileExists(GitIgnoreFile))
				IgnoreContents = CFile::fs_ReadStringFromFile(GitIgnoreFile, true);

			ch8 const *pParse = IgnoreContents;
			fg_ParseToEndOfLine(pParse);
			ch8 const *pLineEnd = "\n";
			if (*pParse == '\r')
				pLineEnd = "\r\n";

			CStr IgnoreLine = fg_Format("/{}{}", CFile::fs_GetFile(_FileName), pLineEnd);
			if (IgnoreContents.f_Find(IgnoreLine) < 0)
			{
				IgnoreContents += IgnoreLine;
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(IgnoreContents), false);
				_BuildSystem.f_WriteFile(FileData, GitIgnoreFile);
			}
		}

		CConfigFile CStateHandler::fs_ParseConfigFile(CStr const &_Contents, CStr const &_FileName)
		{
			CConfigFile ConfigFile;

			CRegistry_CStr Registry;
			{
				ch8 const *pParse = _Contents;
				fg_ParseToEndOfLine(pParse);
				if (*pParse == '\r')
					ConfigFile.m_LineEndings = "\r\n";
			}

			CStr BasePath = CFile::fs_GetPath(_FileName);

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

			return ConfigFile;
		}

		CConfigFile const &CStateHandler::fp_GetConfigFile(CStr const &_FileName)
		{
			DLock(mp_Lock);
			if (auto *pFile = mp_ConfigFiles.f_FindEqual(_FileName))
				return *pFile;
			auto &ConfigFile = mp_ConfigFiles[_FileName];
			if (CFile::fs_FileExists(_FileName))
				ConfigFile = fs_ParseConfigFile(CFile::fs_ReadStringFromFile(_FileName, true), _FileName);

			return ConfigFile;
		}

		TCSet<CStr> CStateHandler::f_GetLastSeenRepositories()
		{
			CStr RepositoryStateFile = mp_OutputDir / "RepositoryState.json";

			if (!CFile::fs_FileExists(RepositoryStateFile))
				return {};

			CEJSON StateFile = CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(RepositoryStateFile, true), RepositoryStateFile);

			TCSet<CStr> SeenRepositories;

			if (auto *pSeenReposJSON = StateFile.f_GetMember("SeenRepositories", EEJSONType_Object))
			{
				for (auto &SeenJSON : pSeenReposJSON->f_Object())
				{
					SeenRepositories[SeenJSON.f_Name()];
				}
			}

			return SeenRepositories;
		}

		void fg_OutputRepositoryInfo(EOutputType _OutputType, CStr const &_Info, CStateHandler &o_StateHandler, CStr const &_RepoName, mint _MaxRepoWidth)
		{
			ch8 const *pRepoColor = CColors::ms_StatusNormal;
			switch (_OutputType)
			{
			case EOutputType_Normal: pRepoColor = CColors::ms_StatusNormal; break;
			case EOutputType_Warning: pRepoColor = CColors::ms_StatusWarning; break;
			case EOutputType_Error: pRepoColor = CColors::ms_StatusError; break;
			}

			CStr RepoName = "{sj*,a-}"_f << _RepoName << _MaxRepoWidth;

			CStr ReplacedRepo = RepoName.f_Replace("/", "{}{}/{}"_f << CColors::ms_Default << DAnsiColor_256(250) << pRepoColor ^ 1);
			{
				DMibLock(o_StateHandler.f_ConsoleOutputLock());
				DMibConOut2
					(
						"{}{}{}   {}\n"
						, pRepoColor
						, ReplacedRepo
						, CColors::ms_Default
						, _Info
					)
				;
			}
		}

		bool fg_HandleRepository
			(
			 	CStr const &_ReposDirectory
			 	, CRepository const &_Repo
			 	, CStateHandler &o_StateHandler
			 	, CBuildSystem const &_BuildSystem
			 	, TCMap<CStr, CRepository const *> const &_AllRepositories
			 	, EHandleRepositoryAction _ReconcileAction
			 	, mint _MaxRepoWidth
			)
		{
			CStr Location = _ReposDirectory + "/" + _Repo.f_GetName();
			CStr BaseDir = _BuildSystem.f_GetBaseDir();
			CStr RepositoryIdentifier = _Repo.f_GetIdentifierName(BaseDir, BaseDir);

			bool bIsRoot = _Repo.m_Type == "Root";
			
			CDisableExceptionTraceScope DisableTrace;
			
			bool bChanged = false;

			auto fLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir)
				{
					CProcessLaunchParams Params{_WorkingDir};
#ifdef DPlatformFamily_OSX
					Params.m_Environment["PATH"] = "/opt/local/bin:" + CStr(fg_GetSys()->f_GetEnvironmentVariable("PATH"));
#endif
					Params.m_bShowLaunched = false;
					return CProcessLaunch::fs_LaunchTool("git", _Params, Params);
				}
			;
			auto fLaunchGitQuestion = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir)
				{
					CProcessLaunchParams Params{_WorkingDir};
#ifdef DPlatformFamily_OSX
					Params.m_Environment["PATH"] = "/opt/local/bin:" + CStr(fg_GetSys()->f_GetEnvironmentVariable("PATH"));
#endif
					Params.m_bShowLaunched = false;
					uint32 ExitCode = 1;
					CStr StdErr;
					CStr StdOut;
					CProcessLaunch::fs_LaunchBlock("git", _Params, StdOut, StdErr, ExitCode, Params);
					if (!StdErr.f_IsEmpty())
						DMibError("Failed to ask git question {vs}: {}{}"_f << _Params << StdErr << StdOut);
					return ExitCode == 0;
				}
			;
			auto fLaunchGitNonEmpty = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir)
				{
					CProcessLaunchParams Params{_WorkingDir};
#ifdef DPlatformFamily_OSX
					Params.m_Environment["PATH"] = "/opt/local/bin:" + CStr(fg_GetSys()->f_GetEnvironmentVariable("PATH"));
#endif
					Params.m_bShowLaunched = false;
					uint32 ExitCode = 1;
					CStr StdErr;
					CStr StdOut;
					CProcessLaunch::fs_LaunchBlock("git", _Params, StdOut, StdErr, ExitCode, Params);
					if (ExitCode)
						return false;

					return !StdOut.f_IsEmpty();
				}
			;
			auto fTryLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir) -> CStr
				{
					CProcessLaunchParams Params{_WorkingDir};
#ifdef DPlatformFamily_OSX
					Params.m_Environment["PATH"] = "/opt/local/bin:" + CStr(fg_GetSys()->f_GetEnvironmentVariable("PATH"));
#endif
					Params.m_bShowLaunched = false;
					uint32 ExitCode = 1;
					CStr StdOut;
					CStr StdErr;
					CProcessLaunch::fs_LaunchBlock("git", _Params, StdOut, StdErr, ExitCode, Params);

					if (ExitCode == 0)
						return {};

					if (!StdErr.f_IsEmpty())
						return StdErr;

					if (!StdOut.f_IsEmpty())
						return StdOut;

					return "Unknown error";
				}
			;


			struct CGitVersion
			{
				bool operator < (CGitVersion const &_Right) const
				{
					return fg_TupleReferences(m_Major, m_Minor, m_Patch) < fg_TupleReferences(_Right.m_Major, _Right.m_Minor, _Right.m_Patch);
				}

				uint32 m_Major = 0;
				uint32 m_Minor = 0;
				uint32 m_Patch = 0;
			};

			auto fGetGitVersion = [&]() -> CGitVersion
				{
					static CGitVersion GitVersion;
					static CMutualSpin Lock;

					DMibLock(Lock);

					if (GitVersion.m_Major)
						return GitVersion;

					CStr VersionStr = fLaunchGit({"--version"}, "").f_Trim();

					CGitVersion Version;

					aint nParsed = 0;
					(CStr::CParse("git version {}.{}.{}") >> Version.m_Major >> Version.m_Minor >> Version.m_Patch).f_Parse(VersionStr, nParsed);
					if (nParsed != 3)
						DMibError("Failed to parse git version");

					GitVersion = Version;

					return GitVersion;
				}
			;

			CStr ConfigHash;
			if (!bIsRoot)
				ConfigHash = o_StateHandler.f_GetHash(_Repo.m_ConfigFile, Location, _Repo.m_Identity);

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

			if (bCloneNew)
			{
				if (ConfigHash.f_IsEmpty())
					fOutputInfo(EOutputType_Normal, "Adding external repository");
				else
					fOutputInfo(EOutputType_Normal, "Adding external repository at commit {}"_f << ConfigHash);

				CStr GitRoot = fg_GetGitRoot(Location);
				if (GitRoot.f_IsEmpty() || _Repo.m_Submodule != "true")
				{
					try
					{
						fLaunchGit({"clone", "-n", _Repo.m_URL, Location}, "");

						TCVector<CStr> Params = {"checkout", "-B", _Repo.m_DefaultBranch};

						if (!ConfigHash.f_IsEmpty())
							Params.f_Insert(ConfigHash);

						fLaunchGit(Params, Location);
						bChanged = true;
					}
					catch (CException const &_Exception)
					{
						CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to clone repository: {}", _Exception));
					}
				}
				else 
				{
					CStr RelativeLocation = CFile::fs_MakePathRelative(Location, GitRoot);
					try
					{
						DLock(*g_SubmoduleAddLock); // Currently git has race issues with submodule adds
						fLaunchGit({"submodule", "add", "-b", _Repo.m_DefaultBranch, "--name", _Repo.m_SubmoduleName, _Repo.m_URL, RelativeLocation}, GitRoot);
						fLaunchGit({"config", "-f", ".gitmodules", fg_Format("submodule.{}.fetchRecurseSubmodules", _Repo.m_SubmoduleName), "on-demand"}, GitRoot);
						bChanged = true;
					}
					catch (CException const &_Exception)
					{
						CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to add submodule repository: {}", _Exception));
					}
				}
			}
			else if (!bIsRoot)
			{
				CStr GitRoot = fg_GetGitRoot(Location);
				if (GitRoot.f_IsEmpty() || _Repo.m_Submodule != "true")
					o_StateHandler.f_AddGitIgnore(Location, _BuildSystem);
			}

			bool bForceReset = fg_GetSys()->f_GetEnvironmentVariable("MalterlibRepositoryHardReset", "") == "true";

			auto GitConfig = fg_GetGitConfig(Location, _Repo.m_Position);
			auto &CurrentRemotes = GitConfig.m_Remotes;
			auto WantedRemotes = _Repo.m_Remotes;
			WantedRemotes["origin"].m_URL = _Repo.m_URL;

			if (_Repo.m_UserName && GitConfig.m_UserName != _Repo.m_UserName)
			{
				fOutputInfo(EOutputType_Normal, "Changing user name '{}' -> '{}'"_f << GitConfig.m_UserName << _Repo.m_UserName);
				fLaunchGit({"config", "--local", "user.name", _Repo.m_UserName}, Location);
			}

			if (_Repo.m_UserEmail && GitConfig.m_UserEmail != _Repo.m_UserEmail)
			{
				fOutputInfo(EOutputType_Normal, "Changing user email '{}' -> '{}'"_f << GitConfig.m_UserEmail << _Repo.m_UserEmail);
				fLaunchGit({"config", "--local", "user.email", _Repo.m_UserEmail}, Location);
			}

			if (!WantedRemotes.f_IsEmpty())
			{
				for (auto iRemote = WantedRemotes.f_GetIterator(); iRemote; ++iRemote)
				{
					auto &RemoteName = iRemote.f_GetKey();
					auto &Remote = *iRemote;
					auto pCurrentRemote = CurrentRemotes.f_FindEqual(RemoteName);
					if (pCurrentRemote)
					{
						if (*pCurrentRemote == Remote.m_URL)
							continue;
						fOutputInfo(EOutputType_Normal, "Changing remote URL '{}={}'"_f << RemoteName << Remote.m_URL);
						fLaunchGit({"remote", "set-url", RemoteName, Remote.m_URL}, Location);
						continue;
					}
					fOutputInfo(EOutputType_Normal, "Adding remote '{}={}'"_f << RemoteName << Remote.m_URL);
					fLaunchGit({"remote", "add", RemoteName, Remote.m_URL}, Location);
					fLaunchGit({"fetch", "--tags", RemoteName}, Location);
				}
				if (bForceReset)
				{
					for (auto iRemote = CurrentRemotes.f_GetIterator(); iRemote; ++iRemote)
					{
						auto &RemoteName = iRemote.f_GetKey();
						if (WantedRemotes.f_FindEqual(RemoteName))
							continue;
						fOutputInfo(EOutputType_Normal, "Removing remote '{}'"_f << RemoteName);
						fLaunchGit({"remote", "remove", RemoteName}, Location);
					}
				}
			}

			if (bIsRoot)
				return false;

			CStr CurrentHash = o_StateHandler.f_GetHash(_Repo.m_StateFile, Location, _Repo.m_Identity);
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
				try
				{
					if (HeadHash != ConfigHash)
					{
						TCVector<CStr> FetchParams = {"fetch", "--all", "--prune", "--tags"};

						if (bForceReset && fGetGitVersion() >= CGitVersion{2, 17})
							FetchParams.f_Insert("--prune-tags");

						fLaunchGit(FetchParams, Location);
					}

					if (bForceReset)
					{
						if (HeadHash != ConfigHash || fLaunchGitNonEmpty({"status", "--porcelain"}, Location))
						{
							fOutputInfo(EOutputType_Warning, "Force Resetting to '{}'"_f << ConfigHash);
							fLaunchGit({"checkout", "-f", "-B", _Repo.m_DefaultBranch, ConfigHash}, Location);
							fLaunchGit({"clean", "-fd"}, Location);
							if (_Repo.m_bUpdateSubmodules)
								fLaunchGit({"submodule", "update", "--init"}, Location);
							bChanged = true;
						}
					}
					else
					{
						EHandleRepositoryAction RecommendedAction = EHandleRepositoryAction_None;

						do
						{
							if (fLaunchGitQuestion({"merge-base", "--is-ancestor", HeadHash, ConfigHash}, Location))
							{
								if (!fLaunchGit({"status", "--porcelain"}, Location).f_IsEmpty())
									RecommendedAction = EHandleRepositoryAction_Rebase;
								else
									RecommendedAction = EHandleRepositoryAction_Reset;
								break;
							}

							if (fLaunchGitQuestion({"merge-base", "--is-ancestor", ConfigHash, HeadHash}, Location))
							{
								RecommendedAction = EHandleRepositoryAction_None;
								break;
							}

							if (!fLaunchGit({"status", "--porcelain"}, Location).f_IsEmpty())
							{
								RecommendedAction = EHandleRepositoryAction_ManualResolve;
								break;
							}

							bool bIsOnRemote = false;

							if (fLaunchGitNonEmpty({"branch", "-r", "--contains", HeadHash}, Location) || fLaunchGitNonEmpty({"tag", "--contains", HeadHash}, Location))
								bIsOnRemote = true;

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

							CStr History = fLaunchGit({"log", "-p", "origin/{}"_f << Repo.m_DefaultBranch, "--", _Repo.m_ConfigFile}, Repo.m_Location);
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

						if (Action == EHandleRepositoryAction_Reset || Action == EHandleRepositoryAction_ResetDelete)
						{
							if (fLaunchGit({"rev-parse", "--abbrev-ref", "HEAD"}, Location).f_Trim() == "HEAD")
							{
								fOutputInfo(EOutputType_Warning, "Resetting to {} and recovering from detached head"_f << ConfigHash);
								fLaunchGit({"checkout", "-f", "-B", _Repo.m_DefaultBranch, ConfigHash}, Location); // Recover from detached head
							}
							else
							{
								fOutputInfo(EOutputType_Warning, "Resetting to {}"_f << ConfigHash);
								fLaunchGit({"reset", "--hard", ConfigHash}, Location);
							}
							if (_Repo.m_bUpdateSubmodules)
								fLaunchGit({"submodule", "update", "--init"}, Location);
						}
						else if (Action == EHandleRepositoryAction_Rebase)
						{
							fOutputInfo(EOutputType_Normal, "Rebasing on top of {}"_f << ConfigHash);

							TCSet<CStr> AllConfigFiles;
							for (auto &pRepository : _AllRepositories)
								AllConfigFiles[pRepository->m_ConfigFile];

							auto fResolveConflicts = [&](CStr const &_ConflictingFiles) -> bool
								{
									bool bAllResolved = true;
									for (auto &File : _ConflictingFiles.f_SplitLine())
									{
										CStr FullPath = CFile::fs_AppendPath(_Repo.m_Location, File);

										if (AllConfigFiles.f_FindEqual(FullPath))
										{
											fOutputInfo(EOutputType_Warning, "Ignoring conflict in config file '{}'"_f << File);
											fLaunchGit({"checkout", "--ours", "--", File}, Location);
											fLaunchGit({"add", File}, Location);
										}
										else
											bAllResolved = false;
									}
									return bAllResolved;
								}
							;

							bool bAllResolved = true;
							if (auto RebaseError = fTryLaunchGit({"rebase", "--autostash", ConfigHash}, Location))
							{
								while (bAllResolved)
								{
									CStr ConflictingFiles = fLaunchGit({"diff", "--name-only", "--diff-filter=U"}, Location);
									if (ConflictingFiles.f_IsEmpty())
									{
										bAllResolved = false;
										break;
									}

									if (!fResolveConflicts(ConflictingFiles))
										bAllResolved = false;

									if (bAllResolved)
									{
										if (fLaunchGit({"status", "--porcelain"}, Location).f_IsEmpty())
										{
											if (!fTryLaunchGit({"rebase", "--skip"}, Location))
												break;
										}
										else
										{
											if (!fTryLaunchGit({"rebase", "--continue"}, Location))
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
								CStr ConflictingFiles = fLaunchGit({"diff", "--name-only", "--diff-filter=U"}, Location);
								if (!ConflictingFiles.f_IsEmpty())
									fResolveConflicts(ConflictingFiles);
								if (_Repo.m_bUpdateSubmodules)
									fLaunchGit({"submodule", "update", "--init"}, Location);
							}
						}
						else if (Action == EHandleRepositoryAction_ManualResolve)
						{
							fOutputInfo(EOutputType_Error, "Manual reconcile against {} needed"_f << ConfigHash);
							bPassException = true;
							DMibError("Manual reconcile needed");
						}
						else if (Action == EHandleRepositoryAction_LeaveRemoved || Action == EHandleRepositoryAction_DeleteRemoved)
						{
							fOutputInfo(EOutputType_Error, "Cannot specify leave or delete removed for existing repository");
							bPassException = true;
							DMibError("Invalid reconcile option");
						}
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
							case EHandleRepositoryAction_ResetDelete:
							default:
								ActionStr = "internal error";
								break;
							}

							fOutputInfo
								(
								 	OutputType
								 	, "{}{}{} recommended for {}{}{} -> {}{}{}"_f
								 	<< CColors::ms_RepositoryName << ActionStr << CColors::ms_Default
								 	<< CColors::ms_ToPush << HeadHash << CColors::ms_Default
								 	<< CColors::ms_ToPush << ConfigHash << CColors::ms_Default
								)
							;
							bPassException = true;
							DMibError(g_ReconcileHelp);
						}
						bChanged = true;
					}
				}
				catch (CException const &_Exception)
				{
					if (bPassException)
						throw;
					fOutputInfo(EOutputType_Error, "Reconcile error: "_f << _Exception.f_GetErrorStr().f_Trim());
					CBuildSystem::fs_ThrowError(_Repo.m_Position, "Failed to reconcile hash '{}': {}"_f << ConfigHash << _Exception.f_GetErrorStr().f_Trim());
				}
			}
			
			CStr GitHeadHash = fg_GetGitHeadHash(Location, _Repo.m_Position);
			o_StateHandler.f_SetHash(_Repo.m_StateFile, Location, GitHeadHash, _Repo.m_Identity);
			o_StateHandler.f_SetHash(_Repo.m_ConfigFile, Location, GitHeadHash, _Repo.m_Identity);
			o_StateHandler.f_AddGitIgnore(_Repo.m_StateFile, _BuildSystem);

			return bChanged;
		}

		CRepoEditor fg_GetRepoEditor(CBuildSystem &_BuildSystem, CBuildSystemData &_Data)
		{
			CStr EditorString = _BuildSystem.f_EvaluateEntityProperty(_Data.m_RootEntity, EPropertyType_Property, "MalterlibRepositoryEditor");

			CRepoEditor Editor;
			Editor.m_bOpenSequential = _BuildSystem.f_EvaluateEntityProperty(_Data.m_RootEntity, EPropertyType_Property, "MalterlibRepositoryEditorSequential") == "true";
			Editor.m_Sleep = _BuildSystem.f_EvaluateEntityProperty(_Data.m_RootEntity, EPropertyType_Property, "MalterlibRepositoryEditorSleep").f_ToFloat(fp32(0.0f));
			Editor.m_WorkingDir = _BuildSystem.f_EvaluateEntityProperty(_Data.m_RootEntity, EPropertyType_Property, "MalterlibRepositoryEditorWorkingDir");
			Editor.m_Application = fg_GetStrSepEscaped(EditorString, " ");

			while (!EditorString.f_IsEmpty())
				Editor.m_Params.f_Insert(fg_GetStrSepEscaped(EditorString, " "));

			return Editor;
		}

		TCVector<TCMap<CStr, CReposLocation>> fg_GetRepos(CBuildSystem &_BuildSystem, CBuildSystemData &_Data)
		{
			TCMap<CStr, CReposLocation> Repos;

			TCMap<CStr, CFilePosition> RepoRoots;

			for (auto &ChildEntity : _Data.m_RootEntity.m_ChildEntitiesOrdered)
			{
				if (ChildEntity.m_Key.m_Type != EEntityType_Repository)
					continue;
				if (!_BuildSystem.f_EvalCondition(ChildEntity, ChildEntity.m_Condition))
					continue;

				CStr Location = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Location");

				if (Location.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.Location");

				CStr ReposDirectory = CFile::fs_GetPath(Location);

				auto &ReposLocation = Repos[ReposDirectory];

				auto RepoMap = ReposLocation.m_Repositories(CFile::fs_GetFile(Location));

				if (!RepoMap.f_WasCreated())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, fg_Format("Duplicate repository location: {}", Location));

				if (!RepoRoots(Location, ChildEntity.m_Position).f_WasCreated())
				{
					CBuildSystemError Error;
					Error.m_Error = "Other specification";
					Error.m_Position = RepoRoots[Location];

					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "Repository location already specified specified previously", fg_CreateVector(Error));
				}

				auto &Repo = *RepoMap;
				Repo.m_Identity = ChildEntity.m_Key.m_Name;
				Repo.m_Location = Location;
				Repo.m_ConfigFile = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "ConfigFile");
				Repo.m_StateFile = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "StateFile");
				Repo.m_URL = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "URL");
				Repo.m_DefaultBranch = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "DefaultBranch");
				Repo.m_DefaultUpstreamBranch = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "DefaultUpstreamBranch");
				Repo.m_Tags.f_AddContainer(_BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Tags").f_Split(";"));
				Repo.m_Submodule = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Submodule");
				Repo.m_SubmoduleName = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "SubmoduleName");
				Repo.m_Type = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Type");
				Repo.m_UserName = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "UserName");
				Repo.m_UserEmail = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "UserEmail");
				Repo.m_bUpdateSubmodules = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "UpdateSubmodules") == "true";

				TCVector<CStr> NoPushRemotes;
				for (auto &Wildcard : _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "NoPushRemotes").f_Split(";"))
				{
					if (Wildcard.f_IsEmpty())
						continue;
					NoPushRemotes.f_Insert(Wildcard);
				}

				CStr Remotes = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Remotes");
				while (!Remotes.f_IsEmpty())
				{
					CStr RemoteString = fg_GetStrSep(Remotes, ";");
					if (RemoteString.f_IsEmpty())
						continue;
					CStr Name = fg_GetStrSep(RemoteString, "=");
					if (Repo.m_Remotes.f_FindEqual(Name))
						_BuildSystem.fs_ThrowError(ChildEntity.m_Position, fg_Format("Same remote '{}' specified multiple times", Name));
					auto &OutRemote = Repo.m_Remotes[Name];
					OutRemote.m_URL = RemoteString;
					for (auto &Wildcard : NoPushRemotes)
					{
						if (fg_StrMatchWildcard(Name.f_GetStr(), Wildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						{
							OutRemote.m_bCanPush = false;
							break;
						}
					}
				}
				Repo.m_Position = ChildEntity.m_Position;

				if (Repo.m_URL.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.URL");
				if (Repo.m_ConfigFile.f_IsEmpty() && Repo.m_Type != "Root")
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.ConfigFile");
				if (Repo.m_StateFile.f_IsEmpty() && Repo.m_Type != "Root")
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.StateFile");
				if (Repo.m_ConfigFile == Repo.m_StateFile && Repo.m_Type != "Root")
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.ConfigFile and Repository.StateFile must not be same file");
				if (Repo.m_DefaultBranch.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.DefaultBranch");
				if (Repo.m_Submodule == "true" && Repo.m_SubmoduleName.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.SubmoduleName");

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

						(*pTheseRepos)[_RepoLocation.f_GetPath()].m_Repositories[_Repo.f_GetName()] = _Repo;
						ToAddRepos[_Repo.m_Location];
						bDoneSomething = true;
					}
				;

				for (auto &RepoLocation : Repos)
				{
					for (auto &Repo : RepoLocation.m_Repositories)
					{
						if (Repo.m_Type != "Root")
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
							if (Repo.m_Type == "Root")
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

	CBuildSystem::ERetry CBuildSystem::fp_HandleRepositories(TCMap<CPropertyKey, CStr> const &_Values, bool _bSkipRepoUpdate, TCMap<CStr, EHandleRepositoryAction> const &_Actions)
	{
		f_InitEntityForEvaluation(mp_Data.m_RootEntity, _Values);
		f_ExpandRepositoryEntities(mp_Data);

		if (_bSkipRepoUpdate)
			return ERetry_None;

		TCVector<TCMap<CStr, CReposLocation>> ReposOrdered = fg_GetRepos(*this, mp_Data);

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

		CStateHandler StateHandler{mp_BaseDir, mp_OutputDir};

		auto fGetReconcileActionByName = [&](CStr const &_RepoName)
			{
				EHandleRepositoryAction Action = EHandleRepositoryAction_None;
				for (auto &WildcardAction : _Actions)
				{
					auto &Wildcard = _Actions.fs_GetKey(WildcardAction);
					if (fg_StrMatchWildcard(_RepoName.f_GetStr(), Wildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						Action = WildcardAction;
				}

				return Action;
			}
		;

		auto fGetReconcileAction = [&](CRepository const &_Repo)
			{
				return fGetReconcileActionByName(_Repo.f_GetIdentifierName(mp_BaseDir, mp_BaseDir));
			}
		;

		mint MaxRepoWidth = 0;

		auto LastSeenRepositories = StateHandler.f_GetLastSeenRepositories();
		for (auto &RepoName : LastSeenRepositories)
			MaxRepoWidth = fg_Max(MaxRepoWidth, (mint)RepoName.f_GetLen());

		TCSet<CStr> SeenRepositories;

		for (auto &Repos : ReposOrdered)
		{
			for (auto &Repos : Repos)
			{
				for (auto &Repo : Repos.m_Repositories)
				{
					CStr RepoName;
					if (Repo.m_Location.f_StartsWith(mp_BaseDir))
					{
						RepoName = CFile::fs_MakePathRelative(Repo.m_Location, mp_BaseDir);
						SeenRepositories[RepoName];
					}
					else
						RepoName = Repo.m_Location;

					MaxRepoWidth = fg_Max(MaxRepoWidth, (mint)RepoName.f_GetLen());
				}
			}
		}

		CThreadPool ThreadPool{fg_Clamp(NSys::fg_Thread_GetVirtualCores() * 2u, 16u, CGitLaunches::fs_MaxProcesses()/2)};

		for (auto &Repos : ReposOrdered)
		{
			fg_ParallellForEach
				(
					Repos
					, [&](auto &_Repos)
					{
						CStr ReposDirectory = _Repos.f_GetPath();
						CFile::fs_CreateDirectory(ReposDirectory);

						StateHandler.f_AddGitIgnore(ReposDirectory + "/RepoLock.MRepoState", *this);

						CLockFile LockFile{ReposDirectory + "/RepoLock.MRepoState"};
						LockFile.f_LockWithException(5.0*60.0);
						fg_ParallellForEach
							(
								_Repos.m_Repositories
								, [&](auto &_Repo)
								{
									if (fg_HandleRepository(ReposDirectory, _Repo, StateHandler, *this, AllRepositories, fGetReconcileAction(_Repo), MaxRepoWidth))
									{
										bChanged.f_Exchange(true);
										CStr RelativePath = CFile::fs_MakePathRelative(_Repo.m_Location, f_GetBaseDir());
										if (RelativePath.f_StartsWith("Binaries/"))
											bBinariesChange.f_Exchange(true);
									}
								}
							 	, ThreadPool
							)
						;
					}
				 	, ThreadPool
				)
			;

			if (bChanged.f_Load())
				break;
		}

		auto MergedFiles = StateHandler.f_GetMergedFiles();
		for (auto iFile = MergedFiles.f_GetIterator(); iFile; ++iFile)
		{
			CRegistry_CStr Registry;
			
			CStr BasePath = CFile::fs_GetPath(iFile.f_GetKey());
			
			for (auto iConfig = iFile->m_Configs.f_GetIterator(); iConfig; ++iConfig)
			{
				auto &Config = *iConfig;
				if (Config.m_bExternalPath)
					Registry.f_SetValueNoPath("~" + iConfig.f_GetKey(), Config.m_Hash);
				else
					Registry.f_SetValueNoPath(CFile::fs_MakePathRelative(iConfig.f_GetKey(), BasePath), Config.m_Hash);
			}
			
			CStr FileName = iFile.f_GetKey();
			CStr FileContents = Registry.f_GenerateStr().f_Replace(DMibNewLine, iFile->m_LineEndings);

			bool bWasCreated = false;
			if (!f_AddGeneratedFile(FileName, FileContents, "", bWasCreated))
				fs_ThrowError(CFilePosition{}, CStr::CFormat("File '{}' already generated with other contents") << FileName);

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(FileContents), false);
				f_WriteFile(FileData, FileName);
			}
		}

		bool bForceReset = fg_GetSys()->f_GetEnvironmentVariable("MalterlibRepositoryHardReset", "") == "true";
		bool bLastSeenActionNeeded = false;

		for (auto &LastSeen : LastSeenRepositories)
		{
			CStr FullRepoPath = mp_BaseDir / LastSeen;
			if (!SeenRepositories.f_FindEqual(LastSeen) && CFile::fs_FileExists(FullRepoPath, EFileAttrib_Directory))
			{
				EHandleRepositoryAction Action = fGetReconcileActionByName(LastSeen);

				if (bForceReset || Action == EHandleRepositoryAction_ResetDelete || Action == EHandleRepositoryAction_DeleteRemoved)
				{
					fg_OutputRepositoryInfo(EOutputType_Warning, "Deleting repository permanently from disk: {}"_f << FullRepoPath, StateHandler, LastSeen, MaxRepoWidth);
					CFile::fs_DeleteDirectoryRecursive(FullRepoPath);
				}
				else if (Action != EHandleRepositoryAction_LeaveRemoved)
				{
					fg_OutputRepositoryInfo(EOutputType_Warning, "Repository has been {}removed{}"_f<< CColors::ms_ToPush << CColors::ms_Default, StateHandler, LastSeen, MaxRepoWidth);
					bLastSeenActionNeeded = true;
				}
			}
		}

		if (bLastSeenActionNeeded)
			DMibError(g_ReconcileRemovedHelp);
		else
		{
			CStr RepositoryStateFile = mp_OutputDir / "RepositoryState.json";

			CEJSON StateFile;

			auto &SeenRepositoriesJSON = StateFile["SeenRepositories"];
			for (auto &LastSeen : SeenRepositories)
				SeenRepositoriesJSON[LastSeen] = 1;

			CStr FileContents = StateFile.f_ToString();

			bool bWasCreated = false;
			if (!f_AddGeneratedFile(RepositoryStateFile, FileContents, "", bWasCreated))
				fs_ThrowError(CFilePosition{}, CStr::CFormat("File '{}' already generated with other contents") << RepositoryStateFile);

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(FileContents), false);
				f_WriteFile(FileData, RepositoryStateFile);
			}
		}

		if (bChanged.f_Load())
		{
			if (bBinariesChange.f_Load())
				return ERetry_Relaunch;
			else
				return ERetry_Again;
		}

		return ERetry_None;
	}
}
