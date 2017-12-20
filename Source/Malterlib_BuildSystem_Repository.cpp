// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Repository.h"
#include <Mib/Process/ProcessLaunch>
#include <Mib/Process/ProcessLaunchActor>
#include <Mib/Encoding/EJSON>

namespace NMib::NBuildSystem
{
	namespace
	{
		TCAggregate<CMutual> g_SubmoduleAddLock = {DAggregateInit};
	}

	namespace NRepository
	{
		void CStateHandler::f_SetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Hash)
		{
			DLock(mp_Lock);
			auto &ConfigFile = mp_NewConfigFiles[_FileName];
			ConfigFile.m_Configs[_RepoPath].m_Hash = _Hash;

			if (auto *pFile = mp_ConfigFiles.f_FindEqual(_FileName))
				ConfigFile.m_LineEndings = pFile->m_LineEndings;
		}

		CStr CStateHandler::f_GetHash(CStr const &_FileName, CStr const &_RepoPath)
		{
			DLock(mp_Lock);
			auto &ConfigFile = fp_GetConfigFile(_FileName);
			auto *pConfig = ConfigFile.m_Configs.f_FindEqual(_RepoPath);
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
				ConfigFile.m_Configs[CFile::fs_GetExpandedPath(Child.f_GetName(), BasePath)].m_Hash = Child.f_GetThisValue();

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

		bool fg_HandleRepository
			(
			 	CStr const &_ReposDirectory
			 	, CRepository const &_Repo
			 	, CStateHandler &o_StateHandler
			 	, CBuildSystem const &_BuildSystem
			 	, TCMap<CStr, CRepository const *> const &_AllRepositories
			)
		{
			CStr Location = _ReposDirectory + "/" + _Repo.f_GetName();
			
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
						DMibError("Failed to ask git question: {}"_f << StdErr);
					return ExitCode == 0;
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

			CStr ConfigHash = o_StateHandler.f_GetHash(_Repo.m_ConfigFile, Location);

			auto fOutputInfo = [&](EOutputType _OutputType, CStr const &_Info)
				{
					ch8 const *pRepoColor = CColors::mc_StatusNormal;
					switch (_OutputType)
					{
					case EOutputType_Normal: pRepoColor = CColors::mc_StatusNormal; break;
					case EOutputType_Warning: pRepoColor = CColors::mc_StatusWarning; break;
					case EOutputType_Error: pRepoColor = CColors::mc_StatusError; break;
					}

					CStr RepoName = CFile::fs_MakePathRelative(_Repo.m_Location, _BuildSystem.f_GetBaseDir());

					CStr ReplacedRepo = RepoName.f_Replace("/", "{}{}/{}"_f << CColors::mc_Default << DColor_256(250) << pRepoColor ^ 1);
					DMibConOut2
						(
							"{}{}{}   {}\n"
							, pRepoColor
							, ReplacedRepo
							, CColors::mc_Default
							, _Info
						)
					;
				}
			;


			if (!CFile::fs_FileExists(Location))
			{
				fOutputInfo(EOutputType_Normal, "Adding external repository at: {}"_f << Location);
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
			else
			{
				CStr GitRoot = fg_GetGitRoot(Location);
				if (GitRoot.f_IsEmpty() || _Repo.m_Submodule != "true")
					o_StateHandler.f_AddGitIgnore(Location, _BuildSystem);
			}
			
			CStr CurrentHash = o_StateHandler.f_GetHash(_Repo.m_StateFile, Location);

			bool bForceReset = _BuildSystem.f_GetEnvironmentVariable("MalterlibRepositoryHardReset", "") == "true";

			if
				(
					!ConfigHash.f_IsEmpty()
				 	&&
				 	(
					 	CurrentHash != ConfigHash
					 	|| (bForceReset && fg_GetGitHeadHash(Location, _Repo.m_Position) != ConfigHash)
					)
				 	&& !fg_IsSubmodule(Location)
				)
			{
				fOutputInfo(EOutputType_Normal, "Reconciling against hash '{}'"_f << ConfigHash);

				try
				{
					fLaunchGit({"fetch", "--all"}, Location);
					bool bShouldReset = false;
					bool bIsNewer = false;
					if (!bForceReset)
					{
						do
						{
							if (fLaunchGitQuestion({"merge-base", "--is-ancestor", CurrentHash, ConfigHash}, Location))
								break;

							if (fLaunchGitQuestion({"merge-base", "--is-ancestor", ConfigHash, CurrentHash}, Location))
							{
								bIsNewer = true;
								break;
							}

							if (!fLaunchGit({"status", "--porcelain"}, Location).f_IsEmpty())
								break;

							auto *pRepo = _AllRepositories.f_FindLargestLessThanEqual(_Repo.m_ConfigFile);
							if (!pRepo || !_Repo.m_ConfigFile.f_StartsWith((*pRepo)->m_Location))
							{
								fOutputInfo(EOutputType_Warning, "Cloud not find containing repo for config file '{}'\n"_f << _Repo.m_ConfigFile);
								break;
							}

							auto &Repo = **pRepo;

							CStr RelativeLocation = CFile::fs_MakePathRelative(_Repo.m_Location, Repo.m_Location);

							CStr History = fLaunchGit({"log", "-p", "origin/{}"_f << Repo.m_DefaultBranch, "--", _Repo.m_ConfigFile}, Repo.m_Location);
							CStr FilteredHistory;
							CStr LookFor = "+{} "_f << RelativeLocation;

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
										bIsNewer = true;
										break;
									}
									bFoundConfig = true;
								}
								if (Commit == CurrentHash)
								{
									if (bFoundConfig)
									{
										bShouldReset = true; // Our current commit is in origin history, it should be safe to do a reset
										break;
									}
									bFoundCurrent = true;
								}
							}
						}
						while (false)
							;
					}

					if (bForceReset)
					{
						fOutputInfo(EOutputType_Warning, "Force Resetting");
						fLaunchGit({"checkout", "-f", "-B", _Repo.m_DefaultBranch, ConfigHash}, Location);
						fLaunchGit({"clean", "-fd"}, Location);
					}
					else if (bShouldReset)
					{
						fOutputInfo(EOutputType_Warning, "Resetting");
						fLaunchGit({"reset", "--hard", ConfigHash}, Location);
					}
					else if (!bIsNewer)
					{
						fOutputInfo(EOutputType_Normal, "Rebasing");

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
						}
					}
				}
				catch (CException const &_Exception)
				{
					fOutputInfo(EOutputType_Error, "Reconcile error: "_f << _Exception.f_GetErrorStr().f_Trim());
					CBuildSystem::fs_ThrowError(_Repo.m_Position, "Failed to reconcile hash '{}': {}"_f << ConfigHash << _Exception.f_GetErrorStr().f_Trim());
				}
				bChanged = true;
			}
			
			CStr GitHeadHash = fg_GetGitHeadHash(Location, _Repo.m_Position);
			o_StateHandler.f_SetHash(_Repo.m_StateFile, Location, GitHeadHash);
			o_StateHandler.f_SetHash(_Repo.m_ConfigFile, Location, GitHeadHash);
			o_StateHandler.f_AddGitIgnore(_Repo.m_StateFile, _BuildSystem);

			auto CurrentRemotes = fg_GetGitRemotes(Location, _Repo.m_Position);

			if (!_Repo.m_URL.f_IsEmpty())
			{
				auto pCurrentRemote = CurrentRemotes.f_FindEqual("origin");
				if (pCurrentRemote && *pCurrentRemote != _Repo.m_URL)
				{
					fOutputInfo(EOutputType_Normal, "Changing origin URL 'origin={}'"_f << _Repo.m_URL);
					fLaunchGit({"remote", "set-url", "origin", _Repo.m_URL}, Location);
				}
			}
			
			if (!_Repo.m_Remotes.f_IsEmpty())
			{
				for (auto iRemote = _Repo.m_Remotes.f_GetIterator(); iRemote; ++iRemote)
				{
					auto &RemoteName = iRemote.f_GetKey();
					auto &RemoteURL = *iRemote;
					auto pCurrentRemote = CurrentRemotes.f_FindEqual(RemoteName);
					if (pCurrentRemote)
					{
						if (*pCurrentRemote == RemoteURL)
							continue;
						fOutputInfo(EOutputType_Normal, "Changing remote URL '{}={}'"_f << RemoteName << RemoteURL);
						fLaunchGit({"remote", "set-url", RemoteName, RemoteURL}, Location);
						continue;
					}				
					fOutputInfo(EOutputType_Normal, "Adding remote '{}={}'"_f << RemoteName << RemoteURL);
					fLaunchGit({"remote", "add", RemoteName, RemoteURL}, Location);
					fLaunchGit({"fetch", RemoteName}, Location);
				}
			}
				
			return bChanged;
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
				CStr Remotes = _BuildSystem.f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Remotes");
				while (!Remotes.f_IsEmpty())
				{
					CStr RemoteString = fg_GetStrSep(Remotes, ";");
					if (RemoteString.f_IsEmpty())
						continue;
					CStr Name = fg_GetStrSep(RemoteString, "=");
					if (Repo.m_Remotes.f_FindEqual(Name))
						_BuildSystem.fs_ThrowError(ChildEntity.m_Position, fg_Format("Same remote '{}' specified multiple times", Name));
					Repo.m_Remotes[Name] = RemoteString;
				}
				Repo.m_Position = ChildEntity.m_Position;

				if (Repo.m_URL.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.URL");
				if (Repo.m_ConfigFile.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.ConfigFile");
				if (Repo.m_StateFile.f_IsEmpty())
					_BuildSystem.fs_ThrowError(ChildEntity.m_Position, "You have to specify Repository.StateFile");
				if (Repo.m_ConfigFile == Repo.m_StateFile)
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

				for (auto &RepoLocation : Repos)
				{
					for (auto &Repo : RepoLocation.m_Repositories)
					{
						if (AddedRepos.f_FindEqual(Repo.m_Location))
							continue;

						bAllAdded = false;

						auto *pDependency = RepoRoots.f_FindLargestLessThanEqual(Repo.m_ConfigFile);
						if (pDependency)
						{
							auto &DependencyRoot = RepoRoots.fs_GetKey(*pDependency);
							if (Repo.m_ConfigFile.f_StartsWith(DependencyRoot))
							{
								if (!AddedRepos.f_FindEqual(DependencyRoot))
								{
									LeftFilePos = Repo.m_Position;
									continue;
								}
							}
						}

						if (!pTheseRepos)
							pTheseRepos = &ReposOrdered.f_Insert();

						(*pTheseRepos)[RepoLocation.f_GetPath()].m_Repositories[Repo.f_GetName()] = Repo;
						ToAddRepos[Repo.m_Location];
						bDoneSomething = true;
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

	CBuildSystem::ERetry CBuildSystem::fp_HandleRepositories(TCMap<CPropertyKey, CStr> const &_Values, bool _bSkipRepoUpdate)
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

		CStateHandler StateHandler;

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
									if (fg_HandleRepository(ReposDirectory, _Repo, StateHandler, *this, AllRepositories))
									{
										bChanged.f_Exchange(true);
										CStr RelativePath = CFile::fs_MakePathRelative(_Repo.m_Location, f_GetBaseDir());
										if (RelativePath.f_StartsWith("Binaries/"))
											bBinariesChange.f_Exchange(true);
									}
								}
							)
						;
					}
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
				Registry.f_SetValueNoPath(CFile::fs_MakePathRelative(iConfig.f_GetKey(), BasePath), iConfig->m_Hash);
			
			CStr FileName = iFile.f_GetKey();
			CStr FileContents = Registry.f_GenerateStr().f_Replace(DMibNewLine, iFile->m_LineEndings);

			bool bWasCreated = false;
			if (!f_AddGeneratedFile(FileName, FileContents, "", bWasCreated, false))
				fs_ThrowError({}, CStr::CFormat("File '{}' already generated with other contents") << FileName);

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(FileContents), false);
				f_WriteFile(FileData, FileName);
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
