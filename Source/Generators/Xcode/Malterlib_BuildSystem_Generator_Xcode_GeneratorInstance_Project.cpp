// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "../../Malterlib_BuildSystem_Helpers.h"
#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <Mib/Process/ProcessLaunch>
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NXcode
{
	TCFuture<void> CGeneratorInstance::f_GenerateProjectFile
		(
			CProject &_Project
			, CStr const &_OutputDir
			, TCMap<CConfiguration, TCSet<CStr>> &_Runnables
			, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable
		) const
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

		CProjectState ProjectState;

		ProjectState.m_ProjectOutputDir = CFile::fs_AppendPath(_OutputDir, CStr(_Project.f_GetName() + ".xcodeproj"));
		ProjectState.f_CreateDirectory(ProjectState.m_ProjectOutputDir);

		co_await fp_EvaluateTargetSettings(ProjectState, _Project);
		co_await g_Yield;
		co_await fp_EvaluateFiles(ProjectState, _Project);
		co_await g_Yield;

		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);
			auto &NativeTarget = _Project.f_GetDefaultNativeTarget(Configuration);
			NativeTarget.m_Name = "{} {}"_f << _Project.f_GetName() << Configuration.f_GetFullName();

			if (NativeTarget.m_Type != "Makefile")
			{
				auto &UsedCTypes = ProjectState.m_UsedCTypes[Configuration];
				if (UsedCTypes.f_HasMoreThanOneElement())
				{
					CStr DefaultType;
					if (UsedCTypes.f_FindEqual(gc_ConstString_Symbol_Cpp.m_String))
						DefaultType = gc_ConstString_Symbol_Cpp;
					else if (UsedCTypes.f_FindEqual(gc_ConstString_Symbol_ObjCpp.m_String))
						DefaultType = gc_ConstString_Symbol_ObjCpp;
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
						NewNativeTarget.m_CustomBuildScripts.f_Clear();
						NativeTarget.m_CTargets.f_Insert(&NewNativeTarget);
					}

					NativeTarget.m_ExcludedTypes = UsedCTypes;
				}
				else if (!UsedCTypes.f_IsEmpty())
					NativeTarget.m_CType = *UsedCTypes.f_FindSmallest();
			}

			NativeTarget.m_bDefaultTarget = true;
		}

		co_await fp_EvaluateFileTypeCompileFlags(ProjectState, _Project);
		co_await g_Yield;

		fp_EvaluateDependencies(_Project);
		co_await g_Yield;

		fp_GenerateCompilerFlags(ProjectState, _Project);
		co_await g_Yield;

		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);
			auto pEvaluatedSettings = ProjectState.m_EvaluatedTargetSettings.f_FindEqual(Configuration);
			DCheck(pEvaluatedSettings);
			auto &EvaluatedSettings = *pEvaluatedSettings;
			auto &NativeTarget = NativeTargets.f_GetFirst();

			for (auto &NativeTarget : NativeTargets)
			{
				NativeTarget.m_Type = EvaluatedSettings.m_TargetType.m_Value;
				NativeTarget.m_ProductName = EvaluatedSettings.m_Name.m_Value;
				NativeTarget.m_XcodeProductName = EvaluatedSettings.m_Element[gc_ConstString_PRODUCT_NAME.m_String].f_GetValue();
				if (!NativeTarget.m_bDefaultTarget)
				{
					NativeTarget.m_XcodeProductName += NativeTarget.m_CType;
					NativeTarget.m_ProductName += NativeTarget.m_CType;
				}

				CStr Extension = EvaluatedSettings.m_Element[gc_ConstString_EXECUTABLE_EXTENSION.m_String].f_GetValue();
				if (!NativeTarget.m_bDefaultTarget)
					Extension = "a";

				if (Extension.f_IsEmpty())
					NativeTarget.m_ProductPath = NativeTarget.m_ProductName;
				else
					NativeTarget.m_ProductPath = "{}.{}"_f <<  NativeTarget.m_ProductName << Extension;

				if (NativeTarget.m_bDefaultTarget)
					NativeTarget.m_ProductType = EvaluatedSettings.m_ProductType.m_Value;
				else
					NativeTarget.m_ProductType = gc_ConstString_com_apple_product_type_library_static;

				NativeTarget.m_ProductSourceTree = gc_ConstString_BUILT_PRODUCTS_DIR;
				NativeTarget.m_BuildActionMask = 0;

				TCMap<CStr, CBuildScript *> OutputToBuildScript;

				for (auto &CustomScript : NativeTarget.m_CustomBuildScripts)
				{
					for (auto &Output : CustomScript.m_Outputs)
						OutputToBuildScript[Output] = &CustomScript;
				}

				TCSet<CBuildScript *> AddedScripts;
				TCVector<CBuildScript> NewCustomBuildScripts;

				auto fAddScript = [&](auto &&_fAddScript, CBuildScript *_pScript)
					{
						if (!AddedScripts(_pScript).f_WasCreated())
							return;

						for (auto &Input : _pScript->m_Inputs)
						{
							auto *pInputScript = OutputToBuildScript.f_FindEqual(Input);
							if (pInputScript)
								_fAddScript(_fAddScript, *pInputScript);
						}

						NewCustomBuildScripts.f_Insert(fg_Move(*_pScript));
					}
				;

				for (auto &CustomScript : NativeTarget.m_CustomBuildScripts)
				{
					fAddScript(fAddScript, &CustomScript);
				}

				NativeTarget.m_CustomBuildScripts = fg_Move(NewCustomBuildScripts);
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

				PerConfig.m_SearchPath = EvaluatedSettings.m_Element[gc_ConstString_CONFIGURATION_BUILD_DIR.m_String].f_GetValue();
				PerConfig.m_CalculatedDependencyExtension = "a";
				PerConfig.m_CalculatedDependencyName = DependentTarget.m_XcodeProductName;
				PerConfig.m_CalculatedPath = "{}.{}"_f << PerConfig.m_CalculatedDependencyName << PerConfig.m_CalculatedDependencyExtension;

				CStr Arch = EvaluatedSettings.m_Element[gc_ConstString_NATIVE_ARCH_ACTUAL.m_String].f_GetValue();
				CStr BuildDir = EvaluatedSettings.m_Element[gc_ConstString_BUILD_DIR.m_String].f_GetValue();
				CStr ObjectPath = "{}/$MalterlibXcodeObjectFileDirName/{}"_f << BuildDir << Arch;
				CStr DummyPath = "{}/Dummy.o"_f << ObjectPath;
				CStr LibraryPath = "{}/{}"_f << PerConfig.m_SearchPath << PerConfig.m_CalculatedPath;

				auto &PreBuildScript = NativeTarget.m_BuildScripts[gc_ConstString_PreBuildScript.m_String];
				PreBuildScript.m_Inputs.f_Insert(LibraryPath);
				PreBuildScript.m_Script +=	R"-----(
if [ -e "{0}" ]; then
	MTool CopyWriteTimeIfNewer "{1}" "{0}"
fi
	)-----"_f
					<< DummyPath.f_EscapeStrNoQuotes("\\'\r\n\t", "\\'rnt")
					<< LibraryPath.f_EscapeStrNoQuotes("\\'\r\n\t", "\\'rnt")
				;

			}
		}

		fp_GenerateBuildConfigurationFiles(ProjectState, _Project, _OutputDir);
		co_await g_Yield;
		fp_GenerateBuildConfigurationFilesList(ProjectState, _Project, _OutputDir, _Project.m_BuildConfigurationList);
		co_await g_Yield;

		bool bSchemesChanged = fp_GenerateSchemes(ProjectState, _Project, _Runnables, _Buildable);

		CStr FileData;

		auto fOutput = [&] (CStr const &_Data)
			{
				FileData += _Data;
				FileData += "\n";
			}
		;

		fOutput("// !$*UTF8*$!\n{\n\tarchiveVersion = 1;\n\tclasses = {\n\t};\n\tobjectVersion = 47;\n\tobjects = {\n");

		fOutput("/* Begin PBXBuildFile section */");
		fp_GeneratePBXBuildFileSection(ProjectState, _Project, FileData);
		fOutput("/* End PBXBuildFile section */\n");

		fOutput("/* Begin PBXBuildRule section */");
		fp_GeneratePBXBuildRule(ProjectState, _Project, FileData);
		fOutput("/* End PBXBuildRule section */\n");

		fOutput("/* Begin PBXContainerItemProxy section */");
		fp_GeneratePBXContainerItemProxySection(_Project, FileData);
		fOutput("/* End PBXContainerItemProxy section */\n");

		fOutput("/* Begin PBXFileReference section */");
		fp_GeneratePBXFileReferenceSection(_Project, _OutputDir, FileData);
		fOutput("/* End PBXFileReference section */\n");

		fOutput("/* Begin PBXFrameworksBuildPhase section */");
		fp_GeneratePBXFrameworksBuildPhaseSection(_Project, FileData);
		fOutput("/* End PBXFrameworksBuildPhase section */\n");

		fOutput("/* Begin PBXGroup section */");
		fp_GeneratePBXGroupSection(_Project, FileData);
		fOutput("/* End PBXGroup section */\n");

		fp_GeneratePBXAggregateTargetSection(_Project, FileData);

		fOutput("/* Begin PBXNativeTarget section */");
		fp_GeneratePBXNativeTargetSection(ProjectState, _Project, FileData);
		fOutput("/* End PBXNativeTarget section */\n");

		fOutput("/* Begin PBXProject section */");
		fp_GeneratePBXProjectSection(_Project, FileData);
		fOutput("/* End PBXProject section */\n");

		fOutput("/* Begin PBXShellScriptBuildPhase section */");
		fp_GeneratePBXShellScriptBuildPhaseSection(_Project, FileData);
		fOutput("/* End PBXShellScriptBuildPhase section */\n");

		fOutput("/* Begin PBXSourcesBuildPhase section */");
		fp_GeneratePBXSourcesBuildPhaseSection(_Project, FileData);
		fOutput("/* End PBXSourcesBuildPhase section */\n");

		fOutput("/* Begin PBXTargetDependency section */");
		fp_GeneratePBXTargetDependencySection(_Project, FileData);
		fOutput("/* End PBXTargetDependency section */\n");

		fOutput("/* Begin XCBuildConfiguration section */");
		fp_GenerateXCBuildConfigurationSection(_Project, FileData);
		fOutput("/* End XCBuildConfiguration section */\n");

		fOutput("/* Begin XCConfigurationList section */");
		fp_GenerateXCConfigurationList(_Project, FileData);
		fOutput("/* End XCConfigurationList section */");

		fOutput("\t};");
		fOutput((CStr::CFormat("\trootObject = {} /* Project object */;") << _Project.f_GetGUID()).f_GetStr());
		fOutput("}");

		{
			CStr GeneratedFile = CFile::fs_AppendPath(ProjectState.m_ProjectOutputDir, gc_ConstString_generatedContainer.m_String);
			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(GeneratedFile, CStr(), _Project.m_pSolution->f_GetName(), bWasCreated))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << GeneratedFile));

			if (bWasCreated)
			{
				CByteVector FileData;
				CFile::fs_WriteStringToVector(FileData, CStr(), false);
				m_BuildSystem.f_WriteFile(FileData, GeneratedFile);
			}
		}
		{
			CStr OutputFile = CFile::fs_AppendPath(ProjectState.m_ProjectOutputDir, gc_ConstString_project_pbxproj.m_String);

			bool bWasCreated;
			if (!m_BuildSystem.f_AddGeneratedFile(OutputFile, FileData, _Project.m_pSolution->f_GetName(), bWasCreated))
				DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << OutputFile));

			if (bWasCreated)
			{
				CByteVector FileDataVector;
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

		co_return {};
	}

	void CGeneratorInstance::fp_CalculateDependencyProductPath
		(
			CProject &_Project
			, CProjectDependency &_Dependency
			, TCMap<CConfiguration, CEntityMutablePointer> const &_EnabledConfigurations
		) const
	{
		TCMap<CConfiguration, CEntityMutablePointer> TargetConfigs;
		for (auto &Entity : _EnabledConfigurations)
		{
			auto &Configuration = _EnabledConfigurations.fs_GetKey(Entity);
			TargetConfigs[Configuration] = _Project.m_EnabledProjectConfigs[Configuration];
		}

		auto Extension = fp_GetConfigValues(TargetConfigs, gc_ConstKey_Target_FileExtension, EEJSONType_String, false);
		auto FileName = fp_GetConfigValues(TargetConfigs, gc_ConstKey_Target_FileName, EEJSONType_String, false);
		auto OutputDirectory = fp_GetConfigValues(TargetConfigs, gc_ConstKey_Target_OutputDirectory, EEJSONType_String, false);
		_Dependency.m_Type = fp_GetSingleConfigValue(TargetConfigs, gc_ConstKey_GeneratorSetting_Xcode_TargetType, EEJSONType_String, false).m_Value.f_Get().f_String();
		auto EnableLinkerGroups = fp_GetConfigValues(TargetConfigs, gc_ConstKey_Target_EnableLinkerGroups, EEJSONType_Boolean, false);
		auto LinkerGroup = fp_GetConfigValues(TargetConfigs, gc_ConstKey_Target_LinkerGroup, EEJSONType_String, true);

		for (auto &Entity : _EnabledConfigurations)
		{
			auto &Configuration = _EnabledConfigurations.fs_GetKey(Entity);

			auto *pPerConfig = _Dependency.m_PerConfig.f_FindEqual(Configuration);
			DCheck(pPerConfig);
			if (!pPerConfig)
				continue;

			auto &PerConfig = *pPerConfig;

			if (_Dependency.m_Type != gc_ConstString_StaticLibrary.m_String && _Dependency.m_Type != gc_ConstString_SharedDynamicLibrary.m_String)
				PerConfig.m_bLink = false;

			auto *pOutputDir = OutputDirectory.f_FindEqual(Configuration);
			PerConfig.m_SearchPath = pOutputDir ? pOutputDir->m_Value.f_Get().f_String().f_RemoveSuffix("/") : CStr();

			auto pExtension = Extension.f_FindEqual(Configuration);
			PerConfig.m_CalculatedDependencyExtension = pExtension ? pExtension->m_Value.f_Get().f_String().f_Replace(".", "") : CStr();

			auto pFileName = FileName.f_FindEqual(Configuration);
			PerConfig.m_CalculatedDependencyName = pFileName->m_Value.f_Get().f_String();

			if (auto pEnabledLinkerGroup = EnableLinkerGroups.f_FindEqual(Configuration); pEnabledLinkerGroup && pEnabledLinkerGroup->m_Value.f_Get().f_Boolean())
			{
				auto pLinkerGroupPerConfig = LinkerGroup.f_FindEqual(Configuration);
				if (pLinkerGroupPerConfig && pLinkerGroupPerConfig->m_Value.f_Get().f_IsValid())
					PerConfig.m_LinkerGroup = pLinkerGroupPerConfig->m_Value.f_Get().f_String();
			}

			if (PerConfig.m_CalculatedDependencyExtension.f_IsEmpty())
				PerConfig.m_CalculatedPath = PerConfig.m_CalculatedDependencyName;
			else
				PerConfig.m_CalculatedPath = "{}.{}"_f << PerConfig.m_CalculatedDependencyName << PerConfig.m_CalculatedDependencyExtension;
		}

	}

	TCFuture<void> CGeneratorInstance::fp_EvaluateFileTypeCompileFlags(CProjectState &_ProjectState, CProject &_Project) const
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

		for (auto Iter = _ProjectState.m_EvaluatedTypesInUse.f_GetIterator(); Iter; ++Iter)
		{
			co_await g_Yield;

			CStr Type = Iter.f_GetKey();

			TCMap<CPropertyKey, CEJSONSorted> StartValuesCompile;
			StartValuesCompile[gc_ConstKey_Compile_Type] = Type;

			TCVector<CEntity *> ToRemove;
			auto Cleanup = g_OnScopeExit / [&]
				{
					for (auto &pToRemove : ToRemove)
						pToRemove->m_pParent->m_ChildEntitiesMap.f_Remove(pToRemove);
				}
			;

			TCMap<CConfiguration, CEntityMutablePointer> Configs;
			TCMap<CConfiguration, CConfigResultCompile> Results;

			for (auto iConfig = _Project.m_EnabledProjectConfigs.f_GetIterator(); iConfig; ++iConfig)
			{
				auto &ConfigEntity = **iConfig;
				CEntityKey NewEntityKey = ConfigEntity.f_GetKey();

				auto NewEntityMap = ConfigEntity.m_ChildEntitiesMap(NewEntityKey, &ConfigEntity);
				auto &TempEntity = *NewEntityMap;
				TempEntity.f_DataWritable().m_Position = ConfigEntity.f_Data().m_Position;

				ToRemove.f_Insert(&TempEntity);

				m_BuildSystem.f_InitEntityForEvaluationNoEnv(TempEntity, StartValuesCompile, EEvaluatedPropertyType_External);

				Configs[iConfig.f_GetKey()] = fg_Explicit(&TempEntity);
				Results[iConfig.f_GetKey()];
			}

			co_await fg_ParallelForEach
				(
					Results
					, [&](CConfigResultCompile &o_Result) -> TCFuture<void>
					{
						co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);
						co_await m_BuildSystem.f_CheckCancelled();

						auto &Config = TCMap<CConfiguration, CConfigResultCompile>::fs_GetKey(o_Result);
						auto &Entity = Configs[Config];

						fp_SetEvaluatedValuesCompile(Entity, false, o_Result);

						co_return {};
					}
					, m_BuildSystem.f_SingleThreaded()
				)
			;
			co_await m_BuildSystem.f_CheckCancelled();

			if (!Results.f_IsEmpty())
				_ProjectState.m_EvaluatedTypeCompileFlags[Type] = fg_Move(Results);
		}

		co_return {};
	}

	template <typename tf_CValue, typename tf_FGetValue>
	void CGeneratorInstance::fsp_GetSingleValue
		(
			TCMap<CConfiguration, CConfigResultCompile> const &_CompileEntities
			, tf_CValue &o_Value
			, tf_FGetValue const &_fGetValue
		)
	{
		CBuildSystemUniquePositions FirstPositions;
		bool bFirst = true;

		for (auto &Entity : _CompileEntities)
		{
			decltype(auto) Value = _fGetValue(Entity);

			if (bFirst)
			{
				bFirst = false;
				o_Value = Value.m_Value;
				FirstPositions = Value.m_Positions;
			}
			else
			{
				if (Value.m_Value != o_Value)
				{
					NContainer::TCVector<CBuildSystemError> OtherErrors;
					if (!FirstPositions.f_IsEmpty())
					{
						auto &Error = OtherErrors.f_Insert();
						Error.m_Error = "See previous definition";
						Error.m_Positions = FirstPositions;
					}

					CBuildSystem::fs_ThrowError
						(
							Value.m_Positions
							, "Value cannot be changed per configuration"
							, OtherErrors
						)
					;
				}
			}
		}
	}

	TCFuture<void> CGeneratorInstance::fp_EvaluateFiles(CProjectState &_ProjectState, CProject &_Project) const
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

		for (auto &File : _Project.m_Files)
		{
			for (auto &Config : File.m_EnabledConfigs)
				File.m_CompileResults[File.m_EnabledConfigs.fs_GetKey(Config)];
		}

		co_await fg_ParallelForEach
			(
				_Project.m_EnabledProjectConfigs
				, [&](auto &_ProjectEntity) -> TCFuture<void>
				{
					co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);
					co_await m_BuildSystem.f_CheckCancelled();

					auto &Config = _Project.m_EnabledProjectConfigs.fs_GetKey(_ProjectEntity);
					mint nFiles = 0;
					for (auto &File : _Project.m_Files)
					{
						auto *pResult = File.m_CompileResults.f_FindEqual(Config);
						if (!pResult)
							continue;

						++nFiles;

						fp_SetEvaluatedValuesCompile(File.m_EnabledConfigs[Config], true, *pResult);
						if ((nFiles % 100) == 0)
							co_await g_Yield;
					}

					co_return {};
				}
				, m_BuildSystem.f_SingleThreaded()
			)
		;
		co_await m_BuildSystem.f_CheckCancelled();

		mint nFiles = 0;

		for (auto &File : _Project.m_Files)
		{
			++nFiles;

			if ((nFiles % 100) == 0)
				co_await g_Yield;

			auto Value = fp_GetSingleConfigValue(File.m_EnabledConfigs, gc_ConstKey_Compile_Type, EEJSONType_String, true);
			auto &ValueRef = Value.m_Value.f_Get();

			auto CustomCommandLines = fp_GetConfigValues(File.m_EnabledConfigs, gc_ConstKey_Compile_Custom_CommandLine, EEJSONType_String, true);

			CStr Type = ValueRef.f_IsValid() ? ValueRef.f_String() : CStr();
			auto CompileTypValue = fp_GetSingleConfigValue(File.m_EnabledConfigs, gc_ConstKey_Compile_GenericCompileType, EEJSONType_String, true);
			auto &CompileTypValueRef = CompileTypValue.m_Value.f_Get();

			CStr CompileType = CompileTypValueRef.f_IsValid() ? CompileTypValueRef.f_String() : CStr();

			auto DisabledConfigs = fp_GetConfigValues(File.m_EnabledConfigs, gc_ConstKey_Compile_Disabled, EEJSONType_Boolean, true);
			{
				bool bWasCustom = false;
				bool bFirst = true;
				CConfiguration LastConfig;
				CBuildSystemUniquePositions const *pLastPositions = nullptr;
				for (auto &CustomCommandLine : CustomCommandLines)
				{
					auto &Configuration = CustomCommandLines.fs_GetKey(CustomCommandLine);

					if (auto pDisabled = DisabledConfigs.f_FindEqual(Configuration); pDisabled->m_Value.f_Get().f_IsValid() && pDisabled->m_Value.f_Get().f_Boolean())
						continue;

					auto fReportInconsistent = [&]
						{
							NContainer::TCVector<CBuildSystemError> OtherErrors;
							if (pLastPositions)
							{
								auto &Error = OtherErrors.f_Insert();
								Error.m_Error = "See previous command line";
								Error.m_Positions = *pLastPositions;
							}

							m_BuildSystem.fs_ThrowError
								(
									CustomCommandLine.m_Positions
									, "Inconsistent usage of Custom_CommandLine for '{}/{}': '{}' and '{}'"_f
									<< File.f_GetNameGroupPath()
									<< CFile::fs_GetFile(File.f_GetName())
									<< LastConfig.f_GetFullName()
									<< Configuration.f_GetFullName()
									, OtherErrors
								)
							;
						}
					;

					if (CustomCommandLine.m_Value.f_Get().f_IsValid())
					{
						if (!bFirst && !bWasCustom)
							fReportInconsistent();

						bWasCustom = true;
						if (!CompileType)
							Type = gc_ConstString_Custom;
					}
					else if (bWasCustom)
						fReportInconsistent();

					LastConfig = Configuration;
					pLastPositions = &CustomCommandLine.m_Positions;
					bFirst = false;
				}
			}

			if (Type.f_IsEmpty())
				m_BuildSystem.fs_ThrowError(Value.m_Positions, "No compile type found");

			File.m_Type = Type;

			// Order them in terms of compile type
			{
				EBuildFileType FileType = EBuildFileType_None;

				if (CompileType == "GenericCompile")
					FileType = EBuildFileType_GenericCompile;
				else if (CompileType == "GenericCompileWithFlags")
					FileType = EBuildFileType_GenericCompileWithFlags;
				else if (CompileType == "GenericNonCompile")
					FileType = EBuildFileType_GenericNonCompile;
				else if (CompileType == "GenericCustom")
					FileType = EBuildFileType_GenericCustom;
				else if (File.m_Type == gc_ConstString_Custom.m_String)
					FileType = EBuildFileType_Custom;
				else if
					(
						File.m_Type == gc_ConstString_Symbol_Cpp.m_String
						|| File.m_Type == gc_ConstString_C.m_String
						|| File.m_Type == gc_ConstString_Symbol_ObjCpp.m_String
						|| File.m_Type == gc_ConstString_ObjC.m_String
						|| File.m_Type == gc_ConstString_Assembler.m_String
					)
				{
					auto InitEarly = fp_GetSingleConfigValue(File.m_EnabledConfigs, gc_ConstKey_Compile_InitEarly, EEJSONType_Boolean, true);
					auto &InitEarlyRef = InitEarly.m_Value.f_Get();

					if (InitEarlyRef.f_IsValid() && InitEarlyRef.f_Boolean())
						FileType = EBuildFileType_CCompileInitEarly;
					else
						FileType = EBuildFileType_CCompile;
				}
				else if (File.m_Type == "Header")
					FileType = EBuildFileType_CInclude;
				else if (File.m_Type == "QtMoc")
					FileType = EBuildFileType_QTMoc;
				else if (File.m_Type == "QtUic")
					FileType = EBuildFileType_QTUic;
				else if (File.m_Type == "QtRcc")
					FileType = EBuildFileType_QTRcc;
				else if (File.m_Type == "MlTwk")
					FileType = EBuildFileType_MlTwk;
				else if (File.m_Type == "MalterlibFS")
					FileType = EBuildFileType_MalterlibFS;

				CBuildFileRef& BuildRef = _Project.mp_OrderedBuildTypes[FileType].f_Insert();
				BuildRef.m_FileName = File.f_GetName();
				BuildRef.m_Name = CFile::fs_GetFile(File.f_GetName());
				BuildRef.m_FileRefGUID = File.f_GetFileRefGUID();

				for (auto &Entity : File.m_EnabledConfigs)
				{
					auto &Configuration = File.m_EnabledConfigs.fs_GetKey(Entity);
					BuildRef.m_BuildGUIDs[Configuration] = File.f_GetBuildRefGUID(Configuration);
				}

				BuildRef.m_CompileFlagsGUID = File.f_GetCompileFlagsGUID();
				BuildRef.m_Type = File.m_Type;
				BuildRef.m_bHasCompilerFlags =
					(
						FileType == EBuildFileType_CCompile
						|| FileType == EBuildFileType_CCompileInitEarly
						|| FileType == EBuildFileType_QTMoc
						|| FileType == EBuildFileType_MalterlibFS
						|| FileType == EBuildFileType_GenericCompileWithFlags
					)
				;

				{
					for (auto &Disabled : DisabledConfigs)
					{
						auto &DisabledRef = Disabled.m_Value.f_Get();
						if (DisabledRef.f_IsValid() && DisabledRef.f_Boolean())
							BuildRef.m_Disabled[DisabledConfigs.fs_GetKey(Disabled)];
					}
				}

				if (FileType == EBuildFileType_CCompile || FileType == EBuildFileType_CCompileInitEarly)
				{
					for (auto &pEntity : File.m_EnabledConfigs)
					{
						auto &Configuration = File.m_EnabledConfigs.fs_GetKey(pEntity);
						if (BuildRef.m_Disabled.f_FindEqual(Configuration))
							continue;
						_ProjectState.m_UsedCTypes[Configuration][File.m_Type];
					}
				}

				if (FileType == EBuildFileType_Custom || FileType == EBuildFileType_GenericCustom)
				{
					for (auto &CustomCommandLine : CustomCommandLines)
					{
						auto &Configuration = CustomCommandLines.fs_GetKey(CustomCommandLine);

						if (BuildRef.m_Disabled.f_FindEqual(Configuration))
							continue;

						auto &FileName = File.f_GetName();

						auto fReplaceVariables = [&](CStr const &_String) -> CStr
							{
								return _String
									.f_Replace("$(InputFileBase)", CFile::fs_GetFileNoExt(FileName))
									.f_Replace("$(InputFile)", CFile::fs_GetFile(FileName))
									.f_Replace("$(InputFilePath)", FileName)
								;
							}
						;

						auto ScriptContents = fReplaceVariables(CustomCommandLine.m_Value.f_Get().f_String()).f_EscapeStrNoQuotes("\\'\r\n\t", "\\'rnt");
						CStr WorkingDirectory = fp_GetConfigValue
							(
								File.m_EnabledConfigs
								, Configuration
								, gc_ConstKey_Compile_Custom_WorkingDirectory
								, EEJSONType_String
								, false
							).m_Value.f_Get().f_String()
						;

						auto InputsJson = fp_GetConfigValue(File.m_EnabledConfigs, Configuration, gc_ConstKey_Compile_Custom_Inputs, EEJSONType_Array, false);
						auto &InputsJsonRef = InputsJson.m_Value.f_Get();
						if (!InputsJsonRef.f_IsStringArray())
							m_BuildSystem.fs_ThrowError(CustomCommandLine.m_Positions, "You need to Custom_Inputs as a string array");

						TCVector<CStr> Inputs;
						for (auto &Input : InputsJsonRef.f_Array())
							Inputs.f_Insert(fReplaceVariables(Input.f_String()));

						auto OutputsJson = fp_GetConfigValue(File.m_EnabledConfigs, Configuration, gc_ConstKey_Compile_Custom_Outputs, EEJSONType_Array, false);
						auto &OutputsJsonRef = OutputsJson.m_Value.f_Get();
						if (!OutputsJsonRef.f_IsStringArray())
							m_BuildSystem.fs_ThrowError(CustomCommandLine.m_Positions, "You need to Custom_Outputs as a string array");

						TCVector<CStr> Outputs;
						for (auto &Output : OutputsJsonRef.f_Array())
							Outputs.f_Insert(fReplaceVariables(Output.f_String()));

						auto &NativeTarget = _Project.f_GetDefaultNativeTarget(Configuration);

						{
							CStr FileName = File.f_GetName();

							auto &Script = NativeTarget.m_CustomBuildScripts.f_Insert();
							Script.m_Name = "Custom {}"_f << FileName;
							Script.m_Script = "#/bin/bash\n\n";
							Script.m_Script += "set -eo pipefail\n\n";
							Script.m_Script += "export PATH=$MalterlibBuildSystemExecutablePath:$PATH\n\n";
							Script.m_Script += "{}\n\n"_f << NativeTarget.m_ScriptExport;
							Script.m_Script += "cd {}\n\n"_f << fg_StrEscapeBashDoubleQuotes(WorkingDirectory);
							Script.m_Script += ScriptContents.f_Replace("\r\n", "\n");
							Script.m_Outputs = Outputs;
							Script.m_Inputs = Inputs;
							Script.m_bCustom = true;

							for (auto &Output: Script.m_Outputs)
							{
								CFileKey FileKey;
								FileKey.m_FileName = Output;

								auto *pFile = _Project.m_Files.f_FindSmallestGreaterThanEqual(FileKey);
								if (pFile && pFile->f_GetName() == Output)
								{
									CStr OutputType = fp_GetConfigValue(pFile->m_EnabledConfigs, Configuration, gc_ConstKey_Compile_Type, EEJSONType_String, false)
										.m_Value.f_Get().f_String()
									;
									Script.m_OutputTypes[OutputType];

									if (FileType == EBuildFileType_CCompile || FileType == EBuildFileType_CCompileInitEarly)
									{
										_ProjectState.m_UsedCTypes[Configuration][OutputType];
										_ProjectState.m_EvaluatedTypesInUse[OutputType];
									}
								}
								else
								{
									CEntityKey EntityKey;
									EntityKey.m_Type = EEntityType_File;
									EntityKey.m_Name = {CEJSONSorted(Output)};

									auto pEntity = File.m_EnabledConfigs.f_FindEqual(Configuration);
									auto pParent = (*pEntity)->m_pParent;

									auto Mapping = pParent->m_ChildEntitiesMap(EntityKey, pParent);

									auto *pNewEntity = &Mapping.f_GetResult();
									pNewEntity->f_DataWritable().m_Position = pFile->m_Position;

									if (Mapping.f_WasCreated())
										pParent->m_ChildEntitiesOrdered.f_Insert(*pNewEntity);

									{
										TCMap<CConfiguration, CEntityMutablePointer> EnabledConfigs;
										EnabledConfigs[Configuration] = fg_Explicit(pNewEntity);

										CStr OutputType = fp_GetConfigValue(EnabledConfigs, Configuration, gc_ConstKey_Compile_Type, EEJSONType_String, false)
											.m_Value.f_Get().f_String()
										;
										Script.m_OutputTypes[OutputType];

										if (FileType == EBuildFileType_CCompile || FileType == EBuildFileType_CCompileInitEarly)
										{
											_ProjectState.m_UsedCTypes[Configuration][OutputType];
											_ProjectState.m_EvaluatedTypesInUse[OutputType];
										}
									}

									if (Mapping.f_WasCreated())
										pParent->m_ChildEntitiesMap.f_Remove(EntityKey);
								}
							}
						}
					}
				}

				File.m_bHasCompilerFlags = BuildRef.m_bHasCompilerFlags;
			}

			// Evaluate compile flags
			auto &CompileEntities = File.m_CompileResults;

			fsp_GetSingleValue
				(
					CompileEntities
					, File.m_LastKnownFileType
					, [&](CConfigResultCompile const &_Result) -> decltype(auto)
					{
						return _Result.m_PBXFileReference.m_LastKnownFileType;
					}
				)
			;
			fsp_GetSingleValue
				(
					CompileEntities
					, File.m_ExplicitFileType
					, [&](CConfigResultCompile const &_Result) -> decltype(auto)
					{
						return _Result.m_PBXFileReference.m_ExplicitFileType;
					}
				)
			;
			fsp_GetSingleValue
				(
					CompileEntities
					, File.m_FileEncoding
					, [&](CConfigResultCompile const &_Result) -> decltype(auto)
					{
						return _Result.m_PBXFileReference.m_FileEncoding;
					}
				)
			;
			fsp_GetSingleValue
				(
					CompileEntities
					, File.m_TabWidth
					, [&](CConfigResultCompile const &_Result) -> decltype(auto)
					{
						return _Result.m_PBXFileReference.m_TabWidth;
					}
				)
			;
			fsp_GetSingleValue
				(
					CompileEntities
					, File.m_UsesTabs
					, [&](CConfigResultCompile const &_Result) -> decltype(auto)
					{
						return _Result.m_PBXFileReference.m_UsesTabs;
					}
				)
			;
			fsp_GetSingleValue
				(
					CompileEntities
					, File.m_IndentWidth
					, [&](CConfigResultCompile const &_Result) -> decltype(auto)
					{
						return _Result.m_PBXFileReference.m_IndentWidth;
					}
				)
			;

			if (!CompileEntities.f_IsEmpty())
				_ProjectState.m_EvaluatedCompileFlags[File.f_GetCompileFlagsGUID()] = fg_Move(CompileEntities);

			_ProjectState.m_EvaluatedTypesInUse[File.m_Type];
			CompileEntities.f_Clear();
		}

		co_return {};
	}

	void CGeneratorInstance::fp_GeneratePBXBuildRule(CProjectState &_ProjectState, CProject &_Project, CStr &o_Output) const
	{
		TCSortedPerform<CStr> ToPerform;

		for (auto FileType : {EBuildFileType_Custom, EBuildFileType_GenericCustom})
		{
			if (!_Project.mp_OrderedBuildTypes.f_FindEqual(FileType))
				continue;

			auto &CustomBuildType = _Project.mp_OrderedBuildTypes[FileType];

			for (auto &File : CustomBuildType)
			{
				auto &FileName = File.m_FileName;

				for (auto &BuildRule : File.m_BuildRules)
				{
					auto &Configuration = File.m_BuildRules.fs_GetKey(BuildRule);

					if (_Project.f_GetDefaultNativeTarget(Configuration).m_ProductType.f_IsEmpty())
						continue;

					ToPerform.f_Add
						(
							BuildRule.m_GUID
							,[FileName, BuildRule, &o_Output]
							{
								CStr OutputFiles;
								CStr InputFiles;
								TCSet<CStr> OutputDirs;
								CStr CreateOutputDirs;

								for (auto &Output : BuildRule.m_Outputs)
								{
									CStr OutputReplaced = Output
										.f_Replace("$(InputFileBase)", "${INPUT_FILE_BASE}")
										.f_Replace("$(InputFile)", "${INPUT_FILE_NAME}")
										.f_Replace("$(InputFilePath)", "${INPUT_FILE_PATH}")
									;

									fg_AddStrSep(OutputFiles, "\t\t\t\t\t\t\"{}\""_f << OutputReplaced, ",\n");
									OutputDirs[CFile::fs_GetPath(OutputReplaced)];
								}

								for (auto &Input : BuildRule.m_Inputs)
								{
									CStr InputReplaced = Input
										.f_Replace("$(InputFileBase)", "${INPUT_FILE_BASE}")
										.f_Replace("$(InputFile)", "${INPUT_FILE_NAME}")
										.f_Replace("$(InputFilePath)", "${INPUT_FILE_PATH}")
									;

									fg_AddStrSep(InputFiles, "\t\t\t\t\t\t\"{}\""_f << InputReplaced, ",\n");
								}

								for (auto &OutputDir : OutputDirs)
									fg_AddStrSep(CreateOutputDirs, "mkdir -p \\\"{}\\\""_f << OutputDir.f_EscapeStrNoQuotes(), "\\n");

								o_Output
									+= CStr::CFormat
									(
		R"-----(				{} /* PBXBuildRule */ = {{
					isa = PBXBuildRule;
					compilerSpec = com.apple.compilers.proxy.script;
					filePatterns = {};
					fileType = pattern.proxy;
					isEditable = 1;
					setIsAlternate = 1;
					outputFiles = (
{}
					);
					inputFiles = (
{}
					);
					script = "#!/bin/bash\nexport PATH=\"$MalterlibBuildSystemExecutablePath:$PATH\"\neval $OTHER_INPUT_FILE_FLAGS\n{}\ncd \"{}\"\n{}\n";
				};
		)-----"
									)
									<< BuildRule.m_GUID
									<< FileName.f_EscapeStr()
									<< OutputFiles
									<< InputFiles
									<< CreateOutputDirs
									<< BuildRule.m_WorkingDirectory.f_EscapeStrNoQuotes()
									<< BuildRule.m_MalterlibCustomBuildCommandLine.f_EscapeStrNoQuotes()
								;

							}
						)
					;
					_ProjectState.m_BuildRules[Configuration][BuildRule.m_GUID] = BuildRule.m_OutputTypes;
				}
			}
		}

		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXBuildFileSection(CProjectState &_ProjectState, CProject &_Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;

		for (auto Iter = _Project.mp_OrderedBuildTypes.f_GetIterator(); Iter; ++Iter)
		{
			if (Iter.f_GetKey() == EBuildFileType_None || Iter.f_GetKey() == EBuildFileType_CInclude || Iter.f_GetKey() == EBuildFileType_GenericNonCompile || Iter.f_GetKey() == EBuildFileType_Custom || Iter.f_GetKey() == EBuildFileType_GenericCustom)
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
									if (_ProjectState.m_CompileFlagsValues.f_Exists(pFile->m_CompileFlagsGUID))
										Value = pFile->m_CompileFlagsGUID;
									else
									{
										CStr SharedFlag = (CStr::CFormat("{}SharedFlags") << fp_MakeNiceSharedFlagValue(pFile->m_Type));
										if (_ProjectState.m_CompileFlagsValues.f_Exists(SharedFlag))
											Value = SharedFlag;
									}

									if (!Value.f_IsEmpty())
									{
										if (m_XcodeVersion >= 14)
											_Output += CStr::CFormat(" settings = {{COMPILER_FLAGS = \"$({})\"; };") << Value;
										else
											_Output += CStr::CFormat(" settings = {{COMPILER_FLAGS = ${}; };") << Value;
									}
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

	void CGeneratorInstance::fp_GeneratePBXFileReferenceSection(CProject &_Project, CStr const &_OutputDir, CStr& _Output) const
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
							CStr PathRelativeToProj = CFile::fs_MakePathRelative(File.f_GetName(), _OutputDir);
							_Output += CStr::CFormat("\t\t{} /* {} */ = {{isa = PBXFileReference; ") << FileRefGUID << FileName;

							if (File.m_LastKnownFileType)
								_Output += CStr::CFormat("lastKnownFileType = {};") << File.m_LastKnownFileType;

							if (File.m_ExplicitFileType)
								_Output += CStr::CFormat("explicitFileType = {};") << File.m_ExplicitFileType;

							if (File.m_FileEncoding)
								_Output += CStr::CFormat("fileEncoding = {};") << File.m_FileEncoding;

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
								_Output += CStr::CFormat("\t\t{} /* {} */ = {{isa = PBXFileReference; includeInIndex = 0; path = \"{}\"; sourceTree = {}; };\n")
									<< NativeTarget.f_GetProductReferenceGUID()
									<< NativeTarget.m_ProductName
									<< NativeTarget.m_ProductPath
									<< NativeTarget.m_ProductSourceTree
								;
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
			for (auto &Dependency : _Project.m_DependenciesOrdered)
			{
				if (Dependency.m_bInternal || Dependency.m_bExternal)
					continue;

				auto pDependency = &Dependency;

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

		auto fOutputGroupChild = [&] (CStr const &_GUID, CStr const &_Name)
		{
			_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << _GUID << _Name;
		};

		TCSet<CStr> OutputGroups;
		auto fOutputGroup = [&] (CStr const &_GUID, CStr const &_Name)
		{
			if (!OutputGroups(_GUID).f_WasCreated())
				return; // Already output

			_Output += CStr::CFormat("\t\t{} /* {} */") << _GUID << _Name;
			_Output += " = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n";

			auto pChildren = GroupChildren.f_FindEqual(_GUID);
			if (pChildren)
			{
				for (auto Iter = pChildren->f_GetIterator(); Iter; ++Iter)
				{
					fOutputGroupChild(Iter->m_GUID, Iter->m_Name);
				}
			}

			_Output += "\t\t\t);\n";
			_Output += CStr::CFormat("\t\t\tname = {};\n") << fg_EscapeXcodeProjectVar(_Name);
			_Output += "\t\t\tsourceTree = \"<group>\";\n\t\t};\n";
		};

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
						fOutputGroup(pGroup->f_GetGUID(), pGroup->m_Name);
					}
				)
			;
		}

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
						fOutputGroup(_Project.f_GetProductRefGroupGUID(), gc_ConstString_Product_Reference);
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
						fOutputGroup(_Project.f_GetConfigurationsGroupGUID(), gc_ConstString_Configurations);
					}
				)
			;
		}

		// Project Dependencies group
		{
			TCVector<CGroupChild> &ProjectDependencies = GroupChildren[_Project.f_GetProjectDependenciesGroupGUID()];

			for (auto &Dependency : _Project.m_DependenciesOrdered)
			{
				if (Dependency.m_bInternal || Dependency.m_bExternal)
					continue;

				CGroupChild& Child = ProjectDependencies.f_Insert();
				Child.m_GUID = Dependency.f_GetFileRefGUID();
				Child.m_Name = Dependency.f_GetName() + ".xcodeproj";
			}

			ToPerform.f_Add
				(
					_Project.f_GetProjectDependenciesGroupGUID()
					, [&]
					{
						fOutputGroup(_Project.f_GetProjectDependenciesGroupGUID(), gc_ConstString_Target_Dependencies);
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
				Group.m_Name = gc_ConstString_Product_Reference;
			}
			{
				auto &Group = lChildren.f_Insert();
				Group.m_GUID = _Project.f_GetConfigurationsGroupGUID();
				Group.m_Name = gc_ConstString_Configurations;
			}
			{
				auto &Group = lChildren.f_Insert();
				Group.m_GUID = _Project.f_GetProjectDependenciesGroupGUID();
				Group.m_Name = gc_ConstString_Target_Dependencies;
			}

			ToPerform.f_Add
				(
					_Project.f_GetGeneratorGroupGUID()
					, [&]
					{
						fOutputGroup(_Project.f_GetGeneratorGroupGUID(), gc_ConstString__Automatic);
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
							fOutputGroupChild(IterGroup->f_GetGUID(), IterGroup->m_Name);
						}
					}
					for (auto IterFile = _Project.m_Files.f_GetIterator(); IterFile; ++IterFile)
					{
						if (IterFile->m_pGroup == nullptr)
							fOutputGroupChild(IterFile->f_GetFileRefGUID(), CFile::fs_GetFile(IterFile->f_GetName()));
					}

					if (OutputRootGroups(_Project.f_GetGeneratorGroupGUID()).f_WasCreated())
						fOutputGroupChild(_Project.f_GetGeneratorGroupGUID(), gc_ConstString__Automatic);
					//fOutputGroupChild(_Project.f_GetConfigurationsGroupGUID(), gc_ConstString_Configurations);
					//fOutputGroupChild(_Project.f_GetProjectDependenciesGroupGUID(), gc_ConstString_Target_Dependencies);

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
		for (auto &Dependency : _Project.m_DependenciesOrdered)
		{
			if (Dependency.m_bInternal || Dependency.m_bExternal)
				continue;

			auto *pDependency = &Dependency;

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
				if (NativeTarget.m_ProductType.f_IsEmpty())
					continue;

				_Output += (CStr::CFormat("\t\t{} /* Sources */") << NativeTarget.f_GetSourcesBuildPhaseGUID());
				_Output += " = {\n\t\t\tisa = PBXSourcesBuildPhase;\n";
				_Output += (CStr::CFormat("\t\t\tbuildActionMask = {};\n\t\t\tfiles = (\n") << NativeTarget.m_BuildActionMask);

				for (auto iBuildFiles = _Project.mp_OrderedBuildTypes.f_GetIterator(); iBuildFiles; ++iBuildFiles)
				{
					if (iBuildFiles.f_GetKey() == EBuildFileType_None || iBuildFiles.f_GetKey() == EBuildFileType_CInclude || iBuildFiles.f_GetKey() == EBuildFileType_GenericNonCompile || iBuildFiles.f_GetKey() == EBuildFileType_Custom || iBuildFiles.f_GetKey() == EBuildFileType_GenericCustom)
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

							if (!NativeTarget.m_IncludedTypes.f_IsEmpty())
							{
								bool bFoundIncluded = false;
								for (auto &Type : pRule->m_OutputTypes)
								{
									if (NativeTarget.m_IncludedTypes.f_FindEqual(Type))
									{
										bFoundIncluded = true;
										break;
									}
								}
								if (!bFoundIncluded)
									continue;
							}
							if (!NativeTarget.m_ExcludedTypes.f_IsEmpty())
							{
								bool bFoundExcluded = false;
								for (auto &Type : pRule->m_OutputTypes)
								{
									if (NativeTarget.m_ExcludedTypes.f_FindEqual(Type))
									{
										bFoundExcluded = true;
										break;
									}
								}
								if (bFoundExcluded)
									continue;
							}
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
				_Output += CStr::CFormat("\t\t{} /* Frameworks */") << NativeTarget.f_GetFrameworksBuildPhaseGUID();
				_Output += " = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n";
				_Output += CStr::CFormat("\t\t\tbuildActionMask = {};\n") << NativeTarget.m_BuildActionMask;
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

			auto fOutputScript = [&] (CBuildScript& _Script)
				{
					_Output += CStr::CFormat("\t\t{} /* {} */ = ") << _Script.f_GetGUID(Configuration) << _Script.m_Name;
					_Output += "{\n\t\t\tisa = PBXShellScriptBuildPhase;\n";
					_Output += CStr::CFormat("\t\t\tbuildActionMask = {};\n\t\t\tfiles = (\n") << NativeTarget.m_BuildActionMask;
					// Output files used in script here
					_Output += "\t\t\t);\n\t\t\tinputPaths = (\n";

					for (CStr const &Input : _Script.m_Inputs)
						_Output += CStr::CFormat("\t\t\t\t\"{}\",\n") << Input;

					_Output += CStr::CFormat("\t\t\t);\n\t\t\tname = {};\n\t\t\toutputPaths = (\n") << fg_EscapeXcodeProjectVar(_Script.m_Name);

					for (CStr const &Output : _Script.m_Outputs)
						_Output += CStr::CFormat("\t\t\t\t\"{}\",\n") << Output;

					_Output += "\t\t\t);\n\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t\tshellPath = /bin/bash;\n";
					_Output += CStr::CFormat("\t\t\tshellScript = \"{}\";\n") << _Script.f_GetScriptSetting();
					_Output += "\t\t\tshowEnvVarsInLog = 0;\n";

					if (!_Script.m_bCustom && !_Script.m_bPreBuild && !_Script.m_bPostBuild)
						_Output += "\t\t\talwaysOutOfDate = 1;\n";

					_Output += "\t\t};\n";
				}
			;

			for (auto &Script : NativeTarget.m_BuildScripts)
				fOutputScript(Script);

			for (auto &Script : NativeTarget.m_CustomBuildScripts)
				fOutputScript(Script);
		}
	}

	void CGeneratorInstance::fp_GenerateXCConfigurationList(CProject &_Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;

		auto fGenerateBuildConfigurationList = [&] (CStr const &_GUID, CStr const &_Name, CStr const &_Description, TCVector<CBuildConfiguration> const &_ConfigList)
		{
			_Output += CStr::CFormat("\t\t{} /* Build configuration list for {} \"{}\" */") << _GUID << _Description << _Name;
			_Output += " = {\n\t\t\tisa = XCConfigurationList;\n\t\t\tbuildConfigurations = (\n";

			for (auto Iter = _ConfigList.f_GetIterator(); Iter; ++Iter)
				_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << Iter->f_GetGUID() << Iter->m_ConfigName;
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
							fGenerateBuildConfigurationList(NativeTarget.f_GetBuildConfigurationListGUID(), NativeTarget.m_Name, "PBXNativeTarget", {NativeTarget.m_BuildConfiguration});
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
					fGenerateBuildConfigurationList(_Project.f_GetBuildConfigurationListGUID(), _Project.f_GetName(), "PBXProject", _Project.m_BuildConfigurationList);
				}
			)
		;

		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GenerateXCBuildConfigurationSection(CProject &_Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;

		auto fGenerateBuildConfigurationList = [&] (CBuildConfiguration const &_Config, bool _bUseReference)
		{
			ToPerform.f_Add
				(
					_Config.f_GetGUID()
					, [&_Output, pConfig = &_Config, _bUseReference]
					{
						auto &Config = *pConfig;
						_Output += CStr::CFormat("\t\t{} /* {} */") << Config.f_GetGUID() << Config.m_ConfigName;
						_Output += " = {\n\t\t\tisa = XCBuildConfiguration;\n";
						if (_bUseReference)
							_Output += CStr::CFormat("\t\t\tbaseConfigurationReference = {} /* {} */;\n") << Config.f_GetFileRefGUID() << CFile::fs_GetFileNoExt(Config.f_GetFile());
						_Output += "\t\t\tbuildSettings = {\n\t\t\t};\n";
						_Output += CStr::CFormat("\t\t\tname = \"{}\";\n") << Config.m_ConfigName;
						_Output += "\t\t};\n";
					}
				)
			;
		};

		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			for (auto &NativeTarget : NativeTargets)
				fGenerateBuildConfigurationList(NativeTarget.m_BuildConfiguration, true);
		}
		for (auto &Config : _Project.m_BuildConfigurationList)
			fGenerateBuildConfigurationList(Config, false);

		ToPerform.f_Perform();
	}

	void CGeneratorInstance::fp_GeneratePBXContainerItemProxySection(CProject& _Project, CStr& _Output) const
	{
		TCSortedPerform<CStr const &> ToPerform;

		for (auto &Dependency : _Project.m_DependenciesOrdered)
		{
			if (Dependency.m_bExternal)
				continue;

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

		for (auto &Dependency : _Project.m_DependenciesOrdered)
		{
			if (Dependency.m_bExternal)
				continue;

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

	void CGeneratorInstance::fp_GeneratePBXAggregateTargetSection(CProject &_Project, CStr& _Output) const
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
				_Output += "/* Begin PBXAggregateTarget section */\n";
			}

			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);

			_Output += CStr::CFormat("\t\t{} /* {} */ = ") << NativeTarget.f_GetGUID() << NativeTarget.m_Name;
			_Output += "{\n\t\t\tisa = PBXAggregateTarget;\n";

			_Output += CStr::CFormat("\t\t\tbuildConfigurationList = {} /* Build configuration list for PBXAggregateTarget \"{}\" */;\n")
				<< NativeTarget.f_GetBuildConfigurationListGUID()
				<< NativeTarget.m_Name
			;

			_Output += "\t\t\tbuildPhases = (\n";

			for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
			{
				if (iScript->m_bPreBuild)
					_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID(Configuration) << iScript->m_Name;
			}
			for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
			{
				if (!iScript->m_bPreBuild && !iScript->m_bPostBuild)
					_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID(Configuration) << iScript->m_Name;
			}
			for (auto iScript = NativeTarget.m_CustomBuildScripts.f_GetIterator(); iScript; ++iScript)
				_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID(Configuration) << iScript->m_Name;

			for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
			{
				if (iScript->m_bPostBuild)
					_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID(Configuration) << iScript->m_Name;
			}

			_Output += "\t\t\t);\n";

			// Dependencies
			_Output += "\n\t\t\tdependencies = (\n";
			for (auto &Dependency : _Project.m_DependenciesOrdered)
			{
				if (Dependency.m_bExternal)
					continue;

				if (!NativeTarget.m_bDefaultTarget && Dependency.m_bInternal)
					continue;

				auto pPerConfig = Dependency.m_PerConfig.f_FindEqual(Configuration);
				if (!pPerConfig)
					continue;

				_Output += CStr::CFormat("\t\t\t\t{} /* PBXTargetDependency */,\n") << pPerConfig->f_GetTargetGUID(Dependency);
			}
			_Output += CStr::CFormat("\t\t\t);\n\t\t\tname = \"{}\";\n\t\t\tproductName = {};\n") << NativeTarget.m_Name << NativeTarget.m_ProductName;
			_Output += "\t\t};\n";
		}
		if (bBegin)
			_Output += "/* End PBXAggregateTarget section */\n\n";
	}

	void CGeneratorInstance::fp_GeneratePBXNativeTargetSection(CProjectState &_ProjectState, CProject &_Project, CStr& _Output) const
	{
		for (auto &NativeTargets : _Project.m_NativeTargets)
		{
			auto &Configuration = _Project.m_NativeTargets.fs_GetKey(NativeTargets);

			auto &DefaultNativeTarget = _Project.f_GetDefaultNativeTarget(Configuration);

			for (auto &NativeTarget : NativeTargets)
			{
				if (NativeTarget.m_ProductType.f_IsEmpty())
					continue;

				_Output += CStr::CFormat("\t\t{} /* {} */") << NativeTarget.f_GetGUID() << NativeTarget.m_Name;
				_Output += " = {\n";
				_Output += CStr::CFormat("\t\t\tisa = PBXNativeTarget;\n\t\t\tbuildConfigurationList = {} /* Build configuration list for PBXNativeTarget \"{}\" */;\n")
					<< NativeTarget.f_GetBuildConfigurationListGUID()
					<< NativeTarget.m_Name
				;
				_Output += "\t\t\tbuildPhases = (\n";

				// Pre build scripts
				for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
				{
					if (iScript->m_bPreBuild)
						_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID(Configuration) << iScript->m_Name;
				}

				for (auto &Script : DefaultNativeTarget.m_CustomBuildScripts)
				{
					if (!NativeTarget.m_IncludedTypes.f_IsEmpty())
					{
						bool bFoundIncluded = false;
						for (auto &Type : Script.m_OutputTypes)
						{
							if (NativeTarget.m_IncludedTypes.f_FindEqual(Type))
							{
								bFoundIncluded = true;
								break;
							}
						}
						if (!bFoundIncluded)
							continue;
					}
					if (!NativeTarget.m_ExcludedTypes.f_IsEmpty())
					{
						bool bFoundExcluded = false;
						for (auto &Type : Script.m_OutputTypes)
						{
							if (NativeTarget.m_ExcludedTypes.f_FindEqual(Type))
							{
								bFoundExcluded = true;
								break;
							}
						}
						if (bFoundExcluded)
							continue;
					}

					_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << Script.f_GetGUID(Configuration) << Script.m_Name;
				}

				// Sources, Frameworks, Headers
				_Output += CStr::CFormat("\t\t\t\t{} /* Sources */,\n") << NativeTarget.f_GetSourcesBuildPhaseGUID();
				_Output += CStr::CFormat("\t\t\t\t{} /* Frameworks */,\n") << NativeTarget.f_GetFrameworksBuildPhaseGUID();
	#if 0
				_Output += CStr::CFormat("\t\t\t\t{} /* Headers */,\n") << NativeTarget.f_GetHeadersBuildPhaseGUID();
	#endif

				// Post build scripts
				for (auto iScript = NativeTarget.m_BuildScripts.f_GetIterator(); iScript; ++iScript)
				{
					if (iScript->m_bPostBuild)
						_Output += CStr::CFormat("\t\t\t\t{} /* {} */,\n") << iScript->f_GetGUID(Configuration) << iScript->m_Name;
				}

				_Output += "\t\t\t);\n";

				// Build rules
				_Output += "\t\t\tbuildRules = (\n";
				auto pBuildRule = _ProjectState.m_BuildRules.f_FindEqual(Configuration);
				if (pBuildRule)
				{
					for (TCSet<CStr> const &OutputTypes : *pBuildRule)
					{
						auto &BuildRule = pBuildRule->fs_GetKey(OutputTypes);

						if (!NativeTarget.m_IncludedTypes.f_IsEmpty())
						{
							bool bFoundIncluded = false;
							for (auto &Type : OutputTypes)
							{
								if (NativeTarget.m_IncludedTypes.f_FindEqual(Type))
								{
									bFoundIncluded = true;
									break;
								}
							}
							if (!bFoundIncluded)
								continue;
						}
						if (!NativeTarget.m_ExcludedTypes.f_IsEmpty())
						{
							bool bFoundExcluded = false;
							for (auto &Type : OutputTypes)
							{
								if (NativeTarget.m_ExcludedTypes.f_FindEqual(Type))
								{
									bFoundExcluded = true;
									break;
								}
							}
							if (bFoundExcluded)
								continue;
						}

						_Output += CStr::CFormat("\t\t\t\t{} /* PBXBuildRule */,\n") << BuildRule;
					}
				}
				_Output += "\t\t\t);\n";

				// Dependencies
				_Output += "\t\t\tdependencies = (\n";
				for (auto &Dependency : _Project.m_DependenciesOrdered)
				{
					if (Dependency.m_bExternal)
						continue;

					if (!NativeTarget.m_bDefaultTarget && Dependency.m_bInternal)
						continue;

					auto pPerConfig = Dependency.m_PerConfig.f_FindEqual(Configuration);
					if (!pPerConfig)
						continue;

					_Output += CStr::CFormat("\t\t\t\t{} /* PBXTargetDependency */,\n") << pPerConfig->f_GetTargetGUID(Dependency);
				}
				_Output += "\t\t\t);\n";
				_Output += CStr::CFormat("\t\t\tname = {};\n\t\t\tproductName = {};\n\t\t\tproductReference = {} /* {} */;\n\t\t\tproductType = \"{}\";\n\t\t};\n")
					<< fg_EscapeXcodeProjectVar(NativeTarget.m_Name)
					<< fg_EscapeXcodeProjectVar(NativeTarget.m_ProductName)
					<< NativeTarget.f_GetProductReferenceGUID()
					<< NativeTarget.m_ProductName
					<< NativeTarget.m_ProductType
				;
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

					CStr Argument = CXMLDocument::f_GetAttribute(pNode, gc_ConstString_argument);

					if (Argument.f_IsEmpty())
						continue;
					ExistingArgs[Argument] = pNode;
				}
				for (CXMLDocument::CNodeIterator iNewChild(_pNewNode); iNewChild; ++iNewChild)
				{
					auto *pNode = iNewChild->ToElement();
					if (!pNode)
						continue;

					CStr Argument = CXMLDocument::f_GetAttribute(pNode, gc_ConstString_argument);

					if (Argument.f_IsEmpty())
						continue;
					NewArgs[Argument] = pNode;
				}
				for (CXMLDocument::CConstNodeIterator iPrevChild(_pPrevNode); iPrevChild; ++iPrevChild)
				{
					auto *pNode = iPrevChild->ToElement();
					if (!pNode)
						continue;

					CStr Argument = CXMLDocument::f_GetAttribute(pNode, gc_ConstString_argument);

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

	bool CGeneratorInstance::fp_GenerateSchemes
		(
			CProjectState &_ProjectState
			, CProject& _Project
			, TCMap<CConfiguration, TCSet<CStr>> &_Runnables
			, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable
		) const
	{
		bool bSchemesChanged = false;

		CStr OutputDir = CFile::fs_AppendPath(_ProjectState.m_ProjectOutputDir, gc_ConstString_xcshareddata.m_String);
		_ProjectState.f_CreateDirectory(OutputDir);

		OutputDir = CFile::fs_AppendPath(OutputDir, gc_ConstString_xcschemes.m_String);
		_ProjectState.f_CreateDirectory(OutputDir);

		for (auto Iter = _Project.m_EnabledProjectConfigs.f_GetIterator(); Iter; ++Iter)
		{
			CConfiguration const& Configuration = Iter.f_GetKey();

			auto *pNativeTarget = _Project.m_NativeTargets.f_FindEqual(Configuration);
			if (!pNativeTarget)
				continue;

			auto &NativeTarget = pNativeTarget->f_GetFirst();

			if (!NativeTarget.m_bGenerateScheme)
				continue;

			CXMLDocument XMLFile(false);

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

			auto fGenerateBuildReference = [&] (CXMLElement* _pParent) -> CStr
			{
				auto pBuildReference = XMLFile.f_CreateElement(_pParent, "BuildableReference");
				XMLFile.f_SetAttribute(pBuildReference, "BuildableIdentifier", "primary");

				XMLFile.f_SetAttribute(pBuildReference, "BlueprintIdentifier", NativeTarget.f_GetGUID());

				CStr BuildableName;
				CStr Extension = _ProjectState.m_EvaluatedTargetSettings[Configuration].m_Element[gc_ConstString_EXECUTABLE_EXTENSION.m_String].f_GetValue();
				if (Extension.f_IsEmpty())
					BuildableName = _ProjectState.m_EvaluatedTargetSettings[Configuration].m_Element[gc_ConstString_PRODUCT_NAME.m_String].f_GetValue();
				else
				{
					BuildableName = CStr::CFormat("{}.{}")
						<< _ProjectState.m_EvaluatedTargetSettings[Configuration].m_Element[gc_ConstString_PRODUCT_NAME.m_String].f_GetValue()
						<< Extension
					;
				}

				XMLFile.f_SetAttribute(pBuildReference, "BuildableName", BuildableName);
				XMLFile.f_SetAttribute(pBuildReference, "BlueprintName", _Project.f_GetName());
				XMLFile.f_SetAttribute(pBuildReference, "ReferencedContainer", (CStr::CFormat("container:{}.xcodeproj") << _Project.f_GetName()).f_GetStr());
				return BuildableName;
			};

			auto fGenerateRunnablePath = [&] (CXMLElement* _pParent) -> bool
			{
				CElement const& Element = _ProjectState.m_EvaluatedTargetSettings[Configuration].m_Element[gc_ConstString_LocalDebuggerCommand.m_String];
				if (Element.f_GetValue().f_IsEmpty())
					return false;
				auto pPathRunnable = XMLFile.f_CreateElement(_pParent, "PathRunnable");
				XMLFile.f_SetAttribute(pPathRunnable, "FilePath", Element.f_GetValue());
				return true;
			};

			// BuildAction
			{
				auto pBuildAction = XMLFile.f_CreateElement(pScheme, "BuildAction");
				XMLFile.f_SetAttribute(pBuildAction, "parallelizeBuildables", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pBuildAction, "buildImplicitDependencies", gc_ConstString_YES);

				auto pBuildActionEntries = XMLFile.f_CreateElement(pBuildAction, "BuildActionEntries");

				auto pBuildActionEntry = XMLFile.f_CreateElement(pBuildActionEntries, "BuildActionEntry");
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForTesting", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForRunning", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForProfiling", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForArchiving", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pBuildActionEntry, "buildForAnalyzing", gc_ConstString_YES);

				_Buildable[Configuration][_Project.f_GetGUID()] = fGenerateBuildReference(pBuildActionEntry);
			}

			// TestAction
			{
				auto pTestAction = XMLFile.f_CreateElement(pScheme, "TestAction");
				XMLFile.f_SetAttribute(pTestAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pTestAction, "selectedDebuggerIdentifier", "Xcode.DebuggerFoundation.Debugger.LLDB");
				XMLFile.f_SetAttribute(pTestAction, "selectedLauncherIdentifier", "Xcode.DebuggerFoundation.Launcher.LLDB");
				XMLFile.f_SetAttribute(pTestAction, "shouldUseLaunchSchemeArgsEnv", gc_ConstString_YES);
				XMLFile.f_CreateElement(pTestAction, "Testables");

				// Buildable
				if (NativeTarget.m_Type == "Application")
				{
					auto pMacroExpansion = XMLFile.f_CreateElement(pTestAction, "MacroExpansion");
					fGenerateBuildReference(pMacroExpansion);
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
				bool bUsingCustomWorkingDirectory = false;
				if (fGenerateRunnablePath(pLaunchAction))
				{
					_Runnables[Configuration][SchemeName];
					CElement const& Element = _ProjectState.m_EvaluatedTargetSettings[Configuration].m_Element[gc_ConstString_customWorkingDirectory.m_String];
					if (!Element.f_GetValue().f_IsEmpty())
					{
						bUsingCustomWorkingDirectory = true;
						XMLFile.f_SetAttribute(pLaunchAction, "useCustomWorkingDirectory", gc_ConstString_YES);
						XMLFile.f_SetAttribute(pLaunchAction, Element.f_GetProperty(), Element.f_GetValue());
					}
				}

				if (!bUsingCustomWorkingDirectory)
					XMLFile.f_SetAttribute(pLaunchAction, "useCustomWorkingDirectory", gc_ConstString_NO);
				XMLFile.f_SetAttribute(pLaunchAction, "ignoresPersistentStateOnLaunch", gc_ConstString_NO);
				XMLFile.f_SetAttribute(pLaunchAction, "debugDocumentVersioning", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pLaunchAction, "debugServiceExtension", "internal");
				XMLFile.f_SetAttribute(pLaunchAction, "allowLocationSimulation", gc_ConstString_YES);

				// Command line options
				if (_ProjectState.m_EvaluatedTargetSettings[Configuration].m_Element.f_Exists(gc_ConstString_CommandLineArgument.m_String))
				{
					CElement const& Element = _ProjectState.m_EvaluatedTargetSettings[Configuration].m_Element[gc_ConstString_CommandLineArgument.m_String];
					if (!Element.f_IsEmpty())
					{
						auto pCommandLineArguments = XMLFile.f_CreateElement(pLaunchAction, "CommandLineArguments");
						for (auto &Argument : Element.f_ValueArray())
						{
							auto pCommandLineArgument = XMLFile.f_CreateElement(pCommandLineArguments, Element.f_GetProperty());
							XMLFile.f_SetAttribute(pCommandLineArgument, gc_ConstString_argument, Argument);
							XMLFile.f_SetAttribute(pCommandLineArgument, "isEnabled", gc_ConstString_YES);
						}
					}
				}

				 XMLFile.f_CreateElement(pLaunchAction, "AdditionalOptions");
			}

			// ProfileAction
			{
				auto pProfileAction = XMLFile.f_CreateElement(pScheme, "ProfileAction");
				XMLFile.f_SetAttribute(pProfileAction, "buildConfiguration", Name);
				XMLFile.f_SetAttribute(pProfileAction, "shouldUseLaunchSchemeArgsEnv", gc_ConstString_YES);
				XMLFile.f_SetAttribute(pProfileAction, "savedToolIdentifier", "");

				// Working directory
				bool bUsingCustomWorkingDirectory = false;
				if (fGenerateRunnablePath(pProfileAction))
				{
					CElement const& Element = _ProjectState.m_EvaluatedTargetSettings[Configuration].m_Element[gc_ConstString_customWorkingDirectory.m_String];
					if (!Element.f_GetValue().f_IsEmpty())
					{
						bUsingCustomWorkingDirectory = true;
						XMLFile.f_SetAttribute(pProfileAction, "useCustomWorkingDirectory", gc_ConstString_YES);
						XMLFile.f_SetAttribute(pProfileAction, Element.f_GetProperty(), Element.f_GetValue());
					}
				}

				if (!bUsingCustomWorkingDirectory)
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

			CStr FileName = CFile::fs_AppendPath(OutputDir,  (CStr::CFormat("{}.xcscheme") << SchemeName).f_GetStr());

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

			// Output the newly generated file
			{
				CStr XMLData = XMLFile.f_GetAsString(EXMLOutputDialect_Xcode);
				bool bWasCreated;
				if (!m_BuildSystem.f_AddGeneratedFile(FileName, XMLData, _Project.m_pSolution->f_GetName(), bWasCreated, EGeneratedFileFlag_NoDateCheck))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

				if (bWasCreated)
				{
					CByteVector FileData;
					CFile::fs_WriteStringToVector(FileData, CStr(XMLData), false);
					if (m_BuildSystem.f_WriteFile(FileData, FileName))
						bSchemesChanged = true;
				}

				// Save the raw generated file to be able to diff against

				FileName += ".gen";
				if (!m_BuildSystem.f_AddGeneratedFile(FileName, RawXMLData, _Project.m_pSolution->f_GetName(), bWasCreated, EGeneratedFileFlag_NoDateCheck))
					DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << FileName));

				if (bWasCreated)
				{
					CByteVector FileData;
					CFile::fs_WriteStringToVector(FileData, CStr(RawXMLData), false);
					m_BuildSystem.f_WriteFile(FileData, FileName);
				}
			}
		}
		return bSchemesChanged;
	}
}
