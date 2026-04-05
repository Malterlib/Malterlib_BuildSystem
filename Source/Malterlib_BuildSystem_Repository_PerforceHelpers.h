// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Perforce/Wrapper>

namespace NMib::NBuildSystem::NRepository
{
	// Collect config files that are NOT inside any git sub-repo (i.e. managed by Perforce root)
	TCSet<CStr> fg_CollectPerforceRootConfigFiles(TCMap<CStr, CRepository *> const &_RepositoryByLocation, CStr const &_BaseDir);

	// Connect to Perforce from the P4CONFIG and get depot paths for config files.
	// Populates o_Client and returns a vector aligned 1:1 with _ConfigFilePaths;
	// entries for files that have no depot mapping yet (e.g. brand-new local
	// files not yet `p4 add`ed) are returned as empty strings, so callers can
	// distinguish "tracked file" from "new file" by index. Throws on connection
	// failure. Must be called on a blocking actor.
	TCVector<CStr> fg_PerforceConnectAndResolveDepotPaths(TCVector<CStr> const &_ConfigFilePaths, CStr const &_BaseDir, CPerforceClientThrow &o_Client);

	// Find the highest changelist where any config files were modified.
	TCFuture<CStr> fg_PerforceGetLatestChangelist(TCVector<CStr> _ConfigFilePaths, CStr _BaseDir);

	// Fetch config file contents from Perforce at the given changelists.
	// Empty CL means "workspace" - read current file from disk.
	TCFuture<TCTuple<CStr, CStr>> fg_PerforceGetConfigFileContents(CStr _ConfigFile, CStr _StartCL, CStr _EndCL);
}
