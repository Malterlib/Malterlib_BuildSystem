// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_GenerateSettings.h"

namespace NMib::NBuildSystem
{
	CGenerateSettings::CGenerateSettings()
		: m_Action("Build")
#ifdef DPlatformFamily_Windows
		, m_Generator("VisualStudio2012")				
#endif
		, m_GenerationFlags(EGenerationFlag_None)
	{
	}

	bool CGenerateSettings::operator == (CGenerateSettings const &_Right) const
	{
		if (m_SourceFile != _Right.m_SourceFile)
			return false;
		if (m_OutputDir != _Right.m_OutputDir)
			return false;
		if (m_Generator != _Right.m_Generator)
			return false;
		if (m_Workspace != _Right.m_Workspace)
			return false;
		if (m_Action != _Right.m_Action)
			return false;
		if (m_GenerationFlags != _Right.m_GenerationFlags)
			return false;
		return true;
	}
}
