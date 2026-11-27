// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository_PerforceHelpers.h"

#include <Mib/File/File>
#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem::NRepository
{
	using namespace NStr;

	TCSet<CStr> fg_CollectPerforceRootConfigFiles(TCMap<CStr, CRepository *> const &_RepositoryByLocation, CStr const &_BaseDir)
	{
		TCSet<CStr> GitRepoLocations;
		for (auto &pRepository : _RepositoryByLocation)
		{
			if (pRepository->m_Location != _BaseDir)
				GitRepoLocations[pRepository->m_Location];
		}

		TCSet<CStr> PerforceRootConfigFiles;
		for (auto &pRepository : _RepositoryByLocation)
		{
			if (pRepository->m_ConfigFile.f_IsEmpty())
				continue;
			CStr ConfigDir = CFile::fs_GetPath(pRepository->m_ConfigFile);
			bool bInsideGitRepo = false;
			for (auto &GitLocation : GitRepoLocations)
			{
				if (CFile::fs_GetCommonPath(ConfigDir, GitLocation) == GitLocation)
				{
					bInsideGitRepo = true;
					break;
				}
			}
			if (!bInsideGitRepo)
				PerforceRootConfigFiles[pRepository->m_ConfigFile];
		}
		return PerforceRootConfigFiles;
	}

	namespace
	{
		TCFuture<CPerforceClient::CConnectionInfo> fg_ReadPerforceRootConnection(CStr _BaseDir, TCMap<CStr, CStr> _Environment, NPerforce::CPerforceIgnore::CRegistrySettings _Registry)
		{
			co_return co_await
				(
					g_Dispatch(fg_BlockingActor()) / [_BaseDir, _Environment, _Registry]() -> CPerforceClient::CConnectionInfo
					{
						auto Settings = NPerforce::CPerforceIgnore::fs_GetSettings(_BaseDir, _Environment, _Registry, _BaseDir);
						return CPerforceClient::fs_GetConnectionInfo(_BaseDir, Settings, _Environment);
					}
				)
			;
		}
	}

	// The connection of p4 run in the Perforce root, with the settings of the generation environment that made it a root. The
	// settings are read from the root's .p4config and the enviro file on a blocking actor.
	TCFuture<CPerforceClient::CConnectionInfo> fg_PerforceRootConnection(CBuildSystem const &_BuildSystem)
	{
		return fg_ReadPerforceRootConnection(_BuildSystem.f_GetBaseDir(), _BuildSystem.f_GetGenerationEnvironment(), _BuildSystem.f_GetPerforceRegistry());
	}

	// The spelling of the Perforce root, the connection's working directory, in _Client's workspace, whose client spec can spell it
	// otherwise, as when the root is reached through a symbolic link.
	NPerforce::CPerforceRootSpelling fg_PerforceRootSpelling(CPerforceClientThrow &_Client)
	{
		return NPerforce::CPerforceRootSpelling::fs_Find(_Client.f_GetConnectionInfo().m_WorkingDirectory, _Client.f_GetClientRoot());
	}

	// Whether a config file in the workspace holds the submitted content. p4 writes a text file with the line ends of the workspace's
	// LineEnd, CRLF for win, and for local on Windows, and CR for mac, while the depot holds LF, so the line ends don't count.
	bool fg_PerforceConfigContentEquals(CStr const &_WorkspaceContent, CStr const &_SubmittedContent)
	{
		return fg_NormalizeLineEndings(_WorkspaceContent) == fg_NormalizeLineEndings(_SubmittedContent);
	}

	// Logs in _Client, made with the root's connection (fg_PerforceRootConnection), on a blocking actor. The depot paths are aligned
	// with _ConfigFilePaths, empty for a file without a submitted revision, so callers tell a new file by its index. Throws where the
	// login or a file's lookup fails. o_pSpelling, when given, is the root's spelling in the workspace, for the other paths given p4 on the connection.
	TCVector<CStr> fg_PerforceConnectAndResolveDepotPaths(TCVector<CStr> const &_ConfigFilePaths, CPerforceClientThrow &_Client, NPerforce::CPerforceRootSpelling *o_pSpelling)
	{
		_Client.f_Login(CStr(), _Client.f_GetConnectionInfo().m_WorkingDirectory);
		auto Spelling = fg_PerforceRootSpelling(_Client);
		if (o_pSpelling)
			*o_pSpelling = Spelling;

		TCVector<CStr> DepotPaths;
		DepotPaths.f_SetLen(_ConfigFilePaths.f_GetLen());
		for (umint iConfigFile = 0; iConfigFile < _ConfigFilePaths.f_GetLen(); ++iConfigFile)
		{
			// The depot file of the file's fstat record where it has a
			// submitted head revision, so the path and the revision come from
			// one record. Callers use an empty depot path as "this file is not
			// yet submitted in Perforce": files the depot or the view doesn't
			// have, and files that are only opened for pending add, which have
			// no head revision, flow through the "new file" path on the caller
			// side, where `p4 filelog` / f_GetFileRevisions would otherwise
			// error because they have no submitted history.
			auto Stats = _Client.f_DepotFileStats(Spelling.f_ServerFileName(_ConfigFilePaths[iConfigFile]));
			if (Stats.m_HeadRev != 0)
				DepotPaths[iConfigFile] = Stats.m_DepotFile;
		}

		return DepotPaths;
	}

	TCFuture<CStr> fg_PerforceGetLatestChangelist(TCVector<CStr> _ConfigFilePaths, CPerforceClient::CConnectionInfo _Connection)
	{
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [_ConfigFilePaths, _Connection]() -> CStr
				{
					CPerforceClientThrow Client(_Connection);
					auto DepotPaths = fg_PerforceConnectAndResolveDepotPaths(_ConfigFilePaths, Client);

					TCVector<CStr> TrackedDepotPaths;
					for (auto &DepotPath : DepotPaths)
					{
						if (!DepotPath.f_IsEmpty())
							TrackedDepotPaths.f_Insert(DepotPath);
					}

					if (TrackedDepotPaths.f_IsEmpty())
						return CStr::fs_ToStr(uint32(0));

					auto Revisions = Client.f_GetFileRevisions(TrackedDepotPaths);

					uint32 HighestCL = 0;
					for (auto &File : Revisions.m_Files)
					{
						for (auto &Rev : File.m_Revisions)
						{
							if (Rev.m_ChangeList > 0 && (uint32)Rev.m_ChangeList > HighestCL)
								HighestCL = (uint32)Rev.m_ChangeList;
						}
					}

					return CStr::fs_ToStr(HighestCL);
				}
			)
		;
	}

	TCFuture<TCTuple<CStr, CStr>> fg_PerforceGetConfigFileContents(CStr _ConfigFile, CStr _StartCL, CStr _EndCL, CPerforceClient::CConnectionInfo _Connection)
	{
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [_ConfigFile, _StartCL, _EndCL, _Connection]() -> TCTuple<CStr, CStr>
				{
					// An empty content is an absent snapshot: a workspace file that is missing, as one never submitted or a pending p4
					// delete leaves it, as well as a revision that isn't there. A read that fails throws.
					auto fReadWorkspace = [&]() -> CStr
						{
							if (!CFile::fs_FileExists(_ConfigFile))
								return {};

							return CFile::fs_ReadStringFromFile(_ConfigFile);
						}
					;

					CStr StartContent;
					CStr EndContent;

					if (_StartCL.f_IsEmpty())
						StartContent = fReadWorkspace();
					else
					{
						// The depot path the other lookups of the config file use. A file without a submitted revision has no
						// earlier content.
						CPerforceClientThrow Client(_Connection);
						if (CStr DepotPath = fg_PerforceConnectAndResolveDepotPaths({_ConfigFile}, Client)[0])
						{
							// A revision that isn't there has no content; a read that fails throws.
							if (auto Content = Client.f_GetTextFileContentsIfExists("{}@{}"_f << DepotPath << _StartCL))
								StartContent = fg_Move(*Content);

							if (!_EndCL.f_IsEmpty())
							{
								if (auto Content = Client.f_GetTextFileContentsIfExists("{}@{}"_f << DepotPath << _EndCL))
									EndContent = fg_Move(*Content);
							}
						}
					}

					if (_EndCL.f_IsEmpty())
						EndContent = fReadWorkspace();

					return {StartContent, EndContent};
				}
			)
		;
	}
}
