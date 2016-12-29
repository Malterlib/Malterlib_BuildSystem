// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NVisualStudio
{
	CGeneratorInstance::~CGeneratorInstance()
	{
		m_BuildSystem.f_SetGeneratorInterface(nullptr);
	}

	CStr CGeneratorInstance::f_GetToolsVersion() const
	{
		if (m_Version == 2012)
			return "4.0";
		else if (m_Version == 2013)
			return "12.0";
		else if (m_Version == 2015)
			return "14.0";
		DError("Implement this");
	}

	CGeneratorInstance::CGeneratorInstance
		(
			CBuildSystem const &_BuildSystem
			, CBuildSystemData const &_BuildSystemData
			, TCMap<CPropertyKey, CStr> const &_InitialValues
			, CStr const &_OutputDir
		)
		: m_BuildSystem(_BuildSystem)
		, m_BuildSystemData(_BuildSystemData)
		, m_OutputDir(_OutputDir)
		, m_Win32Platfrom("Win32")
	{

		m_Version = _BuildSystem.f_GetGenerateSettings().m_Generator.f_Replace("VisualStudio", "").f_ToInt(uint32(2012));

		m_RelativeBasePath = CFile::fs_MakePathRelative(m_BuildSystem.f_GetBaseDir(), m_OutputDir + "/Files/Temp");
		m_RelativeBasePathAbsolute = "$(ProjectDir)" + m_RelativeBasePath;
		m_BuildSystem.f_SetGeneratorInterface(this);
		CEntityKey Key;
		Key.m_Type = EEntityType_GeneratorSetting;
		Key.m_Name = "VisualStudio2012";

		auto pEntity = _BuildSystemData.m_RootEntity.m_ChildEntitiesMap.f_FindEqual(Key);
		if (!pEntity)
			m_BuildSystem.fs_ThrowError(CFilePosition(), "No VisualStudio2012 generator settings found");
		
		_BuildSystem.f_EvaluateData(m_GeneratorSettingsData, _InitialValues, pEntity, nullptr, nullptr, true, true);

		auto pSettings = m_GeneratorSettingsData.m_RootEntity.m_ChildEntitiesMap.f_FindEqual(Key);
		if (!pSettings)
			m_BuildSystem.fs_ThrowError(CFilePosition(), "No VisualStudio2012 generator settings found");
		m_pGeneratorSettings = fg_Explicit(pSettings);
		_BuildSystem.f_EvaluateAllGeneratorSettings(*pSettings);

		CStr EnableSourceControl = NSys::fg_Process_GetEnvironmentVariable(CStr("MalterlibEnableSourceControl"));
		if (EnableSourceControl == "true" || EnableSourceControl == "")
			m_bEnableSourceControl = true;

	}

	CStr CGeneratorInstance::f_GetExpandedPath(CStr const &_Path, CStr const& _Base) const
	{
		return CFile::fs_GetExpandedPath(_Path.f_Replace(m_RelativeBasePathAbsolute, m_BuildSystem.f_GetBaseDir()), _Base);
	}

	bool CGeneratorInstance::f_GetBuiltin(CStr const &_Value, CStr &_Result) const
	{
		if (_Value == "MToolCom")
		{
			CStr FileName = CFile::fs_GetFileNoExt(CFile::fs_GetProgramPath());
			CStr ProgramName;
			CStr BitNess;
			(CStr::CParse("{}_x{}") >> ProgramName >> BitNess).f_Parse(FileName);

			CStr NewProgramPath = CStr(CStr::CFormat("{}.com") << ProgramName);
			CStr Return = CFile::fs_AppendPath(CFile::fs_GetPath(CFile::fs_GetProgramPath()), NewProgramPath);
			if (!NFile::CFile::fs_FileExists(Return, EFileAttrib_File))
			{
				_Result = CFile::fs_GetProgramPath();
				return true;
			}
	#if DMibPPtrBits == 32
			Return += " -x86";
	#endif
			_Result = Return;

			return true;					
		}
		else if (_Value == "BasePathRelativeProject")
		{
			_Result = m_RelativeBasePath;
			return true;
		}
		else if (_Value == "GeneratedBuildSystemDir")
		{
			_Result = m_OutputDir;
			return true;
		}
		else if (_Value == "BasePath")
		{
			_Result = m_RelativeBasePathAbsolute;
			return true;
		}
		else if (_Value == "IntermediateDirectory")
		{
			_Result = "$(IntDir)";
			return true;
		}
		else if (_Value == "OutputDirectory")
		{
			_Result = "$(OutDir)";
			return true;
		}
		else if (_Value == "ProjectPath")
		{
			_Result = ".";
			return true;
		}
		else if (_Value == "Inherit")
		{
			_Result = "{578185E0-2E2A-4481-A34E-BCC3F64CDCA2}";
			return true;
		}
		else if (_Value == "SourceFileName")
		{
			_Result = "%(Filename)";
			return true;
		}
		return false;
	}
}
