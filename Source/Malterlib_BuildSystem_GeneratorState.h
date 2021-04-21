// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem_GenerateSettings.h"
#include "Malterlib_BuildSystem_Find.h"

namespace NMib::NBuildSystem
{
	enum EGeneratedFileFlag
	{
		EGeneratedFileFlag_None = 0
		, EGeneratedFileFlag_NoDateCheck = DMibBit(1)
		, EGeneratedFileFlag_KeepGeneratedFile = DMibBit(2)
		, EGeneratedFileFlag_Symlink = DMibBit(3)
	};

	struct CGeneratorArchiveState
	{
		enum 
		{
			EFileVersion = 0x119
		};

		struct CProcessedFile
		{
			CProcessedFile();
			
			inline_always NStr::CStr const &f_GetFileName() const;
			template <typename tf_CStream>
			void f_Feed(tf_CStream &_Stream) const;
			template <typename tf_CStream>
			void f_Consume(tf_CStream &_Stream);

			bool f_FileChanged(NAtomic::TCAtomic<bool> &o_bChanged, NStr::CStr const &_OutputDirectory, CBuildSystem const &_BuildSystem);

			NContainer::TCSet<NStr::CStr> m_Workspaces;
			NTime::CTime m_WriteTime;
			EGeneratedFileFlag m_Flags = EGeneratedFileFlag_None;
		};
		
		CGeneratorArchiveState();

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
		
		NContainer::TCMap<NStr::CStr, CProcessedFile> m_ExeFile;
		NContainer::TCMap<NStr::CStr, CProcessedFile> m_SourceFiles;
		NContainer::TCMap<NStr::CStr, CProcessedFile> m_ReferencedFiles;
		NContainer::TCMap<NStr::CStr, CProcessedFile> m_GeneratedFiles;
		NContainer::TCMap<CFindOptions, NContainer::TCVector<NFile::CFile::CFoundFile>> m_SourceSearches;
		NContainer::TCMap<NStr::CStr, NStr::CStr> m_Environment;
		EGenerationFlag m_GenerationFlags = EGenerationFlag_None;
	};
}

#include "Malterlib_BuildSystem_GeneratorState.hpp"
