// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem_GenerateSettings.h"
#include "Malterlib_BuildSystem_Find.h"

namespace NMib::NBuildSystem
{
	struct CGeneratorState
	{
		enum 
		{
			EFileVersion = 0x118
		};

		struct CProcessedFile
		{
			CProcessedFile();
			
			inline_always CStr const &f_GetFileName() const;
			template <typename tf_CStream>
			void f_Feed(tf_CStream &_Stream) const;
			template <typename tf_CStream>
			void f_Consume(tf_CStream &_Stream);

			TCSet<CStr> m_Workspaces;
			NTime::CTime m_WriteTime;
			uint8 m_bNoDateCheck = false;
			uint8 m_bKeepGeneratedFile = false;
		};
		
		CGeneratorState();

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
