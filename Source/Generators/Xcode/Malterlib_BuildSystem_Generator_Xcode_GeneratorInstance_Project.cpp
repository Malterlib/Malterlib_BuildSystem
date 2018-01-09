// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "../../Malterlib_BuildSystem_Helpers.h"
#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <Mib/Process/ProcessLaunch>
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NXcode
{
	void CGeneratorInstance::f_GenerateProjectFile(CProject &_Project, CStr const &_OutputDir, TCMap<CConfiguration, TCSet<CStr>> &_Runnables, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable) const
	{
		auto & ThreadLocal = *m_ThreadLocal;
		ThreadLocal.mp_EvaluatedTypesInUse.f_Clear();
		ThreadLocal.mp_UsedCTypes.f_Clear();
		ThreadLocal.mp_XcodeSettingsFromTypes.f_Clear();
		ThreadLocal.mp_CompileFlagsValues.f_Clear();
		ThreadLocal.mp_EvaluatedOverriddenCompileFlags.f_Clear();
		ThreadLocal.mp_EvaluatedTargetSettings.f_Clear();
		ThreadLocal.mp_EvaluatedTypeCompileFlags.f_Clear();
		ThreadLocal.mp_EvaluatedCompileFlags.f_Clear();
		ThreadLocal.mp_OtherCPPFlags.f_Clear();
		ThreadLocal.mp_OtherObjCPPFlags.f_Clear();
		ThreadLocal.mp_OtherAssemblerFlags.f_Clear();
		ThreadLocal.mp_OtherCFlags.f_Clear();
		ThreadLocal.mp_OtherObjCFlags.f_Clear();
		ThreadLocal.mp_MocOutputPatternCPP.f_Clear();
		ThreadLocal.mp_BuildRules.f_Clear();

		ThreadLocal.m_ProjectOutputDir = CFile::fs_AppendPath(_OutputDir, CStr(_Project.f_GetName() + ".xcodeproj"));
		ThreadLocal.f_CreateDirectory(ThreadLocal.m_ProjectOutputDir);

		fp_EvaluateTargetSettings(_Project);
		fp_EvaluateFiles(_Project);

		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);
			auto &NativeTarget = _Project.f_GetDefaultNativeTarget(Configuration);
			NativeTarget.m_Name = "{} {}"_f << _Project.f_GetName() << Configuration.f_GetFullName();
			auto UsedCTypes = ThreadLocal.mp_UsedCTypes[Configuration];
			if (UsedCTypes.f_GetLen() > 1)
			{
				CStr DefaultType;
				if (UsedCTypes.f_FindEqual("C++"))
					DefaultType = "C++";
				else if (UsedCTypes.f_FindEqual("ObjC++"))
					DefaultType = "ObjC++";
				else
					DefaultType = *UsedCTypes.f_FindSmallest();
				NativeTarget.m_CType = DefaultType;
				UsedCTypes.f_Remove(DefaultType);
				for (auto &UsedType : UsedCTypes)
				{
					auto &NewNativeTarget = NativeTargets.f_Insert(NativeTarget);
					NewNativeTarget.m_IncludedTypes[UsedType];
					NewNativeTarget.m_Name = "{}{} {}"_f << _Project.f_GetName() << UsedType << Configuration.f_GetFullName();
					NewNativeTarget.m_CType = UsedType;
					NewNativeTarget.m_BuildScripts.f_Clear();
					NativeTarget.m_CTargets.f_Insert(&NewNativeTarget);
				}

				NativeTarget.m_ExcludedTypes = UsedCTypes;
			}
			else if (!UsedCTypes.f_IsEmpty())
				NativeTarget.m_CType = *UsedCTypes.f_FindSmallest();

			NativeTarget.m_bDefaultTarget = true;
		}

		fp_EvaluateFileTypeCompileFlags(_Project);
		fp_EvaluateDependencies(_Project);

		fp_GenerateCompilerFlags(_Project);

		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);
			auto pEvaluatedSettings = ThreadLocal.mp_EvaluatedTargetSettings.f_FindEqual(Configuration);
			DCheck(pEvaluatedSettings);
			auto &EvaluatedSettings = *pEvaluatedSettings;
			auto &NativeTarget = NativeTargets.f_GetFirst();

			for (auto &NativeTarget : NativeTargets)
			{

				NativeTarget.m_ProductName = EvaluatedSettings.m_Element["Name"].f_GetValue();
				NativeTarget.m_XcodeProductName = EvaluatedSettings.m_Element["PRODUCT_NAME"].f_GetValue();
				if (!NativeTarget.m_bDefaultTarget)
				{
					NativeTarget.m_XcodeProductName += NativeTarget.m_CType;
					NativeTarget.m_ProductName += NativeTarget.m_CType;
				}

				CStr Extension = EvaluatedSettings.m_Element["EXECUTABLE_EXTENSION"].f_GetValue();
				if (!NativeTarget.m_bDefaultTarget)
					Extension = "a";

				if (Extension.f_IsEmpty())
					NativeTarget.m_ProductPath = NativeTarget.m_ProductName;
				else
					NativeTarget.m_ProductPath = "{}.{}"_f <<  NativeTarget.m_ProductName << Extension;

				if (NativeTarget.m_bDefaultTarget)
					NativeTarget.m_ProductType = EvaluatedSettings.m_Element["ProductType"].f_GetValue();
				else
					NativeTarget.m_ProductType = "com.apple.product-type.library.static";

				NativeTarget.m_ProductSourceTree = "BUILT_PRODUCTS_DIR";
				NativeTarget.m_BuildActionMask = 0;
			}

			for (auto *pDependentTarget : NativeTarget.m_CTargets)
			{
				auto &DependentTarget = *pDependentTarget;

				auto DepMap = _Project.m_DependenciesMap(DependentTarget.m_ProductName);
				auto &NewDependency = DepMap.f_GetResult();
				if (DepMap.f_WasCreated())
				{
					_Project.m_DependenciesOrdered.f_Insert(NewDependency);
					NewDependency.m_bInternal = true;
				}

				NewDependency.m_Position = _Project.m_Position;
				NewDependency.m_EnabledConfigs[Configuration] = nullptr;
				auto &PerConfig = NewDependency.m_PerConfig[Configuration];

				PerConfig.m_bLink = true;

				PerConfig.m_SearchPath = EvaluatedSettings.m_Element["CONFIGURATION_BUILD_DIR"].f_GetValue();
				PerConfig.m_CalculatedDependencyExtension = "a";
				PerConfig.m_CalculatedDependencyName = DependentTarget.m_XcodeProductName;
				PerConfig.m_CalculatedPath = "{}.{}"_f << PerConfig.m_CalculatedDependencyName << PerConfig.m_CalculatedDependencyExtension;

				CStr Arch = EvaluatedSettings.m_Element["NATIVE_ARCH_ACTUAL"].f_GetValue();
				CStr BuildDir = EvaluatedSettings.m_Element["BUILD_DIR"].f_GetValue();
				CStr ObjectPath = "{}/$MalterlibXcodeObjectFileDirName/{}"_f << BuildDir << Arch;
				CStr DummyPath = "{}/Dummy.o"_f << ObjectPath;
				CStr LibraryPath = "{}/{}"_f << PerConfig.m_SearchPath << PerConfig.m_CalculatedPath;

				auto &PreBuildScript = NativeTarget.m_BuildScripts["PreBuildScript"];
				PreBuildScript.m_Inputs.f_Insert(LibraryPath);
				PreBuildScript.m_Script += 	R"-----(
if [ -e "{0}" ]; then
	MTool CopyWriteTimeIfNewer "{1}" "{0}"
fi
	)-----"_f
					<< DummyPath.f_EscapeStrNoQuotes("\\'\r\n\t", "\\'rnt")
					<< LibraryPath.f_EscapeStrNoQuotes("\\'\r\n\t", "\\'rnt")
				;

			}
		}

		fp_GenerateBuildConfigurationFiles(_Project, _OutputDir);
		fp_GenerateBuildConfigurationFilesList(_Project, _OutputDir, _Project.m_BuildConfigurationList);
		fp_GenerateToolRunScript(_Project, _OutputDir);

		bool bSchemesChanged = fp_GenerateSchemes(_Project, _Runnables, _Buildable);

		CStr FileData;

		auto fl_Output = [&] (CStr const& _Data)
			{
				FileData += _Data;
				FileData += "\n";
			}
		;

		fl_Output("// !$*UTF8*$!\n{\n\tarchiveVersion = 1;\n\tclasses = {\n\t};\n\tobjectVersion = 47;\n\tobjects = {\n");

		fl_Output("/* Begin PBXBuildFile section */");
		fp_GeneratePBXBuildFileSection(_Project, FileData);
		fl_Output("/* End PBXBuildFile section */\n");

		fl_Output("/* Begin PBXBuildRule section */");
		fp_GeneratePBXBuildRule(_Project, FileData);
		fl_Output("/* End PBXBuildRule section */\n");

		fl_Output("/* Begin PBXContainerItemProxy section */");
		fp_GeneratePBXContainerItemProxySection(_Project, FileData);
		fl_Output("/* End PBXContainerItemProxy section */\n");

		fl_Output("/* Begin PBXFileReference section */");
		fp_GeneratePBXFileReferenceSection(_Project, _OutputDir, FileData);
		fl_Output("/* End PBXFileReference section */\n");

		fl_Output("/* Begin PBXFrameworksBuildPhase section */");
		fp_GeneratePBXFrameworksBuildPhaseSection(_Project, FileData);
		fl_Output("/* End PBXFrameworksBuildPhase section */\n");

		fl_Output("/* Begin PBXGroup section */");
		fp_GeneratePBXGroupSection(_Project, FileData);
		fl_Output("/* End PBXGroup section */\n");

		fp_GeneratePBXLegacyTargetSection(_Project, FileData);

		fl_Output("/* Begin PBXNativeTarget section */");
		fp_GeneratePBXNativeTargetSection(_Project, FileData);
		fl_Output("/* End PBXNativeTarget section */\n");

		fl_Output("/* Begin PBXProject section */");
		fp_GeneratePBXProjectSection(_Project, FileData);
		fl_Output("/* End PBXProject section */\n");
		
		fl_Output("/* Begin PBXShellScriptBuildPhase section */");
		fp_GeneratePBXShellScriptBuildPhaseSection(_Project, FileData);
		fl_Output("/* End PBXShellScriptBuildPhase section */\n");

		fl_Output("/* Begin PBXSourcesBuildPhase section */");
		fp_GeneratePBXSourcesBuildPhaseSection(_Project, FileData);
		fl_Output("/* End PBXSourcesBuildPhase section */\n");

		fl_Output("/* Begin PBXTargetDependency section */");
		fp_GeneratePBXTargetDependencySection(_Project, FileData);
		fl_Output("/* End PBXTargetDependency section */\n");

		fl_Output("/* Begin XCBuildConfiguration section */");
		fp_GenerateXCBuildConfigurationSection(_Project, FileData);
		fl_Output("/* End XCBuildConfiguration section */\n");

		fl_Output("/* Begin XCConfigurationList section */");
		fp_GenerateXCConfigurationList(_Project, FileData);
		fl_Output("/* End XCConfigurationList section */");
		
		fl_Output("\t};");
		fl_Output((CStr::CFormat("\trootObject = {} /* Project object */;") << _Project.f_GetGUID()).f_GetStr());
		fl_Output("}");

		{
			CStr GeneratedFile = CFile::fs_AppendPath(ThreadLocal.m_ProjectOutputDir,  CStr("generatedContainer"));
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(GeneratedFile, CStr(), _Project.m_pSolution->f_GetName(), bWasCreated, false))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << GeneratedFile));

			if (bWasCreated)
			{
				TCVector<uint8> FileData;
				CFile::fs_WriteStringToVector(FileData, CStr());
				m_BuildSystem.f_WriteFile(FileData, GeneratedFile);
			}
		}
		{
			CStr OutputFile = CFile::fs_AppendPath(ThreadLocal.m_ProjectOutputDir,  CStr("project.pbxproj"));

			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(OutputFile, FileData, _Project.m_pSolution->f_GetName(), bWasCreated, false))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << OutputFile));

			if (bWasCreated)
			{
				TCVector<uint8> FileDataVector;
				CFile::fs_WriteStringToVector(FileDataVector, CStr(FileData), false);
				;
				
				if (!m_BuildSystem.f_WriteFile(FileDataVector, OutputFile) && bSchemesChanged)
				{
					// Force Xcode to reload schemes
					NTime::CTime Now = NTime::CTime::fs_NowUTC();
					NFile::CFile Temp;
					Temp.f_Open(OutputFile, EFileOpen_WriteAttribs | EFileOpen_ShareAll | EFileOpen_DontTruncate);
					Temp.f_SetWriteTime(Now);		
					m_BuildSystem.f_SetFileChanged(OutputFile);
				}
			}
		}
	}

	void CGeneratorInstance::fp_CalculateDependencyProductPath(CProject &_Project, CProjectDependency &_Dependency, TCMap<CConfiguration, CEntityPointer> const &_EnabledConfigurations) const
	{
		TCVector<CStr> SearchList;
		SearchList.f_Insert("SharedTarget");

		TCMap<CConfiguration, CEntityPointer> TargetConfigs;
		for (auto &Entity : _EnabledConfigurations)
		{
			auto &Configuration = _EnabledConfigurations.fs_GetKey(Entity);
			TargetConfigs[Configuration] = _Project.m_EnabledProjectConfigs[Configuration];
		}

		TCMap<CConfiguration, CConfigResult> ExtensionEntities;
		fp_GetConfigValue(
			TargetConfigs
			, _Project.m_EnabledProjectConfigs
			, (*_Project.m_EnabledConfigs.f_GetIterator())->m_Position
			, EPropertyType_Target
			, "FileExtension"
			, false
			, false
			, &SearchList
			, nullptr
			, CStr()
			, CStr()
			, ExtensionEntities);

		TCMap<CConfiguration, CConfigResult> ProductNameEntities;
		fp_GetConfigValue(
			TargetConfigs
			, _Project.m_EnabledProjectConfigs
			, (*_Project.m_EnabledConfigs.f_GetIterator())->m_Position
			, EPropertyType_Target
			, "FileName"
			, false
			, false
			, &SearchList
			, nullptr
			, CStr()
			, CStr()
			, ProductNameEntities);

		TCMap<CConfiguration, CConfigResult> OutputEntities;
		fp_GetConfigValue(
			TargetConfigs
			, _Project.m_EnabledProjectConfigs
			, (*_Project.m_EnabledConfigs.f_GetIterator())->m_Position
			, EPropertyType_Target
			, "OutputDirectory"
			, false
			, false
			, &SearchList
			, nullptr
			, CStr()
			, CStr()
			, OutputEntities);

		TCVector<CStr> RootList;
		RootList.f_Insert("Root");

		TCMap<CConfiguration, CConfigResult> TypeEntities;
		fp_GetConfigValue(
			TargetConfigs
			, _Project.m_EnabledProjectConfigs
			, (*_Project.m_EnabledConfigs.f_GetIterator())->m_Position
			, EPropertyType_Target
			, "Type"
			, false
			, false
			, &RootList
			, nullptr
			, CStr()
			, CStr()
			, TypeEntities);

		TCMap<CConfiguration, CConfigResult> EnableLinkerGroupEntities;
		fp_GetConfigValue(
			TargetConfigs
			, _Project.m_EnabledProjectConfigs
			, (*_Project.m_EnabledConfigs.f_GetIterator())->m_Position
			, EPropertyType_Target
			, "EnableLinkerGroups"
			, false
			, false
			, &SearchList
			, nullptr
			, CStr()
			, CStr()
			, EnableLinkerGroupEntities);
		
		TCMap<CConfiguration, CConfigResult> LinkerGroupEntities;
		fp_GetConfigValue(
			TargetConfigs
			, _Project.m_EnabledProjectConfigs
			, (*_Project.m_EnabledConfigs.f_GetIterator())->m_Position
			, EPropertyType_Target
			, "LinkerGroup"
			, false
			, false
			, &SearchList
			, nullptr
			, CStr()
			, CStr()
			, LinkerGroupEntities);
		
		if (!TypeEntities.f_IsEmpty())
			_Dependency.m_Type = (*TypeEntities.f_GetIterator()).m_Element["Type"].f_GetValue();

		for (auto &Entity : _EnabledConfigurations)
		{
			auto &Configuration = _EnabledConfigurations.fs_GetKey(Entity);

			auto *pPerConfig = _Dependency.m_PerConfig.f_FindEqual(Configuration);
			DCheck(pPerConfig);
			if (!pPerConfig)
				continue;

			auto &PerConfig = *pPerConfig;

			if (_Dependency.m_Type != "StaticLibrary" && _Dependency.m_Type != "SharedDynamicLibrary")
				PerConfig.m_bLink = false;
			
			PerConfig.m_SearchPath = OutputEntities[Configuration].m_Element["CONFIGURATION_BUILD_DIR"].f_GetValue();
			PerConfig.m_CalculatedDependencyExtension = ExtensionEntities[Configuration].m_Element["EXECUTABLE_EXTENSION"].f_GetValue();
			PerConfig.m_CalculatedDependencyName = ProductNameEntities[Configuration].m_Element["PRODUCT_NAME"].f_GetValue();

			if (EnableLinkerGroupEntities[Configuration].m_Element["MALTERLIB_ENABLE_LINKER_GROUPS"].f_GetValue() == "true")
				PerConfig.m_LinkerGroup = LinkerGroupEntities[Configuration].m_Element["MALTERLIB_LINKER_GROUP"].f_GetValue();
			
			if (PerConfig.m_CalculatedDependencyExtension.f_IsEmpty())
				PerConfig.m_CalculatedPath = PerConfig.m_CalculatedDependencyName;
			else
				PerConfig.m_CalculatedPath = "{}.{}"_f << PerConfig.m_CalculatedDependencyName << PerConfig.m_CalculatedDependencyExtension;
		}
		
	}

	void CGeneratorInstance::fp_EvaluateFileTypeCompileFlags(CProject& _Project) const
	{
		auto & ThreadLocal = *m_ThreadLocal;
		for (auto Iter = ThreadLocal.mp_EvaluatedTypesInUse.f_GetIterator(); Iter; ++Iter)
		{
			TCMap<CConfiguration, CBuildSystemData> Datas;
			TCMap<CConfiguration, CEntityPointer> Configs;

			CStr Type = Iter.f_GetKey();

			TCVector<CStr> SearchList;
			SearchList.f_Insert(Type);
			if (	Type == "C++" || Type == "C" 
				||	Type == "ObjC++" || Type == "ObjC" )
			{
				SearchList.f_Insert("SharedC");
			}
			SearchList.f_Insert("CompileShared");

			TCMap<CPropertyKey, CStr> StartValuesCompile;
			StartValuesCompile[CPropertyKey(EPropertyType_Compile, "Type")] = Type;

			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				auto &Data = Datas[iConfig.f_GetKey()];

				auto InitialValues = m_BuildSystem.f_GetExternalValues(*(*iConfig)->f_GetRoot());
				auto pStartEntity = iConfig->f_Get();
				auto PathKey = (*iConfig)->f_GetPathKey();
				auto pConfig = m_BuildSystem.f_EvaluateData(Data, InitialValues, pStartEntity, &StartValuesCompile, &PathKey, true, false);
				Configs[iConfig.f_GetKey()] = fg_Explicit(pConfig);
			}

			TCMap<CConfiguration, CConfigResult> Entities;

			fp_SetEvaluatedValues(
				Configs
				, _Project.m_EnabledProjectConfigs
				, false
				, EPropertyType_Compile
				, &SearchList
				, nullptr
				, Type
				, true
				, false
				, Entities);

			if (!Entities.f_IsEmpty())
			{
				ThreadLocal.mp_EvaluatedTypeCompileFlags[Type] = fg_Move(Entities);
			}
		}
	}

	void CGeneratorInstance::fp_EvaluateFiles(CProject& _Project) const
	{
		auto & ThreadLocal = *m_ThreadLocal;
		for (auto IterFile = _Project.m_Files.f_GetIterator(); IterFile; ++IterFile)
		{
			if (IterFile->m_bWasGenerated)
				continue;

			auto Value = fp_GetSingleConfigValue(IterFile->m_EnabledConfigs, EPropertyType_Compile, "Type");
			auto CustomCommandLines = fp_GetConfigValues(IterFile->m_EnabledConfigs, EPropertyType_Compile, "Custom_CommandLine");
			
			CStr Type = Value.m_Value;

			{
				bool bWasCustom = false;
				bool bFirst = true;
				for (auto &CustomCommandLine : CustomCommandLines)
				{
					if (!CustomCommandLine.m_Value.f_IsEmpty())
					{
						if (!bFirst)
							m_BuildSystem.fs_ThrowError(CustomCommandLine.m_Position, "Inconsistent usage of Custom_CommandLine");

						bWasCustom = true;
						Type = "Custom";
					}
					else if (bWasCustom)
						m_BuildSystem.fs_ThrowError(CustomCommandLine.m_Position, "Inconsistent usage of Custom_CommandLine");
					bFirst = false;
				}
			}

			if (Type.f_IsEmpty())
				m_BuildSystem.fs_ThrowError(Value.m_Position, "No compile type found");

			IterFile->m_Type = Type;
			
			IterFile->m_bWasGenerated = true;

			TCVector<CStr> SearchList;
			SearchList.f_Insert("Root");

			TCMap<CConfiguration, CConfigResult> Entities;

			fp_GetConfigValue
				(
					IterFile->m_EnabledConfigs
					, _Project.m_EnabledProjectConfigs
					, (*IterFile->m_EnabledConfigs.f_GetIterator())->m_Position
					, EPropertyType_Compile
					, "Type"
					, true
					, true
					, &SearchList
					, nullptr
					, CStr()
					, CStr()
					, Entities
				)
			;

			if (Entities.f_IsEmpty())
				m_BuildSystem.fs_ThrowError((*IterFile->m_EnabledConfigs.f_GetIterator())->m_Position, "Compile.Type does not exist");
			
			auto Result = *Entities.f_GetIterator();

			IterFile->m_LastKnownFileType = Result.m_Element["Type"].f_GetValue();

			// Extra matching for last known file type
			if (IterFile->m_LastKnownFileType == "text")
			{
				CEntityKey EntityKey;
				EntityKey.m_Type = EEntityType_GeneratorSetting;
				EntityKey.m_Name = "ExtraFileTypeMatches";
				auto pExtraMatches = m_pGeneratorSettings->m_ChildEntitiesMap.f_FindEqual(EntityKey);
				if (pExtraMatches)
				{
					CPropertyKey PropertyKey;
					PropertyKey.m_Type = EPropertyType_Property;
					PropertyKey.m_Name = CFile::fs_GetExtension(IterFile->f_GetName());

					auto pMatch = pExtraMatches->m_EvaluatedProperties.f_FindEqual(PropertyKey);
					if (pMatch)
					{
						IterFile->m_LastKnownFileType = pMatch->m_Value;
					}
				}
			}

			// Order them in terms of compile type
			bint bEvaluateCompileFlags = false;
			{
				EBuildFileType FileType = EBuildFileType_None;

				if (IterFile->m_Type == "Custom")
					FileType = EBuildFileType_Custom;
				else if
					(
						IterFile->m_Type == "C++"
						|| IterFile->m_Type == "C"
						|| IterFile->m_Type == "ObjC++"
						|| IterFile->m_Type == "ObjC"
						|| IterFile->m_Type == "Assembler"
					)
				{
					bool bInitEarly = fp_GetSingleConfigValue(IterFile->m_EnabledConfigs, EPropertyType_Compile, "InitEarly").m_Value == "true";
					
					if (bInitEarly)
						FileType = EBuildFileType_CCompileInitEarly;
					else
						FileType = EBuildFileType_CCompile;
				}
				else if (IterFile->m_Type == "Header")
					FileType = EBuildFileType_CInclude;
				else if (IterFile->m_Type == "QtMoc")
					FileType = EBuildFileType_QTMoc;
				else if (IterFile->m_Type == "QtUic")
					FileType = EBuildFileType_QTUic;
				else if (IterFile->m_Type == "QtRcc")
					FileType = EBuildFileType_QTRcc;
				else if (IterFile->m_Type == "MlTwk")
					FileType = EBuildFileType_MlTwk;
				else if (IterFile->m_Type == "MalterlibFS")
					FileType = EBuildFileType_MalterlibFS;

				if (IterFile->m_Type == "QtMoc" &&
					CFile::fs_GetExtension(IterFile->f_GetName()) == "cpp")
					IterFile->m_LastKnownFileType = "sourcecode.c.cppmoc";

				bEvaluateCompileFlags = FileType != EBuildFileType_None;

				CBuildFileRef& BuildRef = _Project.mp_OrderedBuildTypes[FileType].f_Insert();
				BuildRef.m_FileName = IterFile->f_GetName();
				BuildRef.m_Name = CFile::fs_GetFile(IterFile->f_GetName());
				BuildRef.m_FileRefGUID = IterFile->f_GetFileRefGUID();

				for (auto &Entity : IterFile->m_EnabledConfigs)
				{
					auto &Configuration = IterFile->m_EnabledConfigs.fs_GetKey(Entity);
					BuildRef.m_BuildGUIDs[Configuration] = IterFile->f_GetBuildRefGUID(Configuration);
				}

				BuildRef.m_CompileFlagsGUID = IterFile->f_GetCompileFlagsGUID();
				BuildRef.m_Type = IterFile->m_Type;
				BuildRef.m_bHasCompilerFlags = (FileType == EBuildFileType_CCompile || FileType == EBuildFileType_CCompileInitEarly || FileType == EBuildFileType_QTMoc || FileType == EBuildFileType_MalterlibFS);

				{
					auto DisabledConfigs = fp_GetConfigValues(IterFile->m_EnabledConfigs, EPropertyType_Compile, "Disabled");
					for (auto &Disabled : DisabledConfigs)
					{
						if (Disabled.m_Value == "true")
							BuildRef.m_Disabled[DisabledConfigs.fs_GetKey(Disabled)];
					}
				}

				if (FileType == EBuildFileType_CCompile || FileType == EBuildFileType_CCompileInitEarly)
				{
					for (auto &pEntity : IterFile->m_EnabledConfigs)
					{
						auto &Configuration = IterFile->m_EnabledConfigs.fs_GetKey(pEntity);
						if (BuildRef.m_Disabled.f_FindEqual(Configuration))
							continue;
						ThreadLocal.mp_UsedCTypes[Configuration][IterFile->m_Type];
					}
				}

				if (FileType == EBuildFileType_Custom)
				{
					for (auto &CustomCommandLine : CustomCommandLines)
					{
						auto &Configuration = CustomCommandLines.fs_GetKey(CustomCommandLine);

						if (BuildRef.m_Disabled.f_FindEqual(Configuration))
							continue;

						auto &BuildRefRule = BuildRef.m_BuildRules[Configuration];

						BuildRefRule.m_GUID = IterFile->f_GetBuildRuleGUID(Configuration);

						BuildRefRule.m_MalterlibCustomBuildCommandLine = CustomCommandLine.m_Value.f_EscapeStrNoQuotes("\\'\r\n\t", "\\'rnt");

						CStr WorkingDirectory = fp_GetConfigValue(IterFile->m_EnabledConfigs, Configuration, EPropertyType_Compile, "Custom_WorkingDirectory").m_Value;
						BuildRefRule.m_WorkingDirectory = WorkingDirectory.f_EscapeStrNoQuotes("\\'\r\n\t", "\\'rnt");

						CStr Output = fp_GetConfigValue(IterFile->m_EnabledConfigs, Configuration, EPropertyType_Compile, "Custom_Outputs").m_Value;
						while (!Output.f_IsEmpty())
						{
							CStr ThisOutput = fg_GetStrSep(Output, ";");
							BuildRefRule.m_Outputs.f_Insert(ThisOutput);

							{
								CEntityKey EntityKey;
								EntityKey.m_Type = EEntityType_File;
								EntityKey.m_Name = ThisOutput;

								auto pEntity = IterFile->m_EnabledConfigs.f_FindEqual(Configuration);
								auto pParent = (*pEntity)->m_pParent;

								auto *pNewEntity = &pParent->m_ChildEntitiesMap(EntityKey, pParent).f_GetResult();
								pParent->m_ChildEntitiesOrdered.f_Insert(*pNewEntity);

								TCMap<CConfiguration, CEntityPointer> EnabledConfigs;
								EnabledConfigs[Configuration] = pNewEntity;

								CStr OutputType = fp_GetConfigValue(EnabledConfigs, Configuration, EPropertyType_Compile, "Type").m_Value;
								if (BuildRefRule.m_OutputType.f_IsEmpty())
									BuildRefRule.m_OutputType = OutputType;
								else if (OutputType != BuildRefRule.m_OutputType)
								{
									m_BuildSystem.fs_ThrowError
										(
										 	CustomCommandLine.m_Position, "Output type for custom compile cannot be varied per configuration. '{}' != '{}'"_f
										 	<< OutputType
										 	<< BuildRefRule.m_OutputType
										)
									;
								}

								if (FileType == EBuildFileType_CCompile || FileType == EBuildFileType_CCompileInitEarly)
								{
									ThreadLocal.mp_UsedCTypes[Configuration][OutputType];
									ThreadLocal.mp_EvaluatedTypesInUse[OutputType];
								}

								pParent->m_ChildEntitiesMap.f_Remove(EntityKey);
							}
						}

					}
				}
				
				auto TabWidth = fp_GetSingleConfigValue(IterFile->m_EnabledConfigs, EPropertyType_Compile, "TabWidth");
				auto IndentWidth = fp_GetSingleConfigValue(IterFile->m_EnabledConfigs, EPropertyType_Compile, "IndentWidth");
				auto UsesTabs = fp_GetSingleConfigValue(IterFile->m_EnabledConfigs, EPropertyType_Compile, "UsesTabs");
				
				if (!TabWidth.m_Value.f_IsEmpty())
					IterFile->m_TabWidth = TabWidth.m_Value.f_ToInt(uint8(0));
				if (!IndentWidth.m_Value.f_IsEmpty())
					IterFile->m_IndentWidth = IndentWidth.m_Value.f_ToInt(uint8(0));
				if (!UsesTabs.m_Value.f_IsEmpty())
				{
					if (UsesTabs.m_Value == "true")
						IterFile->m_UsesTabs = 1;
					else if (UsesTabs.m_Value == "false")
						IterFile->m_UsesTabs = 2;
					else
					{
						m_BuildSystem.fs_ThrowError(UsesTabs.m_Position, CStr::CFormat("UsesTabs has to be either true or false ('{}' was specified)") << UsesTabs.m_Value);
					}
				}

				IterFile->m_bHasCompilerFlags = BuildRef.m_bHasCompilerFlags;
			}

			// Evaluate compile flags
			if (bEvaluateCompileFlags)
			{
				TCVector<CStr> SearchList;
				SearchList.f_Insert(IterFile->m_Type);
				if (	IterFile->m_Type == "C++"
					||	IterFile->m_Type == "C"
					|| 	IterFile->m_Type == "ObjC++" 
					||	IterFile->m_Type == "ObjC")
				{
					SearchList.f_Insert("SharedC");
				}
				SearchList.f_Insert("CompileShared");

				TCMap<CConfiguration, CConfigResult> CompileEntities;

				fp_SetEvaluatedValues(
					IterFile->m_EnabledConfigs
					, _Project.m_EnabledProjectConfigs
					, true
					, EPropertyType_Compile
					, &SearchList
					, nullptr
					, IterFile->m_Type
					, false
					, false
					, CompileEntities);

				if (!CompileEntities.f_IsEmpty())
				{
					ThreadLocal.mp_EvaluatedCompileFlags[IterFile->f_GetCompileFlagsGUID()] = fg_Move(CompileEntities);	
				}
				ThreadLocal.mp_EvaluatedTypesInUse[IterFile->m_Type];
			}
		}
	}

	void CGeneratorInstance::fp_GeneratePBXBuildRule(CProject &_Project, CStr &o_Output) const
	{
		TCSortedPerform<CStr> ToPerform;
		
		auto &ThreadLocal = *m_ThreadLocal;
		if (!_Project.mp_OrderedBuildTypes.f_FindEqual(EBuildFileType_Custom))
			return;

		auto &CustomBuildType = _Project.mp_OrderedBuildTypes[EBuildFileType_Custom];

		for (auto &File : CustomBuildType)
		{
			auto &FileName = File.m_FileName;
			for (auto &BuildRule : File.m_BuildRules)
			{
				auto &Configuration = File.m_BuildRules.fs_GetKey(BuildRule);

				ToPerform.f_Add
					(
						BuildRule.m_GUID
						,[FileName, BuildRule, &o_Output]
						{
							CStr OutputFiles;
							for (auto &Output : BuildRule.m_Outputs)
								fg_AddStrSep(OutputFiles, "\t\t\t\t\"{}\""_f << Output, "\n");

							TCSet<CStr> OutputDirs;
							for (auto &Output : BuildRule.m_Outputs)
								OutputDirs[CFile::fs_GetPath(Output)];

							CStr CreateOutputDirs;
							for (auto &OutputDir : OutputDirs)
								fg_AddStrSep(CreateOutputDirs, "mkdir -p \\\"{}\\\""_f << OutputDir.f_EscapeStrNoQuotes(), "\\n");

							o_Output
								+= CStr::CFormat
								(
	R"-----(		{} /* PBXBuildRule */ = {{
				isa = PBXBuildRule;
				compilerSpec = com.apple.compilers.proxy.script;
				filePatterns = {};
				fileType = pattern.proxy;
				isEditable = 1;
				setIsAlternate = 1;
				outputFiles = (
	{}
				);
								 script = "#!/bin/bash\nexport PATH=\"$MalterlibBuildSystemExecutablePath:$PATH\"\neval $OTHER_INPUT_FILE_FLAGS\n{}\ncd \"{}\"\n{}\n";
			};
	)-----"
								)
								<< BuildRule.m_GUID
								<< FileName.f_EscapeStr()
								<< OutputFiles
								<< CreateOutputDirs
								<< BuildRule.m_WorkingDirectory.f_EscapeStrNoQuotes()
								<< BuildRule.m_MalterlibCustomBuildCommandLine.f_EscapeStrNoQuotes()
							;

						}
					)
				;
				ThreadLocal.mp_BuildRules[Configuration][BuildRule.m_GUID] = BuildRule.m_OutputType;
			}
}
		

		ToPerform.f_Perform();
	}
	
	void CGeneratorInstance::fp_GeneratePBXBuildFileSection(CProject &_Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;
		
		auto & ThreadLocal = *m_ThreadLocal;
		for (auto Iter = _Project.mp_OrderedBuildTypes.f_GetIterator(); Iter; ++Iter)
		{
			if (Iter.f_GetKey() == EBuildFileType_None || Iter.f_GetKey() == EBuildFileType_CInclude)
				continue;

			for (auto &File : *Iter)
			{
				auto pFile = &File;
				for (auto &BuildGUID : File.m_BuildGUIDs)
				{
					ToPerform.f_Add
						(
							BuildGUID
							,[&, pFile, BuildGUID]
							{
								_Output
									+= CStr::CFormat("\t\t{} /* {} in Sources */ = {{isa = PBXBuildFile; fileRef = {} /* {} */;")
									<< BuildGUID
									<< pFile->m_Name
									<< pFile->m_FileRefGUID
									<< pFile->m_Name
								;

								if (pFile->m_bHasCompilerFlags)
								{
									CStr Value;
									if (ThreadLocal.mp_CompileFlagsValues.f_Exists(pFile->m_CompileFlagsGUID))
										Value = pFile->m_CompileFlagsGUID;
									else
									{
										CStr SharedFlag = (CStr::CFormat("{}SharedFlags") << fp_MakeNiceSharedFlagValue(pFile->m_Type));
										if (ThreadLocal.mp_CompileFlagsValues.f_Exists(SharedFlag))
											Value = SharedFlag;
									}

									if (!Value.f_IsEmpty())
										_Output += CStr::CFormat(" settings = {{COMPILER_FLAGS = ${}; };") << Value;
								}

								_Output += " };\n";
							}
						)
					;
				}
			}
		}
		
		ToPerform.f_Perform();
	}
	
	void CGeneratorInstance::fp_GeneratePBXFileReferenceSection(CProject &_Project, CStr const& _OutputDir, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;
		// File references
		{
			for (auto IterFile = _Project.m_Files.f_GetIterator(); IterFile; ++IterFile)
			{
				auto *pFile = &*IterFile;
				ToPerform.f_Add
					(
						pFile->f_GetFileRefGUID()
						, [&, pFile]
						{
							auto &File = *pFile;
							CStr FileRefGUID = File.f_GetFileRefGUID();
							CStr FileName = CFile::fs_GetFile(File.f_GetName());
							CStr LastKnownFileType = File.f_GetLastKnownFileType();
							CStr PathRelativeToProj = CFile::fs_MakePathRelative(File.f_GetName(), _OutputDir);
							_Output += CStr::CFormat("\t\t{} /* {} */ = {{isa = PBXFileReference; ") << FileRefGUID << FileName;
							
							// E2D82353D65B5D12A259517E053FEE25 /* InputRemapper_Shared.cpp */ = {isa = PBXFileReference; explicitFileType = sourcecode.cpp.cpp; fileEncoding = 4; indentWidth = 4; name = InputRemapper_Shared.cpp; path = ../../../Projects/InputRemapperOSX/InputRemapper_Shared.cpp; sourceTree = "<group>"; tabWidth = 4; usesTabs = 1; };
							
							if (LastKnownFileType.f_StartsWith("file.") || LastKnownFileType.f_StartsWith("wrapper."))
								_Output += CStr::CFormat("lastKnownFileType = {};") << LastKnownFileType;
							else if (LastKnownFileType == "image.png")
								_Output += CStr::CFormat("explicitFileType = {};") << LastKnownFileType;
							else
								_Output += CStr::CFormat("explicitFileType = {}; fileEncoding = 4;") << LastKnownFileType;
							
							if (File.m_IndentWidth)
								_Output += CStr::CFormat(" indentWidth = {};") << File.m_IndentWidth;

							_Output += CStr::CFormat(" name = {};") << fg_EscapeXcodeProjectVar(FileName);
							
							_Output += CStr::CFormat(" path = {}; sourceTree = \"<group>\";") << fg_EscapeXcodeProjectVar(PathRelativeToProj);

							if (File.m_TabWidth)
								_Output += CStr::CFormat(" tabWidth = {};") << File.m_IndentWidth;

							if (File.m_UsesTabs)
								_Output += CStr::CFormat(" usesTabs = {};") << (File.m_UsesTabs == 1 ? 1 : 0);
							
							_Output += " };\n";
						}
					)
				;
			}
		}

		// Product references
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			for (auto &NativeTarget : NativeTargets)
			{
				if (!NativeTarget.m_ProductType.f_IsEmpty())
				{
					ToPerform.f_Add
						(
							NativeTarget.f_GetProductReferenceGUID()
							, [&]
							{
								CStr Entry = ((CStr::CFormat("isa = PBXFileReference; includeInIndex = 0; path = \"{}\"; sourceTree = {}; ")
									<< NativeTarget.m_ProductPath
									<< NativeTarget.m_ProductSourceTree));

								Entry = "{" + Entry + "}";
								_Output += ((CStr::CFormat("\t\t{} /* {} */ = {};\n") << NativeTarget.f_GetProductReferenceGUID() << NativeTarget.m_ProductName << Entry));
							}
						)
					;
				}
			}
		}

		// Configuration references
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			for (auto &NativeTarget : NativeTargets)
			{
				auto pConfig = &NativeTarget.m_BuildConfiguration;
				ToPerform.f_Add
					(
						pConfig->f_GetFileRefGUID()
						, [&, pConfig]
						{
							_Output
								+= CStr::CFormat("\t\t{} /* {} */ = {{isa = PBXFileReference; explicitFileType = text.xcconfig; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; };\n")
								<< pConfig->f_GetFileRefGUID()
								<< pConfig->f_GetFileNoExt()
								<< pConfig->m_ConfigFileName
								<< CFile::fs_MakePathRelative(pConfig->m_Path, _OutputDir)
							;
						}
					)
				;
			}
		}
		
		// Dependency product references
		{
			for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
			{
				auto pDependency = &*iDependency;

				if (pDependency->m_bInternal)
					continue;

				ToPerform.f_Add
					(
						pDependency->f_GetFileRefGUID()
						, [&, pDependency]
						{
							auto &Dependency = *pDependency;

							_Output
								+= CStr::CFormat("\t\t{} /* {}.xcodeproj */ = {{isa = PBXFileReference; explicitFileType = \"wrapper.pb-project\"; path = {}; sourceTree = \"<group>\"; };\n")
								<< Dependency.f_GetFileRefGUID()
								<< Dependency.f_GetName()
								<< fg_EscapeXcodeProjectVar(Dependency.f_GetName() + ".xcodeproj")
							;
						}
					)
				;
			}
		}
		
		ToPerform.f_Perform();
		
	}

	void CGeneratorInstance::fp_GeneratePBXGroupSection(CProject &_Project, CStr& _Output) const
	{
		struct CGroupChild
		{
			CStr m_GUID;
			CStr m_Name;
		};

		TCMap<CStr, TCVector<CGroupChild>> GroupChildren;

		auto fl_OutputGroupChild = [&] (CStr const& _GUID, CStr const& _Name)
		{
			_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << _GUID << _Name);
		};

		TCSet<CStr> OutputGroups;
		auto fl_OutputGroup = [&] (CStr const& _GUID, CStr const& _Name)
		{
			if (!OutputGroups(_GUID).f_WasCreated())
				return; // Already output
			
			_Output += (CStr::CFormat("\t\t{} /* {} */") << _GUID << _Name);
			_Output += " = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n";

			auto pChildren = GroupChildren.f_FindEqual(_GUID);
			if (pChildren)
			{
				for (auto Iter = pChildren->f_GetIterator(); Iter; ++Iter)
				{
					fl_OutputGroupChild(Iter->m_GUID, Iter->m_Name);
				}
			}

			_Output += "\t\t\t);\n";
			_Output += CStr::CFormat("\t\t\tname = {};\n") << fg_EscapeXcodeProjectVar(_Name);
			_Output += "\t\t\tsourceTree = \"<group>\";\n\t\t};\n";
		};
		
		

		for (auto IterFile = _Project.m_Files.f_GetIterator(); IterFile; ++IterFile)
		{
			if (IterFile->m_pGroup)
			{
				auto & Children = GroupChildren[IterFile->m_pGroup->f_GetGUID()];
				
				CGroupChild& Child = Children.f_Insert();
				Child.m_GUID = IterFile->f_GetFileRefGUID();
				Child.m_Name = CFile::fs_GetFile(IterFile->f_GetName());
			}
		}
		
		
		TCSortedPerform<CStr const &> ToPerform;
		
		TCSet<CStr> AddedGroups;
		
		for (auto IterGroup = _Project.m_Groups.f_GetIterator(); IterGroup; ++IterGroup)
		{		
			if (IterGroup->m_pParent)
			{
				auto & Children = GroupChildren[IterGroup->m_pParent->f_GetGUID()];
				
				CGroupChild& Child = Children.f_Insert();
				Child.m_GUID = IterGroup->f_GetGUID();
				Child.m_Name = CFile::fs_GetFile(IterGroup->m_Name);
			}
			
			auto pGroup = &*IterGroup;

			ToPerform.f_Add
				(
					pGroup->f_GetGUID()
					, [&, pGroup]
					{
						fl_OutputGroup(pGroup->f_GetGUID(), pGroup->m_Name);
					}
				)
			;
		}

		// Product reference group
		{
			TCVector<CGroupChild> &lProducts = GroupChildren[_Project.f_GetProductRefGroupGUID()];

			for (auto &NativeTargets : _Project.m_NativeTargets)
			{
				for (auto &NativeTarget : NativeTargets)
				{
					if (!NativeTarget.m_ProductType.f_IsEmpty())
					{
						CGroupChild& Child = lProducts.f_Insert();
						Child.m_GUID = NativeTarget.f_GetProductReferenceGUID();
						Child.m_Name = NativeTarget.m_ProductName;
					}
				}
			}
			ToPerform.f_Add
				(
					_Project.f_GetProductRefGroupGUID()
					, [&]
					{
						fl_OutputGroup(_Project.f_GetProductRefGroupGUID(), g_ReservedProductRefGroup);
					}
				)
			;
		}

		// Configurations group
		{
			TCVector<CGroupChild> &lConfigurations = GroupChildren[_Project.f_GetConfigurationsGroupGUID()];

			for (auto &NativeTargets : _Project.m_NativeTargets)
			{
				for (auto &NativeTarget : NativeTargets)
				{
					CGroupChild& Child = lConfigurations.f_Insert();
					Child.m_GUID = NativeTarget.m_BuildConfiguration.f_GetFileRefGUID();
					Child.m_Name = CFile::fs_GetFileNoExt(NativeTarget.m_BuildConfiguration.f_GetFile());
				}
			}
			ToPerform.f_Add
				(
					_Project.f_GetConfigurationsGroupGUID()
					, [&]
					{
						fl_OutputGroup(_Project.f_GetConfigurationsGroupGUID(), g_ReservedConfigurationsGroup);
					}
				)
			;
		}

		// Project Dependencies group
		{
			TCVector<CGroupChild> & lProjectDependencies = GroupChildren[_Project.f_GetProjectDependenciesGroupGUID()];
			for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
			{
				if (iDependency->m_bInternal)
					continue;

				CGroupChild& Child = lProjectDependencies.f_Insert();
				Child.m_GUID = iDependency->f_GetFileRefGUID();
				Child.m_Name = iDependency->f_GetName() + ".xcodeproj";
			}

			ToPerform.f_Add
				(
					_Project.f_GetProjectDependenciesGroupGUID()
					, [&]
					{
						fl_OutputGroup(_Project.f_GetProjectDependenciesGroupGUID(), g_ReservedProjectDependenciesGroup);
					}
				)
			;
		}

		// Generator group
		{

			TCVector<CGroupChild> & lChildren = GroupChildren[_Project.f_GetGeneratorGroupGUID()];
			{
				auto &Group = lChildren.f_Insert();
				Group.m_GUID = _Project.f_GetProductRefGroupGUID();
				Group.m_Name = g_ReservedProductRefGroup;
			}
			{
				auto &Group = lChildren.f_Insert();
				Group.m_GUID = _Project.f_GetConfigurationsGroupGUID();
				Group.m_Name = g_ReservedConfigurationsGroup;
			}
			{
				auto &Group = lChildren.f_Insert();
				Group.m_GUID = _Project.f_GetProjectDependenciesGroupGUID();
				Group.m_Name = g_ReservedProjectDependenciesGroup;
			}

			ToPerform.f_Add
				(
					_Project.f_GetGeneratorGroupGUID()
					, [&]
					{
						fl_OutputGroup(_Project.f_GetGeneratorGroupGUID(), g_ReservedGeneratorGroup);
					}
				)
			;
		}			
		
		TCSet<CStr> OutputRootGroups;
		
		ToPerform.f_Add
			(
				_Project.f_GetMainGroupGUID()
				, [&]
				{
					// Generate main group (all unparented groups and files)
					_Output += (CStr::CFormat("\t\t{}") << _Project.f_GetMainGroupGUID());
					_Output += " = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n";

					for (auto IterGroup = _Project.m_Groups.f_GetIterator(); IterGroup; ++IterGroup)
					{
						if (IterGroup->m_pParent == nullptr)
						{
							OutputRootGroups[IterGroup->f_GetGUID()];
							fl_OutputGroupChild(IterGroup->f_GetGUID(), IterGroup->m_Name);
						}
					}
					for (auto IterFile = _Project.m_Files.f_GetIterator(); IterFile; ++IterFile)
					{
						if (IterFile->m_pGroup == nullptr)
							fl_OutputGroupChild(IterFile->f_GetFileRefGUID(), CFile::fs_GetFile(IterFile->f_GetName()));
					}
					
					if (OutputRootGroups(_Project.f_GetGeneratorGroupGUID()).f_WasCreated())
						fl_OutputGroupChild(_Project.f_GetGeneratorGroupGUID(), g_ReservedGeneratorGroup);
					//fl_OutputGroupChild(_Project.f_GetConfigurationsGroupGUID(), g_ReservedConfigurationsGroup);
					//fl_OutputGroupChild(_Project.f_GetProjectDependenciesGroupGUID(), g_ReservedProjectDependenciesGroup);

					_Output += "\t\t\t);\n\t\t\tsourceTree = \"<group>\";\n\t\t};\n";
				}
			)
		;
		
		ToPerform.f_Perform();
		
	}

	void CGeneratorInstance::fp_GeneratePBXProjectSection(CProject &_Project, CStr& _Output) const
	{
		_Output += (CStr::CFormat("\t\t{} /* Project object */ ") << _Project.f_GetGUID());
		if (m_XcodeVersion >= 7)
			_Output += CStr::CFormat("= {{\n\t\t\tisa = PBXProject;\n\t\t\tattributes = {{\n\t\t\t\tLastUpgradeCheck = {sj2,sf0}90;\n\t\t\t};\n") << m_XcodeVersion;
		else if (m_XcodeVersion == 6)
			_Output += "= {\n\t\t\tisa = PBXProject;\n\t\t\tattributes = {\n\t\t\t\tLastUpgradeCheck = 0640;\n\t\t\t};\n";
		else if (m_XcodeVersion == 5)
			_Output += "= {\n\t\t\tisa = PBXProject;\n\t\t\tattributes = {\n\t\t\t\tLastUpgradeCheck = 0510;\n\t\t\t};\n";
		else
			_Output += "= {\n\t\t\tisa = PBXProject;\n\t\t\tattributes = {\n\t\t\t\tLastUpgradeCheck = 0450;\n\t\t\t};\n";
		_Output += (CStr::CFormat("\t\t\tbuildConfigurationList = {} /* Build configuration list for PBXProject \"{}\" */;\n")
					<< _Project.f_GetBuildConfigurationListGUID()
					<< _Project.f_GetName());
		_Output += "\t\t\tcompatibilityVersion = \"Xcode 6.3\";\n";
		_Output += "\t\t\tdevelopmentRegion = English;\n";
		_Output += "\t\t\thasScannedForEncodings = 0;\n";
		_Output += "\t\t\tknownRegions = (\n";
		_Output += "\t\t\t\ten,\n";
		_Output += "\t\t\t);\n";
		_Output += (CStr::CFormat("\t\t\tmainGroup = {};\n") << _Project.f_GetMainGroupGUID());
		_Output += (CStr::CFormat("\t\t\tproductRefGroup = {} /* Product Reference */;\n") << _Project.f_GetProductRefGroupGUID());
		_Output += "\t\t\tprojectDirPath = \"\";\n\t\t\tprojectReferences = (\n";
		TCSortedPerform<CStr const&> ToPerform;
		for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
		{
			if (iDependency->m_bInternal)
				continue;

			auto *pDependency = &*iDependency;
			
			ToPerform.f_Add
			(
				pDependency->f_GetName()
				, [&, pDependency]
				{
					_Output += "\t\t\t\t{\n";
					_Output += (CStr::CFormat("\t\t\t\t\tProductGroup = {} /* Products */;\n") << pDependency->f_GetProductRefGroupGUID());
					_Output += (CStr::CFormat("\t\t\t\t\tProjectRef = {} /* {}.xcodeproj */;\n") << pDependency->f_GetFileRefGUID() << pDependency->f_GetName());
					_Output += "\t\t\t\t},\n";
				}
			);
		}
		ToPerform.f_Perform();
		_Output += "\t\t\t);\n";
		_Output += "\t\t\tprojectRoot = \"\";\n\t\t\ttargets = (\n";
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			for (auto &NativeTarget : NativeTargets)
				_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << NativeTarget.f_GetGUID() << NativeTarget.m_Name);
		}

		_Output += "\t\t\t);\n\t\t};\n";
	}

	void CGeneratorInstance::fp_GeneratePBXSourcesBuildPhaseSection(CProject &_Project, CStr& _Output) const
	{
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);

			for (auto &NativeTarget : NativeTargets)
			{
				_Output += (CStr::CFormat("\t\t{} /* Sources */") << NativeTarget.f_GetSourcesBuildPhaseGUID());
				_Output += " = {\n\t\t\tisa = PBXSourcesBuildPhase;\n";
				_Output += (CStr::CFormat("\t\t\tbuildActionMask = {};\n\t\t\tfiles = (\n") << NativeTarget.m_BuildActionMask);

				for (auto iBuildFiles = _Project.mp_OrderedBuildTypes.f_GetIterator(); iBuildFiles; ++iBuildFiles)
				{
					if (iBuildFiles.f_GetKey() == EBuildFileType_None || iBuildFiles.f_GetKey() == EBuildFileType_CInclude)
						continue;

					for (auto &FileRef : *iBuildFiles)
					{
						if (FileRef.m_Disabled.f_FindEqual(Configuration))
							continue;
						if (!FileRef.m_BuildRules.f_IsEmpty())
						{
							auto pRule = FileRef.m_BuildRules.f_FindEqual(Configuration);
							if (!pRule)
								continue;

							if (!NativeTarget.m_IncludedTypes.f_IsEmpty() && !NativeTarget.m_IncludedTypes.f_FindEqual(pRule->m_OutputType))
								continue;
							if (!NativeTarget.m_ExcludedTypes.f_IsEmpty() && NativeTarget.m_ExcludedTypes.f_FindEqual(pRule->m_OutputType))
								continue;
						}
						else
						{
							if (!NativeTarget.m_IncludedTypes.f_IsEmpty() && !NativeTarget.m_IncludedTypes.f_FindEqual(FileRef.m_Type))
								continue;
							if (!NativeTarget.m_ExcludedTypes.f_IsEmpty() && NativeTarget.m_ExcludedTypes.f_FindEqual(FileRef.m_Type))
								continue;
						}

						auto *pBuildGuid = FileRef.m_BuildGUIDs.f_FindEqual(Configuration);
						if (!pBuildGuid)
							continue;

						_Output += "\t\t\t\t{} /* {} in Sources */,\n"_f << *pBuildGuid << FileRef.m_Name;
					}
				}

				_Output += "\t\t\t);\n\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t};\n";
			}
		}
	}

	void CGeneratorInstance::fp_GeneratePBXFrameworksBuildPhaseSection(CProject &_Project, CStr& _Output) const
	{
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			for (auto &NativeTarget : NativeTargets)
			{
				_Output += (CStr::CFormat("\t\t{} /* Frameworks */") << NativeTarget.f_GetFrameworksBuildPhaseGUID());
				_Output += " = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n";
				_Output += (CStr::CFormat("\t\t\tbuildActionMask = {};\n") << NativeTarget.m_BuildActionMask);
				_Output += "\t\t\tfiles = (\n";
				_Output += "\t\t\t);\n\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t};\n";
			}
		}
	}

	void CGeneratorInstance::fp_GeneratePBXShellScriptBuildPhaseSection(CProject& _Project, CStr& _Output) const
	{
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &NativeTarget = NativeTargets.f_GetFirst();
			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);

			auto fl_OutputScript = [&] (CBuildScript& _Script)
			{
				_Output += (CStr::CFormat("\t\t{} /* {} */ = ") << _Script.f_GetGUID(Configuration) << _Script.m_Name);
				_Output += "{\n\t\t\tisa = PBXShellScriptBuildPhase;\n";
				_Output += (CStr::CFormat("\t\t\tbuildActionMask = {};\n\t\t\tfiles = (\n") << NativeTarget.m_BuildActionMask);
				// Output files used in script here
				_Output += "\t\t\t);\n\t\t\tinputPaths = (\n";

				for (CStr const &Input : _Script.m_Inputs)
					_Output += (CStr::CFormat("\t\t\t\t\"{}\",\n") << Input);

				_Output += CStr::CFormat("\t\t\t);\n\t\t\tname = {};\n\t\t\toutputPaths = (\n") << fg_EscapeXcodeProjectVar(_Script.m_Name);

				for (CStr const &Output : _Script.m_Outputs)
					_Output += (CStr::CFormat("\t\t\t\t\"{}\",\n") << Output);

				_Output += "\t\t\t);\n\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t\tshellPath = /bin/bash;\n";
				_Output += (CStr::CFormat("\t\t\tshellScript = \"{}\";\n") << _Script.f_GetScriptSetting());
				_Output += "\t\t\tshowEnvVarsInLog = 0;\n";

				_Output += "\t\t};\n";
			};

			for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
			{
				fl_OutputScript(*iScript);
			}
		}
	}

	void CGeneratorInstance::fp_GenerateXCConfigurationList(CProject &_Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;
		
		auto fl_GenerateBuildConfigurationList = [&] (CStr const& _GUID, CStr const& _Name, CStr const& _Description, TCVector<CBuildConfiguration> const &_ConfigList)
		{
			_Output += (CStr::CFormat("\t\t{} /* Build configuration list for {} \"{}\" */") << _GUID << _Description << _Name);
			_Output += " = {\n\t\t\tisa = XCConfigurationList;\n\t\t\tbuildConfigurations = (\n";

			for (auto Iter = _ConfigList.f_GetIterator(); Iter; ++Iter)
			{
				_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << Iter->f_GetGUID() << Iter->m_ConfigName);
			}
			_Output += "\t\t\t);\n";	
			_Output += "\t\t\tdefaultConfigurationIsVisible = 0;\n";	
			_Output += CStr::CFormat("\t\t\tdefaultConfigurationName = \"{}\";\n") << _ConfigList.f_GetFirst().m_ConfigName;
			_Output += "\t\t};\n";	
		};

		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			for (auto &NativeTarget : NativeTargets)
			{
				ToPerform.f_Add
					(
						NativeTarget.f_GetBuildConfigurationListGUID()
						, [&, pNativeTarget = &NativeTarget]
						{
							auto &NativeTarget = *pNativeTarget;
							fl_GenerateBuildConfigurationList(NativeTarget.f_GetBuildConfigurationListGUID(), NativeTarget.m_Name, "PBXNativeTarget", {NativeTarget.m_BuildConfiguration});
						}
					)
				;
			}
		}
		ToPerform.f_Add
			(
				_Project.f_GetBuildConfigurationListGUID()
				, [&]
				{
					fl_GenerateBuildConfigurationList(_Project.f_GetBuildConfigurationListGUID(), _Project.f_GetName(), "PBXProject", _Project.m_BuildConfigurationList);
				}
			)
		;
		
		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GenerateXCBuildConfigurationSection(CProject &_Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;
		
		auto fl_GenerateBuildConfigurationList = [&] (CBuildConfiguration const &_Config, bool _bUseReference)
		{
			ToPerform.f_Add
				(
					_Config.f_GetGUID()
					, [&_Output, pConfig = &_Config, _bUseReference]
					{
						auto &Config = *pConfig;
						_Output += (CStr::CFormat("\t\t{} /* {} */") << Config.f_GetGUID() << Config.m_ConfigName);
						_Output += " = {\n\t\t\tisa = XCBuildConfiguration;\n";
						if (_bUseReference)
							_Output += (CStr::CFormat("\t\t\tbaseConfigurationReference = {} /* {} */;\n") << Config.f_GetFileRefGUID() << CFile::fs_GetFileNoExt(Config.f_GetFile()));
						_Output += "\t\t\tbuildSettings = {\n\t\t\t};\n";
						_Output += (CStr::CFormat("\t\t\tname = \"{}\";\n") << Config.m_ConfigName);
						_Output += "\t\t};\n";
					}
				)
			;
		};
		
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			for (auto &NativeTarget : NativeTargets)
				fl_GenerateBuildConfigurationList(NativeTarget.m_BuildConfiguration, true);
		}
		for (auto &Config : _Project.m_BuildConfigurationList)
			fl_GenerateBuildConfigurationList(Config, false);

		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXContainerItemProxySection(CProject& _Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;
		
		for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
		{
			auto &Dependency = *iDependency;
			auto *pDependency = &Dependency;
			for (auto &PerConfig : pDependency->m_PerConfig)
			{
				ToPerform.f_Add
					(
						PerConfig.f_GetContainerItemGUID(Dependency)
						, [&, pDependency, pPerConfig = &PerConfig]
						{
							auto &Dependency = *pDependency;
							auto &PerConfig = *pPerConfig;

							CStr ContainerGUID;
							if (Dependency.m_bInternal)
								ContainerGUID = _Project.f_GetGUID();
							else
								ContainerGUID = Dependency.f_GetFileRefGUID();

							_Output += "\t\t{} /* PBXContainerItemProxy */ = "_f << PerConfig.f_GetContainerItemGUID(Dependency);
							_Output += "{\n\t\t\tisa = PBXContainerItemProxy;\n";
							_Output += "\t\t\tcontainerPortal = {} /* {}.xcodeproj */;\n"_f << ContainerGUID << Dependency.f_GetName();

							CNativeTarget TargetDummy;
							TargetDummy.m_Name = PerConfig.f_GetName(Dependency, "");
							_Output += "\t\t\tproxyType = 1;\n\t\t\tremoteGlobalIDString = {};\n\t\t\tremoteInfo = \"{}\";\n\t\t};\n"_f
								<< TargetDummy.f_GetGUID()
								<< TargetDummy.m_Name
							;
						}
					)
				;
			}
		}
		
		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXTargetDependencySection(CProject& _Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;
		
		for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
		{
			auto &Dependency = *iDependency;
			auto *pDependency = &Dependency;
			for (auto &PerConfig : pDependency->m_PerConfig)
			{
				ToPerform.f_Add
					(
						PerConfig.f_GetTargetGUID(Dependency)
						, [&, pDependency, pPerConfig = &PerConfig]
						{
							auto &Dependency = *pDependency;
							auto &PerConfig = *pPerConfig;

							_Output += "\t\t{} /* PBXTargetDependency */ = "_f << PerConfig.f_GetTargetGUID(Dependency);
							_Output += "{\n\t\t\tisa = PBXTargetDependency;\n";
							_Output += "\t\t\tname = \"{}\";\n\t\t\ttargetProxy = {} /* PBXContainerItemProxy */;\n"_f
								<< PerConfig.f_GetName(Dependency, "")
								<< PerConfig.f_GetContainerItemGUID(Dependency)
							;
							_Output += "\t\t};\n";
						}
					)
				;
			}
		}
		
		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXLegacyTargetSection(CProject &_Project, CStr& _Output) const
	{
		bool bBegin = false;
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &NativeTarget = NativeTargets.f_GetFirst();

			if (!NativeTarget.m_ProductType.f_IsEmpty())
				continue;

			if (!bBegin)
			{
				bBegin = true;
				_Output += "/* Begin PBXLegacyTarget section */\n\n";
			}

			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);

			CStr RunScript = "ExternalBuildToolRun_{}.sh"_f << Configuration.f_GetFullSafeName();

			_Output += (CStr::CFormat("\t\t{} /* {} */ = ") << NativeTarget.f_GetGUID() << NativeTarget.m_Name);
			_Output += "{\n\t\t\tisa = PBXLegacyTarget;\n";
			_Output += (CStr::CFormat("\t\t\tbuildArgumentsString = \"{}\";\n") << (NativeTarget.m_bGeneratedBuildScript ? RunScript : "-c 'exit 0'"));
			_Output += (CStr::CFormat("\t\t\tbuildConfigurationList = {} /* Build configuration list for PBXLegacyTarget \"{}\" */;\n") << NativeTarget.f_GetBuildConfigurationListGUID() << NativeTarget.m_Name);
			_Output += (CStr::CFormat("\t\t\tbuildPhases = ();\n\t\t\tbuildToolPath = \"{}\";\n") << CStr("/bin/bash"));
			_Output += "\t\t\tbuildWorkingDirectory = \"${PROJECT_DIR}/";
			_Output += (CStr::CFormat("{}\";\n") << CFile::fs_MakeNiceFilename(_Project.f_GetName()));
			// Dependencies
			_Output += "\n\t\t\tdependencies = (\n";
			for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
			{
				if (!NativeTarget.m_bDefaultTarget && iDependency->m_bInternal)
					continue;
				auto pPerConfig = iDependency->m_PerConfig.f_FindEqual(Configuration);
				if (!pPerConfig)
					continue;
				_Output += (CStr::CFormat("\t\t\t\t{} /* PBXTargetDependency */,\n") << pPerConfig->f_GetTargetGUID(*iDependency));
			}
			_Output += (CStr::CFormat("\t\t\t);\n\t\t\tname = \"{}\";\n\t\t\tpassBuildSettingsInEnvironment = 1;\n\t\t\tproductName = {};\n") << NativeTarget.m_Name << NativeTarget.m_ProductName);
			_Output += "\t\t};\n";
		}
		if (bBegin)
			_Output += "/* End PBXLegacyTarget section */\n\n";
	}

	void CGeneratorInstance::fp_GeneratePBXNativeTargetSection(CProject &_Project, CStr& _Output) const
	{
		auto &ThreadLocal = *m_ThreadLocal;

		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);

			for (auto &NativeTarget : NativeTargets)
			{
				if (NativeTarget.m_ProductType.f_IsEmpty())
					continue;

				_Output += (CStr::CFormat("\t\t{} /* {} */") << NativeTarget.f_GetGUID() << NativeTarget.m_Name);
				_Output += " = {\n";
				_Output += (CStr::CFormat("\t\t\tisa = PBXNativeTarget;\n\t\t\tbuildConfigurationList = {} /* Build configuration list for PBXNativeTarget \"{}\" */;\n")
					<< NativeTarget.f_GetBuildConfigurationListGUID()
					<< NativeTarget.m_Name);
				_Output += "\t\t\tbuildPhases = (\n";

				// Pre build scripts
				for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
				{
					if (!iScript->m_bPostBuild)
						_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID(Configuration) << iScript->m_Name);
				}

				// Sources, Frameworks, Headers
				_Output += CStr::CFormat("\t\t\t\t{} /* Sources */,\n")
					<< NativeTarget.f_GetSourcesBuildPhaseGUID();
				_Output += CStr::CFormat("\t\t\t\t{} /* Frameworks */,\n")
					<< NativeTarget.f_GetFrameworksBuildPhaseGUID();
	#if 0
				_Output += CStr::CFormat("\t\t\t\t{} /* Headers */,\n")
					<< NativeTarget.f_GetHeadersBuildPhaseGUID();
	#endif

				// Post build scripts
				for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
				{
					if (iScript->m_bPostBuild)
						_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID(Configuration) << iScript->m_Name);
				}

				_Output += "\t\t\t);\n";

				// Build rules
				_Output += "\t\t\tbuildRules = (\n";
				auto pBuildRule = ThreadLocal.mp_BuildRules.f_FindEqual(Configuration);
				if (pBuildRule)
				{
					for (CStr const &OutputType : *pBuildRule)
					{
						auto &BuildRule = pBuildRule->fs_GetKey(OutputType);

						if (!NativeTarget.m_IncludedTypes.f_IsEmpty() && !NativeTarget.m_IncludedTypes.f_FindEqual(OutputType))
							continue;
						if (!NativeTarget.m_ExcludedTypes.f_IsEmpty() && NativeTarget.m_ExcludedTypes.f_FindEqual(OutputType))
							continue;

						_Output += CStr::CFormat("\t\t\t\t{} /* PBXBuildRule */,\n") << BuildRule;
					}
				}
				_Output += "\t\t\t);\n";

				// Dependencies
				_Output += "\t\t\tdependencies = (\n";
				for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
				{
					if (!NativeTarget.m_bDefaultTarget && iDependency->m_bInternal)
						continue;

					auto pPerConfig = iDependency->m_PerConfig.f_FindEqual(Configuration);
					if (!pPerConfig)
						continue;
					_Output += (CStr::CFormat("\t\t\t\t{} /* PBXTargetDependency */,\n") << pPerConfig->f_GetTargetGUID(*iDependency));
				}
				_Output += "\t\t\t);\n";
				_Output += (CStr::CFormat("\t\t\tname = {};\n\t\t\tproductName = {};\n\t\t\tproductReference = {} /* {} */;\n\t\t\tproductType = \"{}\";\n\t\t};\n")
					<< fg_EscapeXcodeProjectVar(NativeTarget.m_Name)
					<< fg_EscapeXcodeProjectVar(NativeTarget.m_ProductName)
					<< NativeTarget.f_GetProductReferenceGUID()
					<< NativeTarget.m_ProductName
					<< NativeTarget.m_ProductType);
			}
		}
	}
	
	void CGeneratorInstance::fspr_MergeScheme(CXMLNode const* _pExistingNode, CXMLNode const* _pPrevNode, CXMLNode* _pNewNode)
	{
		// Merge attributes.
		TCSet<CXMLNode *> AlreadyMerged;
		if (auto *pElement = _pExistingNode->ToElement()) // All nodes are guaranteed to be the same here
		{
			// Find or create the corresponding element at this level
			CXMLElement const *pExistingElement = _pExistingNode->ToElement();
			CXMLElement const *pPrevElement = _pPrevNode->ToElement();
			CXMLElement *pNewElement = _pNewNode->ToElement();

			TCSet<CStr> ExistingAttributes;
			TCSet<CStr> PrevAttributes;
			TCSet<CStr> NewAttributes;
			
			for (CXMLAttribute const *pAttribute = pExistingElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->Next())
				ExistingAttributes[pAttribute->Name()];

			for (CXMLAttribute const *pAttribute = pPrevElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->Next())
				PrevAttributes[pAttribute->Name()];

			// Find change attributes
			for (CXMLAttribute const *pAttribute = pNewElement->FirstAttribute(); pAttribute; )
			{
				auto *pThisAttribute = const_cast<CXMLAttribute *>(pAttribute);
				pAttribute = pAttribute->Next();
				
				auto Name = pThisAttribute->Name();
				
				if (!ExistingAttributes.f_Exists(Name) && PrevAttributes.f_Exists(Name))
				{
					// Removed by user
					pNewElement->DeleteAttribute(Name);
					continue;
				}
				else if (ExistingAttributes.f_Exists(Name) && PrevAttributes.f_Exists(Name)) 
					// This purposefully allows the config to override user setting if config added a new attribute that the user manually added previously
				{
					// Possibly changed by user
					CStr ExistingValue;
					if (auto pAttribute = pExistingElement->Attribute(Name))
						ExistingValue = pAttribute; 
					CStr PrevValue;
					if (auto pAttribute = pPrevElement->Attribute(Name))
						PrevValue = pAttribute;
					if (ExistingValue != PrevValue)
					{
						// User changed value, override
						pThisAttribute->SetAttribute(ExistingValue.f_GetStr());
					}
				}
				NewAttributes[Name];
			}
			
			// Find user added attributes
			for (CXMLAttribute const *pAttribute = pExistingElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->Next())
			{
				auto Name = pAttribute->Name();
				
				if (!NewAttributes.f_Exists(Name) && !PrevAttributes.f_Exists(Name))
				{
					// User added attribute, copy over
					CStr ExistingValue;
					if (auto pAttribute = pExistingElement->Attribute(Name))
						ExistingValue = pAttribute;
					pNewElement->SetAttribute(Name, ExistingValue);
				}
			}

			//DConOut("pNewElement->GetText(): {}\n", pNewElement->Value().c_str());
			//XMLFile.f_SetAttribute(pOverrideElement, CStr(pAttribute->Name().c_str()), CStr(pAttribute->Value().c_str()));
			if (fg_StrCmp(pNewElement->Value(), "CommandLineArguments") == 0)
			{

				TCMap<CStr, CXMLNode const *> ExistingArgs;
				TCMap<CStr, CXMLNode const *> PrevArgs;
				TCMap<CStr, CXMLNode *> NewArgs;
				
				for (CXMLDocument::CConstNodeIterator iExistingChild(_pExistingNode); iExistingChild; ++iExistingChild)
				{
					auto *pNode = iExistingChild->ToElement();
					if (!pNode)
						continue;
					
					CStr Argument = CXMLDocument::f_GetAttribute(pNode, "argument");
					
					if (Argument.f_IsEmpty())
						continue;
					ExistingArgs[Argument] = pNode;
				}
				for (CXMLDocument::CNodeIterator iNewChild(_pNewNode); iNewChild; ++iNewChild)
				{
					auto *pNode = iNewChild->ToElement();
					if (!pNode)
						continue;
					
					CStr Argument = CXMLDocument::f_GetAttribute(pNode, "argument");
					
					if (Argument.f_IsEmpty())
						continue;
					NewArgs[Argument] = pNode;
				}
				for (CXMLDocument::CConstNodeIterator iPrevChild(_pPrevNode); iPrevChild; ++iPrevChild)
				{
					auto *pNode = iPrevChild->ToElement();
					if (!pNode)
						continue;
					
					CStr Argument = CXMLDocument::f_GetAttribute(pNode, "argument");
					
					if (Argument.f_IsEmpty())
						continue;
					PrevArgs[Argument] = pNode;
				}
				
				for (auto iArg = NewArgs.f_GetIterator(); iArg; ++iArg)
				{
					auto *pPrevArg = PrevArgs.f_FindEqual(iArg.f_GetKey());
					auto *pExistingArg = ExistingArgs.f_FindEqual(iArg.f_GetKey());
					
					if (pPrevArg && pExistingArg)
						fspr_MergeScheme(*pExistingArg, *pPrevArg, *iArg); // All the same, merge the contents
					else if (pPrevArg && !pExistingArg)
					{
						// Removed by user
						pNewElement->DeleteChild(*iArg);
					}
				}
				for (auto iArg = ExistingArgs.f_GetIterator(); iArg; ++iArg)
				{
					if (!NewArgs.f_FindEqual(iArg.f_GetKey()) && !PrevArgs.f_FindEqual(iArg.f_GetKey()))
					{
						// Added by user
						pNewElement->InsertEndChild(CXMLDocument::f_DeepClone(*iArg, pNewElement->GetDocument()));
					}
				}
				
				return;
			}
		}
				 
		CXMLDocument::CConstNodeIterator iExistingChild(_pExistingNode);
		CXMLDocument::CConstNodeIterator iPrevChild(_pPrevNode);
		CXMLDocument::CNodeIterator iNewChild(_pNewNode);
		
		
		while (iExistingChild || iPrevChild || iNewChild)
		{
			CXMLNode const* pExistingChild = iExistingChild;
			CXMLNode const* pPrevChild = iPrevChild;
			CXMLNode * pNewChild = iNewChild;

			++iExistingChild;
			++iPrevChild;
			++iNewChild;
			
			if (pExistingChild && !pNewChild)
			{
				if (pPrevChild)
				{
					// This node was deleted in config, lets leave it delete it here as well
				}
				else
				{
					// This node was added by user, lets add it back
					_pNewNode->InsertEndChild(CXMLDocument::f_DeepClone(pExistingChild, _pNewNode->GetDocument()));
				}
			}
			else if (!pExistingChild && pNewChild)
			{
				if (!pPrevChild)
				{
					// This node was added in config, leave as is
				}
				else
				{
					// This node was removed by user, lets remove it here as well
					_pNewNode->DeleteChild(pNewChild);
				}
			}
			else
			{
				if (
						pExistingChild 
						&& pPrevChild 
						&& pNewChild
						&& CXMLDocument::f_GetNodeType(pExistingChild) == CXMLDocument::f_GetNodeType(pPrevChild)
						&& CXMLDocument::f_GetNodeType(pExistingChild) == CXMLDocument::f_GetNodeType(pNewChild)
						&& pExistingChild->Value() == pPrevChild->Value()
						&& pExistingChild->Value() == pNewChild->Value()
					)
					fspr_MergeScheme(pExistingChild, pPrevChild, pNewChild); // All the same, merge the contents
				else
				{
				}
			}
		}
	}
	
	bool CGeneratorInstance::fp_GenerateSchemes(CProject& _Project, TCMap<CConfiguration, TCSet<CStr>> &_Runnables, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable) const
	{
		bool bSchemesChanged = false;
		auto & ThreadLocal = *m_ThreadLocal;

		CStr OutputDir = CFile::fs_AppendPath(ThreadLocal.m_ProjectOutputDir, CStr("xcshareddata"));
		ThreadLocal.f_CreateDirectory(OutputDir);

		OutputDir = CFile::fs_AppendPath(OutputDir, CStr("xcschemes"));
		ThreadLocal.f_CreateDirectory(OutputDir);

		for (auto Iter = _Project.m_EnabledProjectConfigs.f_GetIterator(); Iter; ++Iter)
		{
			CConfiguration const& Configuration = Iter.f_GetKey();

			auto *pNativeTarget = _Project.m_NativeTargets.f_FindEqual(Configuration);
			if (!pNativeTarget)
				continue;

			auto &NativeTarget = pNativeTarget->f_GetFirst();

			CElement const& Element = ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element["GenerateScheme"];
			if (Element.f_GetValue() == "false")
				continue;
			
			CXMLDocument XMLFile(false);
			auto pOldFile = ThreadLocal.m_pXMLFile;
			ThreadLocal.m_pXMLFile = &XMLFile;
			auto Cleanup = g_OnScopeExit > [&]
				{
					ThreadLocal.m_pXMLFile = pOldFile;
				}
			;

			CStr Name = (CStr::CFormat("{} {}") << Configuration.m_Platform << Configuration.m_Configuration);
			
			CStr SchemeName = CFile::fs_MakeNiceFilename((CStr::CFormat("{} {}") << _Project.f_GetName() << Name).f_GetStr());

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

			auto fl_GenerateBuildReference = [&] (CXMLElement* _pParent) -> CStr
			{
				auto pBuildReference = XMLFile.f_CreateElement(_pParent, "BuildableReference");
				XMLFile.f_SetAttribute(pBuildReference, "BuildableIdentifier", "primary");

				XMLFile.f_SetAttribute(pBuildReference, "BlueprintIdentifier", NativeTarget.f_GetGUID());
				
				CStr BuildableName;
				CStr Extension = ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element["EXECUTABLE_EXTENSION"].f_GetValue();
				if (Extension.f_IsEmpty())
					BuildableName = ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element["PRODUCT_NAME"].f_GetValue();
				else
					BuildableName = (CStr::CFormat("{}.{}")	<< ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element["PRODUCT_NAME"].f_GetValue() << Extension);
				
				XMLFile.f_SetAttribute(pBuildReference, "BuildableName", BuildableName);
				XMLFile.f_SetAttribute(pBuildReference, "BlueprintName", _Project.f_GetName());
				XMLFile.f_SetAttribute(pBuildReference, "ReferencedContainer", (CStr::CFormat("container:{}.xcodeproj") << _Project.f_GetName()).f_GetStr());
				return BuildableName;
			};
			
			auto fl_GenerateRunnablePath = [&] (CXMLElement* _pParent) -> bool
			{
				CElement const& Element = ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element["LocalDebuggerCommand"];
				if (Element.f_GetValue().f_IsEmpty())
					return false;
				auto pPathRunnable = XMLFile.f_CreateElement(_pParent, "PathRunnable");
				XMLFile.f_SetAttribute(pPathRunnable, "FilePath", Element.f_GetValue());
				return true;
			};

			// BuildAction
			{
				auto pBuildAction = XMLFile.f_CreateElement(pScheme, "BuildAction");
				XMLFile.f_SetAttribute(pBuildAction, "parallelizeBuildables", "YES");
				XMLFile.f_SetAttribute(pBuildAction, "buildImplicitDependencies", "YES");

				auto pBuildActionEntries = XMLFile.f_CreateElement(pBuildAction, "BuildActionEntries");

				auto pBuildActionEntry = XMLFile.f_CreateElement(pBuildActionEntries, "BuildActionEntry");
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForTesting", "YES");
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForRunning", "YES");
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForProfiling", "YES");
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForArchiving", "YES");
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForAnalyzing", "YES");

				_Buildable[Configuration][_Project.f_GetGUID()] = fl_GenerateBuildReference(pBuildActionEntry);
			}

			// TestAction
			{
				auto pTestAction = XMLFile.f_CreateElement(pScheme, "TestAction");
				XMLFile.f_SetAttribute(pTestAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pTestAction, "selectedDebuggerIdentifier", "Xcode.DebuggerFoundation.Debugger.LLDB");
				XMLFile.f_SetAttribute(pTestAction, "selectedLauncherIdentifier", "Xcode.DebuggerFoundation.Launcher.LLDB");
				XMLFile.f_SetAttribute(pTestAction, "shouldUseLaunchSchemeArgsEnv", "YES");
				XMLFile.f_CreateElement(pTestAction, "Testables");

				// Buildable
				if (NativeTarget.m_Type == "Application")
				{
					auto pMacroExpansion = XMLFile.f_CreateElement(pTestAction, "MacroExpansion");
					fl_GenerateBuildReference(pMacroExpansion);
				}
			}

			// LaunchAction
			{
				auto pLaunchAction = XMLFile.f_CreateElement(pScheme, "LaunchAction");
				XMLFile.f_SetAttribute(pLaunchAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pLaunchAction, "selectedDebuggerIdentifier", "Xcode.DebuggerFoundation.Debugger.LLDB");
				XMLFile.f_SetAttribute(pLaunchAction, "selectedLauncherIdentifier", "Xcode.DebuggerFoundation.Launcher.LLDB");
				XMLFile.f_SetAttribute(pLaunchAction, "launchStyle", "0");

				// Working directory
				bint bUsingCustomWorkingDirectory = false;
				if (fl_GenerateRunnablePath(pLaunchAction))
				{
					_Runnables[Configuration][SchemeName];
					CElement const& Element = ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element["customWorkingDirectory"];
					if (!Element.f_GetValue().f_IsEmpty())
					{
						bUsingCustomWorkingDirectory = true;
						XMLFile.f_SetAttribute(pLaunchAction, "useCustomWorkingDirectory", "YES");
						XMLFile.f_SetAttribute(pLaunchAction, Element.m_Property, Element.f_GetValue());
					}
				}
				
				if (!bUsingCustomWorkingDirectory)
					XMLFile.f_SetAttribute(pLaunchAction, "useCustomWorkingDirectory", "NO");
				XMLFile.f_SetAttribute(pLaunchAction, "ignoresPersistentStateOnLaunch", "NO");
				XMLFile.f_SetAttribute(pLaunchAction, "debugDocumentVersioning", "YES");
				XMLFile.f_SetAttribute(pLaunchAction, "debugServiceExtension", "internal");
				XMLFile.f_SetAttribute(pLaunchAction, "allowLocationSimulation", "YES");

				// Command line options
				if (ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element.f_Exists("CommandLineArgument"))
				{
					CElement const& Element = ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element["CommandLineArgument"];
					if (!Element.f_GetValue().f_IsEmpty())
					{
						auto pCommandLineArguments = XMLFile.f_CreateElement(pLaunchAction, "CommandLineArguments");
						auto pCommandLineArgument = XMLFile.f_CreateElement(pCommandLineArguments, Element.m_Property);
						XMLFile.f_SetAttribute(pCommandLineArgument, "argument", Element.f_GetValue());
						XMLFile.f_SetAttribute(pCommandLineArgument, "isEnabled", "YES");
					}
				}
				
				 XMLFile.f_CreateElement(pLaunchAction, "AdditionalOptions");
			}

			// ProfileAction
			{
				auto pProfileAction = XMLFile.f_CreateElement(pScheme, "ProfileAction");
				XMLFile.f_SetAttribute(pProfileAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pProfileAction, "shouldUseLaunchSchemeArgsEnv", "YES");
				XMLFile.f_SetAttribute(pProfileAction, "savedToolIdentifier", "");

				// Working directory
				bint bUsingCustomWorkingDirectory = false;
				if (fl_GenerateRunnablePath(pProfileAction))
				{
					CElement const& Element = ThreadLocal.mp_EvaluatedTargetSettings[Configuration].m_Element["customWorkingDirectory"];
					if (!Element.f_GetValue().f_IsEmpty())
					{
						bUsingCustomWorkingDirectory = true;
						XMLFile.f_SetAttribute(pProfileAction, "useCustomWorkingDirectory", "YES");
						XMLFile.f_SetAttribute(pProfileAction, Element.m_Property, Element.f_GetValue());
					}
				}

				if (!bUsingCustomWorkingDirectory)
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
			
			CStr FileName = CFile::fs_AppendPath(OutputDir,  (CStr::CFormat("{}.xcscheme") << SchemeName).f_GetStr());
			
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

			// Output the newly generated file
			{
				CStr XMLData = XMLFile.f_GetAsString(EXMLOutputDialect_Xcode);
				bool bWasCreated;
				if (!m_BuildSystem.f_AddGeneratedFile(FileName, XMLData, _Project.m_pSolution->f_GetName(), bWasCreated, true))
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
				if (!m_BuildSystem.f_AddGeneratedFile(FileName, RawXMLData, _Project.m_pSolution->f_GetName(), bWasCreated, true))
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

	void CGeneratorInstance::fp_GenerateToolRunScript(CProject& _Project, CStr const& _OutputDir) const
	{
		auto & ThreadLocal = *m_ThreadLocal;
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &NativeTarget = NativeTargets.f_GetFirst();

			if (NativeTarget.m_Type != "Makefile")
				continue;

			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);

			CStr BuildScript = "\tcase $CONFIGURATION in\n";
			CStr CleanScript = BuildScript;
			
			bint bBuildScript = false;
			bint bCleanScript = false;

			// Pre build scripts
			auto pConfig = ThreadLocal.mp_EvaluatedTargetSettings.f_FindEqual(Configuration);

			if (pConfig)
			{
				CStr Name = (CStr::CFormat("{} {}") << Configuration.m_Platform << Configuration.m_Configuration);

				CStr DependencyFile;
				if (auto pDependencyFile = pConfig->m_Element.f_FindEqual("DependencyFile"))
					DependencyFile = pDependencyFile->f_GetValue();
				
				if (auto pCommandLine_Clean = pConfig->m_Element.f_FindEqual("CommandLine_Clean"))
				{
					CleanScript += CStr::CFormat("\t\t\t\"{}\")\n") << Name;
					CleanScript += CStr::CFormat("\t\t\t\texport MalterlibDependencyFile=\"{}\"\n") << DependencyFile;
					if (NativeTarget.m_ScriptExport)
						CleanScript += CStr::CFormat("\t\t\t\t{}\n") << NativeTarget.m_ScriptExport;
					CleanScript += CStr::CFormat("\t\t\t\tbash {}\n") << pCommandLine_Clean->f_GetValue();
					CleanScript += "\t\t\t\texit $?\n";
					CleanScript += "\t\t\t;;\n";

					bCleanScript = true;
				}

				if (auto pCommandLine_Build = pConfig->m_Element.f_FindEqual("CommandLine_Build"))
				{
					BuildScript += (CStr::CFormat("\t\t\t\"{}\")\n") << Name);
					BuildScript += CStr::CFormat("\t\t\t\texport MalterlibDependencyFile=\"{}\"\n") << DependencyFile;
					if (NativeTarget.m_ScriptExport)
						BuildScript += CStr::CFormat("\t\t\t\t{}\n") << NativeTarget.m_ScriptExport;
					
					for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
					{
						if (!iScript->m_bPostBuild)
						{
							if (iScript->m_ScriptName)
							{
								BuildScript += (CStr::CFormat("\t\t\t\tbash \"{}\"\n") << iScript->m_ScriptName);
								BuildScript += "\t\t\t\tErrorLevel=$?\n";
								BuildScript += "\t\t\t\tif [ $ErrorLevel -ne 0 ] ; then\n";
								BuildScript += "\t\t\t\t\texit $ErrorLevel\n";
								BuildScript += "\t\t\t\tfi\n";
							}
						}
					}
					BuildScript += (CStr::CFormat("\t\t\t\tbash {}\n") << pCommandLine_Build->f_GetValue());
					BuildScript += "\t\t\t\tErrorLevel=$?\n";
					BuildScript += "\t\t\t\tif [ $ErrorLevel -ne 0 ] ; then\n";
					BuildScript += "\t\t\t\t\texit $ErrorLevel\n";
					BuildScript += "\t\t\t\tfi\n";
					for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
					{
						if (iScript->m_bPostBuild)
						{
							if (iScript->m_ScriptName)
							{
								BuildScript += (CStr::CFormat("\t\t\t\tbash \"{}\"\n") << iScript->m_ScriptName);
								BuildScript += "\t\t\t\tErrorLevel=$?\n";
								BuildScript += "\t\t\t\tif [ $ErrorLevel -ne 0 ] ; then\n";
								BuildScript += "\t\t\t\t\texit $ErrorLevel\n";
								BuildScript += "\t\t\t\tfi\n";
							}
						}
					}
					if (!DependencyFile.f_IsEmpty())
					{
						BuildScript += CStr::CFormat("\t\t\t\tif [ ! -f \"{}\" ]; then\n") << DependencyFile;
						BuildScript += CStr::CFormat("\t\t\t\t\tMTool TouchOrCreate \"{}\"\n") << DependencyFile;
						BuildScript += "\t\t\t\tfi\n";
					}
					BuildScript += "\t\t\t;;\n";
					bBuildScript = true;
				}
				
			}
			
			BuildScript += "\t\tesac\n";
			CleanScript += "\t\tesac\n";

			CStr ScriptData;

			if (bBuildScript || bCleanScript)
			{
				NativeTarget.m_bGeneratedBuildScript = true;
				ScriptData = "#!/bin/bash\n\n";
				ScriptData += "export PATH=$MalterlibBuildSystemExecutablePath:$PATH\n";
				
				
				ScriptData += (CStr::CFormat("case $ACTION in\n\t\"\")\n\t\t{}\n\t\t;;\n\t\"clean\")\n\t\t{}\n\t\t;;\nesac\nexit 0")
													<< BuildScript
													<< CleanScript);
			}

			if (!ScriptData.f_IsEmpty())
			{
				CStr OutputFile = CFile::fs_AppendPath(_OutputDir, CFile::fs_MakeNiceFilename(_Project.f_GetName()));
				ThreadLocal.f_CreateDirectory(OutputFile);

				OutputFile = CFile::fs_AppendPath(OutputFile, "ExternalBuildToolRun_{}.sh"_f << Configuration.f_GetFullSafeName());

				bool bWasCreated;
				if (!m_BuildSystem.f_AddGeneratedFile(OutputFile, ScriptData, _Project.m_pSolution->f_GetName(), bWasCreated, false))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << OutputFile));

				if (bWasCreated)
				{
					TCVector<uint8> FileDataVector;
					CFile::fs_WriteStringToVector(FileDataVector, CStr(ScriptData), false);
					m_BuildSystem.f_WriteFile(FileDataVector, OutputFile);
				}
			}
		}
	}
}
