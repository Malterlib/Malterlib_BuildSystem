// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_ConstantKeys.h"
#include "Malterlib_BuildSystem_ConstantStrings.hpp"

namespace NMib::NBuildSystem
{
	constexpr CPropertyKeyReference const gc_ConstKey_Architecture(EPropertyType_Property, gc_Str<"Architecture">);
	constexpr CPropertyKeyReference const gc_ConstKey_ExcludeFiles(EPropertyType_Property, gc_Str<"ExcludeFiles">);
	constexpr CPropertyKeyReference const gc_ConstKey_FullConfiguration(EPropertyType_Property, gc_Str<"FullConfiguration">);
	constexpr CPropertyKeyReference const gc_ConstKey_MalterlibRepositoryEditor(EPropertyType_Property, gc_Str<"MalterlibRepositoryEditor">);
	constexpr CPropertyKeyReference const gc_ConstKey_MalterlibRepositoryEditorSequential(EPropertyType_Property, gc_Str<"MalterlibRepositoryEditorSequential">);
	constexpr CPropertyKeyReference const gc_ConstKey_MalterlibRepositoryEditorSleep(EPropertyType_Property, gc_Str<"MalterlibRepositoryEditorSleep">);
	constexpr CPropertyKeyReference const gc_ConstKey_MalterlibRepositoryEditorWorkingDir(EPropertyType_Property, gc_Str<"MalterlibRepositoryEditorWorkingDir">);
	constexpr CPropertyKeyReference const gc_ConstKey_Platform(EPropertyType_Property, gc_Str<"Platform">);
	constexpr CPropertyKeyReference const gc_ConstKey_Disabled(EPropertyType_Property, gc_Str<"Disabled">);
	constexpr CPropertyKeyReference const gc_ConstKey_HiddenGroup(EPropertyType_Property, gc_Str<"HiddenGroup">);
	constexpr CPropertyKeyReference const gc_ConstKey_MToolVersion(EPropertyType_Property, gc_Str<"MToolVersion">);
	constexpr CPropertyKeyReference const gc_ConstKey_Generator(EPropertyType_Property, gc_Str<"Generator">);
	constexpr CPropertyKeyReference const gc_ConstKey_GeneratorFamily(EPropertyType_Property, gc_Str<"GeneratorFamily">);
	constexpr CPropertyKeyReference const gc_ConstKey_BuildSystemBasePath(EPropertyType_Property, gc_Str<"BuildSystemBasePath">);
	constexpr CPropertyKeyReference const gc_ConstKey_BuildSystemOutputDir(EPropertyType_Property, gc_Str<"BuildSystemOutputDir">);
	constexpr CPropertyKeyReference const gc_ConstKey_BuildSystemFile(EPropertyType_Property, gc_Str<"BuildSystemFile">);
	constexpr CPropertyKeyReference const gc_ConstKey_BuildSystemName(EPropertyType_Property, gc_Str<"BuildSystemName">);
	constexpr CPropertyKeyReference const gc_ConstKey_HostPlatform(EPropertyType_Property, gc_Str<"HostPlatform">);
	constexpr CPropertyKeyReference const gc_ConstKey_HostPlatformFamily(EPropertyType_Property, gc_Str<"HostPlatformFamily">);
	constexpr CPropertyKeyReference const gc_ConstKey_HostArchitecture(EPropertyType_Property, gc_Str<"HostArchitecture">);
	constexpr CPropertyKeyReference const gc_ConstKey_Configuration(EPropertyType_Property, gc_Str<"Configuration">);
	constexpr CPropertyKeyReference const gc_ConstKey_AllRepositories(EPropertyType_Property, gc_Str<"AllRepositories">);

	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_GeneratedFiles(EPropertyType_Builtin, gc_Str<"GeneratedFiles">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_SourceFiles(EPropertyType_Builtin, gc_Str<"SourceFiles">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_BuildSystemSourceAbsolute(EPropertyType_Builtin, gc_Str<"BuildSystemSourceAbsolute">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_BuildSystemSource(EPropertyType_Builtin, gc_Str<"BuildSystemSource">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_GeneratorStateFile(EPropertyType_Builtin, gc_Str<"GeneratorStateFile">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_BasePathAbsolute(EPropertyType_Builtin, gc_Str<"BasePathAbsolute">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_MToolExe(EPropertyType_Builtin, gc_Str<"MToolExe">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_MalterlibExe(EPropertyType_Builtin, gc_Str<"MalterlibExe">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_BasePathRelativeProject(EPropertyType_Builtin, gc_Str<"BasePathRelativeProject">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_GeneratedBuildSystemDir(EPropertyType_Builtin, gc_Str<"GeneratedBuildSystemDir">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_BasePath(EPropertyType_Builtin, gc_Str<"BasePath">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_IntermediateDirectory(EPropertyType_Builtin, gc_Str<"IntermediateDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_OutputDirectory(EPropertyType_Builtin, gc_Str<"OutputDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_ProjectPath(EPropertyType_Builtin, gc_Str<"ProjectPath">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_Inherit(EPropertyType_Builtin, gc_Str<"Inherit">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_SourceFileName(EPropertyType_Builtin, gc_Str<"SourceFileName">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_CMakeRoot(EPropertyType_Builtin, gc_Str<"CMakeRoot">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_ReadFile(EPropertyType_Builtin, gc_Str<"ReadFile">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_FileExists(EPropertyType_Builtin, gc_Str<"FileExists">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_LinkExists(EPropertyType_Builtin, gc_Str<"LinkExists">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_ResolveSymbolicLink(EPropertyType_Builtin, gc_Str<"ResolveSymbolicLink">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_DirectoryExists(EPropertyType_Builtin, gc_Str<"DirectoryExists">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_FileOrDirectoryExists(EPropertyType_Builtin, gc_Str<"FileOrDirectoryExists">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_FindFilesIn(EPropertyType_Builtin, gc_Str<"FindFilesIn">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_FindDirectoriesIn(EPropertyType_Builtin, gc_Str<"FindDirectoriesIn">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_FindFilesRecursiveIn(EPropertyType_Builtin, gc_Str<"FindFilesRecursiveIn">);
	constexpr CPropertyKeyReference const gc_ConstKey_Builtin_FindDirectoriesRecursiveIn(EPropertyType_Builtin, gc_Str<"FindDirectoriesRecursiveIn">);

	constexpr CPropertyKeyReference const gc_ConstKey_This_Identity(EPropertyType_This, gc_Str<"Identity">);
	constexpr CPropertyKeyReference const gc_ConstKey_This_IdentityAsAbsolutePath(EPropertyType_This, gc_Str<"IdentityAsAbsolutePath">);
	constexpr CPropertyKeyReference const gc_ConstKey_This_IdentityPath(EPropertyType_This, gc_Str<"IdentityPath">);
	constexpr CPropertyKeyReference const gc_ConstKey_This_EntityPath(EPropertyType_This, gc_Str<"EntityPath">);
	constexpr CPropertyKeyReference const gc_ConstKey_This_Type(EPropertyType_This, gc_Str<"Type">);

	constexpr CPropertyKeyReference const gc_ConstKey_Compile_AllowNonExisting(EPropertyType_Compile, gc_Str<"AllowNonExisting">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_Custom_CommandLine(EPropertyType_Compile, gc_Str<"Custom_CommandLine">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_Custom_Inputs(EPropertyType_Compile, gc_Str<"Custom_Inputs">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_Custom_Outputs(EPropertyType_Compile, gc_Str<"Custom_Outputs">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_Custom_WorkingDirectory(EPropertyType_Compile, gc_Str<"Custom_WorkingDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_Disabled(EPropertyType_Compile, gc_Str<"Disabled">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_GenericCompileType(EPropertyType_Compile, gc_Str<"GenericCompileType">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_InitEarly(EPropertyType_Compile, gc_Str<"InitEarly">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_PrecompilePrefixHeader(EPropertyType_Compile, gc_Str<"PrecompilePrefixHeader">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_XInternalPrecompiledHeaderOutputFile(EPropertyType_Compile, gc_Str<"XInternalPrecompiledHeaderOutputFile">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_PrefixHeader(EPropertyType_Compile, gc_Str<"PrefixHeader">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_Type(EPropertyType_Compile, gc_Str<"Type">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_SearchPath(EPropertyType_Compile, gc_Str<"SearchPath">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_SystemSearchPath(EPropertyType_Compile, gc_Str<"SystemSearchPath">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_PreprocessorDefines(EPropertyType_Compile, gc_Str<"PreprocessorDefines">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_Custom_Message(EPropertyType_Compile, gc_Str<"Custom_Message">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_TabWidth(EPropertyType_Compile, gc_Str<"TabWidth">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_IndentWidth(EPropertyType_Compile, gc_Str<"IndentWidth">);
	constexpr CPropertyKeyReference const gc_ConstKey_Compile_UsesTabs(EPropertyType_Compile, gc_Str<"UsesTabs">);

	constexpr CPropertyKeyReference const gc_ConstKey_CreateTemplate_Name(EPropertyType_CreateTemplate, gc_Str<"Name">);

	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_Indirect(EPropertyType_Dependency, gc_Str<"Indirect">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_IndirectOrdered(EPropertyType_Dependency, gc_Str<"IndirectOrdered">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_IndirectEvaluated(EPropertyType_Dependency, gc_Str<"IndirectEvaluated">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_IndirectOrderedEvaluated(EPropertyType_Dependency, gc_Str<"IndirectOrderedEvaluated">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_TargetPropertyPath(EPropertyType_Dependency, gc_Str<"TargetPropertyPath">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_Link(EPropertyType_Dependency, gc_Str<"Link">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_Type(EPropertyType_Dependency, gc_Str<"Type">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_Name(EPropertyType_Dependency, gc_Str<"Name">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_Target(EPropertyType_Dependency, gc_Str<"Target">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_TargetProperties(EPropertyType_Dependency, gc_Str<"TargetProperties">);
	constexpr CPropertyKeyReference const gc_ConstKey_Dependency_ObjectLibrary(EPropertyType_Dependency, gc_Str<"ObjectLibrary">);

	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_BeforeImports(EPropertyType_GenerateFile, gc_Str<"BeforeImports">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_Contents(EPropertyType_GenerateFile, gc_Str<"Contents">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_Executable(EPropertyType_GenerateFile, gc_Str<"Executable">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_Group(EPropertyType_GenerateFile, gc_Str<"Group">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_KeepGeneratedFile(EPropertyType_GenerateFile, gc_Str<"KeepGeneratedFile">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_Name(EPropertyType_GenerateFile, gc_Str<"Name">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_NoDateCheck(EPropertyType_GenerateFile, gc_Str<"NoDateCheck">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_Symlink(EPropertyType_GenerateFile, gc_Str<"Symlink">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_SymlinkBasePath(EPropertyType_GenerateFile, gc_Str<"SymlinkBasePath">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_SymlinkDirectory(EPropertyType_GenerateFile, gc_Str<"SymlinkDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_UnicodeBOM(EPropertyType_GenerateFile, gc_Str<"UnicodeBOM">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_UnixLineEnds(EPropertyType_GenerateFile, gc_Str<"UnixLineEnds">);
	constexpr CPropertyKeyReference const gc_ConstKey_GenerateFile_UTF16(EPropertyType_GenerateFile, gc_Str<"UTF16">);

	constexpr CPropertyKeyReference const gc_ConstKey_GeneratorSetting_Workspace(EPropertyType_GeneratorSetting, gc_Str<"Workspace">);
	constexpr CPropertyKeyReference const gc_ConstKey_GeneratorSetting_Target(EPropertyType_GeneratorSetting, gc_Str<"Target">);
	constexpr CPropertyKeyReference const gc_ConstKey_GeneratorSetting_Dependency(EPropertyType_GeneratorSetting, gc_Str<"Dependency">);
	constexpr CPropertyKeyReference const gc_ConstKey_GeneratorSetting_Compile(EPropertyType_GeneratorSetting, gc_Str<"Compile">);

	constexpr CPropertyKeyReference const gc_ConstKey_GeneratorSetting_DefinedProperties(EPropertyType_GeneratorSetting, gc_Str<"DefinedProperties">);
	constexpr CPropertyKeyReference const gc_ConstKey_GeneratorSetting_Xcode_TargetType(EPropertyType_GeneratorSetting, gc_Str<"Xcode_TargetType">);
	constexpr CPropertyKeyReference const gc_ConstKey_GeneratorSetting_IsFile(EPropertyType_GeneratorSetting, gc_Str<"IsFile">);

	constexpr CPropertyKeyReference const gc_ConstKey_Group_HideTargets(EPropertyType_Group, gc_Str<"HideTargets">);

	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_AlwaysFullRebuild(EPropertyType_Import, gc_Str<"CMake_AlwaysFullRebuild">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CacheDirectory(EPropertyType_Import, gc_Str<"CMake_CacheDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CacheDuplicateLines(EPropertyType_Import, gc_Str<"CMake_CacheDuplicateLines">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CacheExcludePatterns(EPropertyType_Import, gc_Str<"CMake_CacheExcludePatterns">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CacheExcludeDependenciesPatterns(EPropertyType_Import, gc_Str<"CMake_CacheExcludeDependenciesPatterns">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CacheIgnoreInputs(EPropertyType_Import, gc_Str<"CMake_CacheIgnoreInputs">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CacheReplaceContents(EPropertyType_Import, gc_Str<"CMake_CacheReplaceContents">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CCompiler(EPropertyType_Import, gc_Str<"CMake_CCompiler">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CCompilerTarget(EPropertyType_Import, gc_Str<"CMake_CCompilerTarget">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CExternalToolChain(EPropertyType_Import, gc_Str<"CMake_CExternalToolChain">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_Config(EPropertyType_Import, gc_Str<"CMake_Config">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CxxCompiler(EPropertyType_Import, gc_Str<"CMake_CxxCompiler">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CxxCompilerTarget(EPropertyType_Import, gc_Str<"CMake_CxxCompilerTarget">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_OutputFilesParseTerminators(EPropertyType_Import, gc_Str<"CMake_OutputFilesParseTerminators">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_CxxExternalToolChain(EPropertyType_Import, gc_Str<"CMake_CxxExternalToolChain">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_DiffHash(EPropertyType_Import, gc_Str<"CMake_DiffHash">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_Environment(EPropertyType_Import, gc_Str<"CMake_Environment">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_ExcludeFromHash(EPropertyType_Import, gc_Str<"CMake_ExcludeFromHash">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_DisableIncludeReplace(EPropertyType_Import, gc_Str<"CMake_DisableIncludeReplace">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_FullRebuildVersion(EPropertyType_Import, gc_Str<"CMake_FullRebuildVersion">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_IncludeInHash(EPropertyType_Import, gc_Str<"CMake_IncludeInHash">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_IntermediateName(EPropertyType_Import, gc_Str<"CMake_IntermediateName">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_Languages(EPropertyType_Import, gc_Str<"CMake_Languages">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_Path(EPropertyType_Import, gc_Str<"CMake_Path">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_Projects(EPropertyType_Import, gc_Str<"CMake_Projects">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_ReplacePrefixes(EPropertyType_Import, gc_Str<"CMake_ReplacePrefixes">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_SysRoot(EPropertyType_Import, gc_Str<"CMake_SysRoot">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_SystemName(EPropertyType_Import, gc_Str<"CMake_SystemName">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_SystemProcessor(EPropertyType_Import, gc_Str<"CMake_SystemProcessor">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_UpdateCache(EPropertyType_Import, gc_Str<"CMake_UpdateCache">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_Variables(EPropertyType_Import, gc_Str<"CMake_Variables">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_VariablesWithPaths(EPropertyType_Import, gc_Str<"CMake_VariablesWithPaths">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_Verbose(EPropertyType_Import, gc_Str<"CMake_Verbose">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_CMake_VerboseHash(EPropertyType_Import, gc_Str<"CMake_VerboseHash">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_SharedTempDirectory(EPropertyType_Import, gc_Str<"SharedTempDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Import_TempDirectory(EPropertyType_Import, gc_Str<"TempDirectory">);

	constexpr CPropertyKeyReference const gc_ConstKey_Repository_ConfigFile(EPropertyType_Repository, gc_Str<"ConfigFile">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_DefaultBranch(EPropertyType_Repository, gc_Str<"DefaultBranch">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_DefaultUpstreamBranch(EPropertyType_Repository, gc_Str<"DefaultUpstreamBranch">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_ExcludeFromSeen(EPropertyType_Repository, gc_Str<"ExcludeFromSeen">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_Location(EPropertyType_Repository, gc_Str<"Location">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_NoPushRemotes(EPropertyType_Repository, gc_Str<"NoPushRemotes">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_ProtectedBranches(EPropertyType_Repository, gc_Str<"ProtectedBranches">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_ProtectedTags(EPropertyType_Repository, gc_Str<"ProtectedTags">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_Remotes(EPropertyType_Repository, gc_Str<"Remotes">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_StateFile(EPropertyType_Repository, gc_Str<"StateFile">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_Submodule(EPropertyType_Repository, gc_Str<"Submodule">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_SubmoduleName(EPropertyType_Repository, gc_Str<"SubmoduleName">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_Tags(EPropertyType_Repository, gc_Str<"Tags">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_Type(EPropertyType_Repository, gc_Str<"Type">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_UpdateSubmodules(EPropertyType_Repository, gc_Str<"UpdateSubmodules">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_URL(EPropertyType_Repository, gc_Str<"URL">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_UserEmail(EPropertyType_Repository, gc_Str<"UserEmail">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_UserName(EPropertyType_Repository, gc_Str<"UserName">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_LfsReleaseStore(EPropertyType_Repository, gc_Str<"LfsReleaseStore">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_TagPreviousOnForcePush(EPropertyType_Repository, gc_Str<"TagPreviousOnForcePush">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_ExtraFetchSpecs(EPropertyType_Repository, gc_Str<"ExtraFetchSpecs">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_Policy(EPropertyType_Repository, gc_Str<"Policy">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_ReleasePackage(EPropertyType_Repository, gc_Str<"ReleasePackage">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_ApplyPolicy(EPropertyType_Repository, gc_Str<"ApplyPolicy">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_ApplyPolicyPretend(EPropertyType_Repository, gc_Str<"ApplyPolicyPretend">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_BootstrapSource(EPropertyType_Repository, gc_Str<"BootstrapSource">);
	constexpr CPropertyKeyReference const gc_ConstKey_Repository_GitIgnoreType(EPropertyType_Repository, gc_Str<"GitIgnoreType">);

	constexpr CPropertyKeyReference const gc_ConstKey_Target_ClCompileSuffix(EPropertyType_Target, gc_Str<"ClCompileSuffix">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_DependencyFile(EPropertyType_Target, gc_Str<"DependencyFile">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_DependencyInjectionGroups(EPropertyType_Target, gc_Str<"DependencyInjectionGroups">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_Disabled(EPropertyType_Target, gc_Str<"Disabled">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_EnableLinkerGroups(EPropertyType_Target, gc_Str<"EnableLinkerGroups">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_ExportScriptEnvironmentContents(EPropertyType_Target, gc_Str<"ExportScriptEnvironmentContents">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_ExternalDependencies(EPropertyType_Target, gc_Str<"ExternalDependencies">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_ExtraGroups(EPropertyType_Target, gc_Str<"ExtraGroups">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_InjectedExtraGroups(EPropertyType_Target, gc_Str<"InjectedExtraGroups">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_FileExtension(EPropertyType_Target, gc_Str<"FileExtension">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_FileName(EPropertyType_Target, gc_Str<"FileName">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_FollowIndirectDependencies(EPropertyType_Target, gc_Str<"FollowIndirectDependencies">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_GenerateScheme(EPropertyType_Target, gc_Str<"GenerateScheme">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_Group(EPropertyType_Target, gc_Str<"Group">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_LinkerGroup(EPropertyType_Target, gc_Str<"LinkerGroup">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_Name(EPropertyType_Target, gc_Str<"Name">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_ObjectLibrary(EPropertyType_Target, gc_Str<"ObjectLibrary">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_OutputDirectory(EPropertyType_Target, gc_Str<"OutputDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_PlatformVersion(EPropertyType_Target, gc_Str<"PlatformVersion">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_TargetFrameworkVersion(EPropertyType_Target, gc_Str<"TargetFrameworkVersion">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_VisualStudioPlatform(EPropertyType_Target, gc_Str<"VisualStudioPlatform">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_DependenciesNames(EPropertyType_Target, gc_Str<"DependenciesNames">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_Dependencies(EPropertyType_Target, gc_Str<"Dependencies">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_DependencyTargets(EPropertyType_Target, gc_Str<"DependencyTargets">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_AllFiles(EPropertyType_Target, gc_Str<"AllFiles">);

	constexpr CPropertyKeyReference const gc_ConstKey_Target_Type(EPropertyType_Target, gc_Str<"Type">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_Language(EPropertyType_Target, gc_Str<"Language">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_IntermediateDirectory(EPropertyType_Target, gc_Str<"IntermediateDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_PostBuildScriptOutputs(EPropertyType_Target, gc_Str<"PostBuildScriptOutputs">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_PostBuildScriptInputs(EPropertyType_Target, gc_Str<"PostBuildScriptInputs">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_PreBuildScriptOutputs(EPropertyType_Target, gc_Str<"PreBuildScriptOutputs">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_PreBuildScriptInputs(EPropertyType_Target, gc_Str<"PreBuildScriptInputs">);
	constexpr CPropertyKeyReference const gc_ConstKey_Target_LinkWithLibraries(EPropertyType_Target, gc_Str<"LinkWithLibraries">);

	constexpr CPropertyKeyReference const gc_ConstKey_Workspace_Enabled(EPropertyType_Workspace, gc_Str<"Enabled">);
	constexpr CPropertyKeyReference const gc_ConstKey_Workspace_ExtraGroups(EPropertyType_Workspace, gc_Str<"ExtraGroups">);
	constexpr CPropertyKeyReference const gc_ConstKey_Workspace_Name(EPropertyType_Workspace, gc_Str<"Name">);
	constexpr CPropertyKeyReference const gc_ConstKey_Workspace_XcodeNewBuildSystem(EPropertyType_Workspace, gc_Str<"XcodeNewBuildSystem">);
	constexpr CPropertyKeyReference const gc_ConstKey_Workspace_IntermediateDirectory(EPropertyType_Workspace, gc_Str<"IntermediateDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Workspace_OutputDirectory(EPropertyType_Workspace, gc_Str<"OutputDirectory">);
	constexpr CPropertyKeyReference const gc_ConstKey_Workspace_AllTargets(EPropertyType_Workspace, gc_Str<"AllTargets">);

	constexpr CPropertyKeyReference const gc_ConstKeys_FullEval[EPropertyType_Max] =
		{
			CPropertyKeyReference(EPropertyType_Invalid, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Property, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Compile, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Target, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Workspace, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Dependency, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Import, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Repository, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_CreateTemplate, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Group, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_This, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Builtin, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_Type, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_GenerateFile, gc_ConstString_FullEval)
			, CPropertyKeyReference(EPropertyType_GeneratorSetting, gc_ConstString_FullEval)
		}
	;

	void fg_CacheConstantKeys(CStringCache &o_StringCache)
	{
		DMibLock(o_StringCache.m_Lock);
		o_StringCache.f_AddConstantString(gc_ConstKey_Architecture);
		o_StringCache.f_AddConstantString(gc_ConstKey_ExcludeFiles);
		o_StringCache.f_AddConstantString(gc_ConstKey_FullConfiguration);
		o_StringCache.f_AddConstantString(gc_ConstKey_MalterlibRepositoryEditor);
		o_StringCache.f_AddConstantString(gc_ConstKey_MalterlibRepositoryEditorSequential);
		o_StringCache.f_AddConstantString(gc_ConstKey_MalterlibRepositoryEditorSleep);
		o_StringCache.f_AddConstantString(gc_ConstKey_MalterlibRepositoryEditorWorkingDir);
		o_StringCache.f_AddConstantString(gc_ConstKey_Platform);
		o_StringCache.f_AddConstantString(gc_ConstKey_Disabled);
		o_StringCache.f_AddConstantString(gc_ConstKey_HiddenGroup);
		o_StringCache.f_AddConstantString(gc_ConstKey_MToolVersion);
		o_StringCache.f_AddConstantString(gc_ConstKey_Generator);
		o_StringCache.f_AddConstantString(gc_ConstKey_GeneratorFamily);
		o_StringCache.f_AddConstantString(gc_ConstKey_BuildSystemBasePath);
		o_StringCache.f_AddConstantString(gc_ConstKey_BuildSystemOutputDir);
		o_StringCache.f_AddConstantString(gc_ConstKey_BuildSystemFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_BuildSystemName);
		o_StringCache.f_AddConstantString(gc_ConstKey_HostPlatform);
		o_StringCache.f_AddConstantString(gc_ConstKey_HostPlatformFamily);
		o_StringCache.f_AddConstantString(gc_ConstKey_HostArchitecture);
		o_StringCache.f_AddConstantString(gc_ConstKey_Configuration);
		o_StringCache.f_AddConstantString(gc_ConstKey_AllRepositories);

		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_GeneratedFiles);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_SourceFiles);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_BuildSystemSourceAbsolute);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_BuildSystemSource);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_GeneratorStateFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_BasePathAbsolute);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_MToolExe);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_MalterlibExe);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_BasePathRelativeProject);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_GeneratedBuildSystemDir);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_BasePath);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_IntermediateDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_OutputDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_ProjectPath);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_Inherit);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_SourceFileName);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_CMakeRoot);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_ReadFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_FileExists);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_LinkExists);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_ResolveSymbolicLink);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_DirectoryExists);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_FileOrDirectoryExists);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_FindFilesIn);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_FindDirectoriesIn);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_FindFilesRecursiveIn);
		o_StringCache.f_AddConstantString(gc_ConstKey_Builtin_FindDirectoriesRecursiveIn);

		o_StringCache.f_AddConstantString(gc_ConstKey_This_Identity);
		o_StringCache.f_AddConstantString(gc_ConstKey_This_IdentityAsAbsolutePath);
		o_StringCache.f_AddConstantString(gc_ConstKey_This_IdentityPath);
		o_StringCache.f_AddConstantString(gc_ConstKey_This_EntityPath);
		o_StringCache.f_AddConstantString(gc_ConstKey_This_Type);

		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_AllowNonExisting);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_Custom_CommandLine);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_Custom_Inputs);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_Custom_Outputs);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_Custom_WorkingDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_Disabled);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_GenericCompileType);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_InitEarly);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_PrecompilePrefixHeader);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_XInternalPrecompiledHeaderOutputFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_PrefixHeader);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_Type);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_SearchPath);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_SystemSearchPath);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_PreprocessorDefines);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_Custom_Message);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_TabWidth);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_IndentWidth);
		o_StringCache.f_AddConstantString(gc_ConstKey_Compile_UsesTabs);

		o_StringCache.f_AddConstantString(gc_ConstKey_CreateTemplate_Name);

		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_Indirect);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_IndirectOrdered);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_IndirectEvaluated);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_IndirectOrderedEvaluated);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_TargetPropertyPath);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_Link);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_Type);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_Name);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_Target);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_TargetProperties);
		o_StringCache.f_AddConstantString(gc_ConstKey_Dependency_ObjectLibrary);

		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_BeforeImports);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_Contents);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_Executable);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_Group);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_KeepGeneratedFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_Name);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_NoDateCheck);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_Symlink);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_SymlinkBasePath);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_SymlinkDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_UnicodeBOM);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_UnixLineEnds);
		o_StringCache.f_AddConstantString(gc_ConstKey_GenerateFile_UTF16);

		o_StringCache.f_AddConstantString(gc_ConstKey_GeneratorSetting_Workspace);
		o_StringCache.f_AddConstantString(gc_ConstKey_GeneratorSetting_Target);
		o_StringCache.f_AddConstantString(gc_ConstKey_GeneratorSetting_Compile);
		o_StringCache.f_AddConstantString(gc_ConstKey_GeneratorSetting_Dependency);
		o_StringCache.f_AddConstantString(gc_ConstKey_GeneratorSetting_DefinedProperties);
		o_StringCache.f_AddConstantString(gc_ConstKey_GeneratorSetting_IsFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_GeneratorSetting_Xcode_TargetType);

		o_StringCache.f_AddConstantString(gc_ConstKey_Group_HideTargets);

		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_AlwaysFullRebuild);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CacheDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CacheDuplicateLines);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CacheExcludePatterns);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CacheExcludeDependenciesPatterns);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CacheIgnoreInputs);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CacheReplaceContents);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CCompiler);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CCompilerTarget);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CExternalToolChain);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_Config);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CxxCompiler);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CxxCompilerTarget);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_OutputFilesParseTerminators);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_CxxExternalToolChain);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_DiffHash);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_Environment);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_ExcludeFromHash);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_DisableIncludeReplace);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_FullRebuildVersion);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_IncludeInHash);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_IntermediateName);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_Languages);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_Path);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_Projects);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_ReplacePrefixes);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_SysRoot);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_SystemName);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_SystemProcessor);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_UpdateCache);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_Variables);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_VariablesWithPaths);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_Verbose);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_CMake_VerboseHash);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_SharedTempDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Import_TempDirectory);

		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_ConfigFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_DefaultBranch);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_DefaultUpstreamBranch);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_ExcludeFromSeen);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_Location);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_NoPushRemotes);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_ProtectedBranches);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_ProtectedTags);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_Remotes);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_StateFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_Submodule);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_SubmoduleName);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_Tags);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_Type);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_UpdateSubmodules);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_URL);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_UserEmail);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_UserName);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_LfsReleaseStore);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_TagPreviousOnForcePush);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_ExtraFetchSpecs);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_Policy);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_ReleasePackage);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_ApplyPolicy);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_ApplyPolicyPretend);
		o_StringCache.f_AddConstantString(gc_ConstKey_Repository_BootstrapSource);

		o_StringCache.f_AddConstantString(gc_ConstKey_Target_ClCompileSuffix);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_DependencyFile);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_DependencyInjectionGroups);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_Disabled);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_EnableLinkerGroups);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_ExportScriptEnvironmentContents);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_ExternalDependencies);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_ExtraGroups);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_InjectedExtraGroups);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_FileExtension);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_FileName);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_FollowIndirectDependencies);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_GenerateScheme);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_Group);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_LinkerGroup);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_Name);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_ObjectLibrary);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_OutputDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_PlatformVersion);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_TargetFrameworkVersion);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_VisualStudioPlatform);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_DependenciesNames);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_Dependencies);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_DependencyTargets);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_Type);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_Language);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_IntermediateDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_PostBuildScriptOutputs);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_PostBuildScriptInputs);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_PreBuildScriptOutputs);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_PreBuildScriptInputs);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_LinkWithLibraries);
		o_StringCache.f_AddConstantString(gc_ConstKey_Target_AllFiles);

		o_StringCache.f_AddConstantString(gc_ConstKey_Workspace_Enabled);
		o_StringCache.f_AddConstantString(gc_ConstKey_Workspace_ExtraGroups);
		o_StringCache.f_AddConstantString(gc_ConstKey_Workspace_Name);
		o_StringCache.f_AddConstantString(gc_ConstKey_Workspace_XcodeNewBuildSystem);
		o_StringCache.f_AddConstantString(gc_ConstKey_Workspace_IntermediateDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Workspace_OutputDirectory);
		o_StringCache.f_AddConstantString(gc_ConstKey_Workspace_AllTargets);

		for (auto &Key : gc_ConstKeys_FullEval)
			o_StringCache.f_AddConstantString(Key);
	}
}
