// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	enum EGenerationFlag // : uint32
	{
		EGenerationFlag_None 				= 0
		, EGenerationFlag_AbsoluteFilePaths 	= DMibBit(0)
		, EGenerationFlag_SingleThreaded		= DMibBit(1)
		, EGenerationFlag_UseCachedEnvironment	= DMibBit(2)
		, EGenerationFlag_DisableUserSettings	= DMibBit(3)
	};

	struct CGenerateSettings
	{
		CGenerateSettings();

		template <typename tf_CStream>
		void f_Feed(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_Consume(tf_CStream &_Stream);

		bool operator == (CGenerateSettings const &_Right) const;

		CStr m_SourceFile;
		CStr m_OutputDir;
		CStr m_Generator;
		CStr m_Workspace;
		CStr m_Action;
		TCVector<CStr> m_ActionParams;
		EGenerationFlag m_GenerationFlags;
	};
}

#include "Malterlib_BuildSystem_GenerateSettings.hpp"
