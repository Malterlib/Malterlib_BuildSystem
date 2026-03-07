// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem_Property.h"
#include "Malterlib_BuildSystem_ValuePotentiallyByRef.h"

namespace NMib::NBuildSystem
{
	struct CBuildSystem;
	struct CBuildSystemData;

	struct CBuildSystemGenerator
	{
		virtual ~CBuildSystemGenerator();
		virtual NContainer::TCMap<CPropertyKey, NEncoding::CEJsonSorted> f_GetValues(CBuildSystem const &_BuildSystem, NStr::CStr const &_OutputDir) = 0;
		virtual NConcurrency::TCUnsafeFuture<void> f_Generate
			(
				CBuildSystem const *_pBuildSystem
				, CBuildSystemData const *_pBuildSystemData
				, NStr::CStr _OutputDir
				, NContainer::TCMap<NStr::CStr, uint32> &o_NumWorkspaceTargets
			) = 0
		;
	};

	struct ICGeneratorInterface
	{
		virtual ~ICGeneratorInterface() = default;
		virtual CValuePotentiallyByRef f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, NStr::CStr const &_Value, bool &o_bSuccess) const = 0;
		virtual NStr::CStr f_GetExpandedPath(NStr::CStr const &_Path, NStr::CStr const &_Base) const = 0;
		virtual CSystemEnvironment f_GetBuildEnvironment(NStr::CStr const &_Platform, NStr::CStr const &_Architecture) const = 0;
	};
}
