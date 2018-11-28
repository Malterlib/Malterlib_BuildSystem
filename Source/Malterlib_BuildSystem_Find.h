// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	struct CFindOptions
	{
		CFindOptions(CStr const &_Path, EFileAttrib _Attribs = (EFileAttrib_File | EFileAttrib_Directory), bool _bRecursive = false, bool _bFollowLinks = true);
		
		bool operator < (CFindOptions const &_Right) const;
		
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		CStr m_Path;
		EFileAttrib m_Attribs;
		uint8 m_bRecursive;
		uint8 m_bFollowLinks;
		TCSet<CStr> m_Exclude;
		
	private:
		template <typename t_CKey2, typename t_CData2>
		friend class NMib::NContainer::TCMapTreeMember;

		CFindOptions();
	};
	
	struct CFindCache
	{
		CFindCache();
		~CFindCache();
		
		TCMap<CFindOptions, TCVector<CFile::CFoundFile>> f_GetAllTagged() const;
		TCVector<CFile::CFoundFile> const &f_FindFiles(CFindOptions const &_Options, bool _bTag) const;
		void f_AddSourceFile(CStr const &_FileName) const;		
		TCSet<CStr> f_GetSourceFiles() const;
		
	private:
		struct CEntry
		{
			TCVector<CFile::CFoundFile> m_FoundFiles;
			TCAtomic<bool> m_bTagged;
			TCAtomic<bool> m_bFinished;
			CMutual m_Lock;
		};
	private:
		mutable align_cacheline CMutual mp_Lock;
		mutable TCMap<CFindOptions, CEntry> mp_SourceSearches;
		mutable align_cacheline CMutual mp_SourceFilesLock;
		mutable TCSet<CStr> mp_SourceFiles;
	};
}

#include "Malterlib_BuildSystem_Find.hpp"
