// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

#include <Mib/Process/ProcessLaunch>

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
	
	CBuildSystemData::CImportData *CBuildSystem::fp_ExpandImportCMake(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const
	{
		CStr FileName = CFile::fs_GetExpandedPath(_Entity.m_Key.m_Name, CFile::fs_GetPath(_Entity.m_Position.m_FileName));
		CStr TempDirectory = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "TempDirectory");
		CStr Platform = f_EvaluateEntityProperty(_Entity, EPropertyType_Property, "Platform");
		CStr Architecture = f_EvaluateEntityProperty(_Entity, EPropertyType_Property, "Architecture");
		
		CProcessLaunchParams LaunchParams;
		LaunchParams.m_bAllowExecutableLocate = true;
		LaunchParams.m_WorkingDirectory = TempDirectory;
		LaunchParams.m_bSeparateStdErr = false;
		LaunchParams.m_bMergeEnvironment = false;
		LaunchParams.m_Environment = mp_GeneratorInterface->f_GetBuildEnvironment(Platform, Architecture);
		CStr HidePrefixes;
		fg_AddStrSep(HidePrefixes, f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "SharedTempDirectory"), ";");
		fg_AddStrSep(HidePrefixes, CFile::fs_GetPath(FileName), ";");
		LaunchParams.m_Environment["CMAKE_MALTERLIB_HIDEPREFIXES"] = HidePrefixes;
#ifdef DPlatformFamily_OSX
		LaunchParams.m_Environment["PATH"] = "/opt/local/bin:" + LaunchParams.m_Environment["PATH"];
#endif
		
		LaunchParams.m_Environment.f_Remove("PRODUCT_SPECIFIC_LDFLAGS");
		LaunchParams.m_Environment.f_Remove("SDKROOT");
		
		CStr CmakeLanguages = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Languages");
		while (!CmakeLanguages.f_IsEmpty())
		{
			CStr Mapping = fg_GetStrSep(CmakeLanguages, ";");
			if (Mapping.f_IsEmpty())
				continue;
			CStr Language = fg_GetStrSep(Mapping, "=");
			CStr MalterlibType = Mapping;
			LaunchParams.m_Environment[fg_Format("CMAKE_MALTERLIB_LANGUAGE_{}", Language)] = MalterlibType;		
		}
		
		TCVector<CStr> Params =
			{
				"-G"
				, "Malterlib - Ninja"
				, CFile::fs_GetPath(FileName)
			}
		;			
		
		Params.f_Insert("-DCMAKE_BUILD_TYPE=" + f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Config"));
		
		{
			CStr CmakeVariables = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Variables");
			while (!CmakeVariables.f_IsEmpty())
			{
				CStr Variable = fg_GetStrSep(CmakeVariables, ";");
				if (Variable.f_IsEmpty())
					continue;
				Params.f_Insert("-D" + Variable);
			}
			CStr SystemName = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_SystemName");
			if (!SystemName.f_IsEmpty())
				Params.f_Insert("-DCMAKE_SYSTEM_NAME=" + SystemName);
			CStr SystemProcessor = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_SystemProcessor");;
			if (!SystemProcessor.f_IsEmpty())
				Params.f_Insert("-DCMAKE_SYSTEM_PROCESSOR=" + SystemProcessor);
			CStr Compiler = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CCompiler");
			if (!Compiler.f_IsEmpty())
				Params.f_Insert("-DCMAKE_C_COMPILER=" + Compiler);
			CStr CompilerTarget = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CCompilerTarget");
			if (!CompilerTarget.f_IsEmpty())
				Params.f_Insert("-DCMAKE_C_COMPILER_TARGET=" + CompilerTarget);
			CStr CxxCompiler = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CxxCompiler");
			if (!CxxCompiler.f_IsEmpty())
				Params.f_Insert("-DCMAKE_CXX_COMPILER=" + CxxCompiler);
			CStr CxxCompilerTarget = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_CxxCompilerTarget");
			if (!CxxCompilerTarget.f_IsEmpty())
				Params.f_Insert("-DCMAKE_CXX_COMPILER_TARGET=" + CxxCompilerTarget);
			
			Params.f_Insert("-DCMAKE_TOOLCHAIN_NO_PREFIX=1");
			
			if (Platform != DMibStringize(DPlatform))
			{
				//Params.f_Insert("-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER");
				//Params.f_Insert("-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY");
				//Params.f_Insert("-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY");
				
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
				
				//
			}
		}
		
		CStr StdOut;
		CStr StdErr;
		
		CStr CmakeExecutable = CFile::fs_GetProgramDirectory() + "/cmake";
#ifdef DPlatformFamily_Windows
		CmakeExecutable += ".exe";
#endif
		f_AddSourceFile(CmakeExecutable);
		
		CStr CmakeRealExecutable = CmakeExecutable;
		if (CFile::fs_GetAttributes(CmakeExecutable) & EFileAttrib_Link)
		{
			CmakeRealExecutable = CFile::fs_ResolveSymbolicLink(CmakeExecutable);
			f_AddSourceFile(CmakeRealExecutable);
		}
		
		CFile::fs_CreateDirectory(TempDirectory);
		
		CStr FullRebuildVersion = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_FullRebuildVersion");
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
		
		CClock Clock{true};
		uint32 ExitCode = 0;
		if (!CProcessLaunch::fs_LaunchBlock(CmakeExecutable, Params, StdOut, StdErr, ExitCode, LaunchParams))
			DMibError(fg_Format("Failed to launch cmake: {}", StdOut + StdErr));
		
		//DMibConOut("Output: {}\n", StdOut + StdErr);
		
		CFile::fs_WriteStringToFile(FullRebuildVersionFile, FullRebuildVersion, false);
		
		//DMibConOut2("{} = {}\n", TempDirectory, Clock.f_GetTime());
		
		if (ExitCode)
			DMibError(fg_Format("cmake failed: {}", StdOut + StdErr));
		
		CStr Projects = f_EvaluateEntityProperty(_Entity, EPropertyType_Import, "CMake_Projects");
		if (Projects.f_IsEmpty())
		{
			auto ProjectFiles = CFile::fs_FindFiles(fg_Format("{}/*.MHeader", TempDirectory));
			if (ProjectFiles.f_IsEmpty())
				DMibError("No MHeader files found after generating cmake");
			for (auto &File : ProjectFiles)
				fg_AddStrSep(Projects, CFile::fs_GetFileNoExt(File), ";");
		}

		auto &Import = _BuildSystemData.m_Imports.f_Insert();
		
		{
			CBuildSystemPreprocessor Preprocessor(Import.m_Registry, _BuildSystemData.m_SourceFiles, mp_FindCache);
			while (!Projects.f_IsEmpty())
			{
				CStr Project = fg_GetStrSep(Projects, ";");
				
				CStr ProjectFileName = fg_Format("{}/{}.MHeader", TempDirectory, Project);
				
				Preprocessor.f_ReadFile(ProjectFileName);
				_BuildSystemData.m_SourceFiles[ProjectFileName];
				
				CStr Dependencies = CFile::fs_ReadStringFromFile(fg_Format("{}/{}.MHeader.dependencies", TempDirectory, Project), true);
				
				TCSet<CStr> SourceFileToAdd;
				ch8 const *pParse = Dependencies.f_GetStr();
				while (*pParse)
				{
					ch8 const *pStart = pParse;
					fg_ParseToEndOfLine(pParse);
					SourceFileToAdd[CStr(pStart, pParse - pStart)];
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
			
			_BuildSystemData.m_RootEntity.f_CopyProperties(pImport->m_RootEntity);
			
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
