// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/BuildSystem/BuildSystem>

namespace
{
	using namespace NMib;
	using namespace NMib::NEncoding;
	using namespace NMib::NStr;
	using namespace NMib::NFile;
	using namespace NMib::NContainer;
	using namespace NMib::NBuildSystem;
	using namespace NMib::NStorage;
	using namespace NMib::NConcurrency;

	constexpr CStr gc_TestGeneratorDependencyFiles = gc_Str<"TestGeneratorDependencyFiles">;

	class CBuildSystemGenerator_Test : public CBuildSystemGenerator, public ICGeneratorInterface
	{
		// CGeneratorInterface
		CValuePotentiallyByRef f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const override
		{
			CStr Result;
			if (_Value == "BasePathRelativeProject")
			{
				Result = ".";
				if (_pStorePositions)
					_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePathRelativeProject")->f_AddValue(Result, false);

				o_bSuccess = true;
				return CEJSONSorted(Result);
			}
			else if (_Value == "BasePath")
			{
				Result = mp_BaseDir;
				if (_pStorePositions)
					_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePath")->f_AddValue(Result, false);

				o_bSuccess = true;
				return CEJSONSorted(Result);
			}
			else if (_Value == "GeneratedBuildSystemDir")
			{
				Result = mp_OutputDir;
				if (_pStorePositions)
					_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.GeneratedBuildSystemDir")->f_AddValue(Result, false);

				o_bSuccess = true;
				return CEJSONSorted(Result);
			}
			else if (_Value == "ProjectPath")
			{
				Result = ".";
				if (_pStorePositions)
					_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.ProjectPath")->f_AddValue(Result, false);

				o_bSuccess = true;
				return CEJSONSorted(Result);
			}
			else if (_Value == "Inherit")
			{
				Result = "";
				if (_pStorePositions)
					_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Inherit")->f_AddValue(Result, false);

				o_bSuccess = true;
				return CEJSONSorted(Result);
			}
			else if (_Value == "IntermediateDirectory")
			{
				Result = mp_OutputDir / "Intermediate";
				if (_pStorePositions)
					_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.IntermediateDirectory")->f_AddValue(Result, false);

				o_bSuccess = true;
				return CEJSONSorted(Result);
			}
			else if (_Value == "OutputDirectory")
			{
				Result = mp_OutputDir / "Output";
				if (_pStorePositions)
					_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.OutputDirectory")->f_AddValue(Result, false);

				o_bSuccess = true;
				return CEJSONSorted(Result);
			}
			else if (_Value == "SourceFileName")
			{
				Result = "";
				if (_pStorePositions)
					_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.SourceFileName")->f_AddValue(Result, false);

				o_bSuccess = true;
				return CEJSONSorted(Result);
			}
			
			return CEJSONSorted();
		}

		NStr::CStr f_GetExpandedPath(NStr::CStr const &_Path, NStr::CStr const &_Base) const override
		{
			return CFile::fs_GetExpandedPath(_Path, _Base);
		}

		CSystemEnvironment f_GetBuildEnvironment(NStr::CStr const &_Platform, NStr::CStr const &_Architecture) const override
		{
			return fg_GetSys()->f_Environment();
		}

		CStr mp_OutputDir;
		CStr mp_BaseDir;

	public:
		TCMap<CPropertyKey, CEJSONSorted> f_GetValues(CBuildSystem const &_BuildSystem, CStr const &_OutputDir) override
		{
			TCMap<CPropertyKey, CEJSONSorted> Values;

			Values[CPropertyKey(_BuildSystem.f_StringCache(), "MToolVersion")] = CBuildSystem::mc_MToolVersion;
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "Generator")] = _BuildSystem.f_GetGenerateSettings().m_Generator;
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "GeneratorFamily")] = "Test";
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "BuildSystemBasePath")] = _BuildSystem.f_GetBaseDir();
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "BuildSystemOutputDir")] = _OutputDir;
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "BuildSystemPlatform")] = "Test";
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "BuildSystemFile")] = _BuildSystem.f_GetGenerateSettings().m_SourceFile;
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "BuildSystemName")] = CFile::fs_GetFileNoExt(_BuildSystem.f_GetGenerateSettings().m_SourceFile);

			Values[CPropertyKey(_BuildSystem.f_StringCache(), "HostPlatform")] = "Test1.0";
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "HostPlatformFamily")] = "Test";
			Values[CPropertyKey(_BuildSystem.f_StringCache(), "HostArchitecture")] = "NoArch";

			return Values;
		}

		TCUnsafeFuture<void> f_Generate(CBuildSystem const *_pBuildSystem, CBuildSystemData const *_pBuildSystemData, CStr _OutputDir, TCMap<CStr, uint32> &o_NumWorkspaceTargets) override
		{
			co_await ECoroutineFlag_CaptureMalterlibExceptions;

			_pBuildSystem->f_StringCache().f_AddString(gc_TestGeneratorDependencyFiles, gc_TestGeneratorDependencyFiles.f_Hash());

			auto &BuildSystem = *_pBuildSystem;
			auto &BuildSystemDataIn = *_pBuildSystemData;

			mp_OutputDir = _OutputDir;
			mp_BaseDir = BuildSystem.f_GetBaseDir();

			BuildSystem.f_SetGeneratorInterface(this);
			auto Cleanup = g_OnScopeExit / [&]
				{
					BuildSystem.f_SetGeneratorInterface(nullptr);
				}
			;

			CStr SolutionDir = _OutputDir;
			CStr BuildSystemBase = BuildSystem.f_GetBaseDir();

			TCMap<CPropertyKey, CEJSONSorted> Values = f_GetValues(BuildSystem, _OutputDir);

			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> Configurations;

			TCVector<TCVector<CConfigurationTuple>> Tuples = BuildSystem.f_EvaluateConfigurationTuples(Values);

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

			co_await BuildSystem.f_GenerateBuildSystem(&Configurations, &Values);

			CEJSONSorted BuildSystemData;

			auto &OutWorkspace = BuildSystemData["Workspaces"].f_Object();

			auto fCopyGroups = [&](CEJSONSorted::CObject &o_Groups, TCMap<CStr, CGroupInfo> const &_Groups, TCMap<CGroupInfo const *, CEJSONSorted::CObject *> &o_Mapping)
				{
					for (auto iGroup = _Groups.f_GetIterator(); iGroup; ++iGroup)
					{
						auto &Group = *(o_Mapping[&*iGroup] = &o_Groups[iGroup.f_GetKey()].f_Object());
						Group["Name"] = iGroup->m_Name;
					}

/*					for (auto iGroup = _Groups.f_GetIterator(); iGroup; ++iGroup)
					{
						if (iGroup->m_pParent)
						{
							auto &OutGroup = o_Groups[iGroup.f_GetKey()].f_Object();
							auto pGroup = o_Mapping.f_FindEqual(&*iGroup->m_pParent);

							DCheck(pGroup && *pGroup);
							if (pGroup)
								OutGroup.m_pParent = *pGroup;
						}
					}*/
				}
			;

			auto fStorePosition = [&](CEJSONSorted &o_Position, CFilePosition const &_Position)
				{
					o_Position["File"] = _Position.m_File;
					o_Position["Character"] = _Position.m_Character;
					o_Position["Line"] = _Position.m_Line;
					o_Position["Column"] = _Position.m_Column;
				}
			;

			for (auto iConfig = Configurations.f_GetIterator(); iConfig; ++iConfig)
			{
				auto Config = iConfig.f_GetKey();
				auto &ConfigData = **iConfig;

				for (auto iWorkspace = ConfigData.m_Workspaces.f_GetIterator(); iWorkspace; ++iWorkspace)
				{
					auto &WorkspaceName = iWorkspace.f_GetKey();
					auto &WorkspaceInfo = **iWorkspace;

					BuildSystem.f_GenerateBuildSystem_Workspace(Config, ConfigData, &WorkspaceInfo, ReservedGroups, gc_TestGeneratorDependencyFiles);

					auto &Workspace = OutWorkspace[WorkspaceName];
					fStorePosition(Workspace["Position"], WorkspaceInfo.m_pEntity->f_Data().m_Position);
					Workspace["Name"] = WorkspaceInfo.m_EntityName;
					Workspace["EnabledConfigs"][Config.f_GetFullName()] = true;

					TCMap<CGroupInfo const *, CEJSONSorted::CObject *> WorkspaceGroupMapping;
					fCopyGroups(Workspace["Groups"].f_Object(), WorkspaceInfo.m_Groups, WorkspaceGroupMapping);

					for (auto iTarget = WorkspaceInfo.m_Targets.f_GetIterator(); iTarget; ++iTarget)
					{
						auto &TargetName = iTarget.f_GetKey();
						auto &TargetInfo = *iTarget;

						auto &Target = Workspace["Targets"][TargetName];
						fStorePosition(Target["OuterPosition"], TargetInfo.m_pOuterEntity->f_Data().m_Position);
						fStorePosition(Target["InnerPosition"], TargetInfo.m_pInnerEntity->f_Data().m_Position);
						Target["Name"] = TargetInfo.m_EntityName;
						Target["EnabledConfigs"][Config.f_GetFullName()] = true;

/*						if (TargetInfo.m_pGroup)
						{
							auto pGroup = WorkspaceGroupMapping.f_FindEqual(&*TargetInfo.m_pGroup);
							DCheck(pGroup);
							if (pGroup)
								Target.m_pGroup = *pGroup;
						}*/

						for (auto iDependency = TargetInfo.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
						{
							auto const &DependencyInfo = *iDependency;
							auto const &DependencyName = iDependency->f_GetName();
							auto &Dependency = Target["Dependencies"][DependencyName];
							fStorePosition(Dependency["Position"], DependencyInfo.m_pEntity->f_Data().m_Position);
							Dependency["EnabledConfigs"][Config.f_GetFullName()] = true;

							Dependency["PerConfig"][Config.f_GetFullName()]["Link"] = DependencyInfo.m_bLink;
						}

						TCMap<CGroupInfo const *, CEJSONSorted::CObject *> TargetGroupMapping;
						fCopyGroups(Target["Groups"].f_Object(), TargetInfo.m_Groups, TargetGroupMapping);

						for (auto iFile = TargetInfo.m_Files.f_GetIterator(); iFile; ++iFile)
						{
							auto &FileName = iFile.f_GetKey();
							auto &FileInfo = *iFile;

							auto &File = Target["Files"][FileName.m_GroupPath][FileName.m_FileName];
							fStorePosition(File["Position"], FileInfo.m_pEntity->f_Data().m_Position);
							File["EnabledConfigs"][Config.f_GetFullName()] = true;
/*							if (FileInfo.m_pGroup)
							{
								auto pGroup = TargetGroupMapping.f_FindEqual(&*FileInfo.m_pGroup);
								DCheck(pGroup);
								if (pGroup)
									File.m_pGroup = *pGroup;
							}*/
						}
					}
				}
			}

			CFile::fs_WriteStringToFile(_OutputDir / "BuildSystemData.json", BuildSystemData.f_ToString());

			co_return {};
		}
	};

	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Test);
}
