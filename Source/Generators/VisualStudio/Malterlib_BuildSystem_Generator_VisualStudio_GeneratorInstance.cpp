// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include "../../Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/XML/XML>
#include <Mib/Process/ProcessLaunch>
#include <Mib/Encoding/JSON>
#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsRegistry>
#include <Mib/Core/PlatformSpecific/WindowsFilePath>
#endif

namespace NMib::NBuildSystem::NVisualStudio
{
	CGeneratorInstance::~CGeneratorInstance()
	{
		m_BuildSystem.f_SetGeneratorInterface(nullptr);
	}

	CStr const &CGeneratorInstance::f_GetToolsVersion() const
	{
		if (m_Version == 2022)
			return gc_ConstString_17_0;
		DError("Implement this (CGeneratorInstance::f_GetToolsVersion)");
	}

	CEJSONSorted const &CGeneratorInstance::f_GetVisualStudioRoot() const
	{
		if (m_VisualStudioRootCached.f_Load())
			return m_VisualStudioRoot;

		DMibLock(m_VisualStudioRootLock);

		if (m_VisualStudioRoot.f_IsValid())
			return m_VisualStudioRoot;

		CEJSONSorted VisualStudioRoot;
		
		switch (m_Version)
		{
		case 2022:
			{
#ifdef DPlatformFamily_Windows
				CStr ProgramData = m_BuildSystem.f_GetEnvironmentVariable("ProgramData");
				CStr CachePath = ProgramData / "Microsoft/VisualStudio/Packages";

				if (NMib::NPlatform::CWin32_Registry Registry; auto Path = Registry.f_Read_Str("SOFTWARE\\Microsoft\\VisualStudio\\Setup", "CachePath", ""))
					CachePath = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(Path);

				CStr InstancesPath = CachePath / "_Instances";

				CStr Errors;

				auto fParseVersion = [&](CStr const &_String) -> TCVector<uint32>
					{
						TCVector<uint32> VersionVector;
						for (auto &String : _String.f_Split("."))
							VersionVector.f_Insert(String.f_ToInt(uint32(0)));
						VersionVector.f_SetLen(4);
						return VersionVector;
					}
				;

				TCVector<zuint32> BestVersion;
				BestVersion.f_SetLen(4);
				CStr BestPath;

				uint32 ExpectedVersion = 17;

				for (auto &InstanceDir : CFile::fs_FindFiles(InstancesPath / "*", EFileAttrib_Directory))
				{
					CStr StateFile = InstanceDir / "state.json";
					try
					{
						CStr JsonContents = CFile::fs_ReadStringFromFile(StateFile);

						CJSONSorted const Json = CJSONSorted::fs_FromString(JsonContents, StateFile);
						auto Version = fParseVersion(Json["installationVersion"].f_String());
						if (Version[0] == ExpectedVersion && Version > BestVersion)
						{
							BestVersion = Version;
							BestPath = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(Json["installationPath"].f_String());
						}
					}
					catch (CException const &_Exception)
					{
						fg_AddStrSep(Errors, _Exception.f_GetErrorStr(), "\n");
					}
				}

				if (BestPath.f_IsEmpty())
					DError("Failed to find Visual Studio {} path. {}\n"_f << m_Version << Errors);

				VisualStudioRoot = BestPath;
#else
				VisualStudioRoot = "/opt/VisualStudio{}"_f << m_Version;
#endif
				break;
			}
		default: DError("Implement this (f_GetVisualStudioRoot())");
		}

		m_VisualStudioRoot = fg_Move(VisualStudioRoot);
		m_VisualStudioRootCached.f_Exchange(true);

		return m_VisualStudioRoot;
	}

	CGeneratorInstance::CGeneratorInstance
		(
			CBuildSystem const &_BuildSystem
			, CBuildSystemData const &_BuildSystemData
			, TCMap<CPropertyKey, CEJSONSorted> const &_InitialValues
			, CStr const &_OutputDir
		)
		: m_BuildSystem(_BuildSystem)
		, m_BuildSystemData(_BuildSystemData)
		, m_OutputDir(_OutputDir)
		, m_Win32Platfrom("Win32")
	{
		m_Version = _BuildSystem.f_GetGenerateSettings().m_Generator.f_Replace("VisualStudio", "").f_ToInt(uint32(2022));

		m_RelativeBasePath = CFile::fs_MakePathRelative(m_BuildSystem.f_GetBaseDir(), m_OutputDir.f_String() + "/Files/Temp");
		m_RelativeBasePathAbsolute = "$(ProjectDir)" + m_RelativeBasePath.f_String();
		m_BuildSystem.f_SetGeneratorInterface(this);

		CStr EnableSourceControl = m_BuildSystem.f_GetEnvironmentVariable("MalterlibEnableSourceControl");
		if (EnableSourceControl == "")
		{
			m_bEnableSourceControl = false;
			CStr Path = m_BuildSystem.f_GetBaseDir();
			while (!Path.f_IsEmpty())
			{
				if (CFile::fs_FileExists(Path + "/.p4config"))
				{
					m_bEnableSourceControl = true;
					break;
				}
				Path = CFile::fs_GetPath(Path);
			}
		}
		else if (EnableSourceControl == gc_ConstString_true.m_String)
			m_bEnableSourceControl = true;

		_BuildSystem.f_RegisterBuiltinVariables
			(
				{
					{CPropertyKey(_BuildSystem.f_StringCache(), EPropertyType_Compile, gc_ConstString_XInternalPrecompiledHeaderOutputFile), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(_BuildSystem.f_StringCache(), EPropertyType_Builtin, gc_ConstString_VisualStudioRoot), DMibBuildSystemTypeWithPosition(g_String)}
				}
			)
		;
	}

	CStr CGeneratorInstance::f_GetExpandedPath(CStr const &_Path, CStr const &_Base) const
	{
		return CFile::fs_GetExpandedPath(_Path.f_Replace(m_RelativeBasePathAbsolute.f_String(), m_BuildSystem.f_GetBaseDir()), _Base);
	}

	CSystemEnvironment CGeneratorInstance::f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const
	{
#if !defined(DPlatformFamily_Windows)
		return fg_GetSys()->f_Environment();
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
		CStr VisualStudioRoot = f_GetVisualStudioRoot().f_String();

		CStr VCVarsDirectory;

		VCVarsDirectory = VisualStudioRoot + "/VC/Auxiliary/Build";

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
		auto Environment = fg_GetSys()->f_Environment();
		Environment.f_Remove("PKG_CONFIG_PATH");

		CStr NewPaths;
		CStr CurrentPaths = Environment["PATH"];
		TCVector<CStr> ExtraPaths;
		while (!CurrentPaths.f_IsEmpty())
		{
			CStr Path = fg_GetStrSep(CurrentPaths, ";");

			if (Path.f_FindNoCase("\\Git\\") >= 0 || Path.f_FindNoCase("\\Strawberry\\c\\") >= 0 || Path.f_FindNoCase("\\Gnu32\\") >= 0 || Path.f_FindNoCase("\\GnuWin32\\") >= 0)
			{
				ExtraPaths.f_Insert(Path);
				continue;
			}
			fg_AddStrSep(NewPaths, Path, ";");
		}
		Environment["PATH"] = NewPaths;
		Params.m_Environment = Environment;
		Params.m_bMergeEnvironment = false;

		CStr Output = CProcessLaunch::fs_LaunchTool("cmd.exe", {"/c", fg_Format("vcvarsall.bat {} & set", VSArchitecture)}, Params);

		Environment.f_Clear();
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

		Environment["PATH"] = Environment["PATH"] + ";" + CStr::fs_Join(ExtraPaths, ";");

		{
			DLock(m_GetEnvironmentLock);
			m_CachedBuildEnvironment[_Architecture] = Environment;
		}
		return Environment;
#endif
	}

	CValuePotentiallyByRef CGeneratorInstance::f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const
	{
		if (_Value == gc_ConstString_BasePathRelativeProject.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePathRelativeProject")->f_AddValue(m_RelativeBasePath, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_RelativeBasePath;
		}
		else if (_Value == gc_ConstString_GeneratedBuildSystemDir.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.GeneratedBuildSystemDir")->f_AddValue(m_OutputDir, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_OutputDir;
		}
		else if (_Value == gc_ConstString_BasePath.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePath")->f_AddValue(m_RelativeBasePathAbsolute, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_RelativeBasePathAbsolute;
		}
		else if (_Value == gc_ConstString_ProjectPath.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.ProjectPath")->f_AddValue(m_Builtin_ProjectPath, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_ProjectPath;
		}
		else if (_Value == gc_ConstString_Inherit.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Inherit")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_Inherit;
		}
		else if (_Value == gc_ConstString_VisualStudioRoot.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.VisualStudioRoot")->f_AddValue(f_GetVisualStudioRoot(), m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &f_GetVisualStudioRoot();
		}
		return CEJSONSorted();
	}
}
