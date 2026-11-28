// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	struct CMalterlibDependencyTracker
	{
		CMalterlibDependencyTracker(bool _bUseHash = false);

		void f_AddInputFile(NStr::CStr const &_File);
		void f_AddOutputFile(NStr::CStr const &_File);
		void f_AddFind
			(
				NStr::CStr const &_SearchPattern
				, bool _bRecurse
				, bool _bFollowLinks
				, NFile::EFileAttrib _Attributes
				, NContainer::TCVector<NStr::CStr> const &_Results
				, NContainer::TCVector<NStr::CStr> const &_Excluded = fg_Default()
			)
		;

		// Write dependency files
		// _File: Path for MalterlibDependency file
		// _DepFile: Optional path for gcc-format dependency file
		//
		// If _DepFile is empty (legacy mode):
		//   - Write everything to MalterlibDependency
		//
		// If _DepFile is provided:
		//   - If using hash mode: skip depfile (MalterlibDependency handles it)
		//   - If not using hash mode: write depfile with inputs,
		//     write MalterlibDependency only if there are Finds
		void f_WriteDependencyFile(NStr::CStr const &_File, NStr::CStr const &_DepFile);

	private:
		struct CDependencyFile
		{
			NStr::CStr m_Path;
			NTime::CTime m_Time;
			NCryptography::CHashDigest_MD5 m_Digest;
		};

		struct CFind : public CDependencyFile
		{
			NStr::CStr m_Pattern;
			NFile::EFileAttrib m_Attributes;
			uint32 m_bRecurse;
			uint32 m_bFollowLinks;
			NContainer::TCVector<NStr::CStr> m_Excluded;
			NContainer::TCVector<NStr::CStr> m_Results;
		};

		NContainer::TCVector<NStr::CStr> mp_Outputs;
		NContainer::TCVector<CDependencyFile> mp_Inputs;
		NContainer::TCVector<CFind> mp_Finds;
		bool mp_bUseHash = false;
	};
}

#ifndef DMibPNoShortCuts
using namespace NMib::NBuildSystem;
#endif

