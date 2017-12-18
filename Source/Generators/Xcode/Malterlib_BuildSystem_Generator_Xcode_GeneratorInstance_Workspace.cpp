// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NXcode
{
	void CGeneratorInstance::f_GenerateWorkspaceFile(CSolution &_Solution, CStr const &_OutputDir, mint _MaxWorkspaceNameLen) const
	{
		auto & ThreadLocal = *m_ThreadLocal;
		CXMLDocument XMLFile(false);
		auto pOldFile = ThreadLocal.m_pXMLFile;
		ThreadLocal.m_pXMLFile = &XMLFile;
		auto Cleanup = fg_OnScopeExit
			(
				[&]()
				{
					ThreadLocal.m_pXMLFile = pOldFile;
				}
			)
		;
		
		CTimer Timer;
		Timer.f_Start();
		CStr OutputDir = CFile::fs_AppendPath(CFile::fs_AppendPath(_OutputDir, "Files"), CStr(CFile::fs_MakeNiceFilename(_Solution.f_GetName())));
		ThreadLocal.f_CreateDirectory(OutputDir);
		
		CMutual SchemesWithRunnablesLock;
		TCMap<CConfiguration, TCSet<CStr>> SchemesWithRunnables;
		TCMap<CConfiguration, TCMap<CStr, CStr>> SchemesWithBuildables;

		fg_ParallellForEach
			(
				_Solution.m_Projects
				, [&](CProject &_Project)
				{
					TCMap<CConfiguration, TCSet<CStr>> Runnables;
					TCMap<CConfiguration, TCMap<CStr, CStr>> Buildables;
					f_GenerateProjectFile(_Project, OutputDir, Runnables, Buildables);
					{
						DLock(SchemesWithRunnablesLock);
						for (auto iConfig = Runnables.f_GetIterator(); iConfig; ++iConfig)
						{
							SchemesWithRunnables[iConfig.f_GetKey()] += *iConfig;
						}
						for (auto iConfig = Buildables.f_GetIterator(); iConfig; ++iConfig)
						{
							SchemesWithBuildables[iConfig.f_GetKey()] += *iConfig;
						}
					}
				}
			)
		;
		
		// Workspace root
		auto pWorkspace = XMLFile.f_CreateDefaultDocument("Workspace");
		XMLFile.f_SetAttribute(pWorkspace, "version", "1.0");
		
		TCMap<TCPointer<CGroup>, TCVector<CGroup*>> MapGroupToGroups;
		TCMap<TCPointer<CGroup>, TCVector<CProject*>> MapProjectsToGroups;
		
		for (auto iGroup = _Solution.m_Groups.f_GetIterator(); iGroup; ++iGroup)
		{
			if (iGroup->m_pParent)
				MapGroupToGroups[iGroup->m_pParent].f_Insert(iGroup);
		}
		
		for (auto iProject = _Solution.m_Projects.f_GetIterator(); iProject; ++iProject)
		{
			if (iProject->m_pGroup)
				MapProjectsToGroups[iProject->m_pGroup].f_Insert(iProject);
		}
		
		TCFunction<void (CXMLElement* _pParent, CGroup* pGroup)> fl_OutputGroup = [&] (CXMLElement* _pParent, CGroup* pGroup)
		{
			if (!pGroup->m_OutputToWorkspace)
			{
				auto pGroupRef = XMLFile.f_CreateElement(_pParent, "Group");
				XMLFile.f_SetAttribute(pGroupRef, "location", "container:");
				XMLFile.f_SetAttribute(pGroupRef, "name", pGroup->m_Name);
				pGroup->m_OutputToWorkspace = true;
				
				TCVector<CGroup*>& lChildGroups = MapGroupToGroups[pGroup];
				for (auto iChildGroup = lChildGroups.f_GetIterator(); iChildGroup; ++iChildGroup)
				{
					fl_OutputGroup(pGroupRef, *iChildGroup);
				}
				
				TCVector<CProject*>& lChildProjects = MapProjectsToGroups[pGroup];
				for (auto iProject = lChildProjects.f_GetIterator(); iProject; ++iProject)
				{
					auto pFileRef = XMLFile.f_CreateElement(pGroupRef, "FileRef");
					XMLFile.f_SetAttribute(pFileRef, "location", (CStr::CFormat("group:Files/{}/{}.xcodeproj") << CFile::fs_MakeNiceFilename(_Solution.f_GetName()) << (*iProject)->f_GetName()).f_GetStr());
				}
			}
		};
		
		for (auto iGroup = _Solution.m_Groups.f_GetIterator(); iGroup; ++iGroup)
		{
			fl_OutputGroup(pWorkspace, &(*iGroup));
		}
		
		for (auto iProject = _Solution.m_Projects.f_GetIterator(); iProject; ++iProject)
		{
			if (!iProject->m_pGroup)
			{
				auto pFileRef = XMLFile.f_CreateElement(pWorkspace, "FileRef");
				XMLFile.f_SetAttribute(pFileRef, "location", (CStr::CFormat("group:Files/{}/{}.xcodeproj") << CFile::fs_MakeNiceFilename(_Solution.f_GetName()) << (*iProject).f_GetName()).f_GetStr());
			}
		}

		OutputDir = CFile::fs_AppendPath(_OutputDir, CStr(CFile::fs_MakeNiceFilename(_Solution.f_GetName()) + ".xcworkspace"));
		ThreadLocal.f_CreateDirectory(OutputDir);
		CStr GeneratedFile = CFile::fs_AppendPath(OutputDir,  CStr("generatedContainer"));
		{
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(GeneratedFile, CStr(), _Solution.f_GetName(), bWasCreated, false))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << GeneratedFile));

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr());
				m_BuildSystem.f_WriteFile(FileData, GeneratedFile);
			}
		}
		CStr FileName = CFile::fs_AppendPath(OutputDir,  CStr("contents.xcworkspacedata"));
		bool bSchemeChanged = fp_GenerateBuildAllSchemes(_Solution, OutputDir, SchemesWithRunnables, SchemesWithBuildables);
		{
			CStr XMLData = XMLFile.f_GetAsString(EXMLOutputDialect_Xcode);
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(FileName, XMLData, _Solution.f_GetName(), bWasCreated, false))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(XMLData));
				bool bFileWritten = m_BuildSystem.f_WriteFile(FileData, FileName);
				if (!bFileWritten && bSchemeChanged)
				{
					// Force Xcode to reload schemes
					NTime::CTime Now = NTime::CTime::fs_NowUTC();
					NFile::CFile Temp;
					Temp.f_Open(FileName, EFileOpen_WriteAttribs | EFileOpen_ShareAll | EFileOpen_DontTruncate);
					Temp.f_SetWriteTime(Now);		
					m_BuildSystem.f_SetFileChanged(FileName);
				}
			}
		}
		
		Timer.f_Stop();
		DConOut("Generated workspace: {sl*,a-} {fe2} s{\n}", _Solution.f_GetName() << _MaxWorkspaceNameLen << Timer.f_GetTime());
	}
	
	bool CGeneratorInstance::fp_GenerateBuildAllSchemes(CSolution &_Solution, CStr const &_OutputDir, TCMap<CConfiguration, TCSet<CStr>> &_Runnables, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable) const
	{
		bool bSchemesChanged = false;
		auto & ThreadLocal = *m_ThreadLocal;
		
		{
			
ch8 const *pDocumentData = 
R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>IDEWorkspaceSharedSettings_AutocreateContextsIfNeeded</key>
	<false/>
</dict>
</plist>
)xxx";
			CStr DocumentData = pDocumentData;

			CStr FileName = CFile::fs_AppendPath(_OutputDir, CStr("xcshareddata/WorkspaceSettings.xcsettings"));
			
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(FileName, DocumentData, _Solution.f_GetName(), bWasCreated, true))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));
			
			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(DocumentData), false);
				ThreadLocal.f_CreateDirectory(CFile::fs_GetPath(FileName));
				if (m_BuildSystem.f_WriteFile(FileData, FileName))
					bSchemesChanged = true;
			}
		}
		
		CStr OutputDir = CFile::fs_AppendPath(_OutputDir, CStr("xcshareddata/xcschemes"));
		ThreadLocal.f_CreateDirectory(OutputDir);
		
		for (auto Iter = _Solution.m_EnabledConfigs.f_GetIterator(); Iter; ++Iter)
		{
			CXMLDocument XMLFile(false);
			auto pOldFile = ThreadLocal.m_pXMLFile;
			ThreadLocal.m_pXMLFile = &XMLFile;
			auto Cleanup = fg_OnScopeExit
				(
					[&]()
					{
						ThreadLocal.m_pXMLFile = pOldFile;
					}
				)
			;
			
			CConfiguration const& Configuration = Iter.f_GetKey();
			CStr Name = (CStr::CFormat("{} {}") << Configuration.m_Platform << Configuration.m_Configuration).f_GetStr();
			
			auto pScheme = XMLFile.f_CreateDefaultDocument("Scheme");
			if (m_XcodeVersion >= 7)
				XMLFile.f_SetAttribute(pScheme, "LastUpgradeVersion", fg_Format("{sj2,sf0}90", m_XcodeVersion));
			else if (m_XcodeVersion == 6)
				XMLFile.f_SetAttribute(pScheme, "LastUpgradeVersion", "0640");
			else if (m_XcodeVersion == 5)
				XMLFile.f_SetAttribute(pScheme, "LastUpgradeVersion", "0510");
			else
				XMLFile.f_SetAttribute(pScheme, "LastUpgradeVersion", "0450");
			XMLFile.f_SetAttribute(pScheme, "version", "1.3");
			
			auto fl_GenerateBuildReference = [&] (CXMLElement* _pParent, CProject& _Project)
			{
				auto &NativeTarget = _Project.f_GetDefaultNativeTarget(Configuration);

				auto pBuildReference = XMLFile.f_CreateElement(_pParent, "BuildableReference");
				XMLFile.f_SetAttribute(pBuildReference, "BuildableIdentifier", "primary");
				XMLFile.f_SetAttribute(pBuildReference, "BlueprintIdentifier", NativeTarget.f_GetGUID());
				
				CStr BuildableName;
				auto pBuildable = _Buildable.f_FindEqual(Configuration);
				if (pBuildable)
				{
					auto pName = pBuildable->f_FindEqual(_Project.f_GetGUID());
					if (pName)
						BuildableName = *pName;
				}
				
				XMLFile.f_SetAttribute(pBuildReference, "BuildableName", BuildableName);
				XMLFile.f_SetAttribute(pBuildReference, "BlueprintName", _Project.f_GetName());
				XMLFile.f_SetAttribute(pBuildReference, "ReferencedContainer", (CStr::CFormat("container:Files/{}/{}.xcodeproj") << _Solution.f_GetName() << _Project.f_GetName()).f_GetStr());
			};
							   
			// BuildAction
			{
				auto pBuildAction = XMLFile.f_CreateElement(pScheme, "BuildAction");
				XMLFile.f_SetAttribute(pBuildAction, "parallelizeBuildables", "YES");
				XMLFile.f_SetAttribute(pBuildAction, "buildImplicitDependencies", "YES");
				
				auto pBuildActionEntries = XMLFile.f_CreateElement(pBuildAction, "BuildActionEntries");
				
				for (auto &Project : _Solution.m_Projects)
				{
					if (!Project.m_EnabledProjectConfigs.f_Exists(Configuration))
						continue;

					if (!Project.m_NativeTargets.f_Exists(Configuration))
						continue;

					auto iConfig = Project.m_EnabledProjectConfigs[Configuration];
					CStr Value = m_BuildSystem.f_EvaluateEntityProperty(*iConfig, EPropertyType_Target, "Disabled");
					if (Value != "true")
					{
						auto pBuildActionEntry = XMLFile.f_CreateElement(pBuildActionEntries, "BuildActionEntry");
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForTesting", "YES");
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForRunning", "YES");
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForProfiling", "YES");
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForArchiving", "YES");
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForAnalyzing", "YES");

						fl_GenerateBuildReference(pBuildActionEntry, Project);
					}
				}
			}
			
			// TestAction
			{
				auto pTestAction = XMLFile.f_CreateElement(pScheme, "TestAction");
				XMLFile.f_SetAttribute(pTestAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pTestAction, "selectedDebuggerIdentifier", "Xcode.DebuggerFoundation.Debugger.LLDB");
				XMLFile.f_SetAttribute(pTestAction, "selectedLauncherIdentifier", "Xcode.DebuggerFoundation.Launcher.LLDB");
				XMLFile.f_SetAttribute(pTestAction, "shouldUseLaunchSchemeArgsEnv", "YES");
				XMLFile.f_CreateElement(pTestAction, "Testables");
			}
			
			// LaunchAction
			{
				auto pLaunchAction = XMLFile.f_CreateElement(pScheme, "LaunchAction");
				XMLFile.f_SetAttribute(pLaunchAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pLaunchAction, "selectedDebuggerIdentifier", "Xcode.DebuggerFoundation.Debugger.LLDB");
				XMLFile.f_SetAttribute(pLaunchAction, "selectedLauncherIdentifier", "Xcode.DebuggerFoundation.Launcher.LLDB");
				XMLFile.f_SetAttribute(pLaunchAction, "launchStyle", "0");
				XMLFile.f_SetAttribute(pLaunchAction, "useCustomWorkingDirectory", "NO");
				XMLFile.f_SetAttribute(pLaunchAction, "customWorkingDirectory", "[MulitLaunchSchemes]");
				
				auto &Runnables = _Runnables[Configuration];
				if (!Runnables.f_IsEmpty())
				{
					auto pCommandLineArguments = XMLFile.f_CreateElement(pLaunchAction, "CommandLineArguments");
					for (auto &Runnable : Runnables)
					{
						auto pCommandLineArgument = XMLFile.f_CreateElement(pCommandLineArguments, "CommandLineArgument");
						XMLFile.f_SetAttribute(pCommandLineArgument, "argument", Runnable);
						XMLFile.f_SetAttribute(pCommandLineArgument, "isEnabled", "YES");
					}
				}
				
				XMLFile.f_SetAttribute(pLaunchAction, "ignoresPersistentStateOnLaunch", "NO");
				XMLFile.f_SetAttribute(pLaunchAction, "debugDocumentVersioning", "YES");
				XMLFile.f_SetAttribute(pLaunchAction, "debugServiceExtension", "internal");
				XMLFile.f_SetAttribute(pLaunchAction, "allowLocationSimulation", "YES");                       
				XMLFile.f_CreateElement(pLaunchAction, "AdditionalOptions");
			}
			
			// ProfileAction
			{
				auto pProfileAction = XMLFile.f_CreateElement(pScheme, "ProfileAction");
				XMLFile.f_SetAttribute(pProfileAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pProfileAction, "shouldUseLaunchSchemeArgsEnv", "YES");
				XMLFile.f_SetAttribute(pProfileAction, "savedToolIdentifier", "");
				XMLFile.f_SetAttribute(pProfileAction, "useCustomWorkingDirectory", "NO");
				XMLFile.f_SetAttribute(pProfileAction, "debugDocumentVersioning", "YES");
			}
			
			// AnalyzeAction
			{
				auto pArchiveAction = XMLFile.f_CreateElement(pScheme, "AnalyzeAction");
				XMLFile.f_SetAttribute(pArchiveAction, "buildConfiguration", Name);
			}
			
			// ArchiveAction
			{
				auto pArchiveAction = XMLFile.f_CreateElement(pScheme, "ArchiveAction");
				XMLFile.f_SetAttribute(pArchiveAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pArchiveAction, "revealArchiveInOrganizer", "YES");
			}
			
			CStr FileName = CFile::fs_AppendPath(OutputDir,  (CStr::CFormat("{}.xcscheme") << CFile::fs_MakeNiceFilename((CStr::CFormat("{} {}") << CStr("Build All") << Name).f_GetStr())).f_GetStr());
			
			CStr RawXMLData = XMLFile.f_GetAsString(EXMLOutputDialect_Xcode);

			// Now merge in any set by a user
			if (NFile::CFile::fs_FileExists(FileName, EFileAttrib_File) && NFile::CFile::fs_FileExists(FileName + ".gen", EFileAttrib_File))
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(RawXMLData), false);
				
				if (CFile::fs_ReadFile(FileName + ".gen") == FileData)
					XMLFile.f_ParseFile(FileName);
				else
				{
					CXMLDocument ExistingScheme;
					ExistingScheme.f_ParseFile(FileName);

					CXMLDocument PrevScheme;
					PrevScheme.f_ParseFile(FileName + ".gen");
					fspr_MergeScheme(ExistingScheme.f_GetRootNode(), PrevScheme.f_GetRootNode(), XMLFile.f_GetRootNode());
				}
			}
			
			{
				CStr XMLData = XMLFile.f_GetAsString(EXMLOutputDialect_Xcode);
				bool bWasCreated;
				if (!m_BuildSystem.f_AddGeneratedFile(FileName, XMLData, _Solution.f_GetName(), bWasCreated, true))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));
				
				if (bWasCreated)
				{
					TCVector<uint8> FileData;
					CFile::fs_WriteStringToVector(FileData, CStr(XMLData), false);
					if (m_BuildSystem.f_WriteFile(FileData, FileName))
						bSchemesChanged = true;
				}
				// Save the raw generated file to be able to diff against
				
				FileName += ".gen";
				if (!m_BuildSystem.f_AddGeneratedFile(FileName, RawXMLData, _Solution.f_GetName(), bWasCreated, true))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

				if (bWasCreated)
				{
					TCVector<uint8> FileData;
					CFile::fs_WriteStringToVector(FileData, CStr(RawXMLData), false);
					m_BuildSystem.f_WriteFile(FileData, FileName);
				}
			}
		}
		return bSchemesChanged;
	}
}
