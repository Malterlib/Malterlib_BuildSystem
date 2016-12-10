// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	CStr const &CGeneratorState::CProcessedFile::f_GetFileName() const
	{
		return TCMap<CStr, CProcessedFile>::fs_GetKey(this);
	}

	template <typename tf_CStream>
	void CGeneratorState::CProcessedFile::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_WriteTime;
		_Stream << m_Workspaces;
		_Stream << m_bNoDateCheck;
	}

	template <typename tf_CStream>
	void CGeneratorState::CProcessedFile::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_WriteTime;
		_Stream >> m_Workspaces;
		_Stream >> m_bNoDateCheck;
	}
	
	template <typename tf_CStream>
	void CGeneratorState::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << EFileVersion;
		_Stream << m_ExeFile;
		_Stream << m_SourceFiles;
		_Stream << m_ReferencedFiles;
		_Stream << m_GeneratedFiles;
		_Stream << m_SourceSearches;
		_Stream << m_Environment;
		_Stream << m_GenerationFlags;
	}

	template <typename tf_CStream>
	void CGeneratorState::f_Consume(tf_CStream &_Stream)
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
		_Stream >> m_GenerationFlags;
	}
}
