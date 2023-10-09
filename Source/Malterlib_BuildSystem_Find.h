// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	struct CFindOptions
	{
		CFindOptions(NStr::CStr const &_Path, NFile::EFileAttrib _Attribs = (NFile::EFileAttrib_File | NFile::EFileAttrib_Directory), bool _bRecursive = false, bool _bFollowLinks = true);
		
		auto operator <=> (CFindOptions const &_Right) const = default;
		
		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		NStr::CStr m_Path;
		NFile::EFileAttrib m_Attribs;
		uint8 m_bRecursive;
		uint8 m_bFollowLinks;
		uint8 m_bExists = false;
		NContainer::TCSet<NStr::CStr> m_Exclude;
		
	private:
		template <typename t_CKey2, typename t_CData2>
		friend struct NMib::NContainer::TCMapNode;

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
		bool f_FileExists(NStr::CStr const &_File, NFile::EFileAttrib _Attributes) const;
		NStr::CStr f_ResolveSymbolicLink(NStr::CStr const &_File) const;
		
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
		mutable align_cacheline NThread::CMutual mp_ResolvedLinksLock;
		mutable NContainer::TCMap<NStr::CStr, NStorage::TCVariant<NStr::CStr, NException::CExceptionPointer>> mp_ResolvedLinks;
	};
}

#include "Malterlib_BuildSystem_Find.hpp"
