// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NVisualStudio
{
	CStr CGeneratorInstance::f_GetNativePlatform(ELanguageType _Language, CStr const &_Platform)
	{
		if (_Language == ELanguageType_Native && _Platform == "x86")
			return m_Win32Platfrom;
		return _Platform;
	}

	CStr CGeneratorInstance::f_GetNativePlatform(CProjectState &_ProjectState, CStr const &_Platform)
	{
		return f_GetNativePlatform(_ProjectState.m_LanguageType, _Platform);
	}

	TCFuture<void> CGeneratorInstance::f_GenerateProjectFile(CProject &_Project, CStr const &_OutputDir) const
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureExceptions);

		CGeneratorSettings TargetSettings;
		co_await TargetSettings.f_PopulateSettings(gc_ConstKey_GeneratorSetting_Target, EPropertyType_Target, m_BuildSystem, _Project.m_EnabledProjectConfigs);
		co_await g_Yield;

		CProjectState ProjectState;
		CProjectXMLState XMLState;

		ProjectState.m_CurrentOutputDir = _OutputDir;

		CStr FileName;
		ELanguageType LanguageType = ELanguageType_Native;
		{
			CBuildSystemUniquePositions Positions;
			auto Language = TargetSettings.f_GetSingleSetting<CStr>(gc_ConstString_Language);

			if (Language.m_Value == "Native")
			{
				LanguageType = ELanguageType_Native;
				FileName = CFile::fs_AppendPath(_OutputDir, CStr(CFile::fs_MakeNiceFilename(_Project.f_GetName()))) + ".vcxproj";
			}
			else if (Language.m_Value == "C#")
			{
				LanguageType = ELanguageType_CSharp;
				XMLState.m_bIsDotNet = true;
				FileName = CFile::fs_AppendPath(_OutputDir, CStr(CFile::fs_MakeNiceFilename(_Project.f_GetName()))) + ".csproj";
			}
			else
				m_BuildSystem.fs_ThrowError(Language.m_Positions, "Unsupported language type: {}"_f << Language.m_Value);
		}

		_Project.m_LanguageType = LanguageType;
		ProjectState.m_LanguageType = LanguageType;

		ProjectState.f_CreateDirectory(_OutputDir);

		CStr PropsFileName = CFile::fs_AppendPath(_OutputDir, CStr(CFile::fs_MakeNiceFilename(_Project.f_GetName()))) + ".props";
		{
			// Project root
			if (XMLState.m_bIsDotNet)
			{
				ProjectState.f_CreateDirectory(_OutputDir / "obj");
				PropsFileName = _OutputDir / "obj" / CFile::fs_GetFile(FileName) + ".before.props";
				XMLState.m_pProject = XMLState.m_XMLFile.f_CreateDefaultDocument("Project");
				CXMLDocument::f_SetAttribute(XMLState.m_pProject, "Sdk", "Microsoft.NET.Sdk");

				XMLState.m_pPropsProject = XMLState.m_PropsXMLFile.f_CreateDefaultDocument("Project");
				XMLState.m_pPreProject = CXMLDocument::f_CreateElement(XMLState.m_pPropsProject, gc_ConstString_PropertyGroup);
			}
			else
			{
				XMLState.m_pPropsProject = XMLState.m_PropsXMLFile.f_CreateDefaultDocument("Project", "xml version=\"1.0\" encoding=\"utf-8\"");
				CXMLDocument::f_SetAttribute(XMLState.m_pPropsProject, "ToolsVersion", f_GetToolsVersion());
				CXMLDocument::f_SetAttribute(XMLState.m_pPropsProject, "xmlns", "http://schemas.microsoft.com/developer/msbuild/2003");

				XMLState.m_pProject = XMLState.m_XMLFile.f_CreateDefaultDocument("Project", "xml version=\"1.0\" encoding=\"utf-8\"");
				CXMLDocument::f_SetAttribute(XMLState.m_pProject, "DefaultTargets", "Build");
				CXMLDocument::f_SetAttribute(XMLState.m_pProject, "ToolsVersion", f_GetToolsVersion());
				CXMLDocument::f_SetAttribute(XMLState.m_pProject, "xmlns", "http://schemas.microsoft.com/developer/msbuild/2003");

				XMLState.m_pPreProject = CXMLDocument::f_CreateElement(XMLState.m_pProject, gc_ConstString_PropertyGroup);
			}

			TCSet<CStr> AllPlatforms;

			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				CStr Platform = m_BuildSystem.f_EvaluateEntityPropertyString(**iConfig, gc_ConstKey_Target_VisualStudioPlatform);
				AllPlatforms[Platform];
				_Project.m_Platforms[iConfig.f_GetKey()] = Platform;
			}

			// Configurations
			if (LanguageType == ELanguageType_Native)
			{
				auto pConfigs = CXMLDocument::f_CreateElement(XMLState.m_pProject, "ItemGroup");
				CXMLDocument::f_SetAttribute(pConfigs, "Label", "ProjectConfigurations");
				for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					CStr Platform = _Project.m_Platforms[iConfig.f_GetKey()];

					auto pConfig = CXMLDocument::f_CreateElement(pConfigs, "ProjectConfiguration");
					CXMLDocument::f_SetAttribute(pConfig, gc_ConstString_Include, iConfig.f_GetKey().m_Configuration + "|" + Platform);
					auto pConfigName = CXMLDocument::f_CreateElement(pConfig, gc_ConstString_Configuration);
					CXMLDocument::f_AddText(pConfigName, iConfig.f_GetKey().m_Configuration);
					auto pPlatformName = CXMLDocument::f_CreateElement(pConfig, gc_ConstString_Platform);
					CXMLDocument::f_AddText(pPlatformName, Platform);
				}
			}
			else if (XMLState.m_bIsDotNet)
				CXMLDocument::f_AddElementAndText(XMLState.m_pPreProject, "Platforms", CStr::fs_Join(TCVector<CStr>::fs_FromContainer(AllPlatforms), ";"));

			// Globals
			{
				XMLState.m_pGlobals = XMLState.m_pPreProject;
				if (LanguageType == ELanguageType_Native)
				{
					XMLState.m_pGlobals = CXMLDocument::f_CreateElement(XMLState.m_pProject, gc_ConstString_PropertyGroup);
					CXMLDocument::f_SetAttribute(XMLState.m_pGlobals, "Label", "Globals");
				}

				CXMLDocument::f_AddElementAndText(XMLState.m_pGlobals, "ProjectGuid", _Project.f_GetGUID());
				if (LanguageType == ELanguageType_Native)
					CXMLDocument::f_AddElementAndText(XMLState.m_pGlobals, "RootNamespace", _Project.f_GetName());
				if (m_bEnableSourceControl)
					CXMLDocument::f_AddElementAndText(XMLState.m_pGlobals, "SccProvider", "Perforce Source Control Provider:{8D316614-311A-48F4-85F7-DF7020F62357}");
			}

			// Default imports
			if (auto ImportTargets = TargetSettings.f_GetSingleSettingWithoutPositions<TCOptional<TCVector<CStr>>>(gc_ConstString_ImportTargets_Defaults))
			{
				for (auto &Target : *ImportTargets)
				{
					auto pImport = CXMLDocument::f_CreateElement(XMLState.m_pProject, gc_ConstString_Import);
					CXMLDocument::f_SetAttribute(pImport, "Project", Target);
				}
			}

			// Configuration
			{
				XMLState.m_pConfiguration = XMLState.m_pPreProject;
				if (LanguageType == ELanguageType_Native)
				{
					XMLState.m_pConfiguration = CXMLDocument::f_CreateElement(XMLState.m_pProject, gc_ConstString_PropertyGroup);
					CXMLDocument::f_SetAttribute(XMLState.m_pConfiguration, "Label", gc_ConstString_Configuration);
				}
			}

			// Base path
			{
				CStr Path = CFile::fs_MakePathRelative(m_BuildSystem.f_GetBaseDir(), _OutputDir + "/Dummy").f_ReplaceChar('/', '\\');
				auto pProperties = CXMLDocument::f_CreateElement(XMLState.m_pProject, gc_ConstString_PropertyGroup);
				CXMLDocument::f_SetAttribute(pProperties, "Label", "Malterlib");
				CXMLDocument::f_AddElementAndText(pProperties, "MalterlibBaseDir", CStr::CFormat("$([System.IO.Path]::GetFullPath(\"$(MSBuildProjectDirectory){}\\\"))") << Path);
			}

			// Props
			if (auto Properties = TargetSettings.f_GetSingleSettingWithoutPositions<TCOptional<TCVector<CStr>>>(gc_ConstString_ImportTargets_Properties))
			{
				for (auto &Proprety : *Properties)
				{
					auto pImport = CXMLDocument::f_CreateElement(XMLState.m_pProject, gc_ConstString_Import);
					CXMLDocument::f_SetAttribute(pImport, "Project", Proprety);
				}
			}

			// User proprety sheets
			if (LanguageType == ELanguageType_Native)
			{
				auto pImportGroup = CXMLDocument::f_CreateElement(XMLState.m_pProject, "ImportGroup");
				CXMLDocument::f_SetAttribute(pImportGroup, "Label", "PropertySheets");

				auto pImport = CXMLDocument::f_CreateElement(pImportGroup, gc_ConstString_Import);
				CXMLDocument::f_SetAttribute(pImport, "Project", "$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props");
				CXMLDocument::f_SetAttribute(pImport, "Condition", "exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')");

				pImport = CXMLDocument::f_CreateElement(pImportGroup, gc_ConstString_Import);
				CXMLDocument::f_SetAttribute(pImport, "Project", CFile::fs_GetFile(PropsFileName));
			}

			if (LanguageType == ELanguageType_Native)
			{
				XMLState.m_pPropsPropertyGroup = CXMLDocument::f_CreateElement(XMLState.m_pPropsProject, gc_ConstString_PropertyGroup);
				XMLState.m_pPropsItemDefinitionGroup = CXMLDocument::f_CreateElement(XMLState.m_pPropsProject, "ItemDefinitionGroup");
			}
			else
				XMLState.m_pPropsPropertyGroup = CXMLDocument::f_CreateElement(XMLState.m_pProject, gc_ConstString_PropertyGroup);

			// Files

			auto CompileTypes = co_await f_GenerateProjectFile_File(_Project, ProjectState);

			// Project properties
			// 

			if (LanguageType == ELanguageType_Native)
			{
				f_GenerateProjectFile_AddPrefixHeaders(_Project, ProjectState);
				co_await f_GenerateProjectFile_File(_Project, ProjectState);
			}

			auto CompileTypeSettingsPerVSType = co_await f_GenerateProjectFile_FileTypes(_Project, ProjectState, CompileTypes);
			co_await f_GenerateProjectFile_Dependency(_Project, ProjectState);

			XMLState.m_pFileItemGroup = CXMLDocument::f_CreateElement(XMLState.m_pProject, "ItemGroup");

			// .Net new SDK project type automatically adds everything it sees inside the directory
			if (LanguageType != ELanguageType_Native)
				CXMLDocument::f_SetAttribute(CXMLDocument::f_CreateElement(XMLState.m_pFileItemGroup, gc_ConstString_None), gc_ConstString_Remove, "**");

			auto Targets = TargetSettings.f_GetSingleSettingWithoutPositions<TCOptional<TCVector<CStr>>>(gc_ConstString_ImportTargets);

			CGeneratorSettings::fs_AddToXMLFiles<false, false>(XMLState, _Project, TargetSettings.f_GetParsedVSSettings<false, false>(nullptr), nullptr);
			f_GenerateProjectFile_AddToXML_File(_Project, ProjectState, XMLState, CompileTypeSettingsPerVSType);
			f_GenerateProjectFile_AddToXML_FileTypes(_Project, ProjectState, XMLState, fg_Move(CompileTypeSettingsPerVSType));
			f_GenerateProjectFile_AddToXML_Dependency(_Project, ProjectState, XMLState);

			// MSBuild Targets
			if (Targets)
			{
				for (auto &Target : *Targets)
				{
					auto pImport = CXMLDocument::f_CreateElement(XMLState.m_pProject, gc_ConstString_Import);
					CXMLDocument::f_SetAttribute(pImport, "Project", Target);
				}
			}
		}

		{
			CStr XMLData = XMLState.m_XMLFile.f_GetAsString(EXMLOutputDialect_VisualStudio);
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(FileName, XMLData, _Project.m_pSolution->f_GetName(), bWasCreated))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

			if (bWasCreated)
			{
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(XMLData));
				m_BuildSystem.f_WriteFile(FileData, FileName);
			}

			_Project.m_FileName = FileName;
		}

		{
			CStr XMLData = XMLState.m_PropsXMLFile.f_GetAsString(EXMLOutputDialect_VisualStudio);
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(PropsFileName, XMLData, _Project.m_pSolution->f_GetName(), bWasCreated))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

			if (bWasCreated)
			{
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(XMLData));
				m_BuildSystem.f_WriteFile(FileData, PropsFileName);
			}

			_Project.m_FileName = FileName;
		}

		if (LanguageType == ELanguageType_Native)
		{
			CXMLDocument FilterXML;
			auto pProject = FilterXML.f_CreateDefaultDocument("Project", "xml version=\"1.0\" encoding=\"utf-8\"");
			CXMLDocument::f_SetAttribute(pProject, "ToolsVersion", f_GetToolsVersion());
			CXMLDocument::f_SetAttribute(pProject, "xmlns", "http://schemas.microsoft.com/developer/msbuild/2003");

			// Files
			{
				auto pItemGroup = CXMLDocument::f_CreateElement(pProject, "ItemGroup");
				for (auto iFile = _Project.m_Files.f_GetIterator(); iFile; ++iFile)
				{
					auto pFileElement = CXMLDocument::f_CreateElement(pItemGroup, iFile->m_VSType);
					CXMLDocument::f_SetAttribute(pFileElement, gc_ConstString_Include, iFile->m_VSFile);
					CStr GroupPath = iFile->f_GetGroupPath().f_ReplaceChar('/', '\\');
					if (!GroupPath.f_IsEmpty())
						CXMLDocument::f_AddElementAndText(pFileElement, "Filter", GroupPath);
				}
			}

			// Groups
			{
				auto pItemGroup = CXMLDocument::f_CreateElement(pProject, "ItemGroup");
				for (auto iGroup = _Project.m_Groups.f_GetIterator(); iGroup; ++iGroup)
				{
					auto pGroupElement = CXMLDocument::f_CreateElement(pItemGroup, "Filter");
					CXMLDocument::f_SetAttribute(pGroupElement, gc_ConstString_Include, iGroup->f_GetPath().f_ReplaceChar('/','\\'));
					CXMLDocument::f_AddElementAndText(pGroupElement, "UniqueIdentifier", iGroup->f_GetGUID());
				}
			}

			CStr XMLData = FilterXML.f_GetAsString(EXMLOutputDialect_VisualStudio);
			CStr FiltersFileName = FileName+".filters";
			bool bWasCreated = false;
			if (!m_BuildSystem.f_AddGeneratedFile(FiltersFileName, XMLData, _Project.m_pSolution->f_GetName(), bWasCreated))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FiltersFileName));

			if (bWasCreated)
			{
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(XMLData));
				m_BuildSystem.f_WriteFile(FileData, FiltersFileName);
			}
		}

		co_return {};
	}
}
