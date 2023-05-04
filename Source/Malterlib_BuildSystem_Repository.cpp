// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Repository.h"
#include <Mib/Process/ProcessLaunch>
#include <Mib/Process/ProcessLaunchActor>
#include <Mib/Encoding/EJSON>
#include <Mib/Concurrency/AsyncDestroy>

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

		CMutual &CStateHandler::f_ConsoleOutputLock()
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
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(IgnoreContents), false);
				_BuildSystem.f_WriteFile(FileData, GitIgnoreFile);
			}
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
				auto RegistryJson = CJSONSorted::fs_FromString(_Contents, _FileName);

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

			CEJSONSorted StateFile = CEJSONSorted::fs_FromString(CFile::fs_ReadStringFromFile(RepositoryStateFile, true), RepositoryStateFile);

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

		TCFuture<bool> DMibWorkaroundUBSanSectionErrors fg_HandleRepository
			(
				CGitLaunches &_Launches
				, CStr const &_ReposDirectory
				, CRepository const &_Repo
				, CStateHandler &o_StateHandler
				, CBuildSystem const &_BuildSystem
				, TCMap<CStr, CRepository const *> const &_AllRepositories
				, EHandleRepositoryAction _ReconcileAction
				, mint _MaxRepoWidth
			)
		{
			co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

			CColors Colors(o_StateHandler.f_AnsiFlags());

			CStr Location = _ReposDirectory + "/" + _Repo.f_GetName();
			CStr BaseDir = _BuildSystem.f_GetBaseDir();
			CStr RepositoryIdentifier = _Repo.f_GetIdentifierName(BaseDir, BaseDir);

			bool bIsRoot = _Repo.m_Type == gc_ConstString_Root.m_String;

			CDisableExceptionTraceScope DisableTrace;

			bool bChanged = false;

			auto fLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir, TCMap<CStr, CStr> const &_Environment = {}) -> TCFuture<CStr>
				{
					co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

					TCVector<CStr> CommandLineParams{"-C", _WorkingDir};
					CommandLineParams.f_Insert(_Params);

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params, _Environment, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode);
					co_return Return.f_GetStdOut();
				}
			;
			auto fLaunchGitQuestion = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir, bool _bErrorOnStdErr) -> TCFuture<bool>
				{
					co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params);

					CStr StdErr = Return.f_GetErrorOut().f_Trim();
					if (_bErrorOnStdErr && !StdErr.f_IsEmpty())
						DMibError("Failed to ask git question {vs}: {}"_f << _Params << Return.f_GetCombinedOut());

					co_return Return.m_ExitCode == 0;
				}
			;
			auto fLaunchGitNonEmpty = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir) -> TCFuture<bool>
				{
					co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

					auto Return = co_await _Launches.f_Launch(_WorkingDir, _Params);

					if (Return.m_ExitCode)
						co_return false;

					co_return !Return.f_GetStdOut().f_IsEmpty();
				}
			;
			auto fTryLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir) -> TCFuture<CStr>
				{
					co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

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
							[&]() -> TCFuture<void>
							{
								co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

								co_await fLaunchGit({"clone", "-n", _Repo.m_URL, Location}, "");

								TCVector<CStr> Params = {"checkout", "-B", _Repo.m_DefaultBranch};

								if (!ConfigHash.f_IsEmpty())
									Params.f_Insert(ConfigHash);

								co_await fLaunchGit(Params, Location);

								bChanged = true;

								co_await fLaunchGit({"branch", "-u", "origin/{}"_f << _Repo.m_DefaultBranch}, Location);

								if (_Repo.m_bUpdateSubmodules)
									co_await fLaunchGit({"submodule", "update", "--init"}, Location);

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
					auto Result = co_await fg_CallSafe
						(
							[&]() -> TCFuture<void>
							{
								co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);
								DLock(*g_SubmoduleAddLock); // Currently git has race issues with submodule adds
								co_await fLaunchGit({"submodule", "add", "-b", _Repo.m_DefaultBranch, "--name", _Repo.m_SubmoduleName, _Repo.m_URL, RelativeLocation}, GitRoot);
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
					o_StateHandler.f_AddGitIgnore(Location, _BuildSystem);
			}

			bool bForceReset = fg_GetSys()->f_GetEnvironmentVariable("MalterlibRepositoryHardReset", "") == gc_ConstString_true.m_String;

			auto GitConfig = fg_GetGitConfig(Location, _Repo.m_Position);
			auto &CurrentRemotes = GitConfig.m_Remotes;
			auto WantedRemotes = _Repo.m_Remotes;
			WantedRemotes["origin"].m_URL = _Repo.m_URL;

			if (_Repo.m_UserName && GitConfig.m_UserName != _Repo.m_UserName)
			{
				fOutputInfo(EOutputType_Normal, "Changing user name '{}' -> '{}'"_f << GitConfig.m_UserName << _Repo.m_UserName);
				co_await fLaunchGit({"config", "--local", "user.name", _Repo.m_UserName}, Location);
			}

			if (_Repo.m_UserEmail && GitConfig.m_UserEmail != _Repo.m_UserEmail)
			{
				fOutputInfo(EOutputType_Normal, "Changing user email '{}' -> '{}'"_f << GitConfig.m_UserEmail << _Repo.m_UserEmail);
				co_await fLaunchGit({"config", "--local", "user.email", _Repo.m_UserEmail}, Location);
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
						co_await fLaunchGit({"remote", "set-url", RemoteName, Remote.m_URL}, Location);
						continue;
					}
					fOutputInfo(EOutputType_Normal, "Adding remote '{}={}'"_f << RemoteName << Remote.m_URL);
					co_await fLaunchGit({"remote", "add", RemoteName, Remote.m_URL}, Location);

					if (_BuildSystem.f_GetGenerateOptions().m_bForceUpdateRemotes)
						co_await fLaunchGit({"fetch", "--tags", "--force", RemoteName}, Location);
					else
						co_await fLaunchGit({"fetch", "--tags", RemoteName}, Location);
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
						[&]() -> TCFuture<void>
						{
							co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

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
									co_await fLaunchGit({"checkout", "-f", "-B", _Repo.m_DefaultBranch, ConfigHash}, Location);
									co_await fLaunchGit({"branch", "-u", "origin/{}"_f << _Repo.m_DefaultBranch}, Location);
									co_await fLaunchGit({"clean", "-fd"}, Location);
									if (_Repo.m_bUpdateSubmodules)
										co_await fLaunchGit({"submodule", "update", "--init"}, Location);

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

									CStr History = co_await fLaunchGit({"log", "-p", "origin/{}"_f << Repo.m_DefaultBranch, "--", _Repo.m_ConfigFile}, Repo.m_Location);
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
										co_await fLaunchGit({"checkout", "-f", "-B", _Repo.m_DefaultBranch, ConfigHash}, Location); // Recover from detached head
									}
									else
									{
										fOutputInfo(EOutputType_Warning, "Resetting to {}"_f << ConfigHash);
										co_await fLaunchGit({"reset", "--hard", ConfigHash}, Location);
									}
									if (_Repo.m_bUpdateSubmodules)
										co_await fLaunchGit({"submodule", "update", "--init"}, Location);
								}
								else if (Action == EHandleRepositoryAction_Rebase)
								{
									fOutputInfo(EOutputType_Normal, "Rebasing on top of {}"_f << ConfigHash);

									TCSet<CStr> AllConfigFiles;
									for (auto &pRepository : _AllRepositories)
										AllConfigFiles[pRepository->m_ConfigFile];

									auto fResolveConflicts = [&](CStr const &_ConflictingFiles) -> TCFuture<bool>
										{
											co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

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
			o_StateHandler.f_AddGitIgnore(_Repo.m_StateFile, _BuildSystem);

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

		TCVector<TCMap<CStr, CReposLocation>> fg_GetRepos(CBuildSystem &_BuildSystem, CBuildSystemData &_Data)
		{
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
					Repo.m_URL = URL;
					Repo.m_DefaultBranch = DefaultBranch;
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

					for (auto &Remote : Remotes.f_Get().f_Array())
					{
						CStr Name = Remote[gc_ConstString_Name].f_String();
						if (Repo.m_Remotes.f_FindEqual(Name))
							_BuildSystem.fs_ThrowError(PropertyInfoRemotes, ChildEntityData.m_Position, fg_Format("Same remote '{}' specified multiple times", Name));

						CStr URL = Remote[gc_ConstString_URL].f_String();

						auto &OutRemote = Repo.m_Remotes[Name];
						OutRemote.m_URL = URL;

						if (auto pValue = Remote.f_GetMember(gc_ConstString_Write))
							OutRemote.m_bCanPush = pValue->f_Boolean();

						if (auto pValue = Remote.f_GetMember(gc_ConstString_DefaultBranch))
							OutRemote.m_DefaultBranch = pValue->f_String();

						for (auto &Wildcard : NoPushRemotes)
						{
							if (fg_StrMatchWildcard(Name.f_GetStr(), Wildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							{
								OutRemote.m_bCanPush = false;
								break;
							}
						}
					}
					Repo.m_Position = ChildEntityData.m_Position;

					if (Repo.m_URL.f_IsEmpty())
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
					if (Repo.m_DefaultBranch.f_IsEmpty())
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

	TCFuture<CBuildSystem::ERetry> CBuildSystem::fp_HandleRepositories(TCMap<CPropertyKey, CEJSONSorted> const &_Values)
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

		f_InitEntityForEvaluation(mp_Data.m_RootEntity, _Values);
		f_ExpandRepositoryEntities(mp_Data);

		if (mp_GenerateOptions.m_bSkipUpdate)
			co_return ERetry_None;

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

		mint MaxRepoWidth = 0;

		auto LastSeenRepositories = StateHandler.f_GetLastSeenRepositories();
		for (auto &RepoName : LastSeenRepositories)
			MaxRepoWidth = fg_Max(MaxRepoWidth, (mint)RepoName.f_GetLen());

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

					MaxRepoWidth = fg_Max(MaxRepoWidth, (mint)RepoName.f_GetLen());
				}
			}
		}

		CGitLaunches Launches{f_GetBaseDir(), "Check repository status", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		if (!mp_FileActors.f_IsConstructed())
		{
			mp_FileActors.f_ConstructFunctor
				(
					[]
					{
						return fg_Construct(fg_Construct(), "File actor");
					}
				)
			;
		}

		for (auto &Repos : ReposOrdered)
		{
			auto FileActor = *mp_FileActors;
			co_await fg_ParallelForEach
				(
					Repos
					, [&, FileActor](auto &_Repos) mutable -> TCFuture<void>
					{
						co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);
						co_await f_CheckCancelled();

						CStr ReposDirectory = _Repos.f_GetPath();
						CStr LockFileName = ReposDirectory + "/RepoLock.MRepoState";

						auto pLockFile = co_await
							(
								g_Dispatch(FileActor) / [&]()
								{
									CFile::fs_CreateDirectory(ReposDirectory);
									StateHandler.f_AddGitIgnore(ReposDirectory + "/RepoLock.MRepoState", *this);
									TCUniquePointer<CLockFile> pLockFile = fg_Construct(LockFileName);

									if (mp_bDebugFileLocks)
										f_OutputConsole("{} File lock: {}\n"_f << pLockFile.f_Get() << LockFileName, true);

									pLockFile->f_LockWithException(5.0*60.0);

									return fg_Move(pLockFile);
								}
							)
						;

						if (mp_bDebugFileLocks)
							f_OutputConsole("{} File locked: {}\n"_f << pLockFile.f_Get() << LockFileName, true);

						auto CleanupLock = g_OnScopeExitActor(FileActor) / [&, pLockFile = fg_Move(pLockFile), fOutputConsole = mp_fOutputConsole]() mutable -> TCFuture<void>
							{
								try
								{
									pLockFile.f_Clear();
									if (mp_bDebugFileLocks && fOutputConsole)
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
								, [&](auto &_Repo) -> TCFuture<void>
								{
									co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);
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
				CJSONSorted StateJson;

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

				CEJSONSorted StateFile;

				auto &SeenRepositoriesJSON = StateFile["SeenRepositories"];
				SeenRepositoriesJSON = EJSONType_Object;
				for (auto &LastSeen : SeenRepositories)
					SeenRepositoriesJSON[LastSeen] = 1;

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
