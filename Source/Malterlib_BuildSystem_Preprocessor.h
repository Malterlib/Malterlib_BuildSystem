// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/BuildSystem/Registry>

#include "Malterlib_BuildSystem_Find.h"

namespace NMib::NBuildSystem
{
	struct CBuildSystemPreprocessor
	{
		CBuildSystemPreprocessor
			(
				CBuildSystemRegistry &_ResultRegistry
				, NContainer::TCMap<NStr::CStr, NStorage::TCSharedPointer<NCryptography::CHashDigest_SHA256>> &_SourceFiles
				, CFindCache const &_FindCache
				, NContainer::TCMap<NStr::CStr, NStr::CStr> const &_Environment
				, CStringCache &_StringCache
			)
		;

		void f_ReadFile(NStr::CStr const &_Path);
		NStr::CStr const &f_GetFileLocation();

	private:
		struct CError
		{
			CBuildSystemRegistry *m_pRootRegistry;
			NStr::CStr m_Error;
		};

		static void fsp_ThrowError(CBuildSystemRegistry const &_Registry, NStr::CStr const &_Error);
		void fp_HandleIncludes(CBuildSystemRegistry &_Registry, NStr::CStr const &_Path, NContainer::TCVector<CError> &o_Errors);
		void fp_FindFilesRecursive(CBuildSystemRegistry &_Registry, NContainer::TCVector<NStr::CStr> &o_Files, NStr::CStr const &_Path, NStr::CStr const &_ToFind);

		CBuildSystemRegistry &mp_ResultRegistry;
		NContainer::TCMap<NStr::CStr, NStorage::TCSharedPointer<NCryptography::CHashDigest_SHA256>> &mp_SourceFiles;
		CFindCache const &mp_FindCache;
		CStringCache &mp_StringCache;
		NContainer::TCMap<NStr::CStr, NStr::CStr> const &mp_Environment;
		NStr::CStr mp_FileLocation;
	};
}
