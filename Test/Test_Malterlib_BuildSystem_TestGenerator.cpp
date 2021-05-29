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

	class CBuildSystemGenerator_Test : public CBuildSystemGenerator, public ICGeneratorInterface
	{
		// CGeneratorInterface
		bool f_GetBuiltin(NStr::CStr const &_Value, NStr::CStr &_Result) const override
		{
			if (_Value == "BasePathRelativeProject")
			{
				_Result = ".";
				return true;
			}
			else if (_Value == "BasePath")
			{
				_Result = mp_BaseDir;
				return true;
			}
			else if (_Value == "GeneratedBuildSystemDir")
			{
				_Result = mp_OutputDir;
				return true;
			}
			else if (_Value == "ProjectPath")
			{
				_Result = ".";
				return true;
			}
			else if (_Value == "Inherit")
			{
				_Result = "";
				return true;
			}
			else if (_Value == "IntermediateDirectory")
			{
				_Result = mp_OutputDir / "Intermediate";
				return true;
			}
			else if (_Value == "OutputDirectory")
			{
				_Result = mp_OutputDir / "Output";
				return true;
			}
			else if (_Value == "SourceFileName")
			{
				_Result = "";
				return true;
			}
			return false;
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
		TCMap<CPropertyKey, CEJSON> f_GetValues(CBuildSystem const &_BuildSystem, CStr const &_OutputDir) override
		{
			TCMap<CPropertyKey, CEJSON> Values;

			Values[CPropertyKey("MToolVersion")] = CBuildSystem::mc_MToolVersion;
			Values[CPropertyKey("Generator")] = _BuildSystem.f_GetGenerateSettings().m_Generator;
			Values[CPropertyKey("GeneratorFamily")] = "Test";
			Values[CPropertyKey("BuildSystemBasePath")] = _BuildSystem.f_GetBaseDir();
			Values[CPropertyKey("BuildSystemOutputDir")] = _OutputDir;
			Values[CPropertyKey("BuildSystemPlatform")] = "Test";
			Values[CPropertyKey("BuildSystemFile")] = _BuildSystem.f_GetGenerateSettings().m_SourceFile;
			Values[CPropertyKey("BuildSystemName")] = CFile::fs_GetFileNoExt(_BuildSystem.f_GetGenerateSettings().m_SourceFile);

			Values[CPropertyKey("HostPlatform")] = "Test1.0";
			Values[CPropertyKey("HostPlatformFamily")] = "Test";
			Values[CPropertyKey("HostArchitecture")] = "NoArch";

			return Values;
		}

		virtual void f_Generate(CBuildSystem const &_BuildSystem, CBuildSystemData const &_BuildSystemData, CStr const &_OutputDir) override
		{
			mp_OutputDir = _OutputDir;
			mp_BaseDir = _BuildSystem.f_GetBaseDir();

			_BuildSystem.f_SetGeneratorInterface(this);
			auto Cleanup = g_OnScopeExit > [&]
				{
					_BuildSystem.f_SetGeneratorInterface(nullptr);
				}
			;

			CStr SolutionDir = _OutputDir;
			CStr BuildSystemBase = _BuildSystem.f_GetBaseDir();

			TCMap<CPropertyKey, CEJSON> Values = f_GetValues(_BuildSystem, _OutputDir);

			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> Configurations;

			TCVector<TCVector<CConfigurationTuple>> Tuples = _BuildSystem.f_EvaluateConfigurationTuples(Values);

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

			_BuildSystem.f_GenerateBuildSystem(Configurations, Values);

			CEJSON BuildSystemData;

			auto &OutWorkspace = BuildSystemData["Workspaces"].f_Object();

			auto fCopyGroups = [&](CEJSON::CObject &o_Groups, TCMap<CStr, CGroupInfo> const &_Groups, TCMap<CGroupInfo const *, CEJSON::CObject *> &o_Mapping)
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

			auto fStorePosition = [&](CEJSON &o_Position, CFilePosition const &_Position)
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

					_BuildSystem.f_GenerateBuildSystem_Workspace(Config, ConfigData, &WorkspaceInfo, ReservedGroups, "TestGeneratorDependencyFiles");

					auto &Workspace = OutWorkspace[WorkspaceName];
					fStorePosition(Workspace["Position"], WorkspaceInfo.m_pEntity->f_Data().m_Position);
					Workspace["Name"] = WorkspaceInfo.m_EntityName;
					Workspace["EnabledConfigs"][Config.f_GetFullName()] = true;

					TCMap<CGroupInfo const *, CEJSON::CObject *> WorkspaceGroupMapping;
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

						TCMap<CGroupInfo const *, CEJSON::CObject *> TargetGroupMapping;
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
		}
	};

	DMibRuntimeClass(CBuildSystemGenerator, CBuildSystemGenerator_Test);
}
