// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <AOCC/AOXMLUtils.h>
#include <Mib/Cryptography/UUID>

namespace NMib::NBuildSystem::NVisualStudio
{
	CStr CGeneratorInstance::f_GetNativePlatform(ELanguageType _Language, CStr const &_Platform)
	{
		if (_Language == ELanguageType_Native && _Platform == "x86")
			return m_Win32Platfrom;
		return _Platform;
	}
	
	CStr CGeneratorInstance::f_GetNativePlatform(CStr const &_Platform)
	{
		return f_GetNativePlatform(m_ThreadLocal->m_LanguageType, _Platform);
	}

	void CGeneratorInstance::f_GenerateProjectFile(CProject &_Project, CStr const &_OutputDir) const
	{
		auto & ThreadLocal = *m_ThreadLocal;
		ThreadLocal.m_CurrentOutputDir = _OutputDir;
		ThreadLocal.m_PrefixHeaders.f_Clear();

		auto fl_GetEntityPropertyGlobal
			= [&](EPropertyType _Type, CStr const &_Name, CFilePosition &_Position) -> CStr
			{
				bool bFirstConfig = true;
				CStr ReturnValue;
				CProperty const *pFrom = nullptr;
				for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					CProperty const *pFromProperty = nullptr;
					CStr ThisValue = m_BuildSystem.f_EvaluateEntityProperty(**iConfig, _Type, _Name, pFromProperty);
					if (bFirstConfig)
					{
						ReturnValue = ThisValue;
						pFrom = pFromProperty;
					}
					else if (ReturnValue != ThisValue)
					{
						m_BuildSystem.fs_ThrowError
							(
								pFromProperty ? pFromProperty->m_Position : pFrom->m_Position
								, CStr::CFormat("You cannot specify different '{}' version for different configurations") << _Name
							)
						;
					}
				}
				if (pFrom)
					_Position = pFrom->m_Position;
				return ReturnValue;
			}
		;

		CStr ClCompileSuffix;
		{
			CFilePosition Position;
			ClCompileSuffix = fl_GetEntityPropertyGlobal(EPropertyType_Target, "ClCompileSuffix", Position);
		}

		ELanguageType LanguageType = ELanguageType_Native;
		{
			CFilePosition Position;
			CStr Language = fl_GetEntityPropertyGlobal(EPropertyType_Target, "Language", Position);

			if (Language.f_IsEmpty() || Language == "Native")
				LanguageType = ELanguageType_Native;
			else if (Language == "C#")
				LanguageType = ELanguageType_CSharp;
			else
				m_BuildSystem.fs_ThrowError(Position, CStr::CFormat("Language '{}' not supported") << Language);
		}

		CStr SettingsPrefix;
		if (LanguageType == ELanguageType_Native)
			SettingsPrefix = "Native_";
		else if (LanguageType == ELanguageType_CSharp)
			SettingsPrefix = "CSharp_";

		ThreadLocal.m_LanguageType = LanguageType;
		_Project.m_LanguageType = LanguageType;

		// Todo support csproj as well

		CStr PrefixGenDir = CFile::fs_AppendPath(_OutputDir, _Project.f_GetName());

		ThreadLocal.f_CreateDirectory(PrefixGenDir);
		ThreadLocal.f_CreateDirectory(_OutputDir);

		CStr FileName;
		if (LanguageType == ELanguageType_Native)
			FileName = CFile::fs_AppendPath(_OutputDir, CStr(CFile::fs_MakeNiceFilename(_Project.f_GetName()))) + ".vcxproj";
		else if (LanguageType == ELanguageType_CSharp)
			FileName = CFile::fs_AppendPath(_OutputDir, CStr(CFile::fs_MakeNiceFilename(_Project.f_GetName()))) + ".csproj";
		CStr PropsFileName = CFile::fs_AppendPath(_OutputDir, CStr(CFile::fs_MakeNiceFilename(_Project.f_GetName()))) + ".props";

		CAOXmlUtils XMLFile;
		CAOXmlUtils PropsXMLFile;
		{
			// Project root
			auto pPropsProject = PropsXMLFile.f_CreateDefaultDocument("Project");
			CAOXmlUtils::f_SetAttribute(pPropsProject, "ToolsVersion", f_GetToolsVersion());
			CAOXmlUtils::f_SetAttribute(pPropsProject, "xmlns", "http://schemas.microsoft.com/developer/msbuild/2003");
			
			auto pProject = XMLFile.f_CreateDefaultDocument("Project");
			CAOXmlUtils::f_SetAttribute(pProject, "DefaultTargets", "Build");
			CAOXmlUtils::f_SetAttribute(pProject, "ToolsVersion", f_GetToolsVersion());
			CAOXmlUtils::f_SetAttribute(pProject, "xmlns", "http://schemas.microsoft.com/developer/msbuild/2003");

			if (LanguageType == ELanguageType_CSharp)
			{
				auto pImport = CAOXmlUtils::f_CreateElement(pProject, "Import");
				CAOXmlUtils::f_SetAttribute(pImport, "Project", "$(MSBuildExtensionsPath)\\$(MSBuildToolsVersion)\\Microsoft.Common.props");
				CAOXmlUtils::f_SetAttribute(pImport, "Condition", "Exists('$(MSBuildExtensionsPath)\\$(MSBuildToolsVersion)\\Microsoft.Common.props')");
			}
			
			// Framework Version
			CAOXmlElement *pPreProjectPropertyGroup;
			{

				// External dependencies

				CFilePosition Position;
				CStr TargetFrameworkVersion = fl_GetEntityPropertyGlobal(EPropertyType_Target, "TargetFrameworkVersion", Position);

				auto pPropertyGroup = CAOXmlUtils::f_CreateElement(pProject, "PropertyGroup");
				pPreProjectPropertyGroup = pPropertyGroup;
				if (!TargetFrameworkVersion.f_IsEmpty())
					CAOXmlUtils::f_AddElementAndText(pPropertyGroup, "TargetFrameworkVersion", TargetFrameworkVersion);
			}

			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				CStr Platform = m_BuildSystem.f_EvaluateEntityProperty(**iConfig, EPropertyType_Target, "VisualStudioPlatform");
				_Project.m_Platforms[iConfig.f_GetKey()] = Platform;
			}

			// Configurations
			if (LanguageType == ELanguageType_Native)
			{
				auto pConfigs = CAOXmlUtils::f_CreateElement(pProject, "ItemGroup");
				CAOXmlUtils::f_SetAttribute(pConfigs, "Label", "ProjectConfigurations");
				for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					CStr Platform = _Project.m_Platforms[iConfig.f_GetKey()];

					auto pConfig = CAOXmlUtils::f_CreateElement(pConfigs, "ProjectConfiguration");
					CAOXmlUtils::f_SetAttribute(pConfig, "Include", iConfig.f_GetKey().m_Configuration + "|" + Platform);
					auto pConfigName = CAOXmlUtils::f_CreateElement(pConfig, "Configuration");
					CAOXmlUtils::f_AddText(pConfigName, iConfig.f_GetKey().m_Configuration);
					auto pPlatformName = CAOXmlUtils::f_CreateElement(pConfig, "Platform");
					CAOXmlUtils::f_AddText(pPlatformName, Platform);
				}
			}
			else if (LanguageType == ELanguageType_CSharp)
			{
				CAOXmlUtils::f_AddElementAndText(pPreProjectPropertyGroup, "AppDesignerFolder", "Properties");

				for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
				{
					CStr Platform = _Project.m_Platforms[iConfig.f_GetKey()];
					auto pConfig = CAOXmlUtils::f_CreateElement(pProject, "PropertyGroup");
					CAOXmlUtils::f_SetAttribute
						(
							pConfig
							, "Condition"
							, CStr::CFormat("'$(Configuration)|$(Platform)' == '{}|{}'") 
							<< iConfig.f_GetKey().m_Configuration 
							<< Platform
						)
					;
				}
			}

			// Globals
			{
				auto pGlobals = pPreProjectPropertyGroup;
				if (LanguageType == ELanguageType_Native)
				{
					pGlobals = CAOXmlUtils::f_CreateElement(pProject, "PropertyGroup");
					CAOXmlUtils::f_SetAttribute(pGlobals, "Label", "Globals");
				}

				CAOXmlUtils::f_AddElementAndText(pGlobals, "ProjectGuid", _Project.f_GetGUID());
				if (LanguageType == ELanguageType_Native)
					CAOXmlUtils::f_AddElementAndText(pGlobals, "RootNamespace", _Project.f_GetName());
				if (m_bEnableSourceControl)
				{
					CAOXmlUtils::f_AddElementAndText(pGlobals, "SccProjectName", "Perforce Project");
					CStr SccLocalPath = CFile::fs_MakePathRelative(m_BuildSystem.f_GetBaseDir(), _OutputDir).f_ReplaceChar('/', '\\');
					CAOXmlUtils::f_AddElementAndText(pGlobals, "SccLocalPath", SccLocalPath);
					CAOXmlUtils::f_AddElementAndText(pGlobals, "SccProvider", "MSSCCI:Perforce SCM");
				}
				if (LanguageType == ELanguageType_Native && m_Version == 2012)
				{
					auto pTargetsPath = CAOXmlUtils::f_AddElementAndText(pGlobals, "VCTargetsPath", "$(VCTargetsPath11)");
					CAOXmlUtils::f_SetAttribute(pTargetsPath, "Condition", "'$(VCTargetsPath11)' != ''");
					//CAOXmlUtils::f_SetAttribute(pTargetsPath, "Condition", "'$(VCTargetsPath11)' != '' and '$(VSVersion)' == '' and $(VisualStudioVersion) == ''");
				}
			}

			// Default imports
			CEntity const *pImportTargets = nullptr;
			{
				CEntityKey EntityKey;
				EntityKey.m_Type = EEntityType_GeneratorSetting;
				if (LanguageType == ELanguageType_Native)
					EntityKey.m_Name = "Native_ImportTargets";
				else if (LanguageType == ELanguageType_CSharp)
					EntityKey.m_Name = "CSharp_ImportTargets";
				auto pFind = m_pGeneratorSettings->m_ChildEntitiesMap.f_FindEqual(EntityKey);
				if (pFind)
					pImportTargets = pFind;
			}

			{
				CEntityKey EntityKey;
				EntityKey.m_Type = EEntityType_GeneratorSetting;
				if (pImportTargets)
				{
					EntityKey.m_Name = "Defaults";
					auto pInner = pImportTargets->m_ChildEntitiesMap.f_FindEqual(EntityKey);
					if (pInner)
					{
						DLockReadLocked(pInner->m_Lock);
						for (auto iImport = pInner->m_EvaluatedProperties.f_GetIterator(); iImport; ++iImport)
						{
							auto pImport = CAOXmlUtils::f_CreateElement(pProject, "Import");
							CAOXmlUtils::f_SetAttribute(pImport, "Project", iImport->m_Value);
						}
					}
				}
			}

			// Configuration
			TCMap<CConfiguration, CConfigResult> TargetTypes;
			
			{
				CAOXmlElement *pConfiguration = pPreProjectPropertyGroup;
				TCVector<CStr> SearchList;
				if (LanguageType == ELanguageType_Native)
				{
					pConfiguration = CAOXmlUtils::f_CreateElement(pProject, "PropertyGroup");
					CAOXmlUtils::f_SetAttribute(pConfiguration, "Label", "Configuration");
				}

				SearchList.f_Insert(SettingsPrefix + "Root");
				if (LanguageType == ELanguageType_CSharp)
					SearchList.f_Insert("DotNet_Root");

				TCMap<CStr, CAOXmlElement *> Parents;
				Parents[CStr()] = pConfiguration;
				auto fl_AddTargetConfig
					= [&](CStr const &_Type)
					{
						return f_AddConfigValue
							(
								_Project.m_EnabledProjectConfigs
								, _Project.m_EnabledProjectConfigs
								, (*_Project.m_EnabledProjectConfigs.f_GetIterator())->m_Position
								, EPropertyType_Target
								, _Type
								, Parents
								, ""
								, false
								, &SearchList
								, nullptr
								, CStr()
								, false
								, false
								, false
								, false
								, _Project
							)
						;
					}
				;
				TargetTypes = fl_AddTargetConfig("Type");

				fl_AddTargetConfig("IntermediateDirectory");
				fl_AddTargetConfig("OutputDirectory");

				if (LanguageType == ELanguageType_Native)
				{
					fl_AddTargetConfig("PlatformToolset");
					fl_AddTargetConfig("CharacterSet");
					fl_AddTargetConfig("UseOfMfc");
					fl_AddTargetConfig("UseOfAtl");
					fl_AddTargetConfig("LinkTimeCodeGeneration");
					fl_AddTargetConfig("DefaultToolArchitecture");
					fl_AddTargetConfig("CLRSupport");
					fl_AddTargetConfig("IgnoreWarnCompileDuplicatedFilename");

				}
				else if (LanguageType == ELanguageType_CSharp)
				{
					fl_AddTargetConfig("FileName");
					fl_AddTargetConfig("DefaultNamespace");
					fl_AddTargetConfig("ProjectDir");
					fl_AddTargetConfig("ToolPath");
				}
			}

			// Base path
			{
				CStr Path = CFile::fs_MakePathRelative(m_BuildSystem.f_GetBaseDir(), _OutputDir + "/Dummy").f_ReplaceChar('/', '\\');
				auto pProperties = CAOXmlUtils::f_CreateElement(pProject, "PropertyGroup");
				CAOXmlUtils::f_SetAttribute(pProperties, "Label", "Malterlib");
				CAOXmlUtils::f_AddElementAndText(pProperties, "MalterlibBaseDir", CStr::CFormat("$([System.IO.Path]::GetFullPath(\"$(MSBuildProjectDirectory){}\\\"))") << Path);
			}

			// Props
			{
				CEntityKey EntityKey;
				EntityKey.m_Type = EEntityType_GeneratorSetting;
				if (pImportTargets)
				{
					EntityKey.m_Name = "Props";
					auto pInner = pImportTargets->m_ChildEntitiesMap.f_FindEqual(EntityKey);
					if (pInner)
					{
						DLockReadLocked(pInner->m_Lock);
						for (auto iImport = pInner->m_EvaluatedProperties.f_GetIterator(); iImport; ++iImport)
						{
							auto pImport = CAOXmlUtils::f_CreateElement(pProject, "Import");
							CAOXmlUtils::f_SetAttribute(pImport, "Project", iImport->m_Value);
						}
					}
				}
			}


			// User proprety sheets
			{
				auto pImportGroup = CAOXmlUtils::f_CreateElement(pProject, "ImportGroup");
				CAOXmlUtils::f_SetAttribute(pImportGroup, "Label", "PropertySheets");
				if (LanguageType == ELanguageType_Native)
				{
					auto pImport = CAOXmlUtils::f_CreateElement(pImportGroup, "Import");
					CAOXmlUtils::f_SetAttribute(pImport, "Project", "$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props");
					CAOXmlUtils::f_SetAttribute(pImport, "Condition", "exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')");
				}
				{
					auto pImport = CAOXmlUtils::f_CreateElement(pImportGroup, "Import");
					CAOXmlUtils::f_SetAttribute(pImport, "Project", CFile::fs_GetFile(PropsFileName));
				}
			}

			// Version

			if (LanguageType == ELanguageType_Native)
			{
				auto pPropertyGroup = CAOXmlUtils::f_CreateElement(pProject, "PropertyGroup");
				CAOXmlUtils::f_AddElementAndText(pPropertyGroup, "_ProjectFileVersion", "10.0.21006.1");
			}

			CAOXmlElement *pProjectPropertyGroup = nullptr;
			CAOXmlElement *pProjectItemDefinitionGroup = nullptr;

			pProjectPropertyGroup = CAOXmlUtils::f_CreateElement(pPropsProject, "PropertyGroup");
			if (LanguageType == ELanguageType_Native)
				pProjectItemDefinitionGroup = CAOXmlUtils::f_CreateElement(pPropsProject, "ItemDefinitionGroup");

			CAOXmlElement *pCompileItemGroup_ClCompile = nullptr;
			CAOXmlElement *pCompileItemGroup_ClCompileAsC = nullptr;
			CAOXmlElement *pCompileItemGroup_ClCompileAsManaged = nullptr;
			CAOXmlElement *pCompileItemGroup_ClCompileShared = nullptr;
			CAOXmlElement *pCompileItemGroup_InnerClCompile = nullptr;
			CAOXmlElement *pCompileItemGroup_InnerClCompileAsC = nullptr;
			CAOXmlElement *pCompileItemGroup_InnerClCompileAsManaged = nullptr;
			TCLinkedList<CAOXmlElement *> Files_C;
			TCLinkedList<CAOXmlElement *> Files_Cpp;
			TCLinkedList<CAOXmlElement *> Files_CppManaged;
			if (LanguageType == ELanguageType_Native)
			{
				{
					auto pItemGroup = CAOXmlUtils::f_CreateElement(pPropsProject, "ItemDefinitionGroup");

					auto pCompile = CAOXmlUtils::f_CreateElement(pItemGroup, "ClCompile");
					pCompileItemGroup_ClCompileShared = pCompile;
				}
				{
					auto pTarget = CAOXmlUtils::f_CreateElement(pPropsProject, "Target");

					CAOXmlUtils::f_SetAttribute(pTarget, "Name", "TransformClCompileProperties_CompileAsC");
					CAOXmlUtils::f_SetAttribute(pTarget, "BeforeTargets", "ClCompile");
//						CAOXmlUtils::f_SetAttribute(pTarget, "DependsOnTargets", "PrepareForBuild");
				
					auto pItemGroup = CAOXmlUtils::f_CreateElement(pTarget, "ItemGroup");
					pCompileItemGroup_ClCompileAsC = pItemGroup;

					auto pCompile = CAOXmlUtils::f_CreateElement(pItemGroup, "ClCompile");
					pCompileItemGroup_InnerClCompileAsC = pCompile;

					CAOXmlUtils::f_SetAttribute(pCompile, "Condition", "'%(ClCompile.CompileAs)'=='CompileAsC'");
				}
				{
					auto pTarget = CAOXmlUtils::f_CreateElement(pPropsProject, "Target");

					CAOXmlUtils::f_SetAttribute(pTarget, "Name", "TransformClCompileProperties_CompileAsManaged");
					CAOXmlUtils::f_SetAttribute(pTarget, "BeforeTargets", "ClCompile");
//						CAOXmlUtils::f_SetAttribute(pTarget, "DependsOnTargets", "PrepareForBuild");
				
					auto pItemGroup = CAOXmlUtils::f_CreateElement(pTarget, "ItemGroup");
					pCompileItemGroup_ClCompileAsManaged = pItemGroup;

					auto pCompile = CAOXmlUtils::f_CreateElement(pItemGroup, "ClCompile");
					pCompileItemGroup_InnerClCompileAsManaged = pCompile;

					CAOXmlUtils::f_SetAttribute(pCompile, "Condition", "'%(ClCompile.CompileAs)'=='CompileAsManaged'");
				}
			
				{
					auto pTarget = CAOXmlUtils::f_CreateElement(pPropsProject, "Target");

					CAOXmlUtils::f_SetAttribute(pTarget, "Name", "TransformClCompileProperties_NotCompileAsC");
					CAOXmlUtils::f_SetAttribute(pTarget, "BeforeTargets", "ClCompile");
//						CAOXmlUtils::f_SetAttribute(pTarget, "DependsOnTargets", "PrepareForBuild");
				
					auto pItemGroup = CAOXmlUtils::f_CreateElement(pTarget, "ItemGroup");
					pCompileItemGroup_ClCompile = pItemGroup;

					auto pCompile = CAOXmlUtils::f_CreateElement(pItemGroup, "ClCompile");
					pCompileItemGroup_InnerClCompile = pCompile;

					CAOXmlUtils::f_SetAttribute(pCompile, "Condition", "('%(ClCompile.CompileAs)'=='' or '%(ClCompile.CompileAs)'=='CompileAsCpp')");
				}
			}

			struct CCompilType
			{
				CStr m_VSType;
				TCSet<CConfiguration> m_EnabledConfigs;
			};

			TCMap<CStr, CCompilType> CompileTypes;
			CAOXmlElement *pFileItemGroup = nullptr;
			// Files
			auto fl_GenerateFiles
				= [&]()
				{
					if (!pFileItemGroup)
						pFileItemGroup = CAOXmlUtils::f_CreateElement(pProject, "ItemGroup");
					for (auto iFile = _Project.m_Files.f_GetIterator(); iFile; ++iFile)
					{
						if (iFile->m_bWasGenerated)
							continue;

						iFile->m_bWasGenerated = true;
						TCVector<CStr> SearchList;
						SearchList.f_Insert(SettingsPrefix + "Root");
						if (LanguageType == ELanguageType_CSharp)
							SearchList.f_Insert("DotNet_Root");
						TCMap<CStr, CAOXmlElement *> Parents;
						Parents[CStr()] = pFileItemGroup;
						auto Entities
							= f_AddConfigValue
							(
								iFile->m_EnabledConfigs
								, _Project.m_EnabledProjectConfigs
								, (*iFile->m_EnabledConfigs.f_GetIterator())->m_Position
								, EPropertyType_Compile
								, "Type"
								, Parents
								, "Include"
								, true
								, &SearchList
								, nullptr
								, CStr()
								, false
								, false // true
								, false
								, true
								, _Project
							)
						;
							
						if (Entities.f_IsEmpty())
							m_BuildSystem.fs_ThrowError((*iFile->m_EnabledConfigs.f_GetIterator())->m_Position, "Internal error");

						auto Result = *Entities.f_GetIterator();
						CStr VSType = CAOXmlUtils::f_GetValue(Result.m_pElement);
						iFile->m_VSType = VSType;
						iFile->m_VSFile = CAOXmlUtils::f_GetAttribute(Result.m_pElement, "Include");

						if (LanguageType == ELanguageType_CSharp)
						{
							CStr GroupPath = iFile->f_GetGroupPath().f_ReplaceChar('/', '\\');
							if (!GroupPath.f_IsEmpty())
								CAOXmlUtils::f_AddElementAndText(Result.m_pElement, "Link", GroupPath + "\\" + CFile::fs_GetFile(iFile->m_VSFile));
						}

						CStr UntranslatedType;
						for (auto &Value : Result.m_UntranslatedValues)
						{
							UntranslatedType = Value;
							ThreadLocal.m_CurrentCompileTypes[Value] = VSType;
						}
						for (auto &Value : Result.m_UntranslatedValues)
							CompileTypes[Value].m_VSType = VSType;

						for (auto iConfig = iFile->m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
						{
							CProperty const *pFromProperty = nullptr;
							CStr ThisValue = m_BuildSystem.f_EvaluateEntityProperty(**iConfig, EPropertyType_Compile, "Disabled", pFromProperty);
							if (ThisValue != "true")
							{
								for (auto &Value : Result.m_UntranslatedValues)
									CompileTypes[Value].m_EnabledConfigs[iConfig.f_GetKey()];
							}
						}


						if (LanguageType == ELanguageType_Native)
						{
							for (auto iConfig = iFile->m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
							{
								if (m_BuildSystem.f_EvaluateEntityProperty(**iConfig, EPropertyType_Compile, "PrecompilePrefixHeader") == "true")
								{
									CStr PrefixHeader = m_BuildSystem.f_EvaluateEntityProperty(**iConfig, EPropertyType_Compile, "PrefixHeader");
									if (!PrefixHeader.f_IsEmpty())
									{
										CStr FilePath = CFile::fs_GetPath((**iConfig).m_Position.m_FileName);
										CStr FullPrefixHeader = f_GetExpandedPath(PrefixHeader, FilePath);

										if (!ThreadLocal.f_FileExists(FullPrefixHeader))
										{
											// Try relative to output
											FullPrefixHeader = f_GetExpandedPath(PrefixHeader, ThreadLocal.m_CurrentOutputDir);
										}
							
										DCheck(!ThreadLocal.m_CurrentCompileTypes.f_IsEmpty());
							
										auto &PrefixHeaderMap = ThreadLocal.m_PrefixHeaders[ThreadLocal.m_CurrentCompileTypes][FullPrefixHeader];
										PrefixHeaderMap.m_bUsed = true;
									}
								}
							}
						}


						TCVector<CStr> SearchLists;
						if (LanguageType == ELanguageType_Native)
						{
							if (UntranslatedType == "C")
								SearchLists.f_Insert(SettingsPrefix + "CompileAsC");
							if (UntranslatedType == "C++Managed")
								SearchLists.f_Insert(SettingsPrefix + "CompileAsManaged");
						}
						if (VSType == "ClCompile")
							SearchLists.f_Insert(SettingsPrefix + VSType + ClCompileSuffix);
						else
							SearchLists.f_Insert(SettingsPrefix + VSType);

						SearchLists.f_Insert(SettingsPrefix + "CompileShared");
						if (LanguageType == ELanguageType_CSharp)
							SearchLists.f_Insert("DotNet_CompileShared");
						SearchLists.f_Insert("CompileShared");
						TCMap<CStr, CAOXmlElement *> FileParents;
						FileParents[CStr()] = Result.m_pElement;

						if (LanguageType == ELanguageType_Native)
						{
							if (UntranslatedType == "C")
								Files_C.f_Insert(Result.m_pElement);
							else if (UntranslatedType == "C++")
								Files_Cpp.f_Insert(Result.m_pElement);								
							else if (UntranslatedType == "C++Managed")
								Files_CppManaged.f_Insert(Result.m_pElement);								
						}

						f_SetEvaluatedValues
							(
								FileParents
								, iFile->m_EnabledConfigs
								, _Project.m_EnabledProjectConfigs
								, true
								, EPropertyType_Compile
								, &SearchLists
								, nullptr
								, CStr()
								, false
								, false // true
								, false
								, _Project
							)
						;
						ThreadLocal.m_CurrentCompileTypes.f_Clear();
					}
				}
			;

			fl_GenerateFiles();

			// Project properties
			{
				// Target
				{
					TCMap<CStr, CAOXmlElement *> ProjectParents;
					if (LanguageType == ELanguageType_Native)
					{
						ProjectParents[CStr()] = pProjectItemDefinitionGroup;
						ProjectParents["PropertyGroup"] = pProjectPropertyGroup;
					}
					else
					{
						ProjectParents[CStr()] = pProjectPropertyGroup;
						ProjectParents["PropertyGroup"] = pProjectPropertyGroup;
					}

					TCMap<CConfiguration, TCVector<CStr>> SearchLists;

					for (auto iType = TargetTypes.f_GetIterator(); iType; ++iType)
					{
						auto &List = SearchLists[iType.f_GetKey()];
						CStr TargetType = CAOXmlUtils::f_GetNodeText(iType->m_pElement);
						DCheck(!TargetType.f_IsEmpty());
						List.f_Insert(SettingsPrefix + TargetType);
						
						if (LanguageType == ELanguageType_Native)
						{
							if (TargetType == "StaticLibrary" || TargetType == "DynamicLibrary")
							{
								List.f_Insert(SettingsPrefix + "SharedLibrary");
								List.f_Insert("SharedLibrary");
							}
							if (TargetType == "Application" || TargetType == "DynamicLibrary")
							{
								List.f_Insert(SettingsPrefix + "SharedExecutable");
								List.f_Insert("SharedExecutable");
							}
						}
						else if (LanguageType == ELanguageType_CSharp)
						{
							if (TargetType == "Library")
							{
								List.f_Insert(SettingsPrefix + "SharedLibrary");
								List.f_Insert("DotNet_SharedLibrary");
								List.f_Insert("SharedLibrary");
							}
							if (TargetType == "Library" || TargetType == "WinExe" || TargetType == "Exe")
							{
								List.f_Insert(SettingsPrefix + "SharedExecutable");
								List.f_Insert("DotNet_SharedExecutable");
								List.f_Insert("SharedExecutable");
							}
						}
						List.f_Insert(SettingsPrefix + "SharedTarget");
						if (LanguageType == ELanguageType_CSharp)
							List.f_Insert("DotNet_SharedTarget");
						List.f_Insert("SharedTarget");
					}
					
					f_SetEvaluatedValues
						(
							ProjectParents
							, _Project.m_EnabledProjectConfigs
							, _Project.m_EnabledProjectConfigs
							, false
							, EPropertyType_Target
							, nullptr
							, &SearchLists
							, CStr()
							, false
							, false
							, LanguageType == ELanguageType_CSharp
							, _Project
						)
					;
				}

				// Compile types
				for (auto iType = CompileTypes.f_GetIterator(); iType; ++iType)
				{
					if (iType->m_EnabledConfigs.f_IsEmpty())
						continue;
					TCMap<CConfiguration, CBuildSystemData> Datas;
					TCMap<CConfiguration, CEntityPointer> Configs;

					TCMap<CStr, CAOXmlElement *> ProjectParents;
					if (LanguageType == ELanguageType_Native)
					{
						if (iType.f_GetKey() == "C")
							ProjectParents[CStr()] = pCompileItemGroup_ClCompileAsC;
						else if (iType.f_GetKey() == "C++Managed")
							ProjectParents[CStr()] = pCompileItemGroup_ClCompileAsManaged;
						else if (iType.f_GetKey() == "C++")
							ProjectParents[CStr()] = pCompileItemGroup_ClCompile;
						else
							ProjectParents[CStr()] = pProjectItemDefinitionGroup;
					}
					else if (LanguageType == ELanguageType_CSharp)
							ProjectParents[CStr()] = pProjectPropertyGroup;
					ProjectParents["PropertyGroup"] = pProjectPropertyGroup;

					//DCheck(iType.f_GetKey() != "C"); // Should be set at file level
					ThreadLocal.m_CurrentCompileTypes[iType.f_GetKey()] = iType->m_VSType;

					TCVector<CStr> SearchList;
					if (LanguageType == ELanguageType_Native)
					{
						if (iType.f_GetKey() == "C")
							SearchList.f_Insert(SettingsPrefix + "CompileAsC");
						if (iType.f_GetKey() == "C++Managed")
							SearchList.f_Insert(SettingsPrefix + "CompileAsManaged");
					}
					if (iType->m_VSType == "ClCompile")
						SearchList.f_Insert(SettingsPrefix + iType->m_VSType + ClCompileSuffix);
					else
						SearchList.f_Insert(SettingsPrefix + iType->m_VSType);

					SearchList.f_Insert(SettingsPrefix + "CompileShared");
					if (LanguageType == ELanguageType_CSharp)
						SearchList.f_Insert("DotNet_CompileShared");
					SearchList.f_Insert("CompileShared");

					TCMap<CPropertyKey, CStr> StartValuesCompile;
					StartValuesCompile[CPropertyKey(EPropertyType_Compile, "Type")] = iType.f_GetKey();

					for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
					{
						if (!iType->m_EnabledConfigs.f_FindEqual(iConfig.f_GetKey()))
							continue;
						auto &Data = Datas[iConfig.f_GetKey()];
							
						auto pStartEntity = iConfig->f_Get();
						auto InitialValues = m_BuildSystem.f_GetExternalValues(*(*iConfig)->f_GetRoot());
						auto PathKey = (*iConfig)->f_GetPathKey();
						auto pConfig = m_BuildSystem.f_EvaluateData(Data, InitialValues, pStartEntity, &StartValuesCompile, &PathKey, true, false);
						Configs[iConfig.f_GetKey()] = fg_Explicit(pConfig);
					}

					bool bPropertyValue = false;
					if (LanguageType == ELanguageType_Native)
					{
						if (iType.f_GetKey() == "C" || iType.f_GetKey() == "C++" || iType.f_GetKey() == "C++Managed")
							bPropertyValue = true;
					}

					CStr DefaultEntity;
					if (LanguageType == ELanguageType_Native)
						DefaultEntity = iType->m_VSType;

					//m_BuildSystem.f_Eval;
					f_SetEvaluatedValues
						(
							ProjectParents
							, Configs
							, _Project.m_EnabledProjectConfigs
							, false
							, EPropertyType_Compile
							, &SearchList
							, nullptr
							, DefaultEntity
							, false // bPropertyValue
							, false
							, LanguageType == ELanguageType_CSharp
							, _Project
						)
					;

					ThreadLocal.m_CurrentCompileTypes.f_Clear();
				}
			}

			if (LanguageType == ELanguageType_Native)
			{
				for (auto iCompilerType = ThreadLocal.m_PrefixHeaders.f_GetIterator(); iCompilerType; ++iCompilerType)
				{
					auto &CompilerTypes = iCompilerType.f_GetKey();
					for (auto iType = CompilerTypes.f_GetIterator(); iType; ++iType)
					{
						CStr CompileType = iType.f_GetKey();
						CStr VSCompileType = *iType;
						for (auto iPrefixHeader = iCompilerType->f_GetIterator(); iPrefixHeader; ++iPrefixHeader)
						{
							if (!iPrefixHeader->m_bUsed || iPrefixHeader->m_Configurations.f_IsEmpty())
								continue;

							CStr FullHeaderPath = iPrefixHeader.f_GetKey();
							TCMap<CConfiguration, CEntityPointer> EnabledConfigs;
							CFilePosition FilePos;
							TCPointer<CGroup> pGroup;
							bool bFile = false;

							{
								auto pFile = _Project.m_Files.f_FindEqual(FullHeaderPath);
								if (pFile)
								{
									EnabledConfigs = pFile->m_EnabledConfigs;
									FilePos = pFile->m_Position;
									pGroup = pFile->m_pGroup;
									bFile = true;
								}
								else
								{
									EnabledConfigs = _Project.m_EnabledProjectConfigs;
									if (!iPrefixHeader->m_Position.m_FileName.f_IsEmpty())
										FilePos = iPrefixHeader->m_Position;
									else
										FilePos = _Project.m_Position;
									if (!ThreadLocal.f_FileExists(FullHeaderPath))
										m_BuildSystem.fs_ThrowError(FilePos, CStr::CFormat("Prefix header '{}' was not found") << FullHeaderPath);
										
								}
								//	m_BuildSystem.fs_ThrowError(_Project.m_Position, CStr::CFormat("Prefix header '{}' not found in project") << FullHeaderPath);
							}

							CStr RelativePath = CFile::fs_MakePathRelative(FullHeaderPath, PrefixGenDir);
					
							CStr GUID = CUniversallyUniqueIdentifier(EUniversallyUniqueIdentifierGenerate_StringHash, g_GeneratorPrefixHeaderUUIDNamespace, RelativePath + CompileType).f_GetAsString();
					
							CStr FileName;
							if (CompileType == "C++")
								FileName = CFile::fs_AppendPath(PrefixGenDir, CStr::CFormat("PP_{}_{nfh,sj8,sf0}.cpp") << _Project.m_EntityName << GUID.f_Hash());
							else if (CompileType == "C++Managed")
								FileName = CFile::fs_AppendPath(PrefixGenDir, CStr::CFormat("PPM_{}_{nfh,sj8,sf0}.cpp") << _Project.m_EntityName << GUID.f_Hash());
							else if (CompileType == "C")
								FileName = CFile::fs_AppendPath(PrefixGenDir, CStr::CFormat("PP_{}_{nfh,sj8,sf0}.c") << _Project.m_EntityName << GUID.f_Hash());
							else
								m_BuildSystem.fs_ThrowError(FilePos, CStr::CFormat("Don't know how to generate precompiled header for compiler type {}") << CompileType);
								
							CStr FileContents;
							FileContents += CStr::CFormat("#include \"{}\"\r\n") << RelativePath;

							bool bWasCreated = false;
							if (!m_BuildSystem.f_AddGeneratedFile(FileName, FileContents, _Project.m_pSolution->f_GetName(), bWasCreated, false))
								m_BuildSystem.fs_ThrowError(FilePos, CStr::CFormat("File '{}' already generated with other contents") << FileName);

							if (bWasCreated)
							{
								TCVector<uint8> FileData;
								CFile::fs_WriteStringToVector(FileData, CStr(FileContents));
								m_BuildSystem.f_WriteFile(FileData, FileName);
							}

							auto FileMap = _Project.m_Files(FileName);

							auto &File = FileMap.f_GetResult();

							File.m_Position = FilePos;

							File.m_pGroup = pGroup;

							CStr PCHFile = CStr::CFormat("$(IntDir){}_{}.pch") << _Project.m_EntityName << GUID;

							for (auto iCondition = iPrefixHeader->m_Elements.f_GetIterator(); iCondition; ++iCondition)
							{
								for (auto iElement = iCondition->f_GetIterator(); iElement; ++iElement)
								{
									auto pNewElement = CAOXmlUtils::f_AddElementAndText(*iElement, "PrecompiledHeaderOutputFile", PCHFile);
#if 0
									CStr Condition;
									Condition = "'%(ClCompile.DefinedProperty_PrecompiledHeaderOutputFile)' != 'true'";
									if (!iCondition.f_GetKey().f_IsEmpty())
									{
										Condition += " and (";
										Condition += iCondition.f_GetKey();
										Condition += ")";
									}
									CAOXmlUtils::f_SetAttribute(pNewElement, "Condition", Condition);
#else
									if (!iCondition.f_GetKey().f_IsEmpty())
										CAOXmlUtils::f_SetAttribute(pNewElement, "Condition", iCondition.f_GetKey());
#endif
								}
							}

							DCheck(!iPrefixHeader->m_Configurations.f_IsEmpty());

							for (auto iConfig = iPrefixHeader->m_Configurations.f_GetIterator(); iConfig; ++iConfig)
							{
								CStr Platform = _Project.m_Platforms[iConfig.f_GetKey()];
								auto pFileConfig = EnabledConfigs.f_FindEqual(iConfig.f_GetKey());
								if (!pFileConfig)
								{
									m_BuildSystem.fs_ThrowError
										(
											FilePos
											, CStr::CFormat("Prefix header is not enabled for config '{} - {}'") 
											<< Platform
											<< iConfig.f_GetKey().m_Configuration
										)
									;
								}
								CEntity *pParent;
								if ((*pFileConfig)->m_Key.m_Type == EEntityType_File)
									pParent = (*pFileConfig)->m_pParent;
								else
									pParent = &fg_RemoveQualifiers(*(*pFileConfig));
								CEntityKey NewEntityKey;
								NewEntityKey.m_Type = EEntityType_File;
								NewEntityKey.m_Name = FileName;
								auto NewEntityMap = pParent->m_ChildEntitiesMap(NewEntityKey, pParent);
								auto &NewEntity = *NewEntityMap;
								pParent->m_ChildEntitiesOrdered.f_Insert(NewEntity);
								if (bFile)
									NewEntity.m_Properties = (*pFileConfig)->m_Properties;
								NewEntity.m_Key = NewEntityKey;
								NewEntity.m_Position = FilePos;
								TCMap<CPropertyKey, CStr> Values;
								Values[CPropertyKey(EPropertyType_Compile, "Type")] = CompileType;
								m_BuildSystem.f_InitEntityForEvaluationNoEnv(NewEntity, Values);
								{
									NewEntity.f_AddProperty
										(
											CPropertyKey(EPropertyType_Compile, "PrecompilePrefixHeader")
											, "XInternalCreate"
											, FilePos
										)
									;
								}
								{
									NewEntity.f_AddProperty
										(
											CPropertyKey(EPropertyType_Compile, "XInternalPrecompiledHeaderOutputFile")
											, PCHFile
											, FilePos
										)
									;
								}
								m_BuildSystem.f_ReEvaluateData(NewEntity);
								File.m_EnabledConfigs[iConfig.f_GetKey()] = fg_Explicit(&NewEntity);
							}
						}
					}
				}
				fl_GenerateFiles();
			}

			// Dependencies
			{
				auto pItemGroup = CAOXmlUtils::f_CreateElement(pProject, "ItemGroup");

				// Project dependencies
				{
					for (auto iDependency = _Project.m_Dependencies.f_GetIterator(); iDependency; ++iDependency)
					{
						auto &Dependency = *iDependency;
						bool bInvalid = false;
						for (auto iConfig = Dependency.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
						{
							if (!_Project.m_EnabledProjectConfigs.f_FindEqual(iConfig.f_GetKey()))
							{
								bInvalid = true;
								break;
							}
						}
						for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
						{
							if (!Dependency.m_EnabledConfigs.f_FindEqual(iConfig.f_GetKey()))
							{
								bInvalid = true;
								break;
							}
						}
						if (bInvalid)
						{
							CStr Configs0;
							for (auto iConfig = Dependency.m_EnabledConfigs.f_GetIterator(); iConfig; ++iConfig)
								fg_AddStrSep(Configs0, iConfig.f_GetKey().f_GetFullName(), "\n");
							CStr Configs1;
							for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
								fg_AddStrSep(Configs1, iConfig.f_GetKey().f_GetFullName(), "\n");
							m_BuildSystem.fs_ThrowError(Dependency.m_Position, fg_Format("Dependencies cannot be varied per configuration ({}):\n{}\n!=\n{}\n", _Project.f_GetName(), Configs0, Configs1));
						}

						CStr ProjectDependency = iDependency.f_GetKey();

						auto pDependProject = _Project.m_pSolution->m_Projects.f_FindEqual(ProjectDependency);

						if (!pDependProject)
							m_BuildSystem.fs_ThrowError(Dependency.m_Position, CStr::CFormat("Dependency {} not found in workspace") << ProjectDependency);

						for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
						{
							if (!pDependProject->m_EnabledProjectConfigs.f_FindEqual(iConfig.f_GetKey()))
								m_BuildSystem.fs_ThrowError
								(
									Dependency.m_Position
									, CStr::CFormat("Dependency project does not have required configuration {} - {}") 
									<< _Project.m_Platforms[iConfig.f_GetKey()]
									<< iConfig.f_GetKey().m_Configuration
								)
							;
						}

						auto pReference = CAOXmlUtils::f_CreateElement(pItemGroup, "ProjectReference");
						CStr DependencyProjectFile = CFile::fs_MakeNiceFilename(pDependProject->f_GetName()) + ".vcxproj";
						CAOXmlUtils::f_SetAttribute(pReference, "Include", DependencyProjectFile);
						CAOXmlUtils::f_AddElementAndText(pReference, "Project", pDependProject->f_GetGUID());
						CAOXmlUtils::f_AddElementAndText(pReference, "Name", pDependProject->f_GetName());

						TCVector<CStr> SearchLists;
						SearchLists.f_Insert(SettingsPrefix + "Dependency");
						if (LanguageType == ELanguageType_CSharp)
							SearchLists.f_Insert("DotNet_Dependency");

						SearchLists.f_Insert("Dependency");
						TCMap<CStr, CAOXmlElement *> FileParents;
						FileParents[CStr()] = pReference;

						f_SetEvaluatedValues
							(
								FileParents
								, Dependency.m_EnabledConfigs
								, _Project.m_EnabledProjectConfigs
								, false
								, EPropertyType_Dependency
								, &SearchLists
								, nullptr
								, CStr()
								, false
								, false
								, false
								, _Project
							)
						;
					}
				}

				// External dependencies
				{
					CFilePosition Position;
					CStr ExternalDependencies = fl_GetEntityPropertyGlobal(EPropertyType_Target, "ExternalDependencies", Position);

					while (!ExternalDependencies.f_IsEmpty())
					{
						CStr Dependency = fg_GetStrSep(ExternalDependencies, ";");
						auto pReference = CAOXmlUtils::f_CreateElement(pItemGroup, "Reference");
						CAOXmlUtils::f_SetAttribute(pReference, "Include", Dependency);
					}
				}
			}

			// MSBuild Targets
			{
				CEntityKey EntityKey;
				EntityKey.m_Type = EEntityType_GeneratorSetting;
				if (pImportTargets)
				{
					EntityKey.m_Name = "Targets";
					auto pInner = pImportTargets->m_ChildEntitiesMap.f_FindEqual(EntityKey);
					if (pInner)
					{
						DLockReadLocked(pInner->m_Lock);
						for (auto iImport = pInner->m_EvaluatedProperties.f_GetIterator(); iImport; ++iImport)
						{
							auto pImport = CAOXmlUtils::f_CreateElement(pProject, "Import");
							CAOXmlUtils::f_SetAttribute(pImport, "Project", iImport->m_Value);
						}
					}
				}
			}
			
			if (LanguageType == ELanguageType_Native)
			{
				mint nCompileTypes = 0;
				bool bCppEnabled = false;
				TCMap<CStr, TCLinkedList<CAOXmlElement *>> Same;
				TCSet<CStr> FullyDefinedC;
				TCSet<CStr> FullyDefinedManaged;
				TCVector<TCSet<CStr> *> FullyDefined;
				{
					CAOXmlNode const *pChild = nullptr;
					bool bAdded = false;
					while ((pChild = CAOXmlUtils::f_Iterate(pCompileItemGroup_InnerClCompileAsC, pChild)))
					{
						if (pChild->Type() == TiXmlNode::ELEMENT)
						{
							if (!bAdded)
							{
								++nCompileTypes;
								bAdded = true;
								FullyDefined.f_Insert(&FullyDefinedC);
							}
							Same[CAOXmlUtils::f_GetAsString(pChild)].f_Insert((CAOXmlElement *)pChild);
							CStr Name = CAOXmlUtils::f_GetValue(pChild);
							CStr Value = CAOXmlUtils::f_GetNodeText(pChild);

							if (Value.f_Find(CStr(CStr::CFormat("%(ClCompile.{})") << Name)) < 0)
								FullyDefinedC[Name];
						}
					}
				}
				{
					CAOXmlNode const *pChild = nullptr;
					bool bAdded = false;
					while ((pChild = CAOXmlUtils::f_Iterate(pCompileItemGroup_InnerClCompileAsManaged, pChild)))
					{
						if (pChild->Type() == TiXmlNode::ELEMENT)
						{
							if (!bAdded)
							{
								++nCompileTypes;
								bAdded = true;
								FullyDefined.f_Insert(&FullyDefinedManaged);
							}
							Same[CAOXmlUtils::f_GetAsString(pChild)].f_Insert((CAOXmlElement *)pChild);
							CStr Name = CAOXmlUtils::f_GetValue(pChild);
							CStr Value = CAOXmlUtils::f_GetNodeText(pChild);

							if (Value.f_Find(CStr(CStr::CFormat("%(ClCompile.{})") << Name)) < 0)
								FullyDefinedManaged[Name];
						}
					}
				}
				{
					CAOXmlNode const *pChild = nullptr;
					bool bAdded = false;
					while ((pChild = CAOXmlUtils::f_Iterate(pCompileItemGroup_InnerClCompile, pChild)))
					{
						if (pChild->Type() == TiXmlNode::ELEMENT)
						{
							if (!bAdded)
							{
								++nCompileTypes;
								bAdded = true;
								bCppEnabled = true;
							}
							Same[CAOXmlUtils::f_GetAsString(pChild)].f_Insert((CAOXmlElement *)pChild);
						}
					}
				}

				if (nCompileTypes <= 1)
				{
					for (auto iSame = Same.f_GetIterator(); iSame; ++iSame)
						pCompileItemGroup_ClCompileShared->InsertEndChild(*iSame->f_GetFirst());
				}
				else
				{
					for (auto iSame = Same.f_GetIterator(); iSame; ++iSame)
					{
						if (iSame->f_GetLen() >= nCompileTypes)
						{
							pCompileItemGroup_ClCompileShared->InsertEndChild(*iSame->f_GetFirst());
							for (auto iXml = iSame->f_GetIterator(); iXml; ++iXml)
							{
								(*iXml)->Parent()->RemoveChild(*iXml);
							}
						}
						else
						{
							auto pElement = iSame->f_GetFirst();

							auto pParent = pElement->Parent();

							if (*pParent == *pCompileItemGroup_InnerClCompileAsC)
							{
								for (auto iFile = Files_C.f_GetIterator(); iFile; ++iFile)
								{
									CAOXmlNode *pChild = nullptr;
									if ((pChild = CAOXmlUtils::f_Iterate((*iFile), pChild)))
										(*iFile)->InsertBeforeChild(pChild, *pElement);
									else
										(*iFile)->InsertEndChild(*pElement);
								}
							}
							else if (*pParent == *pCompileItemGroup_InnerClCompileAsManaged)
							{
								if (!bCppEnabled)
								{
									CStr Name = CAOXmlUtils::f_GetValue(pElement);
									bool bFullyDefined = true;
									for (auto iDefined = FullyDefined.f_GetIterator(); iDefined; ++iDefined)
									{
										if (!(**iDefined).f_FindEqual(Name))
										{
											bFullyDefined = false;
											break;
										}
									}
									if (bFullyDefined)
									{
										pCompileItemGroup_ClCompileShared->InsertEndChild(*pElement);
									}
									else
									{
										for (auto iFile = Files_CppManaged.f_GetIterator(); iFile; ++iFile)
										{
											CAOXmlNode *pChild = nullptr;
											if ((pChild = CAOXmlUtils::f_Iterate((*iFile), pChild)))
												(*iFile)->InsertBeforeChild(pChild, *pElement);
											else
												(*iFile)->InsertEndChild(*pElement);
										}
									}
								}
								else
								{
									for (auto iFile = Files_CppManaged.f_GetIterator(); iFile; ++iFile)
									{
										CAOXmlNode *pChild = nullptr;
										if ((pChild = CAOXmlUtils::f_Iterate((*iFile), pChild)))
											(*iFile)->InsertBeforeChild(pChild, *pElement);
										else
											(*iFile)->InsertEndChild(*pElement);
									}
								}
							}
							else
							{
								CStr Name = CAOXmlUtils::f_GetValue(pElement);
								bool bFullyDefined = true;
								for (auto iDefined = FullyDefined.f_GetIterator(); iDefined; ++iDefined)
								{
									if (!(**iDefined).f_FindEqual(Name))
									{
										bFullyDefined = false;
										break;
									}
								}
								if (bFullyDefined)
								{
									pCompileItemGroup_ClCompileShared->InsertEndChild(*pElement);
								}
								else
								{
									DCheck(*pParent == *pCompileItemGroup_InnerClCompile);
									for (auto iFile = Files_Cpp.f_GetIterator(); iFile; ++iFile)
									{
										CAOXmlNode *pChild = nullptr;
										if ((pChild = CAOXmlUtils::f_Iterate((*iFile), pChild)))
											(*iFile)->InsertBeforeChild(pChild, *pElement);
										else
											(*iFile)->InsertEndChild(*pElement);
									}
								}
							}
						}
					}
				}

				pCompileItemGroup_ClCompile->Parent()->Parent()->RemoveChild(pCompileItemGroup_ClCompile->Parent());
				pCompileItemGroup_ClCompileAsC->Parent()->Parent()->RemoveChild(pCompileItemGroup_ClCompileAsC->Parent());
				pCompileItemGroup_ClCompileAsManaged->Parent()->Parent()->RemoveChild(pCompileItemGroup_ClCompileAsManaged->Parent());

			}

		}

		{
			CStr XMLData = XMLFile.f_GetAsString();
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(FileName, XMLData, _Project.m_pSolution->f_GetName(), bWasCreated, false))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(XMLData));
				m_BuildSystem.f_WriteFile(FileData, FileName);
			}

			_Project.m_FileName = FileName;
		}
		{
			CStr XMLData = PropsXMLFile.f_GetAsString();
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(PropsFileName, XMLData, _Project.m_pSolution->f_GetName(), bWasCreated, false))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(XMLData));
				m_BuildSystem.f_WriteFile(FileData, PropsFileName);
			}

			_Project.m_FileName = FileName;
		}
			
		if (LanguageType == ELanguageType_Native)
		{
			CAOXmlUtils FilterXML;
			auto pProject = FilterXML.f_CreateDefaultDocument("Project");
			CAOXmlUtils::f_SetAttribute(pProject, "ToolsVersion", f_GetToolsVersion());
			CAOXmlUtils::f_SetAttribute(pProject, "xmlns", "http://schemas.microsoft.com/developer/msbuild/2003");

			// Files
			{
				auto pItemGroup = CAOXmlUtils::f_CreateElement(pProject, "ItemGroup");
				for (auto iFile = _Project.m_Files.f_GetIterator(); iFile; ++iFile)
				{
					auto pFileElement = CAOXmlUtils::f_CreateElement(pItemGroup, iFile->m_VSType);
					CAOXmlUtils::f_SetAttribute(pFileElement, "Include", iFile->m_VSFile);
					CStr GroupPath = iFile->f_GetGroupPath().f_ReplaceChar('/', '\\');
					if (!GroupPath.f_IsEmpty())
						CAOXmlUtils::f_AddElementAndText(pFileElement, "Filter", GroupPath);
				}
			}

			// Groups
			{
				auto pItemGroup = CAOXmlUtils::f_CreateElement(pProject, "ItemGroup");
				for (auto iGroup = _Project.m_Groups.f_GetIterator(); iGroup; ++iGroup)
				{
					auto pGroupElement = CAOXmlUtils::f_CreateElement(pItemGroup, "Filter");
					CAOXmlUtils::f_SetAttribute(pGroupElement, "Include", iGroup->f_GetPath().f_ReplaceChar('/','\\'));
					CAOXmlUtils::f_AddElementAndText(pGroupElement, "UniqueIdentifier", iGroup->f_GetGUID());
				}
			}

			CStr XMLData = FilterXML.f_GetAsString();
			CStr FiltersFileName = FileName+".filters";
			bool bWasCreated = false;
			if (!m_BuildSystem.f_AddGeneratedFile(FiltersFileName, XMLData, _Project.m_pSolution->f_GetName(), bWasCreated, false))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FiltersFileName));

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(XMLData));
				m_BuildSystem.f_WriteFile(FileData, FiltersFileName);
			}
		}
	}
}
