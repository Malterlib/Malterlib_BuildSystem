// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	struct CFindOptions
	{
		CFindOptions(NStr::CStr const &_Path, NFile::EFileAttrib _Attribs = (NFile::EFileAttrib_File | NFile::EFileAttrib_Directory), bool _bRecursive = false, bool _bFollowLinks = true);
		
		bool operator < (CFindOptions const &_Right) const;
		
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NStr::CStr m_Path;
		NFile::EFileAttrib m_Attribs;
		uint8 m_bRecursive;
		uint8 m_bFollowLinks;
		NContainer::TCSet<NStr::CStr> m_Exclude;
		
	private:
		template <typename t_CKey2, typename t_CData2>
		friend class NMib::NContainer::TCMapTreeMember;

		CFindOptions();
	};
	
	struct CFindCache
	{
		CFindCache();
		~CFindCache();
		
		NContainer::TCMap<CFindOptions, NContainer::TCVector<NFile::CFile::CFoundFile>> f_GetAllTagged() const;
		NContainer::TCVector<NFile::CFile::CFoundFile> const &f_FindFiles(CFindOptions const &_Options, bool _bTag) const;
		void f_AddSourceFile(NStr::CStr const &_FileName) const;		
		NContainer::TCSet<NStr::CStr> f_GetSourceFiles() const;
		
	private:
		struct CEntry
		{
			NContainer::TCVector<NFile::CFile::CFoundFile> m_FoundFiles;
			NAtomic::TCAtomic<bool> m_bTagged;
			NAtomic::TCAtomic<bool> m_bFinished;
			NThread::CMutual m_Lock;
		};
	private:
		mutable align_cacheline NThread::CMutual mp_Lock;
		mutable NContainer::TCMap<CFindOptions, CEntry> mp_SourceSearches;
		mutable align_cacheline NThread::CMutual mp_SourceFilesLock;
		mutable NContainer::TCSet<NStr::CStr> mp_SourceFiles;
	};
}

#include "Malterlib_BuildSystem_Find.hpp"
