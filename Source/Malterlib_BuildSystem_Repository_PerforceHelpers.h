// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Perforce/FileNames>
#include <Mib/Perforce/Wrapper>

namespace NMib::NBuildSystem::NRepository
{
	// Collect config files that are NOT inside any git sub-repo (i.e. managed by Perforce root)
	TCSet<CStr> fg_CollectPerforceRootConfigFiles(TCMap<CStr, CRepository *> const &_RepositoryByLocation, CStr const &_BaseDir);

	TCFuture<CPerforceClient::CConnectionInfo> fg_PerforceRootConnection(CBuildSystem const &_BuildSystem);
	TCVector<CStr> fg_PerforceConnectAndResolveDepotPaths(TCVector<CStr> const &_ConfigFilePaths, CPerforceClientThrow &_Client, NPerforce::CPerforceRootSpelling *o_pSpelling = nullptr);
	NPerforce::CPerforceRootSpelling fg_PerforceRootSpelling(CPerforceClientThrow &_Client);
	bool fg_PerforceConfigContentEquals(CStr const &_WorkspaceContent, CStr const &_SubmittedContent);

	// Find the highest changelist where any config files were modified.
	TCFuture<CStr> fg_PerforceGetLatestChangelist(TCVector<CStr> _ConfigFilePaths, CPerforceClient::CConnectionInfo _Connection);

	// Fetch config file contents from Perforce at the given changelists.
	// Empty CL means "workspace" - read current file from disk.
	TCFuture<TCTuple<CStr, CStr>> fg_PerforceGetConfigFileContents(CStr _ConfigFile, CStr _StartCL, CStr _EndCL, CPerforceClient::CConnectionInfo _Connection);
}
