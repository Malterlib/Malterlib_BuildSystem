// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_RegisterBuiltinVariables(NContainer::TCMap<CPropertyKey, CBuildSystemSyntax::CType> &&_Variables) const
	{
		for (auto &Type : _Variables)
			mp_BuiltinVariablesDefinitions[_Variables.fs_GetKey(Type)] = CTypeWithPosition{fg_Move(Type)};
	}

	void CBuildSystem::fp_RegisterBuiltinVariables()
	{
		mp_TypeForPropertyAny = CTypeWithPosition{g_Any};

		f_RegisterBuiltinVariables
			(
				{
					{CPropertyKey("MToolVersion"), fg_Defaulted(g_Integer, CBuildSystem::mc_MToolVersion)}
					, {CPropertyKey("Generator"), g_String}
					, {CPropertyKey("GeneratorFamily"), g_String}
					, {CPropertyKey("BuildSystemBasePath"), g_String}
					, {CPropertyKey("BuildSystemOutputDir"), g_String}
					, {CPropertyKey("BuildSystemFile"), g_String}
					, {CPropertyKey("BuildSystemName"), g_String}
					, {CPropertyKey("HostPlatform"), g_String}
					, {CPropertyKey("HostPlatformFamily"), g_String}
					, {CPropertyKey("HostArchitecture"), g_String}
					, {CPropertyKey("HiddenGroup"), g_Boolean}
					, {CPropertyKey("ExcludeFiles"), fg_Defaulted(g_StringArray, EJSONType_Array)}
					, {CPropertyKey("FullConfiguration"), g_String}

					, {CPropertyKey("Platform"), fg_Optional(g_String)}
					, {CPropertyKey("Architecture"), fg_Optional(g_String)}
					, {CPropertyKey("Configuration"), fg_Optional(g_String)}

					, {CPropertyKey("MalterlibRepositoryEditor"), g_String}
					, {CPropertyKey("MalterlibRepositoryEditorSequential"),  fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey("MalterlibRepositoryEditorSleep"), fg_Defaulted(g_FloatingPoint, 0.0)}
					, {CPropertyKey("MalterlibRepositoryEditorWorkingDir"), fg_Defaulted(g_String, "")}

					, {CPropertyKey(EPropertyType_Compile, "Type"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Compile, "Disabled"), fg_Defaulted(g_Boolean, false)}
					,
					{
						CPropertyKey(EPropertyType_Compile, "PrecompilePrefixHeader")
						, fg_Defaulted(CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf{{g_Boolean, "XInternalCreate"}}}, false)
					}
					, {CPropertyKey(EPropertyType_Compile, "PrefixHeader"), fg_Optional(g_String)}
					, {CPropertyKey(EPropertyType_Compile, "AllowNonExisting"), fg_Defaulted(g_Boolean, false)}

					, {CPropertyKey(EPropertyType_Workspace, "Name"), g_String}
					, {CPropertyKey(EPropertyType_Workspace, "AllTargets"), fg_Defaulted(g_StringArray, EJSONType_Array)}
					, {CPropertyKey(EPropertyType_Workspace, "ExtraGroups"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Workspace, "Enabled"), fg_Defaulted(g_Boolean, true)}

					, {CPropertyKey(EPropertyType_Target, "Type"), g_String}
					, {CPropertyKey(EPropertyType_Target, "Language"), fg_Optional(g_String)}
					, {CPropertyKey(EPropertyType_Target, "Name"), g_String}
					, {CPropertyKey(EPropertyType_Target, "IntermediateDirectory"), g_String}
					, {CPropertyKey(EPropertyType_Target, "OutputDirectory"), g_String}
					, {CPropertyKey(EPropertyType_Target, "Disabled"), fg_Defaulted(g_Boolean, false)}

					, {CPropertyKey(EPropertyType_Target, "FileExtension"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Target, "FileName"), g_String}
					, {CPropertyKey(EPropertyType_Target, "EnableLinkerGroups"), fg_Optional(g_Boolean)}
					, {CPropertyKey(EPropertyType_Target, "LinkerGroup"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Target, "ExtraGroups"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Target, "Group"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Target, "FollowIndirectDependencies"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Target, "DependencyFile"), fg_Optional(g_String)}
					, {CPropertyKey(EPropertyType_Target, "Dependencies"), g_StringArray}
					, {CPropertyKey(EPropertyType_Target, "DependenciesNames"), g_StringArray}
					, {CPropertyKey(EPropertyType_Target, "LinkWithLibraries"), fg_Defaulted(fg_Array(fg_OneOf(g_StringArray, g_String)), _[_])}
					, {CPropertyKey(EPropertyType_Target, "PostBuildScriptOutputs"), fg_Defaulted(g_StringArray, _[_])}
					, {CPropertyKey(EPropertyType_Target, "PostBuildScriptInputs"), fg_Defaulted(g_StringArray, _[_])}
					, {CPropertyKey(EPropertyType_Target, "PreBuildScriptOutputs"), fg_Defaulted(g_StringArray, _[_])}
					, {CPropertyKey(EPropertyType_Target, "PreBuildScriptInputs"), fg_Defaulted(g_StringArray, _[_])}

					, {CPropertyKey(EPropertyType_Dependency, "Indirect"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Dependency, "IndirectOrdered"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Dependency, "Link"), fg_Defaulted(g_Boolean, true)}
					, {CPropertyKey(EPropertyType_Dependency, "Target"), fg_Optional(g_String)}
					, {CPropertyKey(EPropertyType_Dependency, "TargetProperties"), fg_Defaulted(g_ObjectWithAny, EJSONType_Object)}

					, {CPropertyKey(EPropertyType_Compile, "SearchPath"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Compile, "SystemSearchPath"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Compile, "PreprocessorDefines"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Compile, "Custom_Message"), fg_Optional(g_String)}
					, {CPropertyKey(EPropertyType_Compile, "Custom_CommandLine"), fg_Optional(g_String)}
					, {CPropertyKey(EPropertyType_Compile, "Custom_Inputs"), fg_Optional(g_StringArray)}
					, {CPropertyKey(EPropertyType_Compile, "Custom_WorkingDirectory"), fg_Optional(g_String)}
					, {CPropertyKey(EPropertyType_Compile, "Custom_Outputs"), fg_Optional(g_StringArray)}
					, {CPropertyKey(EPropertyType_Compile, "InitEarly"), fg_Optional(g_Boolean)}

					, {CPropertyKey(EPropertyType_Compile, "TabWidth"), fg_Optional(g_Integer)}
					, {CPropertyKey(EPropertyType_Compile, "IndentWidth"), fg_Optional(g_Integer)}
					, {CPropertyKey(EPropertyType_Compile, "UsesTabs"), fg_Optional(g_Boolean)}

					, {CPropertyKey(EPropertyType_CreateTemplate, "Name"), g_String}

					, {CPropertyKey(EPropertyType_Builtin, "GeneratedFiles"), g_StringArray}
					, {CPropertyKey(EPropertyType_Builtin, "SourceFiles"), g_StringArray}
					, {CPropertyKey(EPropertyType_Builtin, "BuildSystemSourceAbsolute"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "BuildSystemSource"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "GeneratorStateFile"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "BasePathAbsolute"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "MToolExe"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "MalterlibExe"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "BasePathRelativeProject"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "GeneratedBuildSystemDir"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "BasePath"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "IntermediateDirectory"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "OutputDirectory"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "ProjectPath"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "Inherit"), g_String}
					, {CPropertyKey(EPropertyType_Builtin, "SourceFileName"), g_String}

					, {CPropertyKey(EPropertyType_This, "Identity"), g_String}
					, {CPropertyKey(EPropertyType_This, "IdentityAsAbsolutePath"), g_String}
					, {CPropertyKey(EPropertyType_This, "IdentityPath"), g_String}
					, {CPropertyKey(EPropertyType_This, "Type"), g_String}

					, {CPropertyKey(EPropertyType_Group, "HideTargets"), fg_Defaulted(g_Boolean, false)}

					, {CPropertyKey(EPropertyType_Import, "CMake_Projects"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Import, "CMake_CacheDirectory"), g_String}
					, {CPropertyKey(EPropertyType_Import, "TempDirectory"), g_String}
					, {CPropertyKey(EPropertyType_Import, "CMake_UpdateCache"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Import, "CMake_Verbose"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Import, "CMake_VerboseHash"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Import, "CMake_FullRebuildVersion"), fg_Defaulted(g_String, "0")}
					, {CPropertyKey(EPropertyType_Import, "CMake_CacheExcludePatterns"), g_StringArrayDefaultedEmpty}
					,
					{
						CPropertyKey(EPropertyType_Import, "CMake_CacheReplaceContents")
						, fg_Defaulted
						(
							fg_Array
							(
								CBuildSystemSyntax::CClassType
								{
									{
										{"Find", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"Replace", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"FilePatterns", CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_StringArray, EJSONType_Array)}}
										, {"ExcludeFilePatterns", CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_StringArray, EJSONType_Array)}}
										, {"ApplyToPaths", CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_Boolean, false)}}
									}
									, {}
								}
							)
							, _[_]
						)
					}
					,
					{
						CPropertyKey(EPropertyType_Import, "CMake_CacheDuplicateLines")
						, fg_Defaulted
						(
							fg_Array
							(
								CBuildSystemSyntax::CClassType
								{
									{
										{"Match", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"Find", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"Replace", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"FilePatterns", CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_StringArray, EJSONType_Array)}}
										, {"ExcludeFilePatterns", CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_StringArray, EJSONType_Array)}}
									}
									, {}
								}
							)
							, _[_]
						)
					}
					, {CPropertyKey(EPropertyType_Import, "CMake_Environment"), fg_Defaulted(CBuildSystemSyntax::CType{CBuildSystemSyntax::CClassType({}, g_String)}, EJSONType_Object)}
					, {CPropertyKey(EPropertyType_Import, "CMake_CacheIgnoreInputs"), fg_Defaulted(g_StringArray, EJSONType_Array)}
					, {CPropertyKey(EPropertyType_Import, "CMake_IncludeInHash"), fg_Defaulted(g_StringArray, EJSONType_Array)}
					,
					{
						CPropertyKey(EPropertyType_Import, "CMake_Languages")
						, fg_Defaulted
						(
							fg_Array
							(
								CBuildSystemSyntax::CClassType
								{
									{
										{"CMakeLanguage", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"MalterlibLanguage", CBuildSystemSyntax::CClassType::CMember{g_String}}
									}
									, {}
								}
							)
							, _[_]
						)
					}
					, {CPropertyKey(EPropertyType_Import, "CMake_Config"), g_String}
					, {CPropertyKey(EPropertyType_Import, "CMake_IntermediateName"), g_String}
					, {CPropertyKey(EPropertyType_Import, "CMake_Variables"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Import, "CMake_ExcludeFromHash"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Import, "SharedTempDirectory"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_Path"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Import, "CMake_VariablesWithPaths"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Import, "CMake_SystemName"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_SystemProcessor"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_CCompiler"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_CCompilerTarget"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_CxxCompiler"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_CxxCompilerTarget"), fg_Defaulted(g_String, "")}
					,
					{
						CPropertyKey(EPropertyType_Import, "CMake_ReplacePrefixes")
						, fg_Defaulted
						(
							fg_Array
							(
								CBuildSystemSyntax::CClassType
								{
									{
										{"Find", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"Replace", CBuildSystemSyntax::CClassType::CMember{g_String}}
									}
									, {}
								}
							)
							, _[_]
						)
					}
					, {CPropertyKey(EPropertyType_Import, "CMake_SysRoot"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_CExternalToolChain"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_CxxExternalToolChain"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Import, "CMake_AlwaysFullRebuild"), fg_Defaulted(g_Boolean, true)}

					, {CPropertyKey(EPropertyType_GenerateFile, "Name"), g_String}
					, {CPropertyKey(EPropertyType_GenerateFile, "Contents"), g_String}
					, {CPropertyKey(EPropertyType_GenerateFile, "UnicodeBOM"), fg_Defaulted(g_Boolean, true)}
					, {CPropertyKey(EPropertyType_GenerateFile, "UTF16"),  fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_GenerateFile, "Executable"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_GenerateFile, "UnixLineEnds"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_GenerateFile, "Symlink"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_GenerateFile, "SymlinkDirectory"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_GenerateFile, "NoDateCheck"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_GenerateFile, "KeepGeneratedFile"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_GenerateFile, "SymlinkBasePath"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_GenerateFile, "Group"), fg_Defaulted(g_String, "")}

					, {CPropertyKey(EPropertyType_Repository, "Location"), g_String}
					, {CPropertyKey(EPropertyType_Repository, "ConfigFile"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "StateFile"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "URL"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "DefaultBranch"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "DefaultUpstreamBranch"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "Tags"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Repository, "Submodule"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Repository, "ExcludeFromSeen"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Repository, "SubmoduleName"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "Type"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "UserName"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "UserEmail"), fg_Defaulted(g_String, "")}
					, {CPropertyKey(EPropertyType_Repository, "ProtectedBranches"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Repository, "ProtectedTags"), g_StringArrayDefaultedEmpty}
					, {CPropertyKey(EPropertyType_Repository, "UpdateSubmodules"), fg_Defaulted(g_Boolean, false)}
					, {CPropertyKey(EPropertyType_Repository, "NoPushRemotes"), g_StringArrayDefaultedEmpty}
					,
					{
						CPropertyKey(EPropertyType_Repository, "Remotes")
						, fg_Defaulted
						(
							fg_Array
							(
								CBuildSystemSyntax::CClassType
								{
									{
										{"Name", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"URL", CBuildSystemSyntax::CClassType::CMember{g_String}}
										, {"Write", CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_Boolean, false)}}
									}
									, {}
								}
							)
							, _[_]
						)
					}
				}
			)
		;

		for (EPropertyType PropertyType = EPropertyType_Property; PropertyType < EPropertyType_Max; PropertyType = (EPropertyType)(PropertyType + 1))
			mp_BuiltinVariablesDefinitions[CPropertyKey(PropertyType, "FullEval")] = CTypeWithPosition{fg_Defaulted(g_Boolean, false)};
	}
}
