// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <Mib/XML/XML>
#include <Mib/Process/ProcessLaunch>

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

	CStr CGeneratorInstance::f_GetVisualStudioRoot() const
	{
		uint32 VSVersion = 0;
		switch (m_Version)
		{
		case 2012: VSVersion = 110; break;
		case 2013: VSVersion = 120; break;
		case 2015: VSVersion = 140; break;
		default: DError("Implement this");
		}
		return CFile::fs_GetExpandedPath(NSys::fg_Process_GetEnvironmentVariable(fg_Format("VS{}COMNTOOLS", VSVersion)) + "../..");
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
		if (EnableSourceControl == "")
		{
			m_bEnableSourceControl = false;
			CStr Path = m_BuildSystem.f_GetBaseDir();
			while (Path.f_IsEmpty())
			{
				if (CFile::fs_FileExists(Path + "/.p4config"))
					m_bEnableSourceControl = true;
				Path = CFile::fs_GetPath(Path);
			}
		}		
		else if (EnableSourceControl == "true")
			m_bEnableSourceControl = true;

	}

	CStr CGeneratorInstance::f_GetExpandedPath(CStr const &_Path, CStr const& _Base) const
	{
		return CFile::fs_GetExpandedPath(_Path.f_Replace(m_RelativeBasePathAbsolute, m_BuildSystem.f_GetBaseDir()), _Base);
	}

	TCMap<CStr, CStr> CGeneratorInstance::f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const
	{
#ifndef DPlatformFamily_Windows
		return NSys::fg_Process_GetEnvironmentVariables();
#else
		if (_Platform != "Windows")
			DMibError("Unable to get build environment for non-Windows platform");

		CMutual *pEnvironmentLock;
		{
			DLock(m_GetEnvironmentLock);
			if (auto *pCachedEnvironment = m_CachedBuildEnvironment.f_FindEqual(_Architecture))
				return *pCachedEnvironment;
			pEnvironmentLock = &m_GetEnvironmentLocks[_Architecture];
		}
		DLock(*pEnvironmentLock);
		{
			DLock(m_GetEnvironmentLock);
			if (auto *pCachedEnvironment = m_CachedBuildEnvironment.f_FindEqual(_Architecture))
				return *pCachedEnvironment;
		}
 		CStr VisualStudioRoot = f_GetVisualStudioRoot();

		CStr VCVarsDirectory = VisualStudioRoot + "/VC";

		CStr VSArchitecture;
#ifdef DArchitecture_x86
		if (_Architecture == "x64")
			VSArchitecture = "x86_amd64";
		else if (_Architecture == "x86")
			VSArchitecture = "x86";
		else if (_Architecture == "arm")
			VSArchitecture = "x86_arm";
		else
			DError(fg_Format("Unsupported VS architecture: {}", _Architecture));
#elif DArchitecture_x64
		if (_Architecture == "x64")
			VSArchitecture = "amd64";
		else if (_Architecture == "x86")
			VSArchitecture = "amd64_x86";
		else if (_Architecture == "arm")
			VSArchitecture = "amd64_arm";
		else
			DError(fg_Format("Unsupported VS architecture: {}", _Architecture));
#else
		DError("Cannot get build environment for Visual Studio on this architecture");
#endif

		CProcessLaunchParams Params{VCVarsDirectory};
		Params.m_bShowLaunched = false;
		CStr Output = CProcessLaunch::fs_LaunchTool("cmd.exe", {"/c", fg_Format("vcvarsall.bat {} & set", VSArchitecture)}, Params);

		TCMap<CStr, CStr> Environment;
		ch8 const *pParse = Output;

		while (*pParse)
		{
			fg_ParseWhiteSpace(pParse);
			auto *pStart = pParse;
			fg_ParseToEndOfLine(pParse);
			CStr Line(pStart, pParse - pStart);
			CStr Key = fg_GetStrSep(Line, "=");
			if (Key.f_IsEmpty())
				continue;
			Environment[Key] = Line;
		}

		{
			DLock(m_GetEnvironmentLock);
			m_CachedBuildEnvironment[_Architecture] = Environment;
		}
		return Environment;
#endif
		return {};
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
