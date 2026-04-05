// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include "../../Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/XML/XML>
#include <Mib/Encoding/Json>
#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsRegistry>
#include <Mib/Core/PlatformSpecific/WindowsFilePath>
#endif

namespace NMib::NBuildSystem
{
	auto CBuildSystem::fs_GetVisualStudioVersions(NStr::CStr &o_Errors, CStr _ProgramData) -> NContainer::TCMap<uint32, CVisualStudioVersion>
	{
#ifdef DPlatformFamily_Windows
		CStr CachePath = _ProgramData / "Microsoft/VisualStudio/Packages";

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

		CStr BestPath;

		TCMap<uint32, CBuildSystem::CVisualStudioVersion> Versions;
		TCMap<uint32, TCVector<zuint32>> BestVersions;

		for (auto &InstanceDir : CFile::fs_FindFiles(InstancesPath / "*", EFileAttrib_Directory))
		{
			CStr StateFile = InstanceDir / "state.json";
			try
			{
				CStr JsonContents = CFile::fs_ReadStringFromFile(StateFile);

				CJsonSorted const Json = CJsonSorted::fs_FromString(JsonContents, StateFile);
				auto FullVersion = Json["installationVersion"].f_String();
				auto Version = fParseVersion(FullVersion);
				auto MajorVersion = Version[0];
				auto &BestVersion = BestVersions[MajorVersion];

				if (Version > BestVersion)
				{
					BestVersion = Version;
					auto &OutVersion = Versions[MajorVersion];
					OutVersion.m_FullVersion = FullVersion;
					OutVersion.m_RootPath = NMib::NFile::NPlatform::fg_ConvertFromWindowsPath(Json["installationPath"].f_String());
				}
			}
			catch (CException const &_Exception)
			{
				fg_AddStrSep(o_Errors, _Exception.f_GetErrorStr(), "\n");
			}
		}

		return Versions;
#else
		return {};
#endif
	}
}

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
		else if (m_Version == 2026)
			return gc_ConstString_18_0;
		DError("Implement this (CGeneratorInstance::f_GetToolsVersion)");
	}

	CEJsonSorted const &CGeneratorInstance::f_GetVisualStudioRoot() const
	{
		return m_VisualStudioRootHelper.f_GetVisualStudioRoot(m_Version);
	}

	CGeneratorInstance::CGeneratorInstance
		(
			CBuildSystem const &_BuildSystem
			, CBuildSystemData const &_BuildSystemData
			, TCMap<CPropertyKey, CEJsonSorted> const &_InitialValues
			, CStr const &_OutputDir
		)
		: m_BuildSystem(_BuildSystem)
		, m_BuildSystemData(_BuildSystemData)
		, m_OutputDir(_OutputDir)
		, m_VisualStudioRootHelper(_BuildSystem)
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
					, {CPropertyKey(gc_ConstKey_Builtin_VisualStudioRoot), DMibBuildSystemTypeWithPosition(g_String)}
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
		return m_VisualStudioRootHelper.f_GetBuildEnvironment(m_Version, _Platform, _Architecture);
	}

	CValuePotentiallyByRef CGeneratorInstance::f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const
	{
		if (_Value == gc_ConstKey_Builtin_BasePathRelativeProject.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePathRelativeProject")->f_AddValue(m_RelativeBasePath, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_RelativeBasePath;
		}
		else if (_Value == gc_ConstKey_Builtin_GeneratedBuildSystemDir.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.GeneratedBuildSystemDir")->f_AddValue(m_OutputDir, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_OutputDir;
		}
		else if (_Value == gc_ConstKey_Builtin_BasePath.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePath")->f_AddValue(m_RelativeBasePathAbsolute, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_RelativeBasePathAbsolute;
		}
		else if (_Value == gc_ConstKey_Builtin_ProjectPath.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.ProjectPath")->f_AddValue(m_Builtin_ProjectPath, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_ProjectPath;
		}
		else if (_Value == gc_ConstKey_Builtin_Inherit.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Inherit")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_Inherit;
		}
		else if (_Value == gc_ConstKey_Builtin_VisualStudioRoot.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.VisualStudioRoot")->f_AddValue(f_GetVisualStudioRoot(), m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &f_GetVisualStudioRoot();
		}
		return CEJsonSorted();
	}
}
