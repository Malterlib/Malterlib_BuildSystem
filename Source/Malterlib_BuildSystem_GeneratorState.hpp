// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	NStr::CStr const &CGeneratorArchiveState::CProcessedFile::f_GetFileName() const
	{
		return NContainer::TCMap<NStr::CStr, CProcessedFile>::fs_GetKey(this);
	}

	template <typename tf_CStream>
	void CGeneratorArchiveState::CProcessedFile::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_WriteTime;
		_Stream << m_Workspaces;
		_Stream << m_Flags;
		if (m_Flags & EGeneratedFileFlag_ByDigest)
			_Stream << *m_pDigest;
	}

	template <typename tf_CStream>
	void CGeneratorArchiveState::CProcessedFile::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_WriteTime;
		_Stream >> m_Workspaces;
		_Stream >> m_Flags;
		if (m_Flags & EGeneratedFileFlag_ByDigest)
		{
			m_pDigest = fg_Construct();
			_Stream >> *m_pDigest;
		}
	}
	
	template <typename tf_CStream>
	void CGeneratorArchiveState::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << EFileVersion;
		_Stream << m_ExeFile;
		_Stream << m_SourceFiles;
		_Stream << m_ReferencedFiles;
		_Stream << m_GeneratedFiles;
		_Stream << m_SourceSearches;
		_Stream << m_Environment;
		_Stream << m_NumWorkspaceTargets;
		_Stream << m_GenerationFlags;
	}

	template <typename tf_CStream>
	void CGeneratorArchiveState::f_Consume(tf_CStream &_Stream)
	{
		uint32 Version;
		_Stream >> Version;
		if (Version != EFileVersion)
			return;
		_Stream >> m_ExeFile;
		_Stream >> m_SourceFiles;
		_Stream >> m_ReferencedFiles;
		_Stream >> m_GeneratedFiles;
		_Stream >> m_SourceSearches;
		_Stream >> m_Environment;
		_Stream >> m_NumWorkspaceTargets;
		_Stream >> m_GenerationFlags;
	}
}
