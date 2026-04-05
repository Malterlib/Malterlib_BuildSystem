// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Generator_VisualStudioRootHelper.h"

namespace NMib::NBuildSystem
{
	CVisualStudioRootHelper::CVisualStudioRootHelper(CBuildSystem const &_BuildSystem)
		: m_BuildSystem(_BuildSystem)
	{
	}

	CEJsonSorted const &CVisualStudioRootHelper::f_GetVisualStudioRoot(uint32 _Version) const
	{
		if (m_VisualStudioRootCached.f_Load())
			return m_VisualStudioRoot;

		DMibLock(m_VisualStudioRootLock);

		if (m_VisualStudioRoot.f_IsValid())
			return m_VisualStudioRoot;

		CEJsonSorted VisualStudioRoot;

		switch (_Version)
		{
		case 2022:
		case 2026:
			{
#ifdef DPlatformFamily_Windows
				CStr Errors;
				auto Versions = CBuildSystem::fs_GetVisualStudioVersions(Errors, m_BuildSystem.f_GetEnvironmentVariable("ProgramData"));

				uint32 ExpectedVersion = 17;
				if (_Version == 2026)
					ExpectedVersion = 18;

				auto *pVersion = Versions.f_FindEqual(ExpectedVersion);

				if (!pVersion)
					DError("Failed to find Visual Studio {} path. {}\n"_f << _Version << Errors);

				VisualStudioRoot = pVersion->m_RootPath;
#else
				VisualStudioRoot = "/opt/VisualStudio{}"_f << _Version;
#endif
				break;
			}
		default: DError("Implement this (f_GetVisualStudioRoot())");
		}

		m_VisualStudioRoot = fg_Move(VisualStudioRoot);
		m_VisualStudioRootCached.f_Exchange(true);

		return m_VisualStudioRoot;
	}

	CSystemEnvironment CVisualStudioRootHelper::f_GetBuildEnvironment(uint32 _Version, CStr const &_Platform, CStr const &_Architecture) const
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
		CStr VisualStudioRoot = f_GetVisualStudioRoot(_Version).f_String();

		CStr VCVarsDirectory;

		VCVarsDirectory = VisualStudioRoot + "/VC/Auxiliary/Build";

		auto fArchitectureToVSArchitecture = [&](CStr const &_Architecture) -> CStr
			{
				if (_Architecture == "x64")
					return "amd64";
				else
					return _Architecture;
			}
		;

		CStr TargetVSArchitecture = fArchitectureToVSArchitecture(_Architecture);
		CStr HostVSArchitecture = fArchitectureToVSArchitecture(gc_ConstStringDynamic_DArchitecture);

		CStr VSArchitecture;
		if (TargetVSArchitecture == HostVSArchitecture)
			VSArchitecture = TargetVSArchitecture;
		else
			VSArchitecture = "{}_{}"_f << HostVSArchitecture << TargetVSArchitecture;

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
}
