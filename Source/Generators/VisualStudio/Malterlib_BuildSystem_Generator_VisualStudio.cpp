// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"
#include <Mib/Core/RuntimeType>

namespace NMib::NBuildSystem
{
	bool NVisualStudio::CGeneratorInstance::CProjectState::f_FileExists(CStr const &_Path)
	{
		auto pExists = m_FileExistsCache.f_FindEqual(_Path);
		if (pExists)
			return *pExists;
		bool bExists = CFile::fs_FileExists(_Path, EFileAttrib_File);;
		m_FileExistsCache[_Path] = bExists;
		return bExists;
	}

	void NVisualStudio::CGeneratorInstance::CProjectState::f_CreateDirectory(CStr const &_Path)
	{
		auto Mapped = m_CreateDirectoryCache(_Path);
		if (Mapped.f_WasCreated())
		{
			CFile::fs_CreateDirectory(_Path);
		}
	}

	class CBuildSystemGenerator_VisualStudio : public CBuildSystemGenerator, public CAllowUnsafeThis
	{
	public:
		TCMap<CPropertyKey, CEJsonSorted> f_GetValues(CBuildSystem const &_BuildSystem, CStr const &_OutputDir) override
		{
			TCMap<CPropertyKey, CEJsonSorted> Values;

			Values[gc_ConstKey_Generator] = _BuildSystem.f_GetGenerateSettings().m_Generator;
			Values[gc_ConstKey_GeneratorFamily] = gc_ConstString_VisualStudio;
			Values[gc_ConstKey_BuildSystemBasePath] = _BuildSystem.f_GetBaseDir();
			Values[gc_ConstKey_BuildSystemOutputDir] = _OutputDir;
			Values[gc_ConstKey_BuildSystemFile] = _BuildSystem.f_GetGenerateSettings().m_SourceFile;
			Values[gc_ConstKey_BuildSystemName] = CFile::fs_GetFileNoExt(_BuildSystem.f_GetGenerateSettings().m_SourceFile);

			if (_BuildSystem.f_GetEnvironmentVariable(gc_ConstKey_HostPlatform.m_Name).f_IsEmpty())
				Values[gc_ConstKey_HostPlatform] = gc_ConstStringDynamic_DPlatform;
			if (_BuildSystem.f_GetEnvironmentVariable(gc_ConstKey_HostPlatformFamily.m_Name).f_IsEmpty())
				Values[gc_ConstKey_HostPlatformFamily] = gc_ConstStringDynamic_DPlatformFamily;
			if (_BuildSystem.f_GetEnvironmentVariable(gc_ConstKey_HostArchitecture.m_Name).f_IsEmpty())
				Values[gc_ConstKey_HostArchitecture] = gc_ConstStringDynamic_DArchitecture;

			return Values;
		}

		TCUnsafeFuture<void> f_Generate(CBuildSystem const *_pBuildSystem, CBuildSystemData const *_pBuildSystemData, CStr _OutputDir, TCMap<CStr, uint32> &o_NumWorkspaceTargets) override
		{
			co_await (ECoroutineFlag_CaptureMalterlibExceptions);

			CStopwatch Stopwatch;
			Stopwatch.f_Start();

			auto &BuildSystem = *_pBuildSystem;
			auto &BuildSystemData = *_pBuildSystemData;

			using namespace NVisualStudio;
			CStr SolutionDir = _OutputDir;
			CStr BuildSystemBase = BuildSystem.f_GetBaseDir();
			// Disable &apos; encoding in output XML
			TCMap<CPropertyKey, CEJsonSorted> Values = f_GetValues(BuildSystem, _OutputDir);

			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> Configurations;

			CGeneratorInstance GeneratorInstance(BuildSystem, BuildSystemData, Values, SolutionDir);
			CGeneratorState &GeneratorState = GeneratorInstance.m_State;

			TCVector<TCVector<CConfigurationTuple>> Tuples = BuildSystem.f_EvaluateConfigurationTuples(Values);

			fp64 Time1 = Stopwatch.f_GetTime();
			BuildSystem.f_OutputConsole("Evaluated config tuples {fe2} s{\n}"_f << Time1);

			for (auto iTuple = Tuples.f_GetIterator(); iTuple; ++iTuple)
			{
				CStr Platform;
				CStr Arch;
				CStr Config;
				TCVector<CStr> ExtraConfig;

				for (auto iConfig = iTuple->f_GetIterator(); iConfig; ++iConfig)
				{
					if (iConfig->m_Type == gc_ConstString_Platform.m_String)
						Platform = iConfig->m_Name;
					else if (iConfig->m_Type == gc_ConstString_Architecture.m_String)
						Arch = iConfig->m_Name;
					else if (iConfig->m_Type == gc_ConstString_Configuration.m_String)
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
			co_await BuildSystem.f_GenerateBuildSystem(&Configurations, &Values);
			co_await BuildSystem.f_CheckCancelled();

			umint nConfigs = 0;

			for (auto iConfig = Configurations.f_GetIterator(); iConfig; ++iConfig)
			{
				++nConfigs;
				auto Config = iConfig.f_GetKey();
				auto &ConfigData = *((CLocalConfiguraitonData *)(&**iConfig));

				for (auto iWorkspace = ConfigData.m_Workspaces.f_GetIterator(); iWorkspace; ++iWorkspace)
				{
					auto &WorkspaceName = iWorkspace.f_GetKey();

					auto &Workspace = **GeneratorState.m_Solutions(WorkspaceName, fg_Construct(WorkspaceName));
					Workspace.m_WorkspaceInfos[Config] = fg_Move(*iWorkspace);
				}
				ConfigData.m_Workspaces.f_Clear();
			}

			fp64 Time2 = Stopwatch.f_GetTime();
			BuildSystem.f_OutputConsole("Extracted workspaces, projects and files for {} configurations {fe2} s{\n}"_f << nConfigs << (Time2 - Time1));

			umint MaxSolutionNameLength = 0;
			for (auto &pSolution : GeneratorState.m_Solutions)
				MaxSolutionNameLength = fg_Max(MaxSolutionNameLength, umint(pSolution->f_GetName().f_GetLen()));

			auto OldNumWorkspaceTargets = o_NumWorkspaceTargets;

			o_NumWorkspaceTargets.f_Clear();

			TCVector<TCUniquePointer<CSolution>> SortedWorkspaces;
			for (auto &pWorkspace : GeneratorState.m_Solutions)
			{
				auto &Workspace = *pWorkspace;
				o_NumWorkspaceTargets[Workspace.f_GetName()];

				auto *pTotalTargets = OldNumWorkspaceTargets.f_FindEqual(Workspace.f_GetName());
				if (pTotalTargets)
				{
					Workspace.m_nTotalTargets = *pTotalTargets;
				}
				else
				{
					if (Workspace.f_GetName() == "Tests")
						Workspace.m_nTotalTargets = 150; // Default to assuming Tests has most targets
				}

				SortedWorkspaces.f_Insert(fg_Move(pWorkspace));
			}

			GeneratorState.m_Solutions.f_Clear();

			SortedWorkspaces.f_Sort
				(
					[&](TCUniquePointer<CSolution> const &_pLeft, TCUniquePointer<CSolution> const &_pRight) -> COrdering_Strong
					{
						auto &Left = *_pLeft;
						auto &Right = *_pRight;
						if (auto Result = Right.m_nTotalTargets <=> Left.m_nTotalTargets; Result != 0)
							return Result;

						return Left.m_Name <=> Right.m_Name;
					}
				)
			;

			co_await fg_ParallelForEach
				(
					SortedWorkspaces
					, [&](TCUniquePointer<CSolution> &_pSolution) -> TCUnsafeFuture<void>
					{
						co_await (ECoroutineFlag_CaptureMalterlibExceptions);
						co_await BuildSystem.f_CheckCancelled();

						CStopwatch Stopwatch{true};

						auto pSolution = fg_Move(_pSolution);

						struct CGenerateWorkspaceInfo
						{
							CConfiguration const &m_Config;
							CLocalConfiguraitonData const &m_ConfigData;
							CWorkspaceInfo *m_pWorkspace;
							fp64 m_Time = 0.0;
						};

						TCVector<CGenerateWorkspaceInfo> InfosToProcess;

						for (auto &pWorkspaceInfo : pSolution->m_WorkspaceInfos)
						{
							auto &Config = pSolution->m_WorkspaceInfos.fs_GetKey(pWorkspaceInfo);
							auto *pConfigData = Configurations.f_FindEqual(Config);
							DMibCheck(pConfigData);
							auto &ConfigData = *((CLocalConfiguraitonData *)(&**pConfigData));

							InfosToProcess.f_Insert(CGenerateWorkspaceInfo{Config, ConfigData, pWorkspaceInfo.f_Get()});
						}

						co_await fg_ParallelForEach
							(
								InfosToProcess
								, [&](CGenerateWorkspaceInfo &_WorkspaceInfo) -> TCUnsafeFuture<void>
								{
									co_await (ECoroutineFlag_CaptureMalterlibExceptions);
									co_await BuildSystem.f_CheckCancelled();

									auto Start = Stopwatch.f_GetTime();

									BuildSystem.f_GenerateBuildSystem_Workspace
										(
											_WorkspaceInfo.m_Config
											, _WorkspaceInfo.m_ConfigData
											, _WorkspaceInfo.m_pWorkspace
											, ReservedGroups
											, CStr()
										)
									;

									_WorkspaceInfo.m_Time = Stopwatch.f_GetTime() - Start;

									co_return {};
								}
								, BuildSystem.f_SingleThreaded()
							)
						;
						co_await BuildSystem.f_CheckCancelled();

						umint nTotalTargets = 0;

						for (auto &pWorkspaceInfo : pSolution->m_WorkspaceInfos)
							nTotalTargets += pWorkspaceInfo->m_Targets.f_GetLen();

						o_NumWorkspaceTargets[pSolution->f_GetName()] = nTotalTargets;

						co_await g_Yield;

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
							auto &ConfigData = Info.m_ConfigData;
							auto &Config = Info.m_Config;

							Workspace.m_Position = WorkspaceInfo.m_pEntity->f_Data().m_Position;
							Workspace.m_EntityName = WorkspaceInfo.m_EntityName;

							auto &SolutionConfig = Workspace.m_EnabledConfigs[Config];
							SolutionConfig.m_pEntity = fg_Explicit(WorkspaceInfo.m_pEntity.f_Get());
							SolutionConfig.m_Config = ConfigData.m_SolutionConfiguration;
							SolutionConfig.m_Platform = ConfigData.m_SolutionPlatform;

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
								//Target.m_EnabledConfigs[Config] = TargetInfo.m_pOuterEntity;
								Target.m_EnabledProjectConfigs[Config] = TargetInfo.m_pInnerEntity;

								if (TargetInfo.m_pGroup)
								{
									auto pGroup = WorkspaceGroupMapping.f_FindEqual(&*TargetInfo.m_pGroup);
									DCheck(pGroup);
									if (pGroup)
										Target.m_pGroup = *pGroup;
								}

								for (auto &DependencyInfo : TargetInfo.m_DependenciesOrdered)
								{
									auto const &DependencyName = DependencyInfo.f_GetName();
									auto DepMap = Target.m_Dependencies(DependencyName);
									auto &Dep = DepMap.f_GetResult();
									Dep.m_Position = DependencyInfo.m_pEntity->f_Data().m_Position;

									auto &DebugConfig = Dep.m_PerConfigDebug[Config];
									DebugConfig.m_bIndirect = DependencyInfo.m_bIndirect;
									DebugConfig.m_bIndirectOrdered = DependencyInfo.m_bIndirectOrdered;

									if (DependencyInfo.m_bIndirect && !DependencyInfo.m_bIndirectOrdered)
										continue;

									Dep.m_EnabledConfigs[Config] = DependencyInfo.m_pEntity;

									if (DepMap.f_WasCreated())
										Dep.m_bExternal = DependencyInfo.m_bExternal;
									else if (Dep.m_bExternal != DependencyInfo.m_bExternal)
										BuildSystem.fs_ThrowError(DependencyInfo.m_Position, "External dependencies cannot be varied per configuration");
								}

								TCMap<CGroupInfo const *, CGroup *> TargetGroupMapping;
								fCopyGroups(Target.m_Groups, TargetInfo.m_Groups, TargetGroupMapping);

								for (auto iFile = TargetInfo.m_Files.f_GetIterator(); iFile; ++iFile)
								{
									auto &FileKey = iFile.f_GetKey();
									auto &FileInfo = *iFile;

									auto &File = Target.m_Files[FileKey.m_FileName];
									File.m_Position = FileInfo.m_pEntity->f_Data().m_Position;
									File.m_EnabledConfigs[Config] = FileInfo.m_pEntity;
									if (FileInfo.m_pGroup && !File.m_pGroup)
									{
										auto pGroup = TargetGroupMapping.f_FindEqual(&*FileInfo.m_pGroup);
										DCheck(pGroup);
										if (pGroup)
											File.m_pGroup = *pGroup;
									}
								}
							}
						}

						pSolution->f_FindRecursiveDependencies(BuildSystem);

						co_await GeneratorInstance.f_GenerateSolutionFile(*pSolution, SolutionDir);

						fp64 RealTime = Stopwatch.f_GetTime();

						CAnsiEncoding Encoding(BuildSystem.f_AnsiFlags());

						BuildSystem.f_OutputConsole
							(
								"{}Generate{}  {sl*,a-} {fe2,sl5} s{\n}"_f
								<< Encoding.f_StatusNormal()
								<< Encoding.f_Default()
								<< Workspace.f_GetName()
								<< MaxSolutionNameLength
								<< RealTime
							)
						;

						co_return {};
					}
					, BuildSystem.f_SingleThreaded()
				)
			;

			fp64 Time3 = Stopwatch.f_GetTime();
			BuildSystem.f_OutputConsole("Generated workspaces {fe2} s{\n}"_f << (Time3 - Time2));

			co_return {};
		}
	};

	using CBuildSystemGenerator_VisualStudio2022 = CBuildSystemGenerator_VisualStudio;

	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_VisualStudio2022);

	using CBuildSystemGenerator_VisualStudio2026 = CBuildSystemGenerator_VisualStudio;

	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_VisualStudio2026);

	void fg_Malterlib_BuildSystem_MakeActive_VisualStudio()
	{
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_VisualStudio2022);
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_VisualStudio2026);
	}
}


