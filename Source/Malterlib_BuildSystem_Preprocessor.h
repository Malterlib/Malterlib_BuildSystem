// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_BuildSystem_Find.h"

namespace NMib::NBuildSystem
{
	class CBuildSystemPreprocessor
	{
	public:
		CBuildSystemPreprocessor(CRegistryPreserveAndOrder_CStr &_ResultRegistry, TCSet<CStr> &_SourceFiles, CFindCache const &_FindCache);

		void f_ReadFile(CStr const &_Path);
		CStr const &f_GetFileLocation();

	private:
		static void fsp_ThrowError(CRegistryPreserveAndOrder_CStr const &_Registry, CStr const &_Error);
		void fpr_HandleIncludes(CRegistryPreserveAndOrder_CStr &_Registry, CStr const &_Path);
		void fpr_FindFilesRecursive(CRegistryPreserveAndOrder_CStr &_Registry, TCVector<CStr> &o_Files, CStr const &_Path, CStr const &_ToFind);

		CRegistryPreserveAndOrder_CStr &mp_ResultRegistry;
		TCSet<CStr> &mp_SourceFiles;
		CFindCache const &mp_FindCache;
		CStr mp_FileLocation;
	};
}
