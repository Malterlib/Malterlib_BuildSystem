// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include <Mib/Process/ProcessLaunch>

namespace NMib::NBuildSystem
{
	namespace
	{
		struct CLocalGeneratorInteface : public CGeneratorInterface
		{
			bool f_GetBuiltin(CStr const &_Value, CStr &_Result) const override
			{
				return false;
			}
			CStr f_GetExpandedPath(CStr const &_Path, CStr const& _Base) const override
			{
				return CFile::fs_GetExpandedPath(_Path, _Base);
			}
			TCMap<CStr, CStr> f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const override
			{
				return NSys::fg_Process_GetEnvironmentVariables();
			}
		};
		
		struct CRepository
		{
			CStr const &f_GetName() const
			{
				return TCMap<CStr, CRepository>::fs_GetKey(*this);
			}
			
			CStr m_ConfigFile;
			CStr m_StateFile;
			CStr m_URL;
			CStr m_DefaultBranch;
			CStr m_Submodule;
			CStr m_SubmoduleName;
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
		};
		
		struct CStateHandler
		{
			void f_SetHash(CStr const &_FileName, CStr const &_RepoPath, CStr const &_Hash)
			{
				DLock(mp_Lock);
				mp_NewConfigFiles[_FileName].m_Configs[_RepoPath].m_Hash = _Hash;
			}

			CStr f_GetHash(CStr const &_FileName, CStr const &_RepoPath)
			{
				DLock(mp_Lock);
				auto &ConfigFile = fp_GetConfigFile(_FileName);
				auto *pConfig = ConfigFile.m_Configs.f_FindEqual(_RepoPath);
				if (!pConfig)
					return {};
				return pConfig->m_Hash;
			}
			
			TCMap<CStr, CConfigFile> const &f_GetNewFiles()
			{
				return mp_NewConfigFiles;
			}
			
			void f_AddGitIgnore(CStr const &_FileName, CBuildSystem const &_BuildSystem)
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
			
		private:
			CConfigFile const &fp_GetConfigFile(CStr const &_FileName)
			{
				DLock(mp_Lock);
				if (auto *pFile = mp_ConfigFiles.f_FindEqual(_FileName))
					return *pFile;
				auto &ConfigFile = mp_ConfigFiles[_FileName];
				CStr BasePath = CFile::fs_GetPath(_FileName);
				if (CFile::fs_FileExists(_FileName))
				{
					CRegistry_CStr Registry;
					Registry.f_ParseStr(CFile::fs_ReadStringFromFile(_FileName, true), _FileName);
					for (auto &Child : Registry.f_GetChildren())
						ConfigFile.m_Configs[CFile::fs_GetExpandedPath(Child.f_GetName(), BasePath)].m_Hash = Child.f_GetThisValue();
				}
				
				return ConfigFile;
			}
			
			CMutual mp_Lock;
			TCMap<CStr, CConfigFile> mp_ConfigFiles;
			TCMap<CStr, CConfigFile> mp_NewConfigFiles;
			TCSet<CStr> mp_GitIgnores;
		};
		
		CStr fg_GetGitRoot(CStr const &_Directory)
		{
			CStr CurrentDirectory = _Directory;
			while (!CurrentDirectory.f_IsEmpty())
			{
				if (CFile::fs_FileExists(CurrentDirectory + "/.git", EFileAttrib_Directory))
					return CurrentDirectory;
				CurrentDirectory = CFile::fs_GetPath(CurrentDirectory);
			}
			
			return {};
		}

		CStr fg_GetGitDataDir(CStr const &_GitRoot, CFilePosition const &_Position)
		{
			CStr GitDirectory = _GitRoot + "/.git";
			if (CFile::fs_FileExists(GitDirectory, EFileAttrib_File))
			{
				CStr FileContents = CFile::fs_ReadStringFromFile(GitDirectory, true).f_TrimRight("\n");
				if (!FileContents.f_StartsWith("gitdir: "))
					CBuildSystem::fs_ThrowError(_Position, fg_Format("Unsupported git directory. Expected 'gitdir: ' in '{}'", GitDirectory));
				GitDirectory = CFile::fs_GetExpandedPath(FileContents.f_Extract(8), _GitRoot);	
			}
			if (!CFile::fs_FileExists(GitDirectory, EFileAttrib_Directory))
				CBuildSystem::fs_ThrowError(_Position, fg_Format("Missing git directory: {}", GitDirectory));
			return GitDirectory; 
		}
		
		CStr fg_GetGitHeadHash(CStr const &_GitRoot, CFilePosition const &_Position)
		{
			CStr GitDirectory = fg_GetGitDataDir(_GitRoot, _Position);
			
			CStr HeadRef = CFile::fs_ReadStringFromFile(GitDirectory + "/HEAD", true).f_TrimRight("\n");
			if (HeadRef.f_StartsWith("ref: "))
				return CFile::fs_ReadStringFromFile(GitDirectory + "/" + HeadRef.f_Extract(5), true).f_TrimRight("\n");
			else
				return HeadRef;
		}
		
		TCMap<CStr, CStr> fg_GetGitRemotes(CStr const &_GitRoot, CFilePosition const &_Position)
		{
			CStr GitDirectory = fg_GetGitDataDir(_GitRoot, _Position);
			
			CStr Config = CFile::fs_ReadStringFromFile(GitDirectory + "/config", true);
			
			TCMap<CStr, CStr> Remotes;
			
			auto pParse = Config.f_GetStr();
			CStr LastRemote;
			while (*pParse)
			{
				fg_ParseWhiteSpace(pParse);
				if (fg_StrStartsWith(pParse, "[remote"))
				{
					pParse += 7;
					fg_ParseWhiteSpace(pParse);
					auto pStart = pParse;
					fg_ParseEscape<'\"'>(pParse, '\"');
					
					CStr RemoteName(pStart, pParse - pStart);
					LastRemote = fg_RemoveEscape<'\"'>(RemoteName);
				}
				else if (fg_StrStartsWith(pParse, "url =") && !LastRemote.f_IsEmpty())
				{
					pParse += 5;
					fg_ParseWhiteSpace(pParse);
					auto pStart = pParse;
					fg_ParseToEndOfLine(pParse);
					CStr URL(pStart, pParse - pStart);
					Remotes[LastRemote] = URL;
				}
				else if (*pParse == '[')
					LastRemote.f_Clear();
				fg_ParseToEndOfLine(pParse);
				fg_ParseEndOfLine(pParse);
			}
			
			Remotes.f_Remove("origin");
			
			return Remotes;
		}

		bool fg_IsSubmodule(CStr const &_GitRoot)
		{
			CStr GitDirectory = _GitRoot + "/.git";
			return CFile::fs_FileExists(GitDirectory, EFileAttrib_File);
		}
		
		bool fg_HandleRepository(CStr const &_ReposDirectory, CRepository const &_Repo, CStateHandler &o_StateHandler, CBuildSystem const &_BuildSystem)
		{
			CStr Location = _ReposDirectory + "/" + _Repo.f_GetName();
			
			CDisableExceptionTraceScope DisableTrace;
			
			bool bChanged = false;

			auto fLaunchGit = [&](TCVector<CStr> const &_Params, CStr const &_WorkingDir = {})
				{
					CProcessLaunchParams Params{_WorkingDir};
#ifdef DPlatformFamily_OSX
					Params.m_Environment["PATH"] = "/opt/local/bin:" + CStr(NSys::fg_Process_GetEnvironmentVariable(CStr("PATH")));
#endif
					Params.m_bShowLaunched = false;
					CProcessLaunch::fs_LaunchTool("git", _Params, Params);
				}
			;

			
			if (!CFile::fs_FileExists(Location))
			{
				DMibConOut("Adding external repository: {}{\n}", Location);
				CStr GitRoot = fg_GetGitRoot(Location);
				if (GitRoot.f_IsEmpty() || _Repo.m_Submodule != "true")
				{
					try
					{
						fLaunchGit({"clone", _Repo.m_URL, Location});
						fLaunchGit({"checkout", _Repo.m_DefaultBranch}, Location);
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
						fLaunchGit({"submodule", "add", "-b", _Repo.m_DefaultBranch, "--name", _Repo.m_SubmoduleName, _Repo.m_URL, RelativeLocation}, GitRoot);
						CProcessLaunch::fs_LaunchTool
							(
								"git"
								, {"config", "-f", ".gitmodules", fg_Format("submodule.{}.fetchRecurseSubmodules", _Repo.m_SubmoduleName), "on-demand"}
								, GitRoot
							)
						;
						bChanged = true;
					}
					catch (CException const &_Exception)
					{
						CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to add submodule repository: {}", _Exception));
					}
				}
			}
			
			CStr CurrentHash = o_StateHandler.f_GetHash(_Repo.m_StateFile, Location);
			CStr ConfigHash = o_StateHandler.f_GetHash(_Repo.m_ConfigFile, Location);
			
			if (CurrentHash != ConfigHash && !ConfigHash.f_IsEmpty() && !fg_IsSubmodule(Location))
			{
				DMibConOut2("Checking out specific hash '{}' at '{}'{\n}", ConfigHash, Location);
				try
				{
					CProcessLaunch::fs_LaunchTool
						(
							"git"
							, {"fetch"}
							, Location
						)
					;
					CProcessLaunch::fs_LaunchTool
						(
							"git"
							, {"checkout", ConfigHash}
							, Location
						)
					;
				}
				catch (CException const &_Exception)
				{
					CBuildSystem::fs_ThrowError(_Repo.m_Position, fg_Format("Failed to checkout hash '{}' for repository: {}", ConfigHash, _Exception));
				}
				bChanged = true;
			}
			
			CStr GitHeadHash = fg_GetGitHeadHash(Location, _Repo.m_Position);
			o_StateHandler.f_SetHash(_Repo.m_StateFile, Location, GitHeadHash);
			o_StateHandler.f_SetHash(_Repo.m_ConfigFile, Location, GitHeadHash);
			o_StateHandler.f_AddGitIgnore(_Repo.m_StateFile, _BuildSystem);
			
			if (!_Repo.m_Remotes.f_IsEmpty())
			{
				auto CurrentRemotes = fg_GetGitRemotes(Location, _Repo.m_Position);
				for (auto iRemote = _Repo.m_Remotes.f_GetIterator(); iRemote; ++iRemote)
				{
					auto &RemoteName = iRemote.f_GetKey();
					auto &RemoteURL = *iRemote;
					auto pCurrentRemote = CurrentRemotes.f_FindEqual(RemoteName);
					if (pCurrentRemote)
					{
						if (*pCurrentRemote == RemoteURL)
							continue;
						DMibConOut2("Changing remote URL '{}={}' at '{}'{\n}", RemoteName, RemoteURL, Location);
						fLaunchGit({"remote", "set-url", RemoteName, RemoteURL}, Location);
						continue;
					}				
					DMibConOut2("Adding remote '{}={}' at '{}'{\n}", RemoteName, RemoteURL, Location);
					fLaunchGit({"remote", "add", RemoteName, RemoteURL}, Location);
					fLaunchGit({"fetch", RemoteName}, Location);
				}
			}
				
			return bChanged;
		}
	}
	bool CBuildSystem::fp_HandleRepositories() const
	{
		CLocalGeneratorInteface LocalInterface;
		auto pOldInterface = fg_Move(mp_GeneratorInterface);
		auto Cleanup = g_OnScopeExit > [&]
			{
				mp_GeneratorInterface = fg_Move(pOldInterface);
			}
		;
		mp_GeneratorInterface = &LocalInterface;
		
		TCMap<CStr, CReposLocation> Repos;
		
		for (auto iChild = mp_Data.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
		{
			auto &ChildEntity = *iChild;
			if (ChildEntity.m_Key.m_Type != EEntityType_Repository)
				continue;
			if (!f_EvalCondition(ChildEntity, ChildEntity.m_Condition))
				continue;

			CStr Location = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Location");
			
			if (Location.f_IsEmpty())
				fsp_ThrowError(ChildEntity.m_Position, "You have to specify Repository.Location");
			
			CStr ReposDirectory = CFile::fs_GetPath(Location); 
			
			auto &ReposLocation = Repos[ReposDirectory];
			
			auto RepoMap = ReposLocation.m_Repositories(CFile::fs_GetFile(Location));
			
			if (!RepoMap.f_WasCreated())
				fsp_ThrowError(ChildEntity.m_Position, fg_Format("Duplicate repository location: {}", Location));
			
			auto &Repo = *RepoMap;
			Repo.m_ConfigFile = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "ConfigFile");
			Repo.m_StateFile = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "StateFile");
			Repo.m_URL = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "URL");
			Repo.m_DefaultBranch = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "DefaultBranch");
			Repo.m_Submodule = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Submodule");
			Repo.m_SubmoduleName = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "SubmoduleName");
			CStr Remotes = f_EvaluateEntityProperty(ChildEntity, EPropertyType_Repository, "Remotes");
			while (!Remotes.f_IsEmpty())
			{
				CStr RemoteString = fg_GetStrSep(Remotes, ";");
				if (RemoteString.f_IsEmpty())
					continue;
				CStr Name = fg_GetStrSep(RemoteString, "=");
				if (Repo.m_Remotes.f_FindEqual(Name))
					fsp_ThrowError(ChildEntity.m_Position, fg_Format("Same remote '{}' specified multiple times", Name));
				Repo.m_Remotes[Name] = RemoteString; 
			}
			Repo.m_Position = ChildEntity.m_Position;
			
			if (Repo.m_URL.f_IsEmpty())
				fsp_ThrowError(ChildEntity.m_Position, "You have to specify Repository.URL");
			if (Repo.m_ConfigFile.f_IsEmpty())
				fsp_ThrowError(ChildEntity.m_Position, "You have to specify Repository.ConfigFile");
			if (Repo.m_StateFile.f_IsEmpty())
				fsp_ThrowError(ChildEntity.m_Position, "You have to specify Repository.StateFile");
			if (Repo.m_ConfigFile == Repo.m_StateFile)
				fsp_ThrowError(ChildEntity.m_Position, "You have to specify Repository.ConfigFile and Repository.StateFile must not be same file");
			if (Repo.m_DefaultBranch.f_IsEmpty())
				fsp_ThrowError(ChildEntity.m_Position, "You have to specify Repository.DefaultBranch");
			if (Repo.m_Submodule == "true" && Repo.m_SubmoduleName.f_IsEmpty())
				fsp_ThrowError(ChildEntity.m_Position, "You have to specify Repository.SubmoduleName");
		}
		
		TCAtomic<bool> bChanged;
		
		CStateHandler StateHandler;
		
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
								if (fg_HandleRepository(ReposDirectory, _Repo, StateHandler, *this))
									bChanged.f_Exchange(true);
							}
						)
					;
				}
			)
		;
		
		for (auto iFile = StateHandler.f_GetNewFiles().f_GetIterator(); iFile; ++iFile)
		{
			CRegistry_CStr Registry;
			
			CStr BasePath = CFile::fs_GetPath(iFile.f_GetKey());
			
			for (auto iConfig = iFile->m_Configs.f_GetIterator(); iConfig; ++iConfig)
				Registry.f_SetValueNoPath(CFile::fs_MakePathRelative(iConfig.f_GetKey(), BasePath), iConfig->m_Hash);
			
			CStr FileName = iFile.f_GetKey();
			CStr FileContents = Registry.f_GenerateStr().f_Replace(DMibNewLine, "\n");

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
		
		return bChanged.f_Load();
	}
}
