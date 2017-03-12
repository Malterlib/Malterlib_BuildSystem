// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

#include <Mib/Process/ProcessLaunch>
#include <Mib/File/MalterlibFS>
#include <Mib/File/VirtualFSs/MalterlibFS>
#include <Mib/Cryptography/Hashes/SHA>

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_ExpandDynamicImports(CBuildSystemData &_BuildSystemData) const
	{
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
					if (Key.m_Type == EEntityType_Root)
					{
						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_Import)
						continue;

					if (!fp_ExpandEntity(Child, _Entity, nullptr))
					{
						continue;
					}

					_Entity.m_ChildEntitiesMap.f_Remove(Key);
				}
			}
		;
		fExpandEntities(_BuildSystemData.m_RootEntity);

		TCFunction<void (CEntity &_Entity)> fExpandImports
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
					if (Key.m_Type == EEntityType_Root)
					{
						fExpandImports(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_Import)
						continue;

					fp_ExpandImport(Child, _Entity, _BuildSystemData);
				}
			}
		;
		
		fExpandImports(_BuildSystemData.m_RootEntity);
		{
			DMibLock(mp_SourceFilesLock);
			mp_SourceFiles += _BuildSystemData.m_SourceFiles;
		}
	}
	
	CBuildSystemData::CImportData *CBuildSystem::fp_ExpandImportCMake_FromGeneratedDirectory
		(
			CEntity &_Entity
			, CEntity &_ParentEntity
			, CBuildSystemData &_BuildSystemData
			, CStr const &_Directory
		) const
	{
		CStr Projects = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Projects");
		if (Projects.f_IsEmpty())
		{
			auto ProjectFiles = CFile::fs_FindFiles(fg_Format("{}/*.MHeader", _Directory));
			if (ProjectFiles.f_IsEmpty())
				DMibError(fg_Format("No MHeader files found in CMake generated directory"));
			for (auto &File : ProjectFiles)
				fg_AddStrSep(Projects, CFile::fs_GetFileNoExt(File), ";");
		}

		auto &Import = _BuildSystemData.m_Imports.f_Insert();
		
		{
			CBuildSystemPreprocessor Preprocessor(Import.m_Registry, _BuildSystemData.m_SourceFiles, mp_FindCache);
			while (!Projects.f_IsEmpty())
			{
				CStr Project = fg_GetStrSep(Projects, ";");
				
				CStr ProjectFileName = fg_Format("{}/{}.MHeader", _Directory, Project);
				
				Preprocessor.f_ReadFile(ProjectFileName);
				_BuildSystemData.m_SourceFiles[ProjectFileName];
				
				CStr Dependencies = CFile::fs_ReadStringFromFile(fg_Format("{}/{}.MHeader.dependencies", _Directory, Project), true);
				
				TCSet<CStr> SourceFileToAdd;
				ch8 const *pParse = Dependencies.f_GetStr();
				while (*pParse)
				{
					ch8 const *pStart = pParse;
					fg_ParseToEndOfLine(pParse);
					SourceFileToAdd[CFile::fs_GetExpandedPath(CStr(pStart, pParse - pStart), _Directory)];
					fg_ParseEndOfLine(pParse);
				}				
				
				{
					DMibLock(mp_SourceFilesLock);
					mp_SourceFiles += SourceFileToAdd;
				}
			}
		}
		return &Import;
	}
	
	CBuildSystemData::CImportData *CBuildSystem::fp_ExpandImportCMake(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const
	{
		CStr CmakeCacheDirectory = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CacheDirectory");

		CStr LockDirectory = CmakeCacheDirectory;
		CStr TempDirectory = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "TempDirectory");
		
		if (LockDirectory.f_IsEmpty())
			LockDirectory = TempDirectory;

		bool bUpdateCache = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_UpdateCache") == "true";
		
		// Dependent variables
		CStr GeneratorVersion = "11";
		CStr FullRebuildVersion = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_FullRebuildVersion");
		CStr CacheExcludePatterns = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CacheExcludePatterns");
		CStr CacheReplaceContents = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CacheReplaceContents");
		CStr CmakeLanguages = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Languages");
		CStr CmakeConfig = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Config");
		CStr CmakeVariables = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Variables");

		CStr HashContents = fg_Format("Config (Not checked): {}\n", f_EvaluateEntityProperty(_Entity, EPropertyType_Property, "FullConfiguration"));
		auto fAddStringHash = [&](CHash_SHA512 &o_DependenciesHash, CStr const &_String, ch8 const *_pVariableName)
			{
				HashContents += CStr::CFormat("{}: {}\n") << _pVariableName << _String;
				//DMibConOut2("{} : {}\n", CmakeCacheDirectory, _String);
				o_DependenciesHash.f_AddData(_String.f_GetStr(), _String.f_GetLen());
			}
		;
		
		auto fInitHash = [&](CHash_SHA512 &o_DependenciesHash)
			{
				fAddStringHash(o_DependenciesHash, GeneratorVersion, "GeneratorVersion");
				fAddStringHash(o_DependenciesHash, FullRebuildVersion, "Import.CMake_FullRebuildVersion");
				fAddStringHash(o_DependenciesHash, CacheExcludePatterns, "Import.CMake_CacheExcludePatterns");
				fAddStringHash(o_DependenciesHash, CacheReplaceContents, "Import.CMake_CacheReplaceContents");
				fAddStringHash(o_DependenciesHash, CmakeLanguages, "Import.CMake_Languages");
				fAddStringHash(o_DependenciesHash, CmakeConfig, "Import.CMake_Config");
				fAddStringHash(o_DependenciesHash, CmakeVariables, "Ipmort.CMake_Variables");
			}
		;
		
		CStr FileName = CFile::fs_GetExpandedPath(_Entity.m_Key.m_Name, CFile::fs_GetPath(_Entity.m_Position.m_FileName));
		
		CStr SharedTempDirectory = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "SharedTempDirectory");
		CStr Platform = f_EvaluateEntityProperty(_Entity, EPropertyType_Property, "Platform");
		CStr Architecture = f_EvaluateEntityProperty(_Entity, EPropertyType_Property, "Architecture");
		CStr CmakePath = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Path");
		CStr CmakeVariablesWithPaths = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_VariablesWithPaths");
		CStr CmakeSystemName = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_SystemName");
		CStr CmakeSystemProcessor = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_SystemProcessor");;
		CStr CmakeCompiler = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CCompiler");
		CStr CmakeCompilerTarget = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CCompilerTarget");
		CStr CmakeCxxCompiler = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CxxCompiler");
		CStr CmakeCxxCompilerTarget = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CxxCompilerTarget");
		CStr CmakeReplacePrefixes = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_ReplacePrefixes");
		
		auto fInitConfigHash = [&](CHash_SHA512 &o_DependenciesHash)
			{
				fInitHash(o_DependenciesHash);
				fAddStringHash(o_DependenciesHash, FileName, "FileName");
				fAddStringHash(o_DependenciesHash, SharedTempDirectory, "Import.SharedTempDirectory");
				fAddStringHash(o_DependenciesHash, Platform, "Platform");
				fAddStringHash(o_DependenciesHash, Architecture, "Architecture");
				fAddStringHash(o_DependenciesHash, CmakePath, "Import.CMake_Path");
				fAddStringHash(o_DependenciesHash, CmakeVariablesWithPaths, "Import.CMake_VariablesWithPaths");
				fAddStringHash(o_DependenciesHash, CmakeSystemName, "Import.CMake_SystemName");
				fAddStringHash(o_DependenciesHash, CmakeSystemProcessor, "Import.CMake_SystemProcessor");
				fAddStringHash(o_DependenciesHash, CmakeCompiler, "Import.CMake_CCompiler");
				fAddStringHash(o_DependenciesHash, CmakeCompilerTarget, "Import.CMake_CCompilerTarget");
				fAddStringHash(o_DependenciesHash, CmakeCxxCompiler, "Import.CMake_CxxCompiler");
				fAddStringHash(o_DependenciesHash, CmakeCxxCompilerTarget, "Import.CMake_CxxCompilerTarget");
			}
		;
		
		CHash_SHA512 ConfigHash;
		fInitConfigHash(ConfigHash);
		CStr ConfigHashString = ConfigHash.f_GetDigest().f_GetString();
		CStr ConfigHashContents = HashContents;
		
		auto fReturn = [&](CStr const &_Directory, CStr const &_Hash)
			{
				if (_Hash.f_IsEmpty())
					fsp_ThrowError(_Entity.m_Position, "CMake generation failed");
				if (_Hash != ConfigHashString)
				{
					fsp_ThrowError
						(
							_Entity.m_Position
							, fg_Format
							(
								"Trying to use same cmake cache directory with different settings {}:\n---\n{}---\n{}"
								, LockDirectory
								, ConfigHashContents
								, mp_CMakeGeneratedContents[LockDirectory]
							)
						)
					;
				}
				return fp_ExpandImportCMake_FromGeneratedDirectory(_Entity, _ParentEntity, _BuildSystemData, _Directory);
			}
		;
		
		CMutual *pCMakeGenerateLock;
		{
			DLock(mp_CMakeGenerateLock);
			auto pHash = mp_CMakeGenerated.f_FindEqual(LockDirectory);
			if (pHash)
				return fReturn(LockDirectory, *pHash);
			pCMakeGenerateLock = &mp_CMakeGenerateLocks[LockDirectory];
		}
		DLock(*pCMakeGenerateLock);
		{
			DLock(mp_CMakeGenerateLock);
			auto pHash = mp_CMakeGenerated.f_FindEqual(LockDirectory);
			if (pHash)
				return fReturn(LockDirectory, *pHash);
		}
		
		auto SetInvalidGenerated = g_OnScopeExit > [&]
			{
				DLock(mp_CMakeGenerateLock);
				mp_CMakeGenerated[LockDirectory];
			}
		;

		if (!CmakeCacheDirectory.f_IsEmpty() && CFile::fs_FileExists(CmakeCacheDirectory + "/Dependencies.sha512"))
		{
			CFile::CFindFilesOptions FindOptions{CmakeCacheDirectory + "/*.dependencies", true};
			FindOptions.m_AttribMask = EFileAttrib_File;

			auto FoundFiles = CFile::fs_FindFiles(FindOptions); 

			TCSet<CStr> DependencyFiles;
			
			for (auto &File : FoundFiles)
			{
				if (CFile::fs_GetExtension(File.m_Path) == "dependencies")
				{
					CStr FileContents = CFile::fs_ReadStringFromFile(File.m_Path, true);
					ch8 const *pParse = FileContents;
					while (*pParse)
					{
						auto pLineStart = pParse;
						fg_ParseToEndOfLine(pParse);
						CStr Line(pLineStart, pParse - pLineStart);
						fg_ParseEndOfLine(pParse);
						DependencyFiles[CFile::fs_GetExpandedPath(Line, CFile::fs_GetPath(File.m_Path))];
					}
				}
			}

			CHash_SHA512 DependenciesHash;
			fInitHash(DependenciesHash);

			for (auto &File : DependencyFiles)
			{
				if (!CFile::fs_FileExists(File))
					continue;
				CStr FileContents = CFile::fs_ReadStringFromFile(File, true).f_Replace("\r\n", "\n");
				DependenciesHash.f_AddData(FileContents.f_GetStr(), FileContents.f_GetLen());
			}
			
			bool bCacheUpToDate = CFile::fs_ReadStringFromFile(CmakeCacheDirectory + "/Dependencies.sha512", true) == DependenciesHash.f_GetDigest().f_GetString();
			if (bCacheUpToDate || !bUpdateCache)
			{
				if (!bCacheUpToDate)
				{
					DMibConOut("WARNING: Import cache out of date (CMake), but updating has been disabled with Import.CMake_UpdateCache: {}\n", CmakeCacheDirectory);
				}
				DLock(mp_CMakeGenerateLock);
				mp_CMakeGenerated[LockDirectory] = ConfigHashString;
				mp_CMakeGeneratedContents[LockDirectory] = ConfigHashContents;
				return fReturn(LockDirectory, ConfigHashString);
			}
			DMibConOut("Import cache out of date (CMake): {}\n", CmakeCacheDirectory);
		}
		
		
		CProcessLaunchParams LaunchParams;
		LaunchParams.m_bAllowExecutableLocate = true;
		LaunchParams.m_WorkingDirectory = TempDirectory;
		LaunchParams.m_bSeparateStdErr = false;
		LaunchParams.m_bMergeEnvironment = false;
		LaunchParams.m_Environment = mp_GeneratorInterface->f_GetBuildEnvironment(Platform, Architecture);
		CStr HidePrefixes;
		fg_AddStrSep(HidePrefixes, SharedTempDirectory, ";");
		fg_AddStrSep(HidePrefixes, CFile::fs_GetPath(FileName), ";");
		LaunchParams.m_Environment["CMAKE_MALTERLIB_HIDEPREFIXES"] = HidePrefixes;
		LaunchParams.m_Environment["CMAKE_MALTERLIB_REPLACEPREFIXES"] = CmakeReplacePrefixes;
#ifdef DPlatformFamily_OSX
		LaunchParams.m_Environment["PATH"] = "/opt/local/bin:" + LaunchParams.m_Environment["PATH"];
#endif
		{
			CStr Path;
			while (!CmakePath.f_IsEmpty())
			{
#ifdef DPlatformFamily_Windows
				fg_AddStrSep(Path, fg_GetStrSep(CmakePath, ";").f_ReplaceChar('/', '\\'), ";");
#else
				fg_AddStrSep(Path, fg_GetStrSep(CmakePath, ";"), ":");
#endif
			}
			if (!Path.f_IsEmpty())
			{
#ifdef DPlatformFamily_Windows
				LaunchParams.m_Environment["PATH"] = Path + ";" + LaunchParams.m_Environment["PATH"];
#else
				LaunchParams.m_Environment["PATH"] = Path + ":" + LaunchParams.m_Environment["PATH"];
#endif
			}
		}
		LaunchParams.m_Environment.f_Remove("PRODUCT_SPECIFIC_LDFLAGS");
		LaunchParams.m_Environment.f_Remove("SDKROOT");
		LaunchParams.m_Environment["CMAKE_MALTERLIB_BASEDIR"] = f_GetBaseDir();
		{
			auto Temp = CmakeLanguages; 
			while (!Temp.f_IsEmpty())
			{
				CStr Mapping = fg_GetStrSep(Temp, ";");
				if (Mapping.f_IsEmpty())
					continue;
				CStr Language = fg_GetStrSep(Mapping, "=");
				CStr MalterlibType = Mapping;
				LaunchParams.m_Environment[fg_Format("CMAKE_MALTERLIB_LANGUAGE_{}", Language)] = MalterlibType;		
			}
		}
		
		CStr CmakeExecutable; 
		TCVector<CStr> Params;
		
#		ifdef DMalterlibBuildSystem_EmbedCMake
			CmakeExecutable = CFile::fs_GetProgramPath(); 
			Params.f_Insert("CMake");
#		else
			CmakeExecutable = CFile::fs_GetProgramDirectory() + "/cmake";
#			ifdef DPlatformFamily_Windows
				CmakeExecutable += ".exe";
#			endif
			f_AddSourceFile(CmakeExecutable);
#		endif
		
		Params.f_Insert
			(
				{
					"-G"
					, "Malterlib - Ninja"
					, CFile::fs_GetPath(FileName)
				}
			)
		;			
		
		Params.f_Insert("-DCMAKE_BUILD_TYPE=" + CmakeConfig);
		
		{
			for (mint i = 0; i < 2; ++i)
			{
				CStr Temp;
				if (i == 0)
					Temp = CmakeVariables;
				else
					Temp = CmakeVariablesWithPaths;
				while (!Temp.f_IsEmpty())
				{
					CStr Variable = fg_GetStrSep(Temp, ";");
					if (Variable.f_IsEmpty())
						continue;
					Params.f_Insert("-D" + Variable);
				}
			}
			
			if (!CmakeSystemName.f_IsEmpty())
				Params.f_Insert("-DCMAKE_SYSTEM_NAME=" + CmakeSystemName);
			if (!CmakeSystemProcessor.f_IsEmpty())
				Params.f_Insert("-DCMAKE_SYSTEM_PROCESSOR=" + CmakeSystemProcessor);
			if (!CmakeCompiler.f_IsEmpty())
				Params.f_Insert("-DCMAKE_C_COMPILER=" + CmakeCompiler);
			if (!CmakeCompilerTarget.f_IsEmpty())
				Params.f_Insert("-DCMAKE_C_COMPILER_TARGET=" + CmakeCompilerTarget);
			if (!CmakeCxxCompiler.f_IsEmpty())
				Params.f_Insert("-DCMAKE_CXX_COMPILER=" + CmakeCxxCompiler);
			if (!CmakeCxxCompilerTarget.f_IsEmpty())
				Params.f_Insert("-DCMAKE_CXX_COMPILER_TARGET=" + CmakeCxxCompilerTarget);
			
			Params.f_Insert("-DCMAKE_TOOLCHAIN_NO_PREFIX=1");
			
			if (Platform != DMibStringize(DPlatform))
			{
				CStr SysRoot = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_SysRoot");
				if (!SysRoot.f_IsEmpty())
				{
					Params.f_Insert("-DCMAKE_FIND_ROOT_PATH=" + SysRoot);
					Params.f_Insert("-DCMAKE_SYSROOT=" + SysRoot);
				}

				CStr CompilerExternalToolchain = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CExternalToolChain");
				if (!CompilerExternalToolchain.f_IsEmpty())
					Params.f_Insert("-DCMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN=" + CompilerExternalToolchain);
				CStr CxxCompilerExternalToolchain = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CxxExternalToolChain");
				if (!CxxCompilerExternalToolchain.f_IsEmpty())
					Params.f_Insert("-DCMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN=" + CxxCompilerExternalToolchain);
			}
		}
		
		CStr StdOut;
		CStr StdErr;
		
		CStr CmakeRealExecutable = CmakeExecutable;
		if (CFile::fs_GetAttributes(CmakeExecutable) & EFileAttrib_Link)
		{
			CmakeRealExecutable = CFile::fs_ResolveSymbolicLink(CmakeExecutable);
			f_AddSourceFile(CmakeRealExecutable);
		}
		
		CFile::fs_CreateDirectory(TempDirectory);
		
		CStr FullRebuildVersionFile = TempDirectory + "/MalterlibFullRebuildVersion";
		CStr LastFullRebuildVersion;
		if (CFile::fs_FileExists(FullRebuildVersionFile))
			LastFullRebuildVersion = CFile::fs_ReadStringFromFile(FullRebuildVersionFile, true);
		
		if (FullRebuildVersion != LastFullRebuildVersion)
		{
			CFile::fs_DeleteDirectoryRecursive(TempDirectory);
			CFile::fs_CreateDirectory(TempDirectory);
		}
		
		LaunchParams.m_bShowLaunched = false;
		
		//DMibConOut("ENV: {}\n", LaunchParams.m_Environment);
		//DMibConOut2("Params ({}): {}\n", CmakeExecutable, Params);
		CClock Clock{true};
		uint32 ExitCode = 0;
		if (!CProcessLaunch::fs_LaunchBlock(CmakeExecutable, Params, StdOut, StdErr, ExitCode, LaunchParams))
			DMibError(fg_Format("Failed to launch cmake: {}", StdOut + StdErr));

		//DMibConOut("Output: {}\n", StdOut + StdErr);
		
		if (!CmakeCacheDirectory.f_IsEmpty())
		{
			CFile::fs_CreateDirectory(CmakeCacheDirectory);
			
			CFile::CFindFilesOptions FindOptions{TempDirectory + "/*", true};
			FindOptions.m_AttribMask = EFileAttrib_File;
			{
				CStr Temp = CacheExcludePatterns;
				while (!Temp.f_IsEmpty())
					FindOptions.m_ExcludePatterns.f_Insert(fg_GetStrSep(Temp, ";"));
			}

			TCVector<TCTuple<CStr, CStr>> ReplaceContents;
			{
				CStr Temp = CacheReplaceContents;
				while (!Temp.f_IsEmpty())
				{
					CStr Replace = fg_GetStrSep(Temp, ";");
					CStr Key = fg_GetStrSep(Replace, "=");
					ReplaceContents.f_Insert(fg_Tuple(Key, Replace));
				}
			}

			auto FoundFiles = CFile::fs_FindFiles(FindOptions); 
			mint PathPrefixLen = TempDirectory.f_GetLen() + 1;
			
			CStr SourceBase = CFile::fs_GetPath(FileName);
			CStr SourceBaseFind = SourceBase + "/";  
			CStr BaseDir = f_GetBaseDir();
			CStr BaseDirFind = BaseDir + "/";  
			CStr TempDirectoryFind = TempDirectory + "/";  
			TCSet<CStr> WrittenFiles;
			
			TCSet<CStr> DependencyFiles;
			
			for (auto &File : FoundFiles)
			{
				CStr RelativePath = File.m_Path.f_Extract(PathPrefixLen);
				CStr DestPath = CFile::fs_AppendPath(CmakeCacheDirectory, RelativePath);
				CStr DestPathDirectory = CFile::fs_GetPath(DestPath);
				CStr RelativeSource = CFile::fs_MakePathRelative(SourceBase, DestPathDirectory);
				CStr RelativeDest = CFile::fs_MakePathRelative(CmakeCacheDirectory, DestPathDirectory);
				CStr RelativeBase = CFile::fs_MakePathRelative(BaseDir, DestPathDirectory);

				CStr RelativeSourceBare = RelativeSource;
				CStr RelativeDestBare = RelativeDest;
				CStr RelativeBaseBare = RelativeBase;
				
				if (RelativeSourceBare.f_IsEmpty())
					RelativeSourceBare = ".";
				if (RelativeDestBare.f_IsEmpty())
					RelativeDestBare = ".";
				if (RelativeBaseBare.f_IsEmpty())
					RelativeBaseBare = ".";
				
				if (!RelativeSource.f_IsEmpty())
					RelativeSource += "/";
				if (!RelativeDest.f_IsEmpty())
					RelativeDest += "/";
				if (!RelativeBase.f_IsEmpty())
					RelativeBase += "/";
				
				CStr FileContents = CFile::fs_ReadStringFromFile(File.m_Path, true);
				
				if (CFile::fs_GetExtension(File.m_Path) == "dependencies")
				{
					CStr NewFileContents;
					ch8 const *pParse = FileContents;
					while (*pParse)
					{
						auto pLineStart = pParse;
						fg_ParseToEndOfLine(pParse);
						CStr Line(pLineStart, pParse - pLineStart);
						fg_ParseEndOfLine(pParse);
						if (Line.f_StartsWith(TempDirectory))
						{
							bool bExcluded = false;
							for (auto &ExcludePattern : FindOptions.m_ExcludePatterns)
							{
								if (fg_StrMatchWildcard(Line.f_GetStr(), ExcludePattern.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
								{
									bExcluded = true;
									break;
								}
							}
							if (bExcluded)
								continue;
						}
						else if (Line.f_Find("/CMakeRoot/") >= 0)
							continue;
						
						DependencyFiles[Line];
						
						NewFileContents += Line;
						NewFileContents += "\n";
					}
					FileContents = fg_Move(NewFileContents);
				}
				
				FileContents = FileContents.f_Replace(TempDirectoryFind, RelativeDest);
				FileContents = FileContents.f_Replace(TempDirectory, RelativeDestBare);
				FileContents = FileContents.f_Replace(SourceBaseFind, RelativeSource);
				FileContents = FileContents.f_Replace(SourceBase, RelativeSourceBare);
				FileContents = FileContents.f_Replace(BaseDirFind, RelativeBase);
				FileContents = FileContents.f_Replace(BaseDir, RelativeBaseBare);
				
				for (auto &Replace : ReplaceContents)
					FileContents = FileContents.f_Replace(fg_Get<0>(Replace), fg_Get<1>(Replace));
				
				if (CFile::fs_GetExtension(File.m_Path) == "MHeader")
				{
					auto fGetStripped = [&](CStr const &_String, bool _bExpand = true)
						{
							CStr Stripped = _String.f_Replace("@('", "").f_Replace("'->MakeAbsolute())", "");
							if (!_bExpand)
								return Stripped;
							return CFile::fs_GetExpandedPath(Stripped, DestPathDirectory);
						}
					;
					CRegistryPreserveAndOrder_CStr Registry;
					Registry.f_ParseStr(FileContents, File.m_Path);
					TCMap<CStr, CStr> RemappedOutputs;
					Registry.f_TransformFunc
						(
							[&](CRegistryPreserveAndOrder_CStr &o_This)
							{
								auto pWorkingDirectory = o_This.f_GetChild("Custom_WorkingDirectory");
								if (!pWorkingDirectory)
									return;
								CStr RelativeWorkingDir = fGetStripped(pWorkingDirectory->f_GetThisValue(), false);
								CStr WorkingDirectory = CFile::fs_GetExpandedPath(RelativeWorkingDir, DestPathDirectory);
								
								if (WorkingDirectory == RelativeWorkingDir)
									return;
								
								CStr OutputsString = o_This.f_GetValue("Custom_Outputs", ""); 
								TCVector<CStr> Outputs;
								TCVector<CStr> OriginalOutputs;
								while (!OutputsString.f_IsEmpty())
								{
									auto &Original = OriginalOutputs.f_Insert(fg_GetStrSep(OutputsString, ";"));
									Outputs.f_Insert(fGetStripped(Original));
								}
								
								
								CStr CommandLine = o_This.f_GetValue("Custom_CommandLine", "");

								TCVector<CStr> CommandLineParams;
								while (!CommandLine.f_IsEmpty())
								{
									CStr Param = fg_GetStrSepEscaped(CommandLine, " ");
									
									CStr WorkingDirParam = CFile::fs_GetExpandedPath(Param, WorkingDirectory);
									CStr StrippedParam = fGetStripped(Param);
									CStr *pWorkingDirOutput = nullptr;
									CStr *pStrippedOutput = nullptr;
									mint iOutput = 0;
									for (auto &Output : Outputs)
									{
										if (StrippedParam == Output)
										{
											pStrippedOutput = &Output;
											break;
										}
										if (WorkingDirParam == Output)
										{
											pWorkingDirOutput = &Output;
											break;
										}
										++iOutput;
									}
									if (pStrippedOutput)
									{
										RemappedOutputs[StrippedParam] 
											= Param 
											= OriginalOutputs[iOutput] 
											= fg_Format("@(Target:Target.IntermediateDirectory->MakeAbsolute())/{}", fGetStripped(Param, false))
										;
									}
									else if (pWorkingDirOutput)
									{
										RemappedOutputs[WorkingDirParam] 
											= Param 
											= OriginalOutputs[iOutput] 
											= fg_Format("@(Target:Target.IntermediateDirectory->MakeAbsolute())/{}/{}", RelativeWorkingDir, Param)
										;
									}
									
									CommandLineParams.f_Insert(Param);
								}
								
								for (auto &Output : OriginalOutputs)
									fg_AddStrSep(OutputsString, Output, ";");

								o_This.f_SetValue("Custom_Outputs", OutputsString); 
								o_This.f_SetValue("Custom_CommandLine", CProcessLaunchParams::fs_GetParams(CommandLineParams));
							}
						)
					;
					Registry.f_TransformFunc
						(
							[&](CRegistryPreserveAndOrder_CStr &o_This)
							{
								CStr ExpandedPath = fGetStripped(o_This.f_GetThisValue());
								auto *pRempped = RemappedOutputs.f_FindEqual(ExpandedPath);
								if (pRempped)
									o_This.f_SetThisValue(*pRempped);
							}
						)
					;
					
					FileContents = Registry.f_GenerateStr(); 
				}
				
				{
					CFile::fs_CreateDirectory(CFile::fs_GetPath(DestPath));
					TCVector<uint8> BinaryFileContents;
					CFile::fs_WriteStringToVector(BinaryFileContents, FileContents, false);
					f_WriteFile(BinaryFileContents, DestPath, File.m_Attribs | EFileAttrib_UnixAttributesValid);
				}
				
				WrittenFiles[DestPath];
			}
			
			CHash_SHA512 DependenciesHash;
			fInitHash(DependenciesHash);

			for (auto &File : DependencyFiles)
			{
				CStr FileContents = CFile::fs_ReadStringFromFile(File, true).f_Replace("\r\n", "\n");
				DependenciesHash.f_AddData(FileContents.f_GetStr(), FileContents.f_GetLen());
			}

			{
				TCVector<uint8> BinaryFileContents;
				CFile::fs_WriteStringToVector(BinaryFileContents, DependenciesHash.f_GetDigest().f_GetString(), false);
				f_WriteFile(BinaryFileContents, CmakeCacheDirectory + "/Dependencies.sha512");
				WrittenFiles[CmakeCacheDirectory + "/Dependencies.sha512"];
			}
			
			for (auto &File : CFile::fs_FindFiles(CmakeCacheDirectory + "/*", EFileAttrib_File, true))
			{
				if (!WrittenFiles.f_FindEqual(File))
					CFile::fs_DeleteFile(File);
			}
		}
		
		CFile::fs_WriteStringToFile(FullRebuildVersionFile, FullRebuildVersion, false);
		
		//DMibConOut2("{} = {}\n", TempDirectory, Clock.f_GetTime());
		
		if (ExitCode)
			DMibError(fg_Format("cmake failed: {}", StdOut + StdErr));

		{
			DLock(mp_CMakeGenerateLock);
			SetInvalidGenerated.f_Clear();
			mp_CMakeGenerated[LockDirectory] = ConfigHashString;
			mp_CMakeGeneratedContents[LockDirectory] = ConfigHashContents;
			return fReturn(LockDirectory, ConfigHashString);
		}
	}

	void CBuildSystem::fp_ExpandImport(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const
	{
		CStr FileName = _Entity.m_Key.m_Name;
		
		CBuildSystemData::CImportData *pImport;
		
		if (CFile::fs_GetFile(FileName) == "CMakeLists.txt")
			pImport = fp_ExpandImportCMake(_Entity, _ParentEntity, _BuildSystemData);
		else
		{
			auto &Import = _BuildSystemData.m_Imports.f_Insert();
			CBuildSystemPreprocessor Preprocessor(Import.m_Registry, _BuildSystemData.m_SourceFiles, mp_FindCache);
			Preprocessor.f_ReadFile(FileName);
			_BuildSystemData.m_SourceFiles[FileName];
			pImport = &Import;
		}
		
		if (!pImport->m_Registry.f_GetChildren().f_IsEmpty())
		{
			fp_ParseData(pImport->m_RootEntity, pImport->m_Registry, nullptr);
			
			auto const *pUltimateSource = &_Entity;
			while (pUltimateSource->m_pCopiedFrom)
				pUltimateSource = pUltimateSource->m_pCopiedFrom.f_Get(); 
			while (pUltimateSource->m_pCopiedFromEvaluated)
				pUltimateSource = pUltimateSource->m_pCopiedFromEvaluated.f_Get(); 
			while (pUltimateSource->m_pCopiedFrom)
				pUltimateSource = pUltimateSource->m_pCopiedFrom.f_Get();
			
			if (!pUltimateSource->m_ChildEntitiesOrdered.f_IsEmpty())
				pImport->m_RootEntity.f_MergeEntities(*pUltimateSource);
			
			for (auto &Child : pImport->m_RootEntity.m_ChildEntitiesOrdered)
				Child.f_CopyProperties(_Entity);
			
			_ParentEntity.f_CopyPropertiesAndEval(pImport->m_RootEntity);
			
			auto *pInsertAfter = &_Entity;
			for (auto iEntity = pImport->m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity;)
			{
				auto &Entity = *iEntity;
				++iEntity;
				
				if (_ParentEntity.m_ChildEntitiesMap.f_FindEqual(Entity.m_Key))
					fsp_ThrowError(Entity.m_Position, "Imported entity already exists");
				//pInsertAfter = fp_AddEntity(Entity, _ParentEntity, Key, pInsertAfter, nullptr);
				_ParentEntity.m_ChildEntitiesMap.f_ExtractAndInsert(Entity.m_pParent->m_ChildEntitiesMap, &Entity);
				Entity.m_pParent = &_ParentEntity;
				
				_ParentEntity.m_ChildEntitiesOrdered.f_InsertAfter(Entity, pInsertAfter);
				pInsertAfter = &Entity;
				
				if (Entity.m_Key.m_Type != EEntityType_Target && Entity.m_Key.m_Type != EEntityType_Workspace)
					fpr_EvaluateData(Entity);
			}
		}
	}
}
