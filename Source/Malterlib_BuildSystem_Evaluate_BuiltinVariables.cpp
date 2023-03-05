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
					{CPropertyKey(mp_StringCache, gc_ConstString_MToolVersion), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Integer, CBuildSystem::mc_MToolVersion))}
					, {CPropertyKey(mp_StringCache, gc_ConstString_Generator), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_GeneratorFamily), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_BuildSystemBasePath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_BuildSystemOutputDir), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_BuildSystemFile), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_BuildSystemName), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_HostPlatform), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_HostPlatformFamily), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_HostArchitecture), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_HiddenGroup), DMibBuildSystemTypeWithPosition(g_Boolean)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_ExcludeFiles), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, EJSONType_Array))}
					, {CPropertyKey(mp_StringCache, gc_ConstString_FullConfiguration), DMibBuildSystemTypeWithPosition(g_String)}

					, {CPropertyKey(mp_StringCache, gc_ConstString_Platform), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, gc_ConstString_Architecture), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, gc_ConstString_Configuration), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}

					, {CPropertyKey(mp_StringCache, gc_ConstString_MalterlibRepositoryEditor), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, gc_ConstString_MalterlibRepositoryEditorSequential),  DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, gc_ConstString_MalterlibRepositoryEditorSleep), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_FloatingPoint, 0.0))}
					, {CPropertyKey(mp_StringCache, gc_ConstString_MalterlibRepositoryEditorWorkingDir), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}

					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_Type), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_Disabled), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					,
					{
						CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_PrecompilePrefixHeader)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf{{g_Boolean, gc_ConstString_XInternalCreate}}}, false))
					}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_PrefixHeader), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_AllowNonExisting), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}

					, {CPropertyKey(mp_StringCache, EPropertyType_Workspace, gc_ConstString_Name), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Workspace, gc_ConstString_AllTargets), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, EJSONType_Array))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Workspace, gc_ConstString_ExtraGroups), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Workspace, gc_ConstString_Enabled), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, true))}

					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_Type), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_Language), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_Name), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_IntermediateDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_OutputDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_Disabled), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}

					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_FileExtension), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_FileName), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_EnableLinkerGroups), DMibBuildSystemTypeWithPosition(fg_Optional(g_Boolean))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_LinkerGroup), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_ExtraGroups), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Target_DependencyInjectionGroups), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(gc_ConstKey_Target_InjectedExtraGroups), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_Group), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_FollowIndirectDependencies), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_DependencyFile), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_Dependencies), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(gc_ConstKey_Target_ExternalDependencies), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(gc_ConstKey_Target_DependenciesNames), DMibBuildSystemTypeWithPosition(g_StringArray)}
					,
					{
						CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_LinkWithLibraries)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(fg_Array(fg_OneOf(g_StringArray, g_String)), _[_]))
					}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_PostBuildScriptOutputs), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, _[_]))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_PostBuildScriptInputs), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, _[_]))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_PreBuildScriptOutputs), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, _[_]))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Target, gc_ConstString_PreBuildScriptInputs), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, _[_]))}

					, {CPropertyKey(mp_StringCache, EPropertyType_Dependency, gc_ConstString_Indirect), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Dependency, gc_ConstString_IndirectOrdered), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Dependency, gc_ConstString_Link), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, true))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Dependency, gc_ConstString_Target), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(gc_ConstKey_Dependency_Type), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(gc_ConstKey_Dependency_Name), DMibBuildSystemTypeWithPosition(g_String)}
					,
					{
						CPropertyKey(mp_StringCache, EPropertyType_Dependency, gc_ConstString_TargetProperties)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(g_ObjectWithAny, EJSONType_Object))
					}

					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_SearchPath), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_SystemSearchPath), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_PreprocessorDefines), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_Custom_Message), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_Custom_CommandLine), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_Custom_Inputs), DMibBuildSystemTypeWithPosition(fg_Optional(g_StringArray))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_Custom_WorkingDirectory), DMibBuildSystemTypeWithPosition(fg_Optional(g_String))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_Custom_Outputs), DMibBuildSystemTypeWithPosition(fg_Optional(g_StringArray))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_InitEarly), DMibBuildSystemTypeWithPosition(fg_Optional(g_Boolean))}

					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_TabWidth), DMibBuildSystemTypeWithPosition(fg_Optional(g_Integer))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_IndentWidth), DMibBuildSystemTypeWithPosition(fg_Optional(g_Integer))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Compile, gc_ConstString_UsesTabs), DMibBuildSystemTypeWithPosition(fg_Optional(g_Boolean))}

					, {CPropertyKey(mp_StringCache, EPropertyType_CreateTemplate, gc_ConstString_Name), DMibBuildSystemTypeWithPosition(g_String)}

					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_GeneratedFiles), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_SourceFiles), DMibBuildSystemTypeWithPosition(g_StringArray)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_BuildSystemSourceAbsolute), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_BuildSystemSource), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_GeneratorStateFile), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_BasePathAbsolute), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_MToolExe), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_MalterlibExe), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_BasePathRelativeProject), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_GeneratedBuildSystemDir), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_BasePath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_IntermediateDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_OutputDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_ProjectPath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_Inherit), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Builtin, gc_ConstString_SourceFileName), DMibBuildSystemTypeWithPosition(g_String)}

					, {CPropertyKey(mp_StringCache, EPropertyType_This, gc_ConstString_Identity), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_This, gc_ConstString_IdentityAsAbsolutePath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_This, gc_ConstString_IdentityPath), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_This, gc_ConstString_Type), DMibBuildSystemTypeWithPosition(g_String)}

					, {CPropertyKey(mp_StringCache, EPropertyType_Group, gc_ConstString_HideTargets), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}

					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_Projects), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CacheDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_TempDirectory), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_UpdateCache), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_Verbose), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_VerboseHash), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_FullRebuildVersion), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, "0"))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CacheExcludePatterns), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					,
					{
						CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CacheReplaceContents)
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
						CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CacheDuplicateLines)
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
						CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_Environment)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(CBuildSystemSyntax::CType{CBuildSystemSyntax::CClassType({}, g_String)}, EJSONType_Object))
					}
					,
					{
						CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CacheIgnoreInputs)
						, DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, EJSONType_Array))
					}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_IncludeInHash), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_StringArray, EJSONType_Array))}
					,
					{
						CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_Languages)
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
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_Config), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_IntermediateName), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_Variables), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_ExcludeFromHash), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_SharedTempDirectory), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_Path), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_VariablesWithPaths), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_SystemName), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_SystemProcessor), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CCompiler), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CCompilerTarget), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CxxCompiler), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CxxCompilerTarget), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					,
					{
						CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_ReplacePrefixes)
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
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_SysRoot), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CExternalToolChain), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_CxxExternalToolChain), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Import, gc_ConstString_CMake_AlwaysFullRebuild), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, true))}

					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_Name), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_Contents), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_UnicodeBOM), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, true))}
					, {CPropertyKey(gc_ConstKey_GenerateFile_BeforeImports), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_UTF16),  DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_Executable), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_UnixLineEnds), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_Symlink), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_SymlinkDirectory), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_NoDateCheck), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_KeepGeneratedFile), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_SymlinkBasePath), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_GenerateFile, gc_ConstString_Group), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}

					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_Location), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_ConfigFile), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_StateFile), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_URL), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_DefaultBranch), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_DefaultUpstreamBranch), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_Tags), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_Submodule), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_ExcludeFromSeen), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_SubmoduleName), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_Type), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_UserName), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_UserEmail), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_String, ""))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_ProtectedBranches), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_ProtectedTags), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_UpdateSubmodules), DMibBuildSystemTypeWithPosition(fg_Defaulted(g_Boolean, false))}
					, {CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_NoPushRemotes), DMibBuildSystemTypeWithPosition(g_StringArrayDefaultedEmpty)}
					,
					{
						CPropertyKey(mp_StringCache, EPropertyType_Repository, gc_ConstString_Remotes)
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
