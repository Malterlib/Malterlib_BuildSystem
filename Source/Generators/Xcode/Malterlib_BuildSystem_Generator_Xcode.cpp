// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <Mib/Core/RuntimeType>

namespace NMib::NBuildSystem
{
	class CBuildSystemGenerator_Xcode : public CBuildSystemGenerator
	{
	public:

		TCMap<CPropertyKey, CEJSON> f_GetValues(CBuildSystem const &_BuildSystem, CStr const &_OutputDir) override
		{
			TCMap<CPropertyKey, CEJSON> Values;
			
			Values[CPropertyKey("Generator")] = _BuildSystem.f_GetGenerateSettings().m_Generator;
			Values[CPropertyKey("GeneratorFamily")] = "Xcode";
			Values[CPropertyKey("BuildSystemBasePath")] = _BuildSystem.f_GetBaseDir();
			Values[CPropertyKey("BuildSystemOutputDir")] = _OutputDir;
			Values[CPropertyKey("BuildSystemPlatform")] = "macOS";
			Values[CPropertyKey("BuildSystemFile")] = _BuildSystem.f_GetGenerateSettings().m_SourceFile;
			Values[CPropertyKey("BuildSystemName")] = CFile::fs_GetFileNoExt(_BuildSystem.f_GetGenerateSettings().m_SourceFile);

			if (_BuildSystem.f_GetEnvironmentVariable("HostPlatform").f_IsEmpty())
				Values[CPropertyKey("HostPlatform")] = DMibStringize(DPlatformFamily);
			if (_BuildSystem.f_GetEnvironmentVariable("HostPlatformFamily").f_IsEmpty())
				Values[CPropertyKey("HostPlatformFamily")] = DMibStringize(DPlatformFamily);
			if (_BuildSystem.f_GetEnvironmentVariable("HostArchitecture").f_IsEmpty())
				Values[CPropertyKey("HostArchitecture")] = DMibStringize(DArchitecture);

			return Values;
		}

		virtual void f_Generate(CBuildSystem const &_BuildSystem, CBuildSystemData const &_BuildSystemData, CStr const &_OutputDir) override
		{
			CClock Clock;
			Clock.f_Start();
			using namespace NXcode;
			CStr SolutionDir = _OutputDir;
			CStr BuildSystemBase = _BuildSystem.f_GetBaseDir();
			
			// Disable &apos; encoding in output XML
			TCMap<CPropertyKey, CEJSON> Values = f_GetValues(_BuildSystem, _OutputDir);

			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> Configurations;

			CGeneratorInstance GeneratorInstance(_BuildSystem, _BuildSystemData, Values, SolutionDir);
			CGeneratorState &GeneratorState = GeneratorInstance.m_State;

			TCVector<TCVector<CConfigurationTuple>> Tuples = _BuildSystem.f_EvaluateConfigurationTuples(Values);
			
			fp64 Time1 = Clock.f_GetTime();
			_BuildSystem.f_OutputConsole("Evaluated config tuples {fe2} s{\n}"_f << Time1);

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
				
				CConfiguration ConfigToInsert;
				if (Platform.f_IsEmpty())
					ConfigToInsert.m_Platform = Arch;
				else
					ConfigToInsert.m_Platform = Platform + " " + Arch;
				ConfigToInsert.m_Configuration = Config;
				ConfigToInsert.m_PlatformBase = Platform;
				
				if (!ExtraConfig.f_IsEmpty())
				{
					ConfigToInsert.m_Configuration += " (";
					CStr ExtraConfigs;
					for (auto iExtra = ExtraConfig.f_GetIterator(); iExtra; ++iExtra)
						fg_AddStrSep(ExtraConfigs, *iExtra, " ");
					ConfigToInsert.m_Configuration += ExtraConfigs;
					ConfigToInsert.m_Configuration += ")";
				}
				
				{
					auto &ConfigData = *(Configurations[ConfigToInsert] = fg_Construct());
					ConfigData.m_Tuples = *iTuple;
				}
			}

			TCSet<CStr> ReservedGroups;
			ReservedGroups[g_ReservedGeneratorGroup / g_ReservedConfigurationsGroup];
			ReservedGroups[g_ReservedGeneratorGroup / g_ReservedProductRefGroup];
			ReservedGroups[g_ReservedGeneratorGroup / g_ReservedProjectDependenciesGroup];

			_BuildSystem.f_GenerateBuildSystem(Configurations, Values);

			mint nConfigs = 0;

			for (auto iConfig = Configurations.f_GetIterator(); iConfig; ++iConfig)
			{
				++nConfigs;
				auto Config = iConfig.f_GetKey();
				auto &ConfigData = **iConfig;
				
				for (auto iWorkspace = ConfigData.m_Workspaces.f_GetIterator(); iWorkspace; ++iWorkspace)
				{
					auto &WorkspaceName = iWorkspace.f_GetKey();

					auto &Workspace = **GeneratorState.m_Solutions(WorkspaceName, fg_Construct(WorkspaceName));
					Workspace.m_WorkspaceInfos[Config] = fg_Move(*iWorkspace);
				}
				ConfigData.m_Workspaces.f_Clear();
			}

			fp64 Time2 = Clock.f_GetTime();
			_BuildSystem.f_OutputConsole("Extracted workspaces, projects and files for {} configurations {fe2} s{\n}"_f << nConfigs << (Time2 - Time1));

			mint MaxWorkspaceNameLen = 0;
			for (auto const &pSolution : GeneratorState.m_Solutions)
				MaxWorkspaceNameLen = fg_Max(MaxWorkspaceNameLen, mint(pSolution->f_GetName().f_GetLen()));

			fg_ParallellForEach
				(
					GeneratorState.m_Solutions
					, [&](TCUniquePointer<CSolution> &_pSolution)
					{
						{
							CClock Timer{true};

							auto pSolution = fg_Move(_pSolution);

							struct CGenerateWorkspaceInfo
							{
								CConfiguration const &m_Config;
								CWorkspaceInfo *m_pWorkspace;
								fp64 m_Time = 0.0;
							};

							TCVector<CGenerateWorkspaceInfo> InfosToProcess;

							for (auto &pWorkspaceInfo : pSolution->m_WorkspaceInfos)
							{
								auto &Config = pSolution->m_WorkspaceInfos.fs_GetKey(pWorkspaceInfo);
								InfosToProcess.f_Insert(CGenerateWorkspaceInfo{Config, pWorkspaceInfo.f_Get()});
							}

							auto StartGenerate = Timer.f_GetTime();

							fg_ParallellForEach
								(
									InfosToProcess
									, [&](CGenerateWorkspaceInfo &_WorkspaceInfo)
									{
										auto Start = Timer.f_GetTime();
										auto *pConfigData = Configurations.f_FindEqual(_WorkspaceInfo.m_Config);
										DMibCheck(pConfigData);

										_BuildSystem.f_GenerateBuildSystem_Workspace
											(
												_WorkspaceInfo.m_Config
												, **pConfigData
												, _WorkspaceInfo.m_pWorkspace
												, ReservedGroups
												, "XcodeGeneratorDependencyFiles"
											)
										;

										_WorkspaceInfo.m_Time = Timer.f_GetTime() - Start;
									}
								)
							;

							auto StopGenerate = Timer.f_GetTime();

							auto &Workspace = *pSolution;

							fp64 GenerateTime = 0.0;

							auto fCopyGroups = [](TCMap<CStr, CGroup> &o_Groups, TCMap<CStr, CGroupInfo> const &_Groups, TCMap<CGroupInfo const *, CGroup *> &o_Mapping)
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

							for (auto &Info : InfosToProcess)
							{
								GenerateTime += Info.m_Time;

								auto &WorkspaceInfo = *Info.m_pWorkspace;
								auto &Config = Info.m_Config;

								Workspace.m_Position = WorkspaceInfo.m_pEntity->f_Data().m_Position;
								Workspace.m_EntityName = WorkspaceInfo.m_EntityName;
								Workspace.m_EnabledConfigs[Config] = WorkspaceInfo.m_pEntity;

								TCMap<CGroupInfo const *, CGroup *> WorkspaceGroupMapping;
								fCopyGroups(Workspace.m_Groups, WorkspaceInfo.m_Groups, WorkspaceGroupMapping);

								for (auto iTarget = WorkspaceInfo.m_Targets.f_GetIterator(); iTarget; ++iTarget)
								{
									auto &TargetName = iTarget.f_GetKey();
									auto &TargetInfo = *iTarget;

									auto &Target = *Workspace.m_Projects(TargetName, &Workspace);
									Target.m_Position = TargetInfo.m_pOuterEntity->f_Data().m_Position;
									Target.m_ProjectPosition = TargetInfo.m_pInnerEntity->f_Data().m_Position;
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
										auto DepMap = Target.m_DependenciesMap(DependencyName);
										auto &Dep = DepMap.f_GetResult();
										Target.m_DependenciesOrdered.f_Insert(Dep);

										Dep.m_Position = DependencyInfo.m_pEntity->f_Data().m_Position;
										Dep.m_EnabledConfigs[Config] = DependencyInfo.m_pEntity;
										Dep.m_PerConfig[Config].m_bLink = DependencyInfo.m_bLink;
									}

									TCMap<CGroupInfo const *, CGroup *> TargetGroupMapping;
									fCopyGroups(Target.m_Groups, TargetInfo.m_Groups, TargetGroupMapping);

									for (auto iFile = TargetInfo.m_Files.f_GetIterator(); iFile; ++iFile)
									{
										auto &FileName = iFile.f_GetKey();
										auto &FileInfo = *iFile;

										auto &File = Target.m_Files[FileName];
										File.m_Position = FileInfo.m_pEntity->f_Data().m_Position;
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

							pSolution->f_FindRecursiveDependencies(_BuildSystem);
							GeneratorInstance.f_GenerateWorkspaceFile(Workspace, SolutionDir);
							GeneratorInstance.f_ClearThreadLocal();

							fp64 RealTime = Timer.f_GetTime();
							fp64 TotalTime = (RealTime - (StopGenerate - StartGenerate) + GenerateTime);

							_BuildSystem.f_OutputConsole
								(
									"Generated workspace: {sl*,a-} {fe2,sl5} s  {fe0,sl3}% eval   {fe2,sl5} s wall{\n}"_f
									<< Workspace.f_GetName()
									<< MaxWorkspaceNameLen
									<< TotalTime
									<< (GenerateTime / TotalTime * 100.0)
									<< RealTime
								)
							;
						}
					}
				)
			;

			fp64 Time3 = Clock.f_GetTime();
			_BuildSystem.f_OutputConsole("Generated workspaces {fe2} s{\n}"_f << (Time3 - Time2));
		}
	};
	
	using CBuildSystemGenerator_Xcode4 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode5 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode6 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode7 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode8 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode9 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode10 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode11 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode12 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode13 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode14 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode15 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode16 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode17 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode18 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode19 = CBuildSystemGenerator_Xcode;
	using CBuildSystemGenerator_Xcode20 = CBuildSystemGenerator_Xcode;

	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode4);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode5);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode6);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode7);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode8);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode9);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode10);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode11);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode12);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode13);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode14);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode15);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode16);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode17);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode18);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode19);
	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Xcode20);

	void fg_Malterlib_BuildSystem_MakeActive_Xcode()
	{
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode4);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode5);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode6);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode7);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode8);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode9);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode10);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode11);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode12);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode13);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode14);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode15);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode16);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode17);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode18);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode19);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Xcode20);
	}
}
