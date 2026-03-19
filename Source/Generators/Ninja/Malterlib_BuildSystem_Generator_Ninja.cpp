// Copyright © 2026 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Ninja.h"
#include <Mib/Core/RuntimeType>

namespace NMib::NBuildSystem::NNinja
{
	constinit CLowLevelLockAggregate g_CooperativeTimeSliceCyclesLock;
	constinit int64 g_CooperativeTimeSliceCycles = 0;
}

namespace NMib::NBuildSystem
{

	class CBuildSystemGenerator_Ninja : public CBuildSystemGenerator, public CAllowUnsafeThis
	{
	public:

		CBuildSystemGenerator_Ninja()
		{
			DMibLock(NNinja::g_CooperativeTimeSliceCyclesLock);
			if (NNinja::g_CooperativeTimeSliceCycles == 0)
				NNinja::g_CooperativeTimeSliceCycles = (CSystem_Time::fs_CyclesFrequencyReciprocal() * 20_ms).f_ToInt();
		}

		TCMap<CPropertyKey, CEJsonSorted> f_GetValues(CBuildSystem const &_BuildSystem, CStr const &_OutputDir) override
		{
			TCMap<CPropertyKey, CEJsonSorted> Values;

			Values[gc_ConstKey_Generator] = _BuildSystem.f_GetGenerateSettings().m_Generator;
			Values[gc_ConstKey_GeneratorFamily] = gc_ConstString_Ninja;
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
			co_await ECoroutineFlag_CaptureExceptions;

			CStopwatch Stopwatch;
			Stopwatch.f_Start();

			auto &BuildSystem = *_pBuildSystem;
			auto &BuildSystemData = *_pBuildSystemData;

			using namespace NNinja;
			CStr WorkspaceDir = _OutputDir;
			CStr BuildSystemBase = BuildSystem.f_GetBaseDir();

			TCMap<CPropertyKey, CEJsonSorted> Values = f_GetValues(BuildSystem, _OutputDir);

			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> Configurations;

			CGeneratorInstance GeneratorInstance(BuildSystem, BuildSystemData, Values, WorkspaceDir);
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
			ReservedGroups[gc_ConstString__Automatic.m_String / gc_ConstString_Configurations.m_String];
			ReservedGroups[gc_ConstString__Automatic.m_String / gc_ConstString_Product_Reference.m_String];
			ReservedGroups[gc_ConstString__Automatic.m_String / gc_ConstString_Target_Dependencies.m_String];

			co_await BuildSystem.f_GenerateBuildSystem(&Configurations, &Values);
			co_await BuildSystem.f_CheckCancelled();

			umint nConfigs = 0;

			for (auto iConfig = Configurations.f_GetIterator(); iConfig; ++iConfig)
			{
				++nConfigs;
				auto Config = iConfig.f_GetKey();
				auto &ConfigData = **iConfig;

				for (auto iWorkspace = ConfigData.m_Workspaces.f_GetIterator(); iWorkspace; ++iWorkspace)
				{
					auto &WorkspaceName = iWorkspace.f_GetKey();

					auto &Workspace = **GeneratorState.m_Workspaces(WorkspaceName, fg_Construct(WorkspaceName));
					Workspace.m_WorkspaceInfos[Config] = fg_Move(*iWorkspace);
				}
				ConfigData.m_Workspaces.f_Clear();
			}

			fp64 Time2 = Stopwatch.f_GetTime();
			BuildSystem.f_OutputConsole("Extracted workspaces, projects and files for {} configurations {fe2} s{\n}"_f << nConfigs << (Time2 - Time1));

			umint MaxWorkspaceNameLen = 0;
			for (auto const &pWorkspace : GeneratorState.m_Workspaces)
				MaxWorkspaceNameLen = fg_Max(MaxWorkspaceNameLen, umint(pWorkspace->f_GetName().f_GetLen()));

			auto OldNumWorkspaceTargets = o_NumWorkspaceTargets;

			o_NumWorkspaceTargets.f_Clear();

			TCVector<TCUniquePointer<CWorkspace>> SortedWorkspaces;
			for (auto &pWorkspace : GeneratorState.m_Workspaces)
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

			SortedWorkspaces.f_Sort
				(
					[&](TCUniquePointer<CWorkspace> const &_pLeft, TCUniquePointer<CWorkspace> const &_pRight) -> COrdering_Strong
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
					, [&](TCUniquePointer<CWorkspace> &_pWorkspace) -> TCUnsafeFuture<void>
					{
						co_await ECoroutineFlag_CaptureExceptions;
						co_await BuildSystem.f_CheckCancelled();

						CAnsiEncoding Encoding(BuildSystem.f_AnsiFlags());
						{
							CStopwatch Stopwatch{true};

							auto pWorkspace = fg_Move(_pWorkspace);
							auto &Workspace = *pWorkspace;

							struct CGenerateWorkspaceInfo
							{
								CConfiguration const &m_Config;
								CWorkspaceInfo *m_pWorkspace;
								fp64 m_Time = 0.0;
							};

							TCVector<CGenerateWorkspaceInfo> InfosToProcess;

							for (auto &pWorkspaceInfo : pWorkspace->m_WorkspaceInfos)
							{
								auto &Config = pWorkspace->m_WorkspaceInfos.fs_GetKey(pWorkspaceInfo);
								InfosToProcess.f_Insert(CGenerateWorkspaceInfo{Config, pWorkspaceInfo.f_Get()});
							}

							co_await fg_ParallelForEach
								(
									InfosToProcess
									, [&](CGenerateWorkspaceInfo &_WorkspaceInfo) -> TCUnsafeFuture<void>
									{
										co_await ECoroutineFlag_CaptureExceptions;

										co_await BuildSystem.f_CheckCancelled();

										auto Start = Stopwatch.f_GetTime();
										auto *pConfigData = Configurations.f_FindEqual(_WorkspaceInfo.m_Config);
										DMibCheck(pConfigData);

										BuildSystem.f_GenerateBuildSystem_Workspace
											(
												_WorkspaceInfo.m_Config
												, **pConfigData
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

							for (auto &pWorkspaceInfo : Workspace.m_WorkspaceInfos)
								nTotalTargets += pWorkspaceInfo->m_Targets.f_GetLen();

							o_NumWorkspaceTargets[Workspace.f_GetName()] = nTotalTargets;

							co_await g_Yield;

							fp64 GenerateTime = 0.0;

							for (auto &Info : InfosToProcess)
							{
								GenerateTime += Info.m_Time;

								auto &WorkspaceInfo = *Info.m_pWorkspace;
								auto &Config = Info.m_Config;

								Workspace.m_Position = WorkspaceInfo.m_pEntity->f_Data().m_Position;
								Workspace.m_EntityName = WorkspaceInfo.m_EntityName;
								Workspace.m_EnabledConfigs[Config] = WorkspaceInfo.m_pEntity;

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

									for (auto iFile = TargetInfo.m_Files.f_GetIterator(); iFile; ++iFile)
									{
										auto &FileName = iFile.f_GetKey();
										auto &FileInfo = *iFile;

										auto &File = Target.m_Files[FileName];
										File.m_Position = FileInfo.m_pEntity->f_Data().m_Position;
										File.m_EnabledConfigs[Config] = FileInfo.m_pEntity;
									}
								}
							}

							co_await GeneratorInstance.f_GenerateWorkspaceFile(Workspace, WorkspaceDir);

							co_await BuildSystem.f_CheckCancelled();

							fp64 RealTime = Stopwatch.f_GetTime();

							BuildSystem.f_OutputConsole
								(
									"{}Generate{}  {sl*,a-} {fe2,sl5} s{\n}"_f
									<< Encoding.f_StatusNormal()
									<< Encoding.f_Default()
									<< Workspace.f_GetName()
									<< MaxWorkspaceNameLen
									<< RealTime
								)
							;
						}

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

	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Ninja);

	void fg_Malterlib_BuildSystem_MakeActive_Ninja()
	{
		DMibRuntimeClassMakeActive(CBuildSystemGenerator_Ninja);
	}
}
