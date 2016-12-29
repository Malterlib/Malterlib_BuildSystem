// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <Mib/Core/RuntimeType>

namespace NMib::NBuildSystem
{
	bool NVisualStudio::CGeneratorInstance::CThreadLocal::f_FileExists(CStr const &_Path)
	{
		auto pExists = m_FileExistsCache.f_FindEqual(_Path);
		if (pExists)
			return *pExists;
		bool bExists = CFile::fs_FileExists(_Path, EFileAttrib_File);;
		m_FileExistsCache[_Path] = bExists;
		return bExists;
	}
	
	void NVisualStudio::CGeneratorInstance::CThreadLocal::f_CreateDirectory(CStr const &_Path)
	{
		auto Mapped = m_CreateDirectoryCache(_Path);
		if (Mapped.f_WasCreated())
		{
			CFile::fs_CreateDirectory(_Path);
		}
	}
		
	class CBuildSystemGenerator_VisualStudio : public CBuildSystemGenerator
	{
	public:

		virtual void f_Generate(CBuildSystem const &_BuildSystem, CBuildSystemData const &_BuildSystemData, CStr const &_OutputDir) override
		{
			CClock Clock;
			Clock.f_Start();
			
			using namespace NVisualStudio;
			CStr SolutionDir = _OutputDir;
			CStr BuildSystemBase = _BuildSystem.f_GetBaseDir();
			// Disable &apos; encoding in output XML
			TCMap<CPropertyKey, CStr> Values;
			Values[CPropertyKey("Generator")] = _BuildSystem.f_GetGenerateSettings().m_Generator;
			Values[CPropertyKey("GeneratorFamily")] = "VisualStudio";
			Values[CPropertyKey("BuildSystemBasePath")] = _BuildSystem.f_GetBaseDir();
			Values[CPropertyKey("BuildSystemOutputDir")] = _OutputDir;
			Values[CPropertyKey("BuildSystemFile")] = _BuildSystem.f_GetGenerateSettings().m_SourceFile;
			Values[CPropertyKey("BuildSystemName")] = CFile::fs_GetFileNoExt(_BuildSystem.f_GetGenerateSettings().m_SourceFile);

			if (NSys::fg_Process_GetEnvironmentVariable(CStr("HostPlatform")).f_IsEmpty())
				Values[CPropertyKey("HostPlatform")] = DMibStringize(DPlatform);
			if (NSys::fg_Process_GetEnvironmentVariable(CStr("HostPlatformFamily")).f_IsEmpty())
				Values[CPropertyKey("HostPlatformFamily")] = DMibStringize(DPlatformFamily);
			if (NSys::fg_Process_GetEnvironmentVariable(CStr("HostArchitecture")).f_IsEmpty())
				Values[CPropertyKey("HostArchitecture")] = DMibStringize(DArchitecture);
			
			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> Configurations;

			CGeneratorInstance GeneratorInstance(_BuildSystem, _BuildSystemData, Values, SolutionDir);
			CGeneratorState &GeneratorState = GeneratorInstance.m_State;

			TCVector<TCVector<CConfigurationTuple>> Tuples = _BuildSystem.f_EvaluateConfigurationTuples(Values);
			
			fp64 Time1 = Clock.f_GetTime();
			DConOut("Evaluated config tuples {fe2} s{\n}", Time1);

			for (auto iTuple = Tuples.f_GetIterator(); iTuple; ++iTuple)
			{
				CStr Platform;
				CStr Arch;
				CStr Config;
				TCVector<CStr> ExtraConfig;

				for (auto iConfig = iTuple->f_GetIterator(); iConfig; ++iConfig)
				{
					if (iConfig->m_Type == "Platform")
						Platform = iConfig->m_Name;
					else if (iConfig->m_Type == "Architecture")
						Arch = iConfig->m_Name;
					else if (iConfig->m_Type == "Configuration")
						Config = iConfig->m_Name;
					else if (!iConfig->m_Name.f_IsEmpty())
						ExtraConfig.f_Insert(iConfig->m_Name);
				}

				CStr ExtraConfigText;

				if (!ExtraConfig.f_IsEmpty())
				{
					ExtraConfigText += " (";
					bool bFirst = true;
					for (auto iExtra = ExtraConfig.f_GetIterator(); iExtra; ++iExtra)
					{
						if (bFirst)
						{
							bFirst = false;
							ExtraConfigText += *iExtra;
						}
						else
						{
							ExtraConfigText += " ";
							ExtraConfigText += *iExtra;
						}
					}
					ExtraConfigText += ")";
				}

				
				CConfiguration ConfigToInsert;
				if (Platform.f_IsEmpty())
					ConfigToInsert.m_Platform = Arch;
				else
					ConfigToInsert.m_Platform = Platform + " " + Arch;
				ConfigToInsert.m_Configuration = Config;
				
				if (!ExtraConfigText.f_IsEmpty())
					ConfigToInsert.m_Configuration += ExtraConfigText;
				
				{
					TCUniquePointer<CLocalConfiguraitonData> NewConfig = fg_Construct();
					
					auto &ConfigData = *NewConfig;
					ConfigData.m_Tuples = *iTuple;
					ConfigData.m_FullConfiguration = CStr::CFormat("{} - {} - {}{}") << Platform << Arch << Config << ExtraConfigText;
					ConfigData.m_SolutionPlatform = CStr::CFormat("{} - {}") << Platform << Arch;
					ConfigData.m_SolutionConfiguration = CStr::CFormat("{}{}") << Config << ExtraConfigText;
					
					*(Configurations[ConfigToInsert] = fg_Move(NewConfig));
				}
				
				
			}

			TCSet<CStr> ReservedGroups;
			_BuildSystem.f_GenerateBuildSystem(Configurations, Values, ReservedGroups, CStr());
			fp64 Time2 = Clock.f_GetTime();
			DConOut("Extracted workspaces, projects and files {fe2} s{\n}", Time2 - Time1);

			mint nConfigs = 0;
			auto fl_CopyGroups
				= [&](TCMap<CStr, CGroup> &o_Groups, TCMap<CStr, CGroupInfo> const &_Groups, TCMap<CGroupInfo const *, CGroup *> &o_Mapping)
				{
					for (auto iGroup = _Groups.f_GetIterator(); iGroup; ++iGroup)
					{
						auto pGroup = (o_Mapping[&*iGroup] = &o_Groups[iGroup.f_GetKey()]);
						pGroup->m_Name = iGroup->m_Name;
					}
					for (auto iGroup = _Groups.f_GetIterator(); iGroup; ++iGroup)
					{
						if (iGroup->m_pParent)
						{
							auto &OutGroup = o_Groups[iGroup.f_GetKey()];
							auto pGroup = o_Mapping.f_FindEqual(&*iGroup->m_pParent);

							DCheck(pGroup && *pGroup);
							if (pGroup)
								OutGroup.m_pParent = *pGroup;
						}
					}
				}
			;
			
			for (auto iConfig = Configurations.f_GetIterator(); iConfig; ++iConfig)
			{
				++nConfigs;
				auto Config = iConfig.f_GetKey();
				auto &ConfigData = *((CLocalConfiguraitonData *)(&**iConfig));
				
				for (auto iWorkspace = ConfigData.m_Workspaces.f_GetIterator(); iWorkspace; ++iWorkspace)
				{
					auto &WorkspaceName = iWorkspace.f_GetKey();
					auto &WorkspaceInfo = *iWorkspace;
					
					auto &Workspace = GeneratorState.m_Solutions[WorkspaceName];
					Workspace.m_Position = WorkspaceInfo.m_pEntity->m_Position;
					Workspace.m_EntityName = WorkspaceInfo.m_EntityName;
					auto &SolutionConfig = Workspace.m_EnabledConfigs[Config];
					SolutionConfig.m_pEntity = fg_Explicit(WorkspaceInfo.m_pEntity.f_Get());
					SolutionConfig.m_Config = ConfigData.m_SolutionConfiguration;
					SolutionConfig.m_Platform = ConfigData.m_SolutionPlatform;

					
					TCMap<CGroupInfo const *, CGroup *> WorkspaceGroupMapping;
					fl_CopyGroups(Workspace.m_Groups, WorkspaceInfo.m_Groups, WorkspaceGroupMapping);
					
					if (!Workspace.m_Link.f_IsInTree())
						GeneratorState.m_SolutionsByEntity.f_Insert(Workspace);
					
					for (auto iTarget = WorkspaceInfo.m_Targets.f_GetIterator(); iTarget; ++iTarget)
					{
						auto &TargetName = iTarget.f_GetKey();
						auto &TargetInfo = *iTarget;

						auto &Target = *Workspace.m_Projects(TargetName, &Workspace);
						Target.m_Position = TargetInfo.m_pOuterEntity->m_Position;
						Target.m_ProjectPosition = TargetInfo.m_pInnerEntity->m_Position;
						Target.m_EntityName = TargetInfo.m_EntityName;
						Target.m_EnabledConfigs[Config] = TargetInfo.m_pOuterEntity;
						Target.m_EnabledProjectConfigs[Config] = TargetInfo.m_pInnerEntity;

						if (TargetInfo.m_pGroup)
						{
							auto pGroup = WorkspaceGroupMapping.f_FindEqual(&*TargetInfo.m_pGroup);
							DCheck(pGroup);
							if (pGroup)
								Target.m_pGroup = *pGroup;
						}
						
						for (auto iDependency = TargetInfo.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
						{
							auto const &DependencyInfo = *iDependency;
							auto const &DependencyName = iDependency->f_GetName();
							auto DepMap = Target.m_Dependencies(DependencyName);
							auto &Dep = DepMap.f_GetResult();

							Dep.m_Position = DependencyInfo.m_pEntity->m_Position;
							Dep.m_EnabledConfigs[Config] = DependencyInfo.m_pEntity;
						}
						
						TCMap<CGroupInfo const *, CGroup *> TargetGroupMapping;
						fl_CopyGroups(Target.m_Groups, TargetInfo.m_Groups, TargetGroupMapping);
						
						for (auto iFile = TargetInfo.m_Files.f_GetIterator(); iFile; ++iFile)
						{
							auto &FileName = iFile.f_GetKey();
							auto &FileInfo = *iFile;

							auto &File = Target.m_Files[FileName];
							File.m_Position = FileInfo.m_pEntity->m_Position;
							File.m_EnabledConfigs[Config] = FileInfo.m_pEntity;
							if (FileInfo.m_pGroup)
							{
								auto pGroup = TargetGroupMapping.f_FindEqual(&*FileInfo.m_pGroup);
								DCheck(pGroup);
								if (pGroup)
									File.m_pGroup = *pGroup;
							}
						}							
					}
				}
			}
			fp64 Time3 = Clock.f_GetTime();
			DConOut("Translated workspaces, projects and files for {} configurations {fe2} s{\n}", nConfigs << Time3 - Time2);

			mint MaxSolutionNameLength = 0;
			for (auto iSolution = GeneratorState.m_Solutions.f_GetIterator(); iSolution; ++iSolution)
				MaxSolutionNameLength = fg_Max(MaxSolutionNameLength, mint(iSolution->f_GetName().f_GetLen()));
			
			fg_ParallellForEach
				(
					GeneratorState.m_Solutions
					, [&](CSolution &_Solution)
					{
						_Solution.f_FindRecursiveDependencies(_BuildSystem);
						//GeneratorInstance.f_GenerateSolutionFile(*iSolution, "T:/Test");
						GeneratorInstance.f_GenerateSolutionFile(_Solution, SolutionDir, MaxSolutionNameLength);
					}
				)
			;

			fp64 Time4 = Clock.f_GetTime();
			DConOut("Generated workspaces {fe2} s{\n}", Time4 - Time3);
		}
	};

	using CBuildSystemGenerator_VisualStudio2012 = CBuildSystemGenerator_VisualStudio;  
	using CBuildSystemGenerator_VisualStudio2013 = CBuildSystemGenerator_VisualStudio;  
	using CBuildSystemGenerator_VisualStudio2015 = CBuildSystemGenerator_VisualStudio;  
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_VisualStudio2012);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_VisualStudio2013);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_VisualStudio2015);
	void fg_Malterlib_BuildSystem_MakeActive_VisualStudio()
	{
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_VisualStudio2012);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_VisualStudio2013);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_VisualStudio2015);
	}
}


