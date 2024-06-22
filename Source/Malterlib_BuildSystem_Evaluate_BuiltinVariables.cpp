// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_RegisterBuiltinVariables(NContainer::TCMap<CPropertyKey, CTypeWithPosition> &&_Variables) const
	{
		for (auto &Type : _Variables)
		{
			auto &Key = _Variables.fs_GetKey(Type);
			auto &NewType = (mp_BuiltinVariablesDefinitions[Key] = fg_Move(Type));

			auto KeyType = Key.f_GetType();
			if (KeyType == EPropertyType_Compile)
			{
				if (Key.m_Name == gc_ConstString_Disabled.m_String)
					continue;
			}
			else if (KeyType == EPropertyType_Target)
			{
				if (Key.m_Name == gc_ConstString_Disabled.m_String)
					continue;
			}

			if (NewType.m_Type.f_IsDefaulted())
				mp_DefaultedBuiltinVariablesDefinitions[KeyType][&NewType];
		}
	}

	void CBuildSystem::f_ForEachDefaultedBuiltinVariableDefinition
		(
			EPropertyType _Type
			, TCFunction<void (CPropertyKey const &_Key, CTypeWithPosition const &_Type)> const &_fOnDefinition
		) const
	{
		for (auto &pType : mp_DefaultedBuiltinVariablesDefinitions[_Type])
			_fOnDefinition(mp_BuiltinVariablesDefinitions.fs_GetKey(*pType), *pType);
	}

	void CBuildSystem::fp_RegisterBuiltinVariables()
	{
		mp_TypeForPropertyAny = CTypeWithPosition{g_Any};

		f_RegisterBuiltinVariables
			(
				{
					{CPropertyKey(gc_ConstKey_MToolVersion), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Integer, CBuildSystem::mc_MToolVersion))}
					, {CPropertyKey(gc_ConstKey_Generator), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_GeneratorFamily), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_BuildSystemBasePath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_BuildSystemOutputDir), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_BuildSystemFile), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_BuildSystemName), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_HostPlatform), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_HostPlatformFamily), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_HostArchitecture), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_HiddenGroup), DMibBuildSystemTypeWithPosition(g_Boolean)}
					, {CPropertyKey(gc_ConstKey_ExcludeFiles), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, EJSONType_Array))}
					, {CPropertyKey(gc_ConstKey_FullConfiguration), DMibBuildSystemTypeWithPosition(g_String)}

					, {CPropertyKey(gc_ConstKey_Platform), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Architecture), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Configuration), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}

					, {CPropertyKey(gc_ConstKey_MalterlibRepositoryEditor), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_MalterlibRepositoryEditorSequential),  DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_MalterlibRepositoryEditorSleep), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_FloatingPoint, 0.0))}
					, {CPropertyKey(gc_ConstKey_MalterlibRepositoryEditorWorkingDir), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}

					, {CPropertyKey(gc_ConstKey_Workspace_Name), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Workspace_AllTargets), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, EJSONType_Array))}
					, {CPropertyKey(gc_ConstKey_Workspace_ExtraGroups), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Workspace_Enabled), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, true))}

					, {CPropertyKey(gc_ConstKey_Target_Type), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Target_Language), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Target_Name), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Target_ObjectLibrary), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Target_IntermediateDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Target_OutputDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Target_Disabled), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}

					, {CPropertyKey(gc_ConstKey_Target_FileExtension), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Target_FileName), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Target_EnableLinkerGroups), DMibBuildSystemTypeWithPosition(fg_Optional(g_Boolean))}
					, {CPropertyKey(gc_ConstKey_Target_LinkerGroup), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Target_ExtraGroups), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Target_DependencyInjectionGroups), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Target_InjectedExtraGroups), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Target_Group), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Target_FollowIndirectDependencies), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Target_DependencyFile), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Target_Dependencies), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(gc_ConstKey_Target_DependencyTargets), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(gc_ConstKey_Target_ExternalDependencies), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(gc_ConstKey_Target_DependenciesNames), DMibBuildSystemTypeWithPosition(g_StringArray)}
					,
					{
						CPropertyKey(gc_ConstKey_Target_LinkWithLibraries)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(fg_Array(fg_OneOf(g_StringArray, g_String)), _[_]))
					}
					, {CPropertyKey(gc_ConstKey_Target_PostBuildScriptOutputs), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, _[_]))}
					, {CPropertyKey(gc_ConstKey_Target_PostBuildScriptInputs), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, _[_]))}
					, {CPropertyKey(gc_ConstKey_Target_PreBuildScriptOutputs), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, _[_]))}
					, {CPropertyKey(gc_ConstKey_Target_PreBuildScriptInputs), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, _[_]))}

					, {CPropertyKey(gc_ConstKey_Dependency_Indirect), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Dependency_IndirectOrdered), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Dependency_Link), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, true))}
					, {CPropertyKey(gc_ConstKey_Dependency_Target), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Dependency_ObjectLibrary), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}

					, {CPropertyKey(gc_ConstKey_Dependency_Type), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Dependency_Name), DMibBuildSystemTypeWithPosition(g_String)}
					,
					{
						CPropertyKey(gc_ConstKey_Dependency_TargetProperties)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(g_ObjectWithAny, EJSONType_Object))
					}

					, {CPropertyKey(gc_ConstKey_Compile_Type), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Compile_Disabled), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					,
					{
						CPropertyKey(gc_ConstKey_Compile_PrecompilePrefixHeader)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf{{g_Boolean, gc_ConstString_XInternalCreate}}}, false))
					}
					, {CPropertyKey(gc_ConstKey_Compile_PrefixHeader), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Compile_AllowNonExisting), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Compile_SearchPath), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Compile_SystemSearchPath), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Compile_PreprocessorDefines), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Compile_Custom_Message), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Compile_Custom_CommandLine), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Compile_Custom_Inputs), DMibBuildSystemTypeWithPosition(fg_Optional(g_StringArray))}
					, {CPropertyKey(gc_ConstKey_Compile_Custom_WorkingDirectory), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Compile_Custom_Outputs), DMibBuildSystemTypeWithPosition(fg_Optional(g_StringArray))}
					, {CPropertyKey(gc_ConstKey_Compile_InitEarly), DMibBuildSystemTypeWithPosition(fg_Optional(g_Boolean))}

					, {CPropertyKey(gc_ConstKey_Compile_TabWidth), DMibBuildSystemTypeWithPosition(fg_Optional(g_Integer))}
					, {CPropertyKey(gc_ConstKey_Compile_IndentWidth), DMibBuildSystemTypeWithPosition(fg_Optional(g_Integer))}
					, {CPropertyKey(gc_ConstKey_Compile_UsesTabs), DMibBuildSystemTypeWithPosition(fg_Optional(g_Boolean))}

					, {CPropertyKey(gc_ConstKey_CreateTemplate_Name), DMibBuildSystemTypeWithPosition(g_String)}

					, {CPropertyKey(gc_ConstKey_Builtin_GeneratedFiles), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(gc_ConstKey_Builtin_SourceFiles), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(gc_ConstKey_Builtin_BuildSystemSourceAbsolute), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_BuildSystemSource), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_GeneratorStateFile), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_BasePathAbsolute), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_MToolExe), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_MalterlibExe), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_BasePathRelativeProject), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_GeneratedBuildSystemDir), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_BasePath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_IntermediateDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_OutputDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_ProjectPath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_Inherit), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_SourceFileName), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_CMakeRoot), DMibBuildSystemTypeWithPosition(g_String)}

					, {CPropertyKey(gc_ConstKey_This_Identity), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_This_IdentityAsAbsolutePath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_This_IdentityPath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_This_EntityPath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_This_Type), DMibBuildSystemTypeWithPosition(g_String)}

					, {CPropertyKey(gc_ConstKey_Group_HideTargets), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}

					, {CPropertyKey(gc_ConstKey_Import_CMake_Projects), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CacheDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Import_TempDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_UpdateCache), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_Verbose), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_VerboseHash), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_FullRebuildVersion), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, "0"))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CacheExcludePatterns), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CacheExcludeDependenciesPatterns), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					,
					{
						CPropertyKey(gc_ConstKey_Import_CMake_CacheReplaceContents)
						, DMibBuildSystemTypeWithPosition
						(
							fg_Defaulted
							(
								fg_Array
								(
									CBuildSystemSyntax::CClassType
									{
										{
											{gc_ConstString_Find, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_Replace, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_FilePatterns, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_StringArray, EJSONType_Array)}}
											, {gc_ConstString_ExcludeFilePatterns, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_StringArray, EJSONType_Array)}}
											, {gc_ConstString_ApplyToPaths, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_Boolean, false)}}
										}
										, {}
									}
								)
								, _[_]
							)
						)
					}
					,
					{
						CPropertyKey(gc_ConstKey_Import_CMake_CacheDuplicateLines)
						, DMibBuildSystemTypeWithPosition
						(
							fg_Defaulted
							(
								fg_Array
								(
									CBuildSystemSyntax::CClassType
									{
										{
											{gc_ConstString_Match, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_Find, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_Replace, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_FilePatterns, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_StringArray, EJSONType_Array)}}
											, {gc_ConstString_ExcludeFilePatterns, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_StringArray, EJSONType_Array)}}
										}
										, {}
									}
								)
								, _[_]
							)
						)
					}
					,
					{
						CPropertyKey(gc_ConstKey_Import_CMake_Environment)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(CBuildSystemSyntax::CType{CBuildSystemSyntax::CClassType({}, g_String)}, EJSONType_Object))
					}
					,
					{
						CPropertyKey(gc_ConstKey_Import_CMake_CacheIgnoreInputs)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, EJSONType_Array))
					}
					, {CPropertyKey(gc_ConstKey_Import_CMake_IncludeInHash), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, EJSONType_Array))}
					,
					{
						CPropertyKey(gc_ConstKey_Import_CMake_Languages)
						, DMibBuildSystemTypeWithPosition
						(
							fg_Defaulted
							(
								fg_Array
								(
									CBuildSystemSyntax::CClassType
									{
										{
											{gc_ConstString_CMakeLanguage, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_MalterlibLanguage, CBuildSystemSyntax::CClassType::CMember{g_String}}
										}
										, {}
									}
								)
								, _[_]
							)
						)
					}
					, {CPropertyKey(gc_ConstKey_Import_CMake_Config), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_IntermediateName), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_Variables), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_ExcludeFromHash), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_DisableIncludeReplace), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Import_SharedTempDirectory), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_Path), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_VariablesWithPaths), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Import_CMake_SystemName), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_SystemProcessor), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CCompiler), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CCompilerTarget), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CxxCompiler), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CxxCompilerTarget), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					,
					{
						CPropertyKey(gc_ConstKey_Import_CMake_ReplacePrefixes)
						, DMibBuildSystemTypeWithPosition
						(
							fg_Defaulted
							(
								fg_Array
								(
									CBuildSystemSyntax::CClassType
									{
										{
											{gc_ConstString_Find, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_Replace, CBuildSystemSyntax::CClassType::CMember{g_String}}
										}
										, {}
									}
								)
								, _[_]
							)
						)
					}
					, {CPropertyKey(gc_ConstKey_Import_CMake_SysRoot), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CExternalToolChain), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_CxxExternalToolChain), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Import_CMake_AlwaysFullRebuild), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, true))}

					, {CPropertyKey(gc_ConstKey_GenerateFile_Name), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_GenerateFile_Contents), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_GenerateFile_UnicodeBOM), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, true))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_BeforeImports), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_UTF16),  DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_Executable), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_UnixLineEnds), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_Symlink), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_SymlinkDirectory), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_NoDateCheck), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_KeepGeneratedFile), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_SymlinkBasePath), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_Group), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}

					, {CPropertyKey(gc_ConstKey_Repository_Location), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Repository_ConfigFile), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_StateFile), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_URL), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_DefaultBranch), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_DefaultUpstreamBranch), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_Tags), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Repository_Submodule), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Repository_ExcludeFromSeen), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Repository_SubmoduleName), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_Type), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_UserName), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_UserEmail), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Repository_ProtectedBranches), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Repository_ProtectedTags), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Repository_UpdateSubmodules), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Repository_BootstrapSource), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(gc_ConstKey_Repository_NoPushRemotes), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Repository_LfsReleaseStore), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					,
					{
						CPropertyKey(gc_ConstKey_Repository_Remotes)
						, DMibBuildSystemTypeWithPosition
						(
							fg_Defaulted
							(
								fg_Array
								(
									CBuildSystemSyntax::CClassType
									{
										{
											{gc_ConstString_Name, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_URL, CBuildSystemSyntax::CClassType::CMember{g_String}}
											, {gc_ConstString_Write, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_Boolean, false)}}
											, {gc_ConstString_DefaultBranch, CBuildSystemSyntax::CClassType::CMember{g_String, true}}
											, {gc_ConstString_LfsReleaseStore, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_Boolean, false)}}
											, {gc_ConstKey_Repository_ApplyPolicy.m_Name, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_Boolean, false)}}
											, {gc_ConstKey_Repository_ApplyPolicyPretend.m_Name, CBuildSystemSyntax::CClassType::CMember{fg_Defaulted(g_Boolean, false)}}
											, {gc_ConstKey_Repository_Policy.m_Name, CBuildSystemSyntax::CClassType::CMember{fg_UserType(gc_ConstString_CRepositoryPolicy), true}}
											, 
											{
												gc_ConstKey_Repository_ReleasePackage.m_Name
												, CBuildSystemSyntax::CClassType::CMember{fg_UserType(gc_ConstString_CRepositoryReleasePackage), true}
											}
										}
										, {}
									}
								)
								, _[_]
							)
						)
					}
				}
			)
		;

		for (EPropertyType PropertyType = EPropertyType_Property; PropertyType < EPropertyType_Max; PropertyType = (EPropertyType)(PropertyType + 1))
			mp_BuiltinVariablesDefinitions[CPropertyKey(mp_StringCache, PropertyType, gc_ConstString_FullEval)] = DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false));
	}
}
