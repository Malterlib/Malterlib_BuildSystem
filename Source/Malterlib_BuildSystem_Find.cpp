// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_BuildSystem_Find.h"

namespace NMib::NBuildSystem
{
	CFindOptions::CFindOptions()
		: m_Attribs(EFileAttrib_None)
		, m_bRecursive(false)
		, m_bFollowLinks(true)
	{
	}

	CFindOptions::CFindOptions(CStr const &_Path, NFile::EFileAttrib _Attribs, bool _bRecursive, bool _bFollowLinks)
		: m_Path(_Path)
		, m_Attribs(_Attribs)
		, m_bRecursive(_bRecursive)
		, m_bFollowLinks(_bFollowLinks)
	{
	}

	CFindCache::CFindCache() = default;
	CFindCache::~CFindCache() = default;

	TCMap<CFindOptions, TCVector<CFile::CFoundFile>> CFindCache::f_GetAllTagged() const
	{
		TCMap<CFindOptions, TCVector<CFile::CFoundFile>> Ret;
		for (auto iFile = mp_SourceSearches.f_GetIterator(); iFile; ++iFile)
		{
			if (iFile->m_bTagged.f_Load())
				Ret[iFile.f_GetKey()] = iFile->m_FoundFiles;
		}
		return Ret;
	}

	CStr CFindCache::f_ResolveSymbolicLink(NStr::CStr const &_File) const
	{
		DLock(mp_ResolvedLinksLock);
		auto Mapping = mp_ResolvedLinks(_File);
		if (Mapping.f_WasCreated())
		{
			try
			{
				(*Mapping) = CFile::fs_ResolveSymbolicLink(_File);
			}
			catch (CException const &_Exception)
			{
				(*Mapping) = _Exception.f_ExceptionPointer();
			}
		}

		if (!(*Mapping).f_IsOfType<CStr>())
			std::rethrow_exception((*Mapping).f_GetAsType<CExceptionPointer>());

		return (*Mapping).f_GetAsType<CStr>();
	}

	bool CFindCache::f_FileExists(NStr::CStr const &_File, EFileAttrib _Attributes) const
	{
		CFindOptions Options(_File, _Attributes);
		Options.m_bExists = true;

		return !f_FindFiles(Options, true).f_IsEmpty();
	}

	TCVector<CFile::CFoundFile> const &CFindCache::f_FindFiles(CFindOptions const &_Options, bool _bTag) const
	{
		CEntry *pEntry;
		{
			DLock(mp_Lock);
			pEntry = &mp_SourceSearches[_Options];
		}

		if (!pEntry->m_bFinished.f_Load())
		{
			DLock(pEntry->m_Lock);
			if (!pEntry->m_bFinished.f_Load())
			{
				if (_Options.m_bExists)
				{
					if (CFile::fs_FileExists(_Options.m_Path, _Options.m_Attribs))
						pEntry->m_FoundFiles = {{_Options.m_Path, _Options.m_Attribs}};
				}
				else
				{
					CFile::CFindFilesOptions Options(_Options.m_Path, _Options.m_bRecursive);
					Options.m_AttribMask = _Options.m_Attribs;
					Options.m_bFollowLinks = _Options.m_bFollowLinks;
					Options.m_ExcludePatterns.f_Insert(gc_ConstString____DS_Store);
					for (auto &Exclude : _Options.m_Exclude)
						Options.m_ExcludePatterns.f_Insert(Exclude);
					pEntry->m_FoundFiles = CFile::fs_FindFiles(Options);
				}
				pEntry->m_bFinished = true;
			}
		}
		if (_bTag)
			pEntry->m_bTagged.f_Exchange(true);
		return pEntry->m_FoundFiles;
	}

	void CFindCache::f_AddSourceFile(CStr const &_FileName, TCSharedPointer<CHashDigest_SHA256> &&_pDigest) const
	{
		DLock(mp_SourceFilesLock);
		mp_SourceFiles[_FileName] = fg_Move(_pDigest);
	}

	TCMap<CStr, TCSharedPointer<CHashDigest_SHA256>> CFindCache::f_GetSourceFiles() const
	{
		DLock(mp_SourceFilesLock);
		return mp_SourceFiles;
	}
}
