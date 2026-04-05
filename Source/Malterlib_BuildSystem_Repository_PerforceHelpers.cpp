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

	TCVector<CStr> fg_PerforceConnectAndResolveDepotPaths(TCVector<CStr> const &_ConfigFilePaths, CStr const &_BaseDir, CPerforceClientThrow &o_Client)
	{
		if (!CPerforceClientThrow::fs_GetFromP4Config(_BaseDir + "/.", o_Client))
			DMibError("Failed to connect to Perforce");

		TCVector<CStr> DepotPaths;
		DepotPaths.f_SetLen(_ConfigFilePaths.f_GetLen());
		for (umint i = 0; i < _ConfigFilePaths.f_GetLen(); ++i)
		{
			// `p4 where` (used by f_GetDepotPath) just maps a client-view
			// path to its depot spelling; it succeeds for any path that
			// falls inside the client's view whether or not the file has
			// actually been `p4 add`ed. Callers use an empty depot path as
			// "this file is not yet submitted in Perforce", so we gate on
			// f_FileExistsInDepot (via `p4 fstat`) — which is true only
			// for files with a committed head revision. Files that are
			// only opened for pending add get an empty depot path here
			// and flow through the "new file" path on the caller side;
			// feeding them to `p4 filelog` / f_GetFileRevisions would
			// otherwise error because they have no submitted history.
			// A stray `p4 add` on such files during a rerun is a silent
			// no-op (Perforce refuses to re-add an already-opened file).
			CStr DepotPath;
			if
				(
					o_Client.f_NoThrow().f_GetDepotPath(_ConfigFilePaths[i], DepotPath)
					&& o_Client.f_NoThrow().f_FileExistsInDepot(_ConfigFilePaths[i])
				)
			{
				DepotPaths[i] = fg_Move(DepotPath);
			}
		}

		return DepotPaths;
	}

	TCFuture<CStr> fg_PerforceGetLatestChangelist(TCVector<CStr> _ConfigFilePaths, CStr _BaseDir)
	{
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [_ConfigFilePaths, _BaseDir]() -> CStr
				{
					CPerforceClientThrow Client;
					auto DepotPaths = fg_PerforceConnectAndResolveDepotPaths(_ConfigFilePaths, _BaseDir, Client);

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

	TCFuture<TCTuple<CStr, CStr>> fg_PerforceGetConfigFileContents(CStr _ConfigFile, CStr _StartCL, CStr _EndCL)
	{
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [_ConfigFile, _StartCL, _EndCL]() -> TCTuple<CStr, CStr>
				{
					CStr StartContent;
					CStr EndContent;

					if (_StartCL.f_IsEmpty())
						StartContent = CFile::fs_ReadStringFromFile(_ConfigFile);
					else
					{
						CPerforceClientThrow Client;
						if (!CPerforceClientThrow::fs_GetFromP4Config(_ConfigFile, Client))
							DMibError("Failed to connect to Perforce");

						CStr DepotPath;
						if (!Client.f_NoThrow().f_GetDepotPath(_ConfigFile, DepotPath))
							DMibError("Failed to get depot path for: {}"_f << _ConfigFile);

						Client.f_NoThrow().f_GetTextFileContents("{}@{}"_f << DepotPath << _StartCL, StartContent);

						if (!_EndCL.f_IsEmpty())
							Client.f_NoThrow().f_GetTextFileContents("{}@{}"_f << DepotPath << _EndCL, EndContent);
					}

					if (_EndCL.f_IsEmpty())
						EndContent = CFile::fs_ReadStringFromFile(_ConfigFile);

					return {StartContent, EndContent};
				}
			)
		;
	}
}
