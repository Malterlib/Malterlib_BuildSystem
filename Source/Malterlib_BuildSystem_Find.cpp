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
				CFile::CFindFilesOptions Options(_Options.m_Path, _Options.m_bRecursive);
				Options.m_AttribMask = _Options.m_Attribs;
				Options.m_bFollowLinks = _Options.m_bFollowLinks;
				Options.m_ExcludePatterns.f_Insert("*/.DS_Store");
				for (auto &Exclude : _Options.m_Exclude)
					Options.m_ExcludePatterns.f_Insert(Exclude);
				pEntry->m_FoundFiles = CFile::fs_FindFiles(Options);
				pEntry->m_bFinished = true;
			}
		}
		if (_bTag)
			pEntry->m_bTagged = true;
		return pEntry->m_FoundFiles;
	}
	
	void CFindCache::f_AddSourceFile(CStr const &_FileName) const
	{
		DLock(mp_SourceFilesLock);
		mp_SourceFiles[_FileName];
	}
	
	TCSet<CStr> CFindCache::f_GetSourceFiles() const
	{
		DLock(mp_SourceFilesLock);
		return mp_SourceFiles;
	}
}
