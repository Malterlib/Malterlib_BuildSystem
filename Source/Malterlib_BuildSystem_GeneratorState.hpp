// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
		_Stream << uint32(EFileVersion);
		_Stream << uint32(EFileVersion_OldestReader);
		_Stream << m_GeneratedFiles;
		_Stream << m_ExeFile;
		_Stream << m_SourceFiles;
		_Stream << m_ReferencedFiles;
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

		if (Version == EFileVersion_GeneratedFilesLast)
		{
			_Stream >> m_ExeFile;
			_Stream >> m_SourceFiles;
			_Stream >> m_ReferencedFiles;
			_Stream >> m_GeneratedFiles;
			_Stream >> m_SourceSearches;
			_Stream >> m_Environment;
			_Stream >> m_NumWorkspaceTargets;
			_Stream >> m_GenerationFlags;
			return;
		}

		if (Version < EFileVersion)
			DMibError(NStr::CStr::CFormat("Generator state of version 0x{nh} is too old to read") << Version);

		uint32 OldestReader;
		_Stream >> OldestReader;
		if (OldestReader > EFileVersion)
			DMibError(NStr::CStr::CFormat("Generator state of version 0x{nh} needs a reader of version 0x{nh} or later") << Version << OldestReader);

		_Stream >> m_GeneratedFiles;
		_Stream >> m_ExeFile;
		_Stream >> m_SourceFiles;
		_Stream >> m_ReferencedFiles;
		_Stream >> m_SourceSearches;
		_Stream >> m_Environment;
		_Stream >> m_NumWorkspaceTargets;
		_Stream >> m_GenerationFlags;
	}
}
