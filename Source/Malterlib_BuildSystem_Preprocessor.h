// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/BuildSystem/Registry>

#include "Malterlib_BuildSystem_Find.h"

namespace NMib::NBuildSystem
{
	class CBuildSystemPreprocessor
	{
	public:
		CBuildSystemPreprocessor(CBuildSystemRegistry &_ResultRegistry, TCSet<CStr> &_SourceFiles, CFindCache const &_FindCache, TCMap<CStr, CStr> const &_Environment);

		void f_ReadFile(CStr const &_Path);
		CStr const &f_GetFileLocation();

	private:
		struct CError
		{
			CBuildSystemRegistry *m_pRootRegistry;
			CStr m_Error;
		};

		static void fsp_ThrowError(CBuildSystemRegistry const &_Registry, CStr const &_Error);
		void fpr_HandleIncludes(CBuildSystemRegistry &_Registry, CStr const &_Path, TCVector<CError> &o_Errors);
		void fpr_FindFilesRecursive(CBuildSystemRegistry &_Registry, TCVector<CStr> &o_Files, CStr const &_Path, CStr const &_ToFind);

		CBuildSystemRegistry &mp_ResultRegistry;
		TCSet<CStr> &mp_SourceFiles;
		CFindCache const &mp_FindCache;
		TCMap<CStr, CStr> const &mp_Environment;
		CStr mp_FileLocation;
	};
}
