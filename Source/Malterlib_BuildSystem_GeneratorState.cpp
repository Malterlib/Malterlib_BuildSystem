// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_GeneratorState.h"

namespace NMib::NBuildSystem
{
	CGeneratorState::CProcessedFile::CProcessedFile()
		: m_bNoDateCheck(false)
	{
	}

	CGeneratorState::CGeneratorState()
		: m_GenerationFlags(EGenerationFlag_None)
	{
	}
}
