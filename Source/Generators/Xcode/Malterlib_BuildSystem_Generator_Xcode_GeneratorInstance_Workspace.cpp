// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <Mib/XML/XML>
#include <Mib/Process/Platform>

namespace NMib::NBuildSystem::NXcode
{
	TCUnsafeFuture<void> CGeneratorInstance::f_GenerateWorkspaceFile(CSolution &_Solution, CStr const &_OutputDir) const
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CXMLDocument XMLFile(false);

		CWorkspaceState WorkspaceState;

		CStr OutputDir = CFile::fs_AppendPath(CFile::fs_AppendPath(_OutputDir, "Files"), CStr(CFile::fs_MakeNiceFilename(_Solution.f_GetName())));
		WorkspaceState.f_CreateDirectory(OutputDir);

		TCMap<CConfiguration, TCSet<CStr>> SchemesWithRunnables;
		TCMap<CConfiguration, TCMap<CStr, CStr>> SchemesWithBuildables;

		for (auto &Project : _Solution.m_Projects)
		{
			TCMap<CConfiguration, TCSet<CStr>> Runnables;
			TCMap<CConfiguration, TCMap<CStr, CStr>> Buildables;
			co_await f_GenerateProjectFile(Project, OutputDir, Runnables, Buildables);

			for (auto iConfig = Runnables.f_GetIterator(); iConfig; ++iConfig)
				SchemesWithRunnables[iConfig.f_GetKey()] += *iConfig;

			for (auto iConfig = Buildables.f_GetIterator(); iConfig; ++iConfig)
				SchemesWithBuildables[iConfig.f_GetKey()] += *iConfig;

			co_await g_Yield;
		}

		// Workspace root
		auto pWorkspace = XMLFile.f_CreateDefaultDocument(gc_ConstString_Workspace, "xml version=\"1.0\" encoding=\"UTF-8\"");
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

		TCFunction<void (CXMLElement* _pParent, CGroup* pGroup)> fOutputGroup = [&] (CXMLElement* _pParent, CGroup* pGroup)
		{
			if (!pGroup->m_OutputToWorkspace)
			{
				auto pGroupRef = XMLFile.f_CreateElement(_pParent, gc_ConstString_Group);
				XMLFile.f_SetAttribute(pGroupRef, "location", "container:");
				XMLFile.f_SetAttribute(pGroupRef, "name", pGroup->m_Name);
				pGroup->m_OutputToWorkspace = true;

				TCVector<CGroup*>& lChildGroups = MapGroupToGroups[pGroup];
				for (auto iChildGroup = lChildGroups.f_GetIterator(); iChildGroup; ++iChildGroup)
				{
					fOutputGroup(pGroupRef, *iChildGroup);
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
			fOutputGroup(pWorkspace, &(*iGroup));
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
		WorkspaceState.f_CreateDirectory(OutputDir);
		CStr GeneratedFile = CFile::fs_AppendPath(OutputDir, CStr(gc_ConstString_generatedContainer));
		{
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(GeneratedFile, CStr(), _Solution.f_GetName(), bWasCreated))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << GeneratedFile));

			if (bWasCreated)
			{
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(), false);
				m_BuildSystem.f_WriteFile(FileData, GeneratedFile);
			}
		}
		CStr FileName = CFile::fs_AppendPath(OutputDir,  CStr("contents.xcworkspacedata"));
		bool bSchemeChanged = fp_GenerateBuildAllSchemes(WorkspaceState, _Solution, OutputDir, SchemesWithRunnables, SchemesWithBuildables);
		{
			CStr XMLData = XMLFile.f_GetAsString(EXMLOutputDialect_Xcode);
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(FileName, XMLData, _Solution.f_GetName(), bWasCreated))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

			if (bWasCreated)
			{
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(XMLData), false);
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

		co_return {};
	}

	bool CGeneratorInstance::fp_GenerateBuildAllSchemes
		(
			CWorkspaceState &_WorkspaceState
			, CSolution &_Solution
			, CStr const &_OutputDir
			, TCMap<CConfiguration, TCSet<CStr>> &_Runnables
			, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable
		) const
	{
		bool bSchemesChanged = false;

		auto fWriteSchemeFile = [&](CStr const &_FileName, CStr const &_Data, bool _bDoWrite = true) -> bool
			{
				bool bWasCreated;
				if (!m_BuildSystem.f_AddGeneratedFile(_FileName, _Data, _Solution.f_GetName(), bWasCreated, EGeneratedFileFlag_NoDateCheck))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << _FileName));

				if (bWasCreated && (_bDoWrite || !CFile::fs_FileExists(_FileName)))
				{
					CByteVector FileData;
					CFile::fs_WriteStringToVector(FileData, _Data, false);
					_WorkspaceState.f_CreateDirectory(CFile::fs_GetPath(_FileName));
					if (m_BuildSystem.f_WriteFile(FileData, _FileName))
					{
						bSchemesChanged = true;
						return true;
					}
				}
				return false;
			}
		;

		{
			ch8 const *pDocumentData;
			auto Value = fp_GetSingleConfigValue(_Solution.m_EnabledConfigs, gc_ConstKey_Workspace_XcodeNewBuildSystem, EEJSONType_Boolean, true);

			if (Value.m_Value.f_Get().f_IsValid() && Value.m_Value.f_Get().f_Boolean())
			{
				pDocumentData =
R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>DisableBuildSystemDeprecationWarning</key>
	<true/>
	<key>DisableBuildSystemDeprecationDiagnostic</key>
	<true/>
	<key>IDEWorkspaceSharedSettings_AutocreateContextsIfNeeded</key>
	<false/>
	<key>PreviewsEnabled</key>
	<false/>
</dict>
</plist>
)xxx";
			}
			else
			{
				pDocumentData =
R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>BuildSystemType</key>
	<string>Original</string>
	<key>DisableBuildSystemDeprecationWarning</key>
	<true/>
	<key>DisableBuildSystemDeprecationDiagnostic</key>
	<true/>
	<key>IDEWorkspaceSharedSettings_AutocreateContextsIfNeeded</key>
	<false/>
	<key>PreviewsEnabled</key>
	<false/>
</dict>
</plist>
)xxx";
			}

			fWriteSchemeFile(_OutputDir / "xcshareddata/WorkspaceSettings.xcsettings", pDocumentData);
		}

		{
			auto IntermediatePath = fp_GetSingleConfigValue(_Solution.m_EnabledConfigs, gc_ConstKey_Workspace_IntermediateDirectory, EEJSONType_String, "").m_Value.f_Get().f_String() / "";
			auto OutputPath = fp_GetSingleConfigValue(_Solution.m_EnabledConfigs, gc_ConstKey_Workspace_OutputDirectory, EEJSONType_String, "").m_Value.f_Get().f_String() / "";

			CStr DocumentData =
R"xxx(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>BuildLocationStyle</key>
	<string>CustomLocation</string>
	<key>CustomBuildIntermediatesPath</key>
	<string>{}</string>
	<key>CustomBuildProductsPath</key>
	<string>{}</string>
	<key>CustomBuildLocationType</key>
	<string>Absolute</string>
	<key>DerivedDataCustomLocation</key>
	<string>{}</string>
	<key>DerivedDataLocationStyle</key>
	<string>AbsolutePath</string>
	<key>IssueFilterStyle</key>
	<string>ShowActiveSchemeOnly</string>
	<key>LiveSourceIssuesEnabled</key>
	<true/>
	<key>ShowSharedSchemesAutomaticallyEnabled</key>
	<true/>
</dict>
</plist>
)xxx"_f
				<< IntermediatePath
				<< OutputPath
				<< (CFile::fs_GetPath(IntermediatePath) / "Derived")
			;

			CStr FileName = _OutputDir / ("xcuserdata/{}.xcuserdatad/WorkspaceSettings.xcsettings"_f << NProcess::NPlatform::fg_Process_GetUserName());

			bool bUpdated = fWriteSchemeFile(FileName + ".generated", DocumentData);
			fWriteSchemeFile(FileName, DocumentData, bUpdated); // Only write first time or when above contents are changed
		}

		CStr OutputDir = CFile::fs_AppendPath(_OutputDir, CStr("xcshareddata/xcschemes"));
		_WorkspaceState.f_CreateDirectory(OutputDir);

		for (auto Iter = _Solution.m_EnabledConfigs.f_GetIterator(); Iter; ++Iter)
		{
			CXMLDocument XMLFile(false);

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

			auto fGenerateBuildReference = [&] (CXMLElement* _pParent, CProject& _Project)
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
				XMLFile.f_SetAttribute(pBuildAction, "parallelizeBuildables", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pBuildAction, "buildImplicitDependencies", gc_ConstString_YES);

				auto pBuildActionEntries = XMLFile.f_CreateElement(pBuildAction, "BuildActionEntries");

				for (auto &Project : _Solution.m_Projects)
				{
					auto pConfig = Project.m_EnabledProjectConfigs.f_FindEqual(Configuration);
					if (!pConfig)
						continue;

					if (!Project.m_NativeTargets.f_Exists(Configuration))
						continue;

					if (!m_BuildSystem.f_EvaluateEntityPropertyBool(**pConfig, gc_ConstKey_Target_Disabled, false))
					{
						auto pBuildActionEntry = XMLFile.f_CreateElement(pBuildActionEntries, "BuildActionEntry");
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForTesting", gc_ConstString_YES);
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForRunning", gc_ConstString_YES);
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForProfiling", gc_ConstString_YES);
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForArchiving", gc_ConstString_YES);
						XMLFile.f_SetAttribute(pBuildActionEntry, "buildForAnalyzing", gc_ConstString_YES);

						fGenerateBuildReference(pBuildActionEntry, Project);
					}
				}
			}

			// TestAction
			{
				auto pTestAction = XMLFile.f_CreateElement(pScheme, "TestAction");
				XMLFile.f_SetAttribute(pTestAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pTestAction, "selectedDebuggerIdentifier", "Xcode.DebuggerFoundation.Debugger.LLDB");
				XMLFile.f_SetAttribute(pTestAction, "selectedLauncherIdentifier", "Xcode.DebuggerFoundation.Launcher.LLDB");
				XMLFile.f_SetAttribute(pTestAction, "shouldUseLaunchSchemeArgsEnv", gc_ConstString_YES);
				XMLFile.f_CreateElement(pTestAction, "Testables");
			}

			// LaunchAction
			{
				auto pLaunchAction = XMLFile.f_CreateElement(pScheme, "LaunchAction");
				XMLFile.f_SetAttribute(pLaunchAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pLaunchAction, "selectedDebuggerIdentifier", "Xcode.DebuggerFoundation.Debugger.LLDB");
				XMLFile.f_SetAttribute(pLaunchAction, "selectedLauncherIdentifier", "Xcode.DebuggerFoundation.Launcher.LLDB");
				XMLFile.f_SetAttribute(pLaunchAction, "launchStyle", "0");
				XMLFile.f_SetAttribute(pLaunchAction, "useCustomWorkingDirectory", gc_ConstString_NO);
				XMLFile.f_SetAttribute(pLaunchAction, gc_ConstString_customWorkingDirectory, "[MulitLaunchSchemes]");

				auto &Runnables = _Runnables[Configuration];
				if (!Runnables.f_IsEmpty())
				{
					auto pCommandLineArguments = XMLFile.f_CreateElement(pLaunchAction, "CommandLineArguments");
					for (auto &Runnable : Runnables)
					{
						auto pCommandLineArgument = XMLFile.f_CreateElement(pCommandLineArguments, gc_ConstString_CommandLineArgument);
						XMLFile.f_SetAttribute(pCommandLineArgument, gc_ConstString_argument, Runnable);
						XMLFile.f_SetAttribute(pCommandLineArgument, "isEnabled", gc_ConstString_NO);
					}
				}

				XMLFile.f_SetAttribute(pLaunchAction, "ignoresPersistentStateOnLaunch", gc_ConstString_NO);
				XMLFile.f_SetAttribute(pLaunchAction, "debugDocumentVersioning", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pLaunchAction, "debugServiceExtension", "internal");
				XMLFile.f_SetAttribute(pLaunchAction, "allowLocationSimulation", gc_ConstString_YES);
				XMLFile.f_CreateElement(pLaunchAction, "AdditionalOptions");
			}

			// ProfileAction
			{
				auto pProfileAction = XMLFile.f_CreateElement(pScheme, "ProfileAction");
				XMLFile.f_SetAttribute(pProfileAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pProfileAction, "shouldUseLaunchSchemeArgsEnv", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pProfileAction, "savedToolIdentifier", "");
				XMLFile.f_SetAttribute(pProfileAction, "useCustomWorkingDirectory", gc_ConstString_NO);
				XMLFile.f_SetAttribute(pProfileAction, "debugDocumentVersioning", gc_ConstString_YES);
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
				XMLFile.f_SetAttribute(pArchiveAction, "revealArchiveInOrganizer", gc_ConstString_YES);
			}

			CStr FileName = CFile::fs_AppendPath(OutputDir,  (CStr::CFormat("{}.xcscheme") << CFile::fs_MakeNiceFilename((CStr::CFormat("{} {}") << CStr("Build All") << Name).f_GetStr())).f_GetStr());

			CStr RawXMLData = XMLFile.f_GetAsString(EXMLOutputDialect_Xcode);

			// Now merge in any set by a user
			if (NFile::CFile::fs_FileExists(FileName, EFileAttrib_File) && NFile::CFile::fs_FileExists(FileName + ".gen", EFileAttrib_File))
			{
				CByteVector FileData;
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

			fWriteSchemeFile(FileName, XMLFile.f_GetAsString(EXMLOutputDialect_Xcode));

			// Save the raw generated file to be able to diff against
			fWriteSchemeFile(FileName + ".gen", RawXMLData);
		}
		return bSchemesChanged;
	}
}
