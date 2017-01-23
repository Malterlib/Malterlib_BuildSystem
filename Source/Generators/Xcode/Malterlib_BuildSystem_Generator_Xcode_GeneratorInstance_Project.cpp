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
		ThreadLocal.mp_XcodeSettingsFromFiles.f_Clear();
		ThreadLocal.mp_XcodeSettingsFromFilesExcluded.f_Clear();
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

		_Project.m_NativeTarget.m_Name = _Project.f_GetName();
		
		fp_EvaluateTargetSettings(_Project);
		fp_EvaluateFiles(_Project);
		fp_EvaluateFileTypeCompileFlags(_Project);
		fp_EvaluateDependencies(_Project);
		fp_GenerateCompilerFlags(_Project);

		fp_ProcessExcludedFiles(_OutputDir);
		
		fp_GenerateBuildConfigurationFiles(_Project, _OutputDir, _Project.m_NativeTarget.m_BuildConfigurationList, false);
		fp_GenerateBuildConfigurationFiles(_Project, _OutputDir, _Project.m_BuildConfigurationList, true);
		fp_GenerateToolRunScript(_Project, _OutputDir);

		// Calculated target settings (must be the same across all configurations?)
		{
			_Project.m_NativeTarget.m_ProductName = ThreadLocal.mp_EvaluatedTargetSettings.f_GetIterator()->m_Element["Name"].f_GetValue();

			CStr Extension = ThreadLocal.mp_EvaluatedTargetSettings.f_GetIterator()->m_Element["EXECUTABLE_EXTENSION"].f_GetValue();
			if (Extension.f_IsEmpty())
				_Project.m_NativeTarget.m_ProductPath = _Project.m_NativeTarget.m_ProductName;
			else
				_Project.m_NativeTarget.m_ProductPath = _Project.m_NativeTarget.m_ProductPath = (CStr::CFormat("{}.{}")	<<  _Project.m_NativeTarget.m_ProductName << Extension);
	
			_Project.m_NativeTarget.m_ProductType = ThreadLocal.mp_EvaluatedTargetSettings.f_GetIterator()->m_Element["ProductType"].f_GetValue();
			_Project.m_NativeTarget.m_ProductSourceTree = "BUILT_PRODUCTS_DIR";
			_Project.m_NativeTarget.m_BuildActionMask = 0;
		}

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
		
#if 0
		fl_Output("/* Begin PBXHeadersBuildPhase section */");
		fp_GeneratePBXHeadersBuildPhaseSection(_Project, FileData);
		fl_Output("/* End PBXHeadersBuildPhase section */\n");
#endif

		if (_Project.m_NativeTarget.m_ProductType.f_IsEmpty())
		{
			fl_Output("/* Begin PBXLegacyTarget section */");
			fp_GeneratePBXLegacyTargetSection(_Project, FileData);
			fl_Output("/* End PBXLegacyTarget section */\n");
		}

		fl_Output("/* Begin PBXNativeTarget section */");
		fp_GeneratePBXNativeTargetSection(_Project, FileData);
		fl_Output("/* End PBXNativeTarget section */\n");

		fl_Output("/* Begin PBXProject section */");
		fp_GeneratePBXProjectSection(_Project, FileData);
		fl_Output("/* End PBXProject section */\n");
		
#ifdef DGenerateAll
		fl_Output("/* Begin PBXReferenceProxy section */");
		fp_GeneratePBXReferenceProxySection(_Project, FileData);
		fl_Output("/* End PBXReferenceProxy section */\n");
#endif
		
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

	void CGeneratorInstance::fp_CalculateDependencyProductPath(CProject& _Project, CProjectDependency& _Dependency) const
	{
		TCVector<CStr> SearchList;
		SearchList.f_Insert("SharedTarget");

		TCMap<CConfiguration, CConfigResult> ExtensionEntities;
		fp_GetConfigValue(
			_Project.m_EnabledProjectConfigs
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
			_Project.m_EnabledProjectConfigs
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
			_Project.m_EnabledProjectConfigs
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
			_Project.m_EnabledProjectConfigs
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
			_Project.m_EnabledProjectConfigs
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
			_Project.m_EnabledProjectConfigs
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

		for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
		{				
			auto &PerConfig = _Dependency.m_PerConfig[iConfig.f_GetKey()];
			if (_Dependency.m_Type != "StaticLibrary")
				PerConfig.m_bLink = false;
			
			PerConfig.m_SearchPath = OutputEntities[iConfig.f_GetKey()].m_Element["CONFIGURATION_BUILD_DIR"].f_GetValue();
			PerConfig.m_CalculatedDependencyExtension = ExtensionEntities[iConfig.f_GetKey()].m_Element["EXECUTABLE_EXTENSION"].f_GetValue();
			PerConfig.m_CalculatedDependencyName = ProductNameEntities[iConfig.f_GetKey()].m_Element["PRODUCT_NAME"].f_GetValue();

			if (EnableLinkerGroupEntities[iConfig.f_GetKey()].m_Element["MALTERLIB_ENABLE_LINKER_GROUPS"].f_GetValue() == "true")
				PerConfig.m_LinkerGroup = LinkerGroupEntities[iConfig.f_GetKey()].m_Element["MALTERLIB_LINKER_GROUP"].f_GetValue();
			
			if (PerConfig.m_CalculatedDependencyExtension.f_IsEmpty())
				PerConfig.m_CalculatedPath = PerConfig.m_CalculatedDependencyName;
			else
				PerConfig.m_CalculatedPath = (CStr::CFormat("{}.{}") << PerConfig.m_CalculatedDependencyName << PerConfig.m_CalculatedDependencyExtension);
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
			auto CustomCommandLine = fp_GetSingleConfigValue(IterFile->m_EnabledConfigs, EPropertyType_Compile, "Custom_CommandLine");
			
			CStr Type = Value.m_Value;
			if (!CustomCommandLine.m_Value.f_IsEmpty())
				Type = "Custom";

			if (Type.f_IsEmpty())
				m_BuildSystem.fs_ThrowError(Value.m_Position, "No compile type found");

			IterFile->m_Type = Type;
			
			IterFile->m_bWasGenerated = true;

			TCVector<CStr> SearchList;
			SearchList.f_Insert("Root");

			TCMap<CConfiguration, CConfigResult> Entities;

			fp_GetConfigValue(
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
				, Entities);

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
				EBuildFileTypes FileType = ENone;

				if (IterFile->m_Type == "Custom")
					FileType = ECustom;
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
						FileType = ECCompile_InitEarly;
					else
						FileType = ECCompile;
				}
				else if (IterFile->m_Type == "Header")
					FileType = ECInclude;
				else if (IterFile->m_Type == "QtMoc")
					FileType = EQTMoc;
				else if (IterFile->m_Type == "QtUic")
					FileType = EQTUic;
				else if (IterFile->m_Type == "QtRcc")
					FileType = EQTRcc;
				else if (IterFile->m_Type == "MlTwk")
					FileType = EMlTwk;
				else if (IterFile->m_Type == "MalterlibFS")
					FileType = EMalterlibFS;

				if (IterFile->m_Type == "QtMoc" &&
					CFile::fs_GetExtension(IterFile->f_GetName()) == "cpp")
					IterFile->m_LastKnownFileType = "sourcecode.c.cppmoc";

				bEvaluateCompileFlags = FileType != ENone;

				CBuildFileRef& BuildRef = _Project.mp_OrderedBuildTypes[FileType].f_Insert();
				BuildRef.m_FileName = IterFile->f_GetName();
				BuildRef.m_Name = CFile::fs_GetFile(IterFile->f_GetName());
				BuildRef.m_FileRefGUID = IterFile->f_GetFileRefGUID();
				BuildRef.m_BuildGUID = IterFile->f_GetBuildRefGUID();
				BuildRef.m_CompileFlagsGUID = IterFile->f_GetCompileFlagsGUID();
				BuildRef.m_Type = IterFile->m_Type;
				BuildRef.m_bHasCompilerFlags = (FileType == ECCompile || FileType == ECCompile_InitEarly || FileType == EQTMoc || FileType == EMalterlibFS);
				
				if (FileType == ECustom)
				{
					BuildRef.m_CustomCommandLine = CustomCommandLine.m_Value;
					BuildRef.m_CustomWorkingDirectory = fp_GetSingleConfigValue(IterFile->m_EnabledConfigs, EPropertyType_Compile, fg_Format("Custom_WorkingDirectory")).m_Value;
					CStr Output = fp_GetSingleConfigValue(IterFile->m_EnabledConfigs, EPropertyType_Compile, "Custom_Outputs").m_Value;
					while (!Output.f_IsEmpty())
						BuildRef.m_CustomOutputs.f_Insert(fg_GetStrSep(Output, ";"));
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
					ThreadLocal.mp_EvaluatedCompileFlags[IterFile->f_GetBuildRefGUID()] = fg_Move(CompileEntities);	
				}
				ThreadLocal.mp_EvaluatedTypesInUse[IterFile->m_Type];
			}
		}
	}

	void CGeneratorInstance::fp_GeneratePBXBuildRule(CProject &_Project, CStr &o_Output) const
	{
		TCSortedPerform<CStr> ToPerform;
		
		auto &ThreadLocal = *m_ThreadLocal;
		if (!_Project.mp_OrderedBuildTypes.f_FindEqual(ECustom))
			return;
		auto &Type = _Project.mp_OrderedBuildTypes[ECustom];
		mint FileIndex = 0;
		for (auto iFile = Type.f_GetIterator(); iFile; ++iFile)
		{
			CStr GUID = fg_Format("{}{nfh,sl2,sf0}", iFile->m_BuildGUID, FileIndex);
			CStr OutputFiles;
			for (auto &Output : iFile->m_CustomOutputs)
				fg_AddStrSep(OutputFiles, CStr::CFormat("\t\t\t\t{},") << Output.f_EscapeStr("\\\"\r\n\t", "\\\"rnt"), "\n");
			
			TCVector<CStr> ParsedParams;
			NStr::CStr Params = iFile->m_CustomCommandLine;
			while (!Params.f_IsEmpty())
				ParsedParams.f_Insert(fg_GetStrSepEscaped(Params, " "));
			
			CStr GeneratedParams = CProcessLaunchParams::fs_GetParams(ParsedParams);

			ToPerform.f_Add
				(
					GUID
					,[pFile = &*iFile, OutputFiles, GUID, GeneratedParams, &o_Output]
					{
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
			script = "export PATH=$MalterlibBuildSystemExecutablePath:$PATH\ncd \"{}\"\n{}\n";
		};
)-----"
							)
							<< GUID 
							<< pFile->m_FileName.f_EscapeStr()
							<< OutputFiles
							<< pFile->m_CustomWorkingDirectory.f_EscapeStrNoQuotes("\\\"\r\n\t", "\\\"rnt")
							<< GeneratedParams.f_EscapeStrNoQuotes("\\\"\r\n\t", "\\\"rnt")
						;
						
					}
				)
			;
			ThreadLocal.mp_BuildRules[GUID];
		}
		
		ToPerform.f_Perform();
	}
	
	void CGeneratorInstance::fp_GeneratePBXBuildFileSection(CProject &_Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;
		
		auto & ThreadLocal = *m_ThreadLocal;
		for (auto Iter = _Project.mp_OrderedBuildTypes.f_GetIterator(); Iter; ++Iter)
		{
			if (Iter.f_GetKey() == ENone || Iter.f_GetKey() == ECInclude)
				continue;

			for (auto FIter = (*Iter).f_GetIterator(); FIter; ++FIter)
			{
				auto pFile = &*FIter;
				ToPerform.f_Add
					(
						pFile->m_BuildGUID
						,[&, pFile]
						{
							_Output 
								+= CStr::CFormat("\t\t{} /* {} in Sources */ = {{isa = PBXBuildFile; fileRef = {} /* {} */;") 
								<< pFile->m_BuildGUID 
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
		{
			if (!_Project.m_NativeTarget.m_ProductType.f_IsEmpty())
			{
				ToPerform.f_Add
					(
						_Project.m_NativeTarget.f_GetProductReferenceGUID()
						, [&]
						{
							CStr Entry = ((CStr::CFormat("isa = PBXFileReference; includeInIndex = 0; path = \"{}\"; sourceTree = {}; ")
								<< _Project.m_NativeTarget.m_ProductPath
								<< _Project.m_NativeTarget.m_ProductSourceTree));

							Entry = "{" + Entry + "}";
							_Output += ((CStr::CFormat("\t\t{} /* {} */ = {};\n") << _Project.m_NativeTarget.f_GetProductReferenceGUID() << _Project.m_NativeTarget.m_ProductName << Entry));
						}
					)
				;
			}
		}

		// Configuration references
		{
			for (auto CIter = _Project.m_NativeTarget.m_BuildConfigurationList.f_GetIterator(); CIter; ++CIter)
			{
				auto pConfig = &*CIter;
				ToPerform.f_Add
					(
						pConfig->f_GetFileRefGUID()
						, [&, pConfig]
						{
							_Output 
								+= CStr::CFormat("\t\t{} /* {} */ = {{isa = PBXFileReference; explicitFileType = text.xcconfig; name = \"{}\"; path = \"{}\"; sourceTree = \"<group>\"; };\n") 
								<< pConfig->f_GetFileRefGUID() 
								<< pConfig->f_GetFileNoExt() 
								<< pConfig->m_Name
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

			if (!_Project.m_NativeTarget.m_ProductType.f_IsEmpty())
			{
				TCVector<CGroupChild> & lProducts = GroupChildren[_Project.f_GetProductRefGroupGUID()];
				CGroupChild& Child = lProducts.f_Insert();
				Child.m_GUID = _Project.m_NativeTarget.f_GetProductReferenceGUID();
				Child.m_Name = _Project.m_NativeTarget.m_ProductName;
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

		// Dependency product reference groups
#ifdef DGenerateAll
		{
			for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
			{
				if (iDependency->m_Type != "Makefile")
				{
					auto pPerConfig = iDependency->m_PerConfig.f_FindSmallest();
					if (!pPerConfig)
						continue;
					TCVector<CGroupChild> & lProducts = GroupChildren[pDependency->f_GetProductRefGroupGUID()];
					
					CGroupChild& Child = lProducts.f_Insert();
					Child.m_GUID = iDependency->f_GetReferenceProxyGUID();
					Child.m_Name = pPerConfig->m_CalculatedPath;
				}
				
				auto *pDependency = &*iDependency;

				ToPerform.f_Add
					(
						pDependency->f_GetProductRefGroupGUID()
						, [&, pDependency]
						{
							fl_OutputGroup(pDependency->f_GetProductRefGroupGUID(), "Product");
						}
					)
				;
			}
		}
#endif

		// Configurations group
		{
			TCVector<CGroupChild> & lConfigurations = GroupChildren[_Project.f_GetConfigurationsGroupGUID()];
			for (auto CIter = _Project.m_NativeTarget.m_BuildConfigurationList.f_GetIterator(); CIter; ++CIter)
			{
				CGroupChild& Child = lConfigurations.f_Insert();
				Child.m_GUID = CIter->f_GetFileRefGUID();
				Child.m_Name = CFile::fs_GetFileNoExt(CIter->f_GetFile());
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
		_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << _Project.m_NativeTarget.f_GetGUID() << _Project.m_NativeTarget.m_Name);
		_Output += "\t\t\t);\n\t\t};\n";
	}

#if 0
	void CGeneratorInstance::fp_GeneratePBXHeadersBuildPhaseSection(CProject &_Project, CStr& _Output) const
	{
		_Output += (CStr::CFormat("\t\t{} /* Headers */") << _Project.m_NativeTarget.f_GetHeadersBuildPhaseGUID());
		_Output += " = {\n\t\t\tisa = PBXHeadersBuildPhase;\n";
		_Output += (CStr::CFormat("\t\t\tbuildActionMask = {};\n\t\t\tfiles = (\n") << _Project.m_NativeTarget.m_BuildActionMask);

		for (auto Iter = _Project.mp_OrderedBuildTypes.f_GetIterator(); Iter; ++Iter)
		{
			if (Iter.f_GetKey() != ECInclude)
				continue;

			for (auto FIter = (*Iter).f_GetIterator(); FIter; ++FIter)
			{
				_Output += (CStr::CFormat("\t\t\t\t{} /* {} in Headers */,\n") << FIter->m_BuildGUID << FIter->m_Name);
			}
		}

		_Output += "\t\t\t);\n\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t};\n";
	}
#endif

	void CGeneratorInstance::fp_GeneratePBXSourcesBuildPhaseSection(CProject &_Project, CStr& _Output) const
	{
		_Output += (CStr::CFormat("\t\t{} /* Sources */") << _Project.m_NativeTarget.f_GetSourcesBuildPhaseGUID());
		_Output += " = {\n\t\t\tisa = PBXSourcesBuildPhase;\n";
		_Output += (CStr::CFormat("\t\t\tbuildActionMask = {};\n\t\t\tfiles = (\n") << _Project.m_NativeTarget.m_BuildActionMask);

		for (auto Iter = _Project.mp_OrderedBuildTypes.f_GetIterator(); Iter; ++Iter)
		{
			if (Iter.f_GetKey() == ENone || Iter.f_GetKey() == ECInclude)
				continue;

			for (auto FIter = (*Iter).f_GetIterator(); FIter; ++FIter)
			{
				_Output += (CStr::CFormat("\t\t\t\t{} /* {} in Sources */,\n") << FIter->m_BuildGUID << FIter->m_Name);
			}
		}
		
		_Output += "\t\t\t);\n\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t};\n";
	}

	void CGeneratorInstance::fp_GeneratePBXFrameworksBuildPhaseSection(CProject &_Project, CStr& _Output) const
	{
		_Output += (CStr::CFormat("\t\t{} /* Frameworks */") << _Project.m_NativeTarget.f_GetFrameworksBuildPhaseGUID());
		_Output += " = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n";
		_Output += (CStr::CFormat("\t\t\tbuildActionMask = {};\n") << _Project.m_NativeTarget.m_BuildActionMask);
		_Output += "\t\t\tfiles = (\n";
		_Output += "\t\t\t);\n\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t};\n";
	}

	void CGeneratorInstance::fp_GeneratePBXReferenceProxySection(CProject &_Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;
		for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
		{
			auto * pDependency = &*iDependency;

			if (pDependency->m_Type != "Makefile")
			{
				ToPerform.f_Add
					(
						pDependency->f_GetReferenceProxyGUID()
						, [&, pDependency]
						{
							auto pPerConfig = pDependency->m_PerConfig.f_FindSmallest();
							
							if (!pPerConfig)
								return;
							
							_Output += (CStr::CFormat("\t\t{} /* {} */ = ") << pDependency->f_GetReferenceProxyGUID() << pPerConfig->m_CalculatedPath);
							_Output += "{\n\t\t\tisa = PBXReferenceProxy;\n";
							_Output += (CStr::CFormat("\t\t\tname = {};\n\t\t\tpath = {};\n\t\t\tremoteRef = {} /* PBXContainerItemProxy */;\n\t\t\tsourceTree = BUILT_PRODUCTS_DIR;\n")
								<< pPerConfig->m_CalculatedPath
								<< pPerConfig->m_CalculatedPath
								<< pDependency->f_GetContainerItemProductGUID());
							_Output += "\t\t};\n";
						}
					)
				;
			}
		}
		
		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXShellScriptBuildPhaseSection(CProject& _Project, CStr& _Output) const
	{
		auto fl_OutputScript = [&] (CBuildScript& _Script)
		{
			_Output += (CStr::CFormat("\t\t{} /* {} */ = ") << _Script.f_GetGUID() << _Script.m_Name);
			_Output += "{\n\t\t\tisa = PBXShellScriptBuildPhase;\n";
			_Output += (CStr::CFormat("\t\t\tbuildActionMask = {};\n\t\t\tfiles = (\n") << _Project.m_NativeTarget.m_BuildActionMask);
			// Output files used in script here
			_Output += "\t\t\t);\n\t\t\tinputPaths = (\n";
			
			for (auto &Input : _Script.m_Inputs)
				_Output += (CStr::CFormat("\t\t\t\t\"{}\",\n") << Input);
			
			_Output += CStr::CFormat("\t\t\t);\n\t\t\tname = {};\n\t\t\toutputPaths = (\n") << fg_EscapeXcodeProjectVar(_Script.m_Name);
			
			for (auto &Output : _Script.m_Outputs)
				_Output += (CStr::CFormat("\t\t\t\t\"{}\",\n") << Output);
			  
			_Output += "\t\t\t);\n\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t\tshellPath = /bin/bash;\n";
			_Output += (CStr::CFormat("\t\t\tshellScript = \"{}\";\n") << _Script.f_GetScriptSetting());
			_Output += "\t\t\tshowEnvVarsInLog = 0;\n";

			_Output += "\t\t};\n";
		};

		for (auto iScript = _Project.m_NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
		{
			fl_OutputScript(*iScript);
		}
	}

	void CGeneratorInstance::fp_GenerateXCConfigurationList(CProject &_Project, CStr& _Output) const
	{

		TCSortedPerform<CStr const &> ToPerform;
		
		auto fl_GenerateBuildConfigurationList = [&] (CStr const& _GUID, CStr const& _Name, CStr const& _Description, TCVector<CBuildConfiguration> & _ConfigList)
		{
			_Output += (CStr::CFormat("\t\t{} /* Build configuration list for {} \"{}\" */") << _GUID << _Description << _Name);
			_Output += " = {\n\t\t\tisa = XCConfigurationList;\n\t\t\tbuildConfigurations = (\n";

			for (auto Iter = _ConfigList.f_GetIterator(); Iter; ++Iter)
			{
				_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << Iter->f_GetGUID() << Iter->m_Name);
			}
			_Output += "\t\t\t);\n";	
			_Output += "\t\t\tdefaultConfigurationIsVisible = 0;\n";	
			_Output += CStr::CFormat("\t\t\tdefaultConfigurationName = \"{}\";\n") << _ConfigList.f_GetFirst().m_Name;
			_Output += "\t\t};\n";	
		};

		ToPerform.f_Add
			(
				_Project.m_NativeTarget.f_GetBuildConfigurationListGUID()
				, [&]
				{
					fl_GenerateBuildConfigurationList(_Project.m_NativeTarget.f_GetBuildConfigurationListGUID(), _Project.m_NativeTarget.m_Name, "PBXNativeTarget", _Project.m_NativeTarget.m_BuildConfigurationList);
				}
			)
		;
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
		
		auto fl_GenerateBuildConfigurationList = [&] (TCVector<CBuildConfiguration>& _ConfigList)
		{
			for (auto Iter = _ConfigList.f_GetIterator(); Iter; ++Iter)
			{
				auto pIter = &*Iter;
				ToPerform.f_Add
					(
						pIter->f_GetGUID()
						, [&, pIter]
						{
							_Output += (CStr::CFormat("\t\t{} /* {} */") << pIter->f_GetGUID() << pIter->m_Name);
							_Output += " = {\n\t\t\tisa = XCBuildConfiguration;\n";
							_Output += (CStr::CFormat("\t\t\tbaseConfigurationReference = {} /* {} */;\n") << pIter->f_GetFileRefGUID() << CFile::fs_GetFileNoExt(pIter->f_GetFile()));
							_Output += "\t\t\tbuildSettings = {\n\t\t\t};\n";
							_Output += (CStr::CFormat("\t\t\tname = \"{}\";\n") << pIter->m_Name);
							_Output += "\t\t};\n";
						}
					)
				;
			}
		};
		
		fl_GenerateBuildConfigurationList(_Project.m_NativeTarget.m_BuildConfigurationList);
		fl_GenerateBuildConfigurationList(_Project.m_BuildConfigurationList);
		
		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXContainerItemProxySection(CProject& _Project, CStr& _Output) const
	{
		
		auto fl_GeneratePBXContainerItemProxy = [&] (CProjectDependency& _Dependency, bint _bProductProxy)
		{
			_Output += (CStr::CFormat("\t\t{} /* PBXContainerItemProxy */ = ") << (_bProductProxy ? _Dependency.f_GetContainerItemProductGUID() : _Dependency.f_GetContainerItemGUID()));
			_Output += "{\n\t\t\tisa = PBXContainerItemProxy;\n";
			_Output += (CStr::CFormat("\t\t\tcontainerPortal = {} /* {}.xcodeproj */;\n") << _Dependency.f_GetFileRefGUID() << _Dependency.f_GetName());

			CNativeTarget TargetDummy;
			TargetDummy.m_Name = _Dependency.f_GetName();
			_Output += (CStr::CFormat("\t\t\tproxyType = {};\n\t\t\tremoteGlobalIDString = {};\n\t\t\tremoteInfo = {};\n\t\t};\n") 
											<< (_bProductProxy ? CStr("2") : CStr("1")) 
											<< (_bProductProxy ? TargetDummy.f_GetProductReferenceGUID() : TargetDummy.f_GetGUID())
											<< _Dependency.f_GetName());
		};

		TCSortedPerform<CStr const &> ToPerform;
		
		for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
		{
			auto *pDependency = &*iDependency;
#ifdef DGenerateAll
			if (iDependency->m_Type != "Makefile")
			{
				ToPerform.f_Add
					(
						pDependency->f_GetContainerItemProductGUID()
						, [&, pDependency]
						{
							fl_GeneratePBXContainerItemProxy(*pDependency, true);
						}
					)
				;
			}
#endif
			ToPerform.f_Add
				(
					pDependency->f_GetContainerItemGUID()
					, [&, pDependency]
					{
						fl_GeneratePBXContainerItemProxy(*pDependency, false);
					}
				)
			;
		}
		
		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXTargetDependencySection(CProject& _Project, CStr& _Output) const
	{
		auto fl_GeneratePBXTargetDependency = [&] (CProjectDependency& _Dependency)
		{
			_Output += (CStr::CFormat("\t\t{} /* PBXTargetDependency */ = ") << _Dependency.f_GetTargetGUID());
			_Output += "{\n\t\t\tisa = PBXTargetDependency;\n";
			_Output += (CStr::CFormat("\t\t\tname = {};\n\t\t\ttargetProxy = {} /* PBXContainerItemProxy */;\n") << _Dependency.f_GetName() << _Dependency.f_GetContainerItemGUID());
			_Output += "\t\t};\n";
		};

		TCSortedPerform<CStr const &> ToPerform;
		
		for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
		{
			auto pDependency = &*iDependency;
			
			ToPerform.f_Add
				(
					pDependency->f_GetTargetGUID()
					, [&, pDependency]()
					{
						fl_GeneratePBXTargetDependency(*pDependency);
					}
				)
			;
		}
		
		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXLegacyTargetSection(CProject &_Project, CStr& _Output) const
	{
		if (_Project.m_NativeTarget.m_ProductType.f_IsEmpty())
		{
			_Output += (CStr::CFormat("\t\t{} /* {} */ = ") << _Project.m_NativeTarget.f_GetGUID() << _Project.m_NativeTarget.m_Name);
			_Output += "{\n\t\t\tisa = PBXLegacyTarget;\n";
			_Output += (CStr::CFormat("\t\t\tbuildArgumentsString = \"{}\";\n") << (_Project.m_GeneratedBuildScript ? "ExternalBuildToolRun.sh" : ""));
			_Output += (CStr::CFormat("\t\t\tbuildConfigurationList = {} /* Build configuration list for PBXLegacyTarget \"{}\" */;\n") << _Project.m_NativeTarget.f_GetBuildConfigurationListGUID() << _Project.m_NativeTarget.m_Name);
			_Output += (CStr::CFormat("\t\t\tbuildPhases = ();\n\t\t\tbuildToolPath = \"{}\";\n") << CStr("/bin/bash"));
			_Output += "\t\t\tbuildWorkingDirectory = \"${PROJECT_DIR}/";
			_Output += (CStr::CFormat("{}\";\n") << CFile::fs_MakeNiceFilename(_Project.f_GetName()));
			// Dependencies
			_Output += "\n\t\t\tdependencies = (\n";
			for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
			{
				_Output += (CStr::CFormat("\t\t\t\t{} /* PBXTargetDependency */,\n") << iDependency->f_GetTargetGUID());
			}
			_Output += (CStr::CFormat("\t\t\t);\n\t\t\tname = \"{}\";\n\t\t\tpassBuildSettingsInEnvironment = 1;\n\t\t\tproductName = {};\n") << _Project.m_NativeTarget.m_Name << _Project.m_NativeTarget.m_ProductName);
			_Output += "\t\t};\n";
		}
	}

	void CGeneratorInstance::fp_GeneratePBXNativeTargetSection(CProject &_Project, CStr& _Output) const
	{
		if (!_Project.m_NativeTarget.m_ProductType.f_IsEmpty())
		{
			_Output += (CStr::CFormat("\t\t{} /* {} */") << _Project.m_NativeTarget.f_GetGUID() << _Project.m_NativeTarget.m_Name);
			_Output += " = {\n";
			_Output += (CStr::CFormat("\t\t\tisa = PBXNativeTarget;\n\t\t\tbuildConfigurationList = {} /* Build configuration list for PBXNativeTarget \"{}\" */;\n")
				<< _Project.m_NativeTarget.f_GetBuildConfigurationListGUID()
				<< _Project.m_NativeTarget.m_Name);
			_Output += "\t\t\tbuildPhases = (\n";

			// Pre build scripts
			for (auto iScript = _Project.m_NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
			{
				if (!iScript->m_bPostBuild)
					_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID() << iScript->m_Name);
			}
			
			// Sources, Frameworks, Headers
			_Output += CStr::CFormat("\t\t\t\t{} /* Sources */,\n")
				<< _Project.m_NativeTarget.f_GetSourcesBuildPhaseGUID();
			_Output += CStr::CFormat("\t\t\t\t{} /* Frameworks */,\n")
				<< _Project.m_NativeTarget.f_GetFrameworksBuildPhaseGUID();
#if 0
			_Output += CStr::CFormat("\t\t\t\t{} /* Headers */,\n")
				<< _Project.m_NativeTarget.f_GetHeadersBuildPhaseGUID();
#endif

			// Post build scripts
			for (auto iScript = _Project.m_NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
			{
				if (iScript->m_bPostBuild)
					_Output += (CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID() << iScript->m_Name);
			}

			_Output += "\t\t\t);\n";

			// Build rules
			_Output += "\t\t\tbuildRules = (\n";
			auto &ThreadLocal = *m_ThreadLocal;
			for (auto &BuildRule : ThreadLocal.mp_BuildRules)
				_Output += CStr::CFormat("\t\t\t\t{} /* PBXBuildRule */,\n") << BuildRule;
			_Output += "\t\t\t);\n";

			// Dependencies
			_Output += "\t\t\tdependencies = (\n";
			for (auto iDependency = _Project.m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
			{
				_Output += (CStr::CFormat("\t\t\t\t{} /* PBXTargetDependency */,\n") << iDependency->f_GetTargetGUID());
			}
			_Output += "\t\t\t);\n";
			_Output += (CStr::CFormat("\t\t\tname = {};\n\t\t\tproductName = {};\n\t\t\tproductReference = {} /* {} */;\n\t\t\tproductType = \"{}\";\n\t\t};\n")
				<< fg_EscapeXcodeProjectVar(_Project.m_NativeTarget.m_Name)
				<< fg_EscapeXcodeProjectVar(_Project.m_NativeTarget.m_ProductName)
				<< _Project.m_NativeTarget.f_GetProductReferenceGUID()
				<< _Project.m_NativeTarget.m_ProductName
				<< _Project.m_NativeTarget.m_ProductType);
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
				XMLFile.f_SetAttribute(pBuildReference, "BlueprintIdentifier", _Project.m_NativeTarget.f_GetGUID());
				
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
				if (_Project.m_NativeTarget.m_Type == "Application")
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
		if (_Project.m_NativeTarget.m_Type == "Makefile")
		{
			CStr BuildScript = "\tcase $CONFIGURATION in\n";
			CStr CleanScript = BuildScript;
			
			bint bBuildScript;
			bint bCleanScript;

			// Pre build scripts

			CStr FormatClean;
			FormatClean += "\t\t\t\"{}\")\n";
			FormatClean += "\t\t\t\texport MalterlibDependencyFile=\"{}\"\n";
			FormatClean += "\t\t\t\tbash {}\n";
			FormatClean += "\t\t\t\texit $?\n";
			FormatClean += "\t\t\t;;\n";
			
			
			for (auto iConfig = ThreadLocal.mp_EvaluatedTargetSettings.f_GetIterator(); iConfig; ++iConfig)
			{
				CConfiguration const& Configuration = iConfig.f_GetKey();
				CStr Name = (CStr::CFormat("{} {}") << Configuration.m_Platform << Configuration.m_Configuration);

				CStr DependencyFile;
				if (auto pDependencyFile = iConfig->m_Element.f_FindEqual("DependencyFile"))
					DependencyFile = pDependencyFile->f_GetValue();
				
				if (auto pCommandLine_Clean = iConfig->m_Element.f_FindEqual("CommandLine_Clean"))
				{
					CleanScript += (CStr::CFormat(FormatClean) << DependencyFile << Name << pCommandLine_Clean->f_GetValue());
					bCleanScript = true;
				}

				if (auto pCommandLine_Build = iConfig->m_Element.f_FindEqual("CommandLine_Build"))
				{
					BuildScript += (CStr::CFormat("\t\t\t\"{}\")\n") << Name);
					BuildScript += CStr::CFormat("\t\t\t\texport MalterlibDependencyFile=\"{}\"\n") << DependencyFile;
					
					for (auto iScript = _Project.m_NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
					{
						if (!iScript->m_bPostBuild)
						{
							auto pScriptName = iScript->m_ScriptNames.f_FindEqual(iConfig.f_GetKey());
							if (pScriptName)
							{
								BuildScript += (CStr::CFormat("\t\t\t\tbash \"{}\"\n") << *pScriptName);
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
					for (auto iScript = _Project.m_NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
					{
						if (iScript->m_bPostBuild)
						{
							auto pScriptName = iScript->m_ScriptNames.f_FindEqual(iConfig.f_GetKey());
							if (pScriptName)
							{
								BuildScript += (CStr::CFormat("\t\t\t\tbash \"{}\"\n") << *pScriptName);
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
				_Project.m_GeneratedBuildScript = true;
				ScriptData = (CStr::CFormat("#!/bin/bash\n\nexport PATH=$MalterlibBuildSystemExecutablePath:$PATH\ncase $ACTION in\n\t\"\")\n\t\t{}\n\t\t;;\n\t\"clean\")\n\t\t{}\n\t\t;;\nesac\nexit 0")
													<< BuildScript
													<< CleanScript);
			}

			if (!ScriptData.f_IsEmpty())
			{
				CStr OutputFile = CFile::fs_AppendPath(_OutputDir, CFile::fs_MakeNiceFilename(_Project.f_GetName()));
				ThreadLocal.f_CreateDirectory(OutputFile);


				OutputFile = CFile::fs_AppendPath(OutputFile, CStr("ExternalBuildToolRun.sh"));

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
