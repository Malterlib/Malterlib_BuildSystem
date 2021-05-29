// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

#include <Mib/Process/ProcessLaunch>
#include <Mib/File/MalterlibFS>
#include <Mib/File/VirtualFSs/MalterlibFS>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Perforce/Wrapper>

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
					auto &Key = Child.f_GetKey();
					if (Key.m_Type == EEntityType_Root)
					{
						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_Import)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
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
					auto &Key = Child.f_GetKey();
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

	CBuildSystemData::CImportData CBuildSystem::fp_ExpandImportCMake_FromGeneratedDirectory
		(
			CEntity &_Entity
			, CBuildSystemData &_BuildSystemData
			, CStr const &_Directory
		) const
	{
		TCVector<CStr> Projects = f_EvaluateEntityPropertyStringArray(_Entity, EPropertyType_Import, "CMake_Projects", TCVector<CStr>());
		if (Projects.f_IsEmpty())
		{
			auto ProjectFiles = CFile::fs_FindFiles(fg_Format("{}/*.MHeader", _Directory));
			if (ProjectFiles.f_IsEmpty())
				DMibError(fg_Format("No MHeader files found in CMake generated directory"));
			for (auto &File : ProjectFiles)
				Projects.f_Insert(CFile::fs_GetFileNoExt(File));
		}

		CBuildSystemData::CImportData Import;

		{
			CBuildSystemPreprocessor Preprocessor(Import.m_Registry, _BuildSystemData.m_SourceFiles, mp_FindCache, mp_Environment);
			for (auto &Project : Projects)
			{
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

		return Import;
	}

	CBuildSystemData::CImportData CBuildSystem::fp_ExpandImportCMake(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const
	{
		auto &EntityData = _Entity.f_Data();

		CStr CmakeCacheDirectory = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_CacheDirectory");

		CStr LockDirectory = CmakeCacheDirectory;
		CStr TempDirectory = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "TempDirectory");

		if (LockDirectory.f_IsEmpty())
			LockDirectory = TempDirectory;

		bool bUpdateCache = f_EvaluateEntityPropertyBool(_Entity, EPropertyType_Import, "CMake_UpdateCache", false);
		bool bVerbose = f_EvaluateEntityPropertyBool(_Entity, EPropertyType_Import, "CMake_Verbose", false);
		bool bVerboseHash = f_EvaluateEntityPropertyBool(_Entity, EPropertyType_Import, "CMake_VerboseHash", false);
		bool bDiffHash = f_EvaluateEntityPropertyBool(_Entity, EPropertyType_Import, "CMake_DiffHash", false);

#ifdef DPlatformFamily_Windows
		auto fReplace = [&](auto &&_String, auto &&_Find, auto &&_ReplaceWith)
			{
				return _String.f_ReplaceNoCase(_Find, _ReplaceWith);
			}
		;
		auto fStartsWith = [&](auto &&_String, auto &&_Find)
			{
				return _String.f_StartsWithNoCase(_Find);
			}
		;
#else
		auto fReplace = [&](auto &&_String, auto &&_Find, auto &&_ReplaceWith)
			{
				return _String.f_Replace(_Find, _ReplaceWith);
			}
		;
		auto fStartsWith = [&](auto &&_String, auto &&_Find)
			{
				return _String.f_StartsWith(_Find);
			}
		;
#endif

		// Dependent variables
		CStr GeneratorVersion = "20";
		CStr GeneratorFullRebuildVersion = "4";
		CStr FullRebuildVersion = "{}-{}"_f << f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_FullRebuildVersion") << GeneratorFullRebuildVersion;
		TCVector<CStr> CacheExcludePatterns = f_EvaluateEntityPropertyStringArray(_Entity, EPropertyType_Import, "CMake_CacheExcludePatterns", TCVector<CStr>());
		CEJSON CacheReplaceContents = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CacheReplaceContents");
		CEJSON CmakeLanguages = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Languages");
		CStr CmakeConfig = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_Config");
		TCVector<CStr> CmakeVariables = f_EvaluateEntityPropertyStringArray(_Entity, EPropertyType_Import, "CMake_Variables", TCVector<CStr>());

		TCVector<CStr> CmakeExcludeFromHash = f_EvaluateEntityPropertyStringArray(_Entity, EPropertyType_Import, "CMake_ExcludeFromHash", TCVector<CStr>());

		CStr HashContents = fg_Format("Config (Not checked): {}\n", f_EvaluateEntityPropertyString(_Entity, EPropertyType_Property, "FullConfiguration"));
		auto fAddStringHash = [&](CHash_SHA512 &o_DependenciesHash, CStr const &_String, ch8 const *_pVariableName, bool _bPerformExclude)
			{
				CStr FilteredString = _String;
				if (_bPerformExclude)
				{
					for (auto &Exclude : CmakeExcludeFromHash)
					{
						if (Exclude.f_IsEmpty())
							continue;
						FilteredString = FilteredString.f_Replace(Exclude, "");
					}
				}
				HashContents += CStr::CFormat("{}: {}\n") << _pVariableName << FilteredString;
				o_DependenciesHash.f_AddData(FilteredString.f_GetStr(), FilteredString.f_GetLen());
			}
		;

		auto fInitHash = [&](CHash_SHA512 &o_DependenciesHash, bool _bPerformExclude)
			{
				HashContents.f_Clear();
				fAddStringHash(o_DependenciesHash, GeneratorVersion, "GeneratorVersion", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, FullRebuildVersion, "Import.CMake_FullRebuildVersion", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CacheExcludePatterns, "Import.CMake_CacheExcludePatterns", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CacheReplaceContents, "Import.CMake_CacheReplaceContents", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CmakeLanguages, "Import.CMake_Languages", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, CmakeConfig, "Import.CMake_Config", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CmakeVariables, "Import.CMake_Variables", _bPerformExclude);
			}
		;

		CStr FileName = CFile::fs_GetExpandedPath(_Entity.f_GetKeyName(), CFile::fs_GetPath(EntityData.m_Position.m_File));

		CStr SharedTempDirectory = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "SharedTempDirectory", CStr());
		CStr Platform = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Property, "Platform");
		CStr Architecture = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Property, "Architecture");
		TCVector<CStr> CmakePaths = f_EvaluateEntityPropertyStringArray(_Entity, EPropertyType_Import, "CMake_Path", TCVector<CStr>());
		TCVector<CStr> CmakeVariablesWithPaths = f_EvaluateEntityPropertyStringArray(_Entity, EPropertyType_Import, "CMake_VariablesWithPaths", TCVector<CStr>());
		CStr CmakeSystemName = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_SystemName", CStr());
		CStr CmakeSystemProcessor = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_SystemProcessor", CStr());
		CStr CmakeCompiler = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_CCompiler", CStr());
		CStr CmakeCompilerTarget = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_CCompilerTarget", CStr());
		CStr CmakeCxxCompiler = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_CxxCompiler", CStr());
		CStr CmakeCxxCompilerTarget = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_CxxCompilerTarget", CStr());
		CEJSON CmakeReplacePrefixes = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_ReplacePrefixes");

		auto fInitConfigHash = [&](CHash_SHA512 &o_DependenciesHash)
			{
				fInitHash(o_DependenciesHash, false);
				fAddStringHash(o_DependenciesHash, FileName, "FileName", false);
				fAddStringHash(o_DependenciesHash, SharedTempDirectory, "Import.SharedTempDirectory", false);
				fAddStringHash(o_DependenciesHash, Platform, "Platform", false);
				fAddStringHash(o_DependenciesHash, Architecture, "Architecture", false);
				fAddStringHash(o_DependenciesHash, "{}"_f << CmakePaths, "Import.CMake_Path", false);
				fAddStringHash(o_DependenciesHash, "{}"_f << CmakeVariablesWithPaths, "Import.CMake_VariablesWithPaths", false);
				fAddStringHash(o_DependenciesHash, CmakeSystemName, "Import.CMake_SystemName", false);
				fAddStringHash(o_DependenciesHash, CmakeSystemProcessor, "Import.CMake_SystemProcessor", false);
				fAddStringHash(o_DependenciesHash, CmakeCompiler, "Import.CMake_CCompiler", false);
				fAddStringHash(o_DependenciesHash, CmakeCompilerTarget, "Import.CMake_CCompilerTarget", false);
				fAddStringHash(o_DependenciesHash, CmakeCxxCompiler, "Import.CMake_CxxCompiler", false);
				fAddStringHash(o_DependenciesHash, CmakeCxxCompilerTarget, "Import.CMake_CxxCompilerTarget", false);
				fAddStringHash(o_DependenciesHash, CmakeReplacePrefixes.f_ToString(), "Import.CMake_ReplacePrefixes", false);
			}
		;

		CHash_SHA512 ConfigHash;
		fInitConfigHash(ConfigHash);
		CStr ConfigHashString = ConfigHash.f_GetDigest().f_GetString();
		CStr ConfigHashContents = fg_Move(HashContents);

		auto fReturn = [&](CStr const &_Directory, CStr const &_Hash)
			{
				if (_Hash.f_IsEmpty())
					fsp_ThrowError(EntityData.m_Position, "CMake generation failed");

				if (_Hash != ConfigHashString)
				{
					fsp_ThrowError
						(
							EntityData.m_Position
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

				return fp_ExpandImportCMake_FromGeneratedDirectory(_Entity, _BuildSystemData, _Directory);
			}
		;

		CMutual *pCMakeGenerateLock;
		{
			CStr Hash;
			{
				DLock(mp_CMakeGenerateLock);
				auto pHash = mp_CMakeGenerated.f_FindEqual(LockDirectory);
				if (pHash)
					Hash = *pHash;
				pCMakeGenerateLock = &mp_CMakeGenerateLocks[LockDirectory];
			}
			if (Hash)
				return fReturn(LockDirectory, Hash);
		}
		DLock(*pCMakeGenerateLock);
		{
			CStr Hash;
			{
				DLock(mp_CMakeGenerateLock);
				auto pHash = mp_CMakeGenerated.f_FindEqual(LockDirectory);
				if (pHash)
					Hash = *pHash;
			}
			if(Hash)
			{
				DUnlock(*pCMakeGenerateLock);
				return fReturn(LockDirectory, Hash);
			}
		}

		auto SetInvalidGenerated = g_OnScopeExit > [&]
			{
				DLock(mp_CMakeGenerateLock);
				mp_CMakeGenerated[LockDirectory];
			}
		;

		CStr LastHashContentsFile = CmakeCacheDirectory / "MalterlibHashContents.txt";

		if (!CmakeCacheDirectory.f_IsEmpty() && CFile::fs_FileExists(CmakeCacheDirectory / "Dependencies.sha512"))
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
			fInitHash(DependenciesHash, true);

			for (auto &File : DependencyFiles)
			{
				if (!CFile::fs_FileExists(File))
					continue;
				CStr FileContents = CFile::fs_ReadStringFromFile(File, true).f_Replace("\r\n", "\n");
				DependenciesHash.f_AddData(FileContents.f_GetStr(), FileContents.f_GetLen());
			}

			CStr LastDependenciesHash = CFile::fs_ReadStringFromFile(CmakeCacheDirectory + "/Dependencies.sha512", true);
			CStr NewDependenciesHash = DependenciesHash.f_GetDigest().f_GetString();

			if (bVerboseHash)
				f_OutputConsole("Import hash string for '{}': {}\n"_f << LockDirectory << HashContents);

			bool bCacheUpToDate = NewDependenciesHash == LastDependenciesHash;
			if (bCacheUpToDate || !bUpdateCache)
			{
				if (!bCacheUpToDate)
				{
					f_OutputConsole("WARNING: Import cache out of date (CMake), but updating has been disabled with Import.CMake_UpdateCache: {}\n"_f << CmakeCacheDirectory);
				}
				{
					DLock(mp_CMakeGenerateLock);
					mp_CMakeGenerated[LockDirectory] = ConfigHashString;
					mp_CMakeGeneratedContents[LockDirectory] = ConfigHashContents;
				}
				DUnlock(*pCMakeGenerateLock);
				return fReturn(LockDirectory, ConfigHashString);
			}
			if (bDiffHash)
			{
				if (CFile::fs_FileExists(LastHashContentsFile))
				{
					CStr LastHashContents = CFile::fs_ReadStringFromFile(LastHashContentsFile, true);
					if (HashContents != LastHashContents)
						NSys::fg_Debug_DiffStrings(LastHashContents, HashContents, "Last Hash Contents", "New Hash Contents");
				}
			}

			f_OutputConsole("Import cache out of date (CMake): {}\n"_f << CmakeCacheDirectory);
		}

		CProcessLaunchParams LaunchParams;
		LaunchParams.m_bAllowExecutableLocate = true;
		LaunchParams.m_WorkingDirectory = TempDirectory;
		LaunchParams.m_bSeparateStdErr = false;
		LaunchParams.m_bMergeEnvironment = false;
		LaunchParams.m_Environment = mp_GeneratorInterface->f_GetBuildEnvironment(Platform, Architecture);
		CStr HidePrefixes;
		if (SharedTempDirectory)
			fg_AddStrSep(HidePrefixes, SharedTempDirectory, ";");
		fg_AddStrSep(HidePrefixes, CFile::fs_GetPath(FileName), ";");
		LaunchParams.m_Environment["CMAKE_MALTERLIB_HIDEPREFIXES"] = HidePrefixes;
		{
			CStr PrefixesString;
			for (auto const &Entry : CmakeReplacePrefixes.f_Array())
				fg_AddStrSep(PrefixesString, "{}={}"_f << Entry["Find"].f_String() << Entry["Replace"].f_String(), ";");
			LaunchParams.m_Environment["CMAKE_MALTERLIB_REPLACEPREFIXES"] = PrefixesString;
		}
		{
			CStr Path;
			for (auto &CmakePath : CmakePaths)
			{
#ifdef DPlatformFamily_Windows
				fg_AddStrSep(Path, CmakePath.f_ReplaceChar('/', '\\'), ";");
#else
				fg_AddStrSep(Path, CmakePath, ":");
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
			for (auto &Laungage : CmakeLanguages.f_Array())
				LaunchParams.m_Environment[fg_Format("CMAKE_MALTERLIB_LANGUAGE_{}", Laungage["CMakeLanguage"].f_String())] = Laungage["MalterlibLanguage"].f_String();
		}

		CStr CmakeExecutable;
		TCVector<CStr> Params;

#		ifdef DMalterlibBuildSystem_EmbedCMake
			CmakeExecutable = CFile::fs_GetProgramDirectory() / "MToolCMake";
#		else
			CmakeExecutable = CFile::fs_GetProgramDirectory() + "/cmake";
#		endif
#		ifdef DPlatformFamily_Windows
			CmakeExecutable += ".exe";
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
			for (auto const *pVariables : {&CmakeVariables, &CmakeVariablesWithPaths})
			{
				for (auto &Variable : *pVariables)
					Params.f_Insert("-D" + Variable);
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
				CStr SysRoot = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_SysRoot", CStr());
				if (!SysRoot.f_IsEmpty())
				{
					Params.f_Insert("-DCMAKE_FIND_ROOT_PATH=" + SysRoot);
					Params.f_Insert("-DCMAKE_SYSROOT=" + SysRoot);
					LaunchParams.m_Environment["PKG_CONFIG_SYSROOT_DIR"] = SysRoot;
					LaunchParams.m_Environment["PKG_CONFIG_LIBDIR"] = "{}:{}"_f << (SysRoot / "usr/lib/pkgconfig") << (SysRoot / "usr/share/pkgconfig");
					LaunchParams.m_Environment["PKG_CONFIG_PATH"] = "";
				}

				CStr CompilerExternalToolchain = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_CExternalToolChain", CStr());
				if (!CompilerExternalToolchain.f_IsEmpty())
					Params.f_Insert("-DCMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN=" + CompilerExternalToolchain);
				CStr CxxCompilerExternalToolchain = f_EvaluateEntityPropertyString(_Entity, EPropertyType_Import, "CMake_CxxExternalToolChain", CStr());
				if (!CxxCompilerExternalToolchain.f_IsEmpty())
					Params.f_Insert("-DCMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN=" + CxxCompilerExternalToolchain);
			}
		}

		CStr StdOut;
		CStr StdErr;

		CFile::fs_CreateDirectory(TempDirectory);

		CStr FullRebuildVersionFile = TempDirectory + "/MalterlibFullRebuildVersion";

		CStr LastFullRebuildVersion;
		if (CFile::fs_FileExists(FullRebuildVersionFile))
			LastFullRebuildVersion = CFile::fs_ReadStringFromFile(FullRebuildVersionFile, true);

		bool bCmakeAlwaysFullRebuild = f_EvaluateEntityPropertyBool(_Entity, EPropertyType_Import, "CMake_AlwaysFullRebuild", false);

		if (bCmakeAlwaysFullRebuild || FullRebuildVersion != LastFullRebuildVersion || CFile::fs_FileExists(TempDirectory / "failed"))
		{
			CFile::fs_DeleteDirectoryRecursive(TempDirectory);
			CFile::fs_CreateDirectory(TempDirectory);
		}

		LaunchParams.m_bShowLaunched = false;

		//DMibConOut2("ENV: {}\n", LaunchParams.m_Environment);
		//DMibConOut2("Params ({}): {}\n", CmakeExecutable, Params);
		CClock Clock{true};
		uint32 ExitCode = 0;
		if (!CProcessLaunch::fs_LaunchBlock(CmakeExecutable, Params, StdOut, StdErr, ExitCode, LaunchParams))
		{
			CFile::fs_Touch(TempDirectory / "failed");
			DMibError(fg_Format("Failed to launch cmake: {}", StdOut + StdErr));
		}

		if (ExitCode)
		{
			CFile::fs_Touch(TempDirectory / "failed");
			DMibError(fg_Format("cmake failed: {}", StdOut + StdErr));
		}

		if (!CmakeCacheDirectory.f_IsEmpty())
		{
			CFile::fs_CreateDirectory(CmakeCacheDirectory);

			CFile::CFindFilesOptions FindOptions{TempDirectory + "/*", true};
			FindOptions.m_AttribMask = EFileAttrib_File;
			{
				for (auto &ExcludePattern : CacheExcludePatterns)
					FindOptions.m_ExcludePatterns.f_Insert(ExcludePattern);
			}

			TCVector<TCTuple<CStr, CStr>> ReplaceContents;
			{
 				for (auto &Replace : CacheReplaceContents.f_Array())
					ReplaceContents.f_Insert(fg_Tuple(Replace["Find"].f_String(), Replace["Replace"].f_String()));
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
						if (fStartsWith(Line, TempDirectory))
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

						NewFileContents += Line;
						NewFileContents += "\n";
					}
					FileContents = fg_Move(NewFileContents);
				}

				for (auto &Replace : ReplaceContents)
					FileContents = fReplace(FileContents, fg_Get<0>(Replace), fg_Get<1>(Replace));

				FileContents = fReplace(FileContents, TempDirectoryFind, RelativeDest);
				FileContents = fReplace(FileContents, TempDirectory, RelativeDestBare);
				FileContents = fReplace(FileContents, SourceBaseFind, RelativeSource);
				FileContents = fReplace(FileContents, SourceBase, RelativeSourceBare);
				FileContents = fReplace(FileContents, BaseDirFind, RelativeBase);
				FileContents = fReplace(FileContents, BaseDir, RelativeBaseBare);

				if (CFile::fs_GetExtension(File.m_Path) == "MHeader")
				{
					auto fGetStripped = [&](CStr const &_String, bool _bExpand = true)
						{
							if (!_bExpand)
								return _String;
							return CFile::fs_GetExpandedPath(_String, DestPathDirectory);
						}
					;
					CBuildSystemRegistry Registry;
					Registry.f_ParseStr(FileContents, File.m_Path);
#ifdef DPlatformFamily_Windows
					TCMap<CStr, CBuildSystemSyntax::CValue, CCompare_TStrNoCase> RemappedOutputs;
#else
					TCMap<CStr, CBuildSystemSyntax::CValue> RemappedOutputs;
#endif
					auto fParseString = [&](CStr const &_String) -> CBuildSystemSyntax::CValue
						{
							CBuildSystemRegistry Registry;
							Registry.f_ParseStr("Key " + _String);
							return fg_Move(Registry.f_GetChildIterator()->f_GetThisValue().m_Value);
						}
					;

					Registry.f_TransformFunc
						(
							[&](CBuildSystemRegistry &o_This)
							{
								CStr WorkingDirectorySource;

								TCVector<CBuildSystemSyntax::CParam *> OriginalOutputs;
								TCVector<CBuildSystemSyntax::CParam *> CommandLine;

								for (auto &Child : o_This.f_GetChildren())
								{
									auto Location = Child.f_GetLocation();
									auto &Name = Child.f_GetName();

									if (!Name.f_IsValue())
										continue;

									auto &KeyValue = Name.m_Value.f_GetAsType<CBuildSystemSyntax::CValue>();

									if (!KeyValue.f_IsIdentifier())
										continue;

									auto &Identifier = KeyValue.f_Identifier();

									if (!Identifier.f_IsNameConstantString())
										continue;

									auto &Value = Child.f_GetThisValue();

									if (Identifier.f_NameConstantString() == "Custom_WorkingDirectory")
									{
										auto &Expression = Value.m_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CExpression>();
										auto &FunctionCall = Expression.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
										auto &Param = FunctionCall.m_Params[0];
										auto &ParamValue = Param.m_Param.f_GetAsType<CEJSON>();
										WorkingDirectorySource = ParamValue.f_String();
									}

									if (Identifier.f_NameConstantString() == "Custom_Outputs")
									{
										for (auto &Output : Child.f_GetThisValue().m_Value.f_Array().m_Array)
										{
											auto &Expression = Output.f_Get().m_Value.f_GetAsType<CBuildSystemSyntax::CExpression>();
											auto &FunctionCall = Expression.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
											auto &Param = FunctionCall.m_Params[0];

											OriginalOutputs.f_Insert(&fg_RemoveQualifiers(Param));
										}
									}

									if (Identifier.f_NameConstantString() == "Custom_CommandLine")
									{
										for (auto &Token : Child.f_GetThisValue().m_Value.f_EvalString().m_Tokens)
										{
											if (Token.f_IsExpression())
											{
												auto &Expression = Token.f_Expression();
												auto &Param = Expression.f_Param().f_Expression().f_FunctionCall().m_Params[0].f_Expression().f_FunctionCall().m_Params[0];
												CommandLine.f_Insert(&fg_RemoveQualifiers(Param));
											}
										}
									}
								}

								auto Location = o_This.f_GetLocation();

								if (!WorkingDirectorySource)
									return;

								CStr RelativeWorkingDir = WorkingDirectorySource;
								CStr WorkingDirectory = CFile::fs_GetExpandedPath(RelativeWorkingDir, DestPathDirectory);

								if (WorkingDirectory == RelativeWorkingDir)
									return;

								TCVector<CStr> Outputs;
								for (auto &pOriginalOutput : OriginalOutputs)
									Outputs.f_Insert(fGetStripped(pOriginalOutput->f_Json().f_String()));

								for (auto &pParam : CommandLine)
								{
									CStr WorkingDirParam = CFile::fs_GetExpandedPath(pParam->f_Json().f_String(), WorkingDirectory);
									CStr StrippedParam = fGetStripped(pParam->f_Json().f_String());
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
										auto NewValue = fParseString(fg_Format("`@(Target:Target.IntermediateDirectory)/{}`", fGetStripped(pParam->f_Json().f_String(), false)));
										RemappedOutputs[StrippedParam] = NewValue;
										*OriginalOutputs[iOutput] = *pParam = CBuildSystemSyntax::CParam{NewValue.f_EvalString()};
									}
									else if (pWorkingDirOutput)
									{
										auto NewValue = fParseString(fg_Format("`@(Target:Target.IntermediateDirectory)/{}/{}`", RelativeWorkingDir, pParam->f_Json().f_String()));
										RemappedOutputs[StrippedParam] = NewValue;
										*OriginalOutputs[iOutput] = *pParam = CBuildSystemSyntax::CParam{NewValue.f_EvalString()};
									}
								}
							}
						)
					;

					Registry.f_TransformFunc
						(
							[&](CBuildSystemRegistry &o_This)
							{
								if (!o_This.f_GetThisValue().m_Value.f_IsConstantString())
									return;

								CStr ExpandedPath = fGetStripped(o_This.f_GetThisValue().m_Value.f_ConstantString());

								auto *pRemapped = RemappedOutputs.f_FindEqual(ExpandedPath);
								if (pRemapped)
									o_This.f_SetThisValue(CBuildSystemSyntax::CRootValue{*pRemapped});
							}
						)
					;

					FileContents = Registry.f_GenerateStr();
				}
				else if (CFile::fs_GetExtension(File.m_Path) == "dependencies")
				{
					ch8 const *pParse = FileContents;
					while (*pParse)
					{
						auto pLineStart = pParse;
						fg_ParseToEndOfLine(pParse);
						CStr Line(pLineStart, pParse - pLineStart);
						fg_ParseEndOfLine(pParse);

						DependencyFiles[CFile::fs_GetExpandedPath(Line, CFile::fs_GetPath(DestPath))];
					}
				}

				{
					CFile::fs_CreateDirectory(CFile::fs_GetPath(DestPath));
					CByteVector BinaryFileContents;
					CFile::fs_WriteStringToVector(BinaryFileContents, FileContents, false);
					f_WriteFile(BinaryFileContents, DestPath, File.m_Attribs | EFileAttrib_UnixAttributesValid);
				}

				WrittenFiles[DestPath];
			}

			CHash_SHA512 DependenciesHash;
			fInitHash(DependenciesHash, true);

			for (auto &File : DependencyFiles)
			{
				CStr FileContents = CFile::fs_ReadStringFromFile(File, true).f_Replace("\r\n", "\n");
				DependenciesHash.f_AddData(FileContents.f_GetStr(), FileContents.f_GetLen());
			}

			{
				CByteVector BinaryFileContents;
				CFile::fs_WriteStringToVector(BinaryFileContents, DependenciesHash.f_GetDigest().f_GetString(), false);
				f_WriteFile(BinaryFileContents, CmakeCacheDirectory + "/Dependencies.sha512");
				WrittenFiles[CmakeCacheDirectory + "/Dependencies.sha512"];
			}

			EFileAttrib SupportedAttributes = CFile::fs_GetSupportedAttributes();
			EFileAttrib ValidAttributes = CFile::fs_GetValidAttributes();

			for (auto &File : CFile::fs_FindFiles(CmakeCacheDirectory + "/*", EFileAttrib_File, true))
			{
				if (!WrittenFiles.f_FindEqual(File))
				{
					EFileAttrib Attributes = CFile::fs_GetAttributes(File);

					if ((Attributes & EFileAttrib_ReadOnly) || (!(Attributes & EFileAttrib_UserWrite) && (SupportedAttributes & EFileAttrib_UserWrite)))
					{
						try
						{
							CPerforceClientThrow Client;
							if (CPerforceClientThrow::fs_GetFromP4Config(File, Client))
							{
								Client.f_Delete(File);
								f_OutputConsole("Deleted file in Perforce: {}{\n}"_f << File);
								continue;
							}
						}
						catch (NException::CException const &_Error)
						{
							CStr Error = _Error.f_GetErrorStr();
							f_OutputConsole("Failed delete file in Perforce:{\n}{}{\n}"_f << Error, true);
						}
						CFile::fs_SetAttributes(File, (Attributes & ~EFileAttrib_ReadOnly)  | (SupportedAttributes & EFileAttrib_UserWrite) | ValidAttributes);
					}
					CFile::fs_DeleteFile(File);
				}
			}
		}

		if (bVerbose)
			f_OutputConsole("{}\n{}\n"_f << StdOut << StdErr, true);

		CFile::fs_WriteStringToFile(FullRebuildVersionFile, FullRebuildVersion, false);
		CFile::fs_WriteStringToFile(LastHashContentsFile, HashContents, false);

		{
			{
				DLock(mp_CMakeGenerateLock);
				SetInvalidGenerated.f_Clear();
				mp_CMakeGenerated[LockDirectory] = ConfigHashString;
				mp_CMakeGeneratedContents[LockDirectory] = ConfigHashContents;
			}
			DUnlock(*pCMakeGenerateLock);
			return fReturn(LockDirectory, ConfigHashString);
		}
	}

	void CBuildSystem::fp_ExpandImport(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const
	{
		CStr FileName = _Entity.f_GetKeyName();

		CBuildSystemData::CImportData Import = [&]()
			{
				if (CFile::fs_GetFile(FileName) == "CMakeLists.txt")
					return fp_ExpandImportCMake(_Entity, _ParentEntity, _BuildSystemData);
				else
				{
					CBuildSystemData::CImportData Import;
					CBuildSystemPreprocessor Preprocessor(Import.m_Registry, _BuildSystemData.m_SourceFiles, mp_FindCache, mp_Environment);
					Preprocessor.f_ReadFile(FileName);
					_BuildSystemData.m_SourceFiles[FileName];
					return Import;
				}
			}
			()
		;

		if (!Import.m_Registry.f_GetChildren().f_IsEmpty())
		{
			fp_ParseData(Import.m_RootEntity, Import.m_Registry, nullptr);

			if (!_Entity.m_ChildEntitiesOrdered.f_IsEmpty())
				Import.m_RootEntity.f_CopyEntities(_Entity, EEntityCopyFlag_MergeEntities);

			for (auto &Child : Import.m_RootEntity.m_ChildEntitiesOrdered)
				Child.f_CopyProperties(_Entity);

			_ParentEntity.f_CopyProperties(fg_Move(Import.m_RootEntity));

			auto *pInsertAfter = &_Entity;
			for (auto iEntity = Import.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity;)
			{
				auto &Entity = *iEntity;
				auto Key = Entity.f_GetKey();
				++iEntity;

				if (_ParentEntity.m_ChildEntitiesMap.f_FindEqual(Key))
					fsp_ThrowError(Entity.f_Data().m_Position, "Imported entity already exists");

				_ParentEntity.m_ChildEntitiesMap.f_ExtractAndInsert(Entity.m_pParent->m_ChildEntitiesMap, &Entity);
				Entity.m_pParent = &_ParentEntity;
				_ParentEntity.m_ChildEntitiesOrdered.f_InsertAfter(Entity, pInsertAfter);

				pInsertAfter = &Entity;

				if (Key.m_Type != EEntityType_Target && Key.m_Type != EEntityType_Workspace)
					fpr_EvaluateData(Entity);
			}
		}
	}
}
