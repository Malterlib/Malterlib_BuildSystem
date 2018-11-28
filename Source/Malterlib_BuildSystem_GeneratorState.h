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
		, EGeneratedFileFlag_NoDateCheck = DBit(1)
		, EGeneratedFileFlag_KeepGeneratedFile = DBit(2)
		, EGeneratedFileFlag_Symlink = DBit(3)
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
			
			inline_always CStr const &f_GetFileName() const;
			template <typename tf_CStream>
			void f_Feed(tf_CStream &_Stream) const;
			template <typename tf_CStream>
			void f_Consume(tf_CStream &_Stream);

			bool f_FileChanged(TCAtomic<bool> &o_bChanged, CStr const &_OutputDirectory);

			TCSet<CStr> m_Workspaces;
			NTime::CTime m_WriteTime;
			EGeneratedFileFlag m_Flags = EGeneratedFileFlag_None;
		};
		
		CGeneratorArchiveState();

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);
		
		TCMap<CStr, CProcessedFile> m_ExeFile;
		TCMap<CStr, CProcessedFile> m_SourceFiles;
		TCMap<CStr, CProcessedFile> m_ReferencedFiles;
		TCMap<CStr, CProcessedFile> m_GeneratedFiles;
		TCMap<CFindOptions, TCVector<CFile::CFoundFile>> m_SourceSearches;
		TCMap<CStr, CStr> m_Environment;
		EGenerationFlag m_GenerationFlags = EGenerationFlag_None;
	};
}

#include "Malterlib_BuildSystem_GeneratorState.hpp"
