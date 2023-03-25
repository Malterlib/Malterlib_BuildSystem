// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

#include <Mib/Process/ProcessLaunch>
#include <Mib/File/MalterlibFS>
#include <Mib/File/VirtualFSs/MalterlibFS>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Perforce/Wrapper>
#include <Mib/Container/Convert>
#include <Mib/String/MultiReplace>

namespace
{
#ifdef DPlatformFamily_Windows
	using CMultiReplace = TCMultiReplace<false>;
#else
	using CMultiReplace = TCMultiReplace<true>;
#endif

	struct CImportTarget
	{
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_String) const
		{
			o_String += typename tf_CStr::CFormat("\n\tDependencies: {}") << m_Dependencies;
			o_String += typename tf_CStr::CFormat("\n\tOutputs: {}") << m_Outputs;
			o_String += typename tf_CStr::CFormat("\n\tInputs: {}") << m_Inputs;
		}

		struct CDependency
		{
			CBuildSystemSyntax::CValue *m_pValue = nullptr;
			CBuildSystemRegistry *m_pBuildSystemRegistry = nullptr;
			bool m_bIsOrdered = false;
		};

		TCMap<CStr, CDependency> m_Dependencies;
		TCSet<CStr> m_Outputs;
		TCSet<CStr> m_Inputs;
		CBuildSystemRegistry *m_pBuildSystemRegistry = nullptr;
		CBuildSystemSyntax::CValue *m_pNameValue = nullptr;
		CBuildSystemRegistry *m_pBaseName = nullptr;
	};
}

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
			mp_SourceFiles += _BuildSystemData.m_MutableSourceFiles;
		}
	}

	CBuildSystemData::CImportData CBuildSystem::fp_ExpandImportCMake_FromGeneratedDirectory
		(
			CEntity &_Entity
			, CBuildSystemData &_BuildSystemData
			, CStr const &_Directory
		) const
	{
		TCVector<CStr> Projects = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_Import_CMake_Projects, TCVector<CStr>());
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
			CBuildSystemPreprocessor Preprocessor(Import.m_Registry, _BuildSystemData.m_MutableSourceFiles, mp_FindCache, mp_Environment, mp_StringCache);
			for (auto &Project : Projects)
			{
				CStr ProjectFileName = fg_Format("{}/{}.MHeader", _Directory, Project);

				Preprocessor.f_ReadFile(ProjectFileName);
				_BuildSystemData.m_MutableSourceFiles[ProjectFileName];

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

	CBuildSystemSyntax::CEvalString fg_ParseEvalString(CStringCache &o_StringCache, CStr const &_String)
	{
		CBuildSystemRegistry Registry;
		CStr EvalString;
		CStr EscapedString;
		ch8 const *pParse = _String.f_GetStr();
		while (*pParse)
		{
			if (*pParse == '@' && pParse[1] != '(')
			{
				EscapedString.f_AddStr("@@");
			}
			else
				EscapedString.f_AddChar(*pParse);
			++pParse;
		}

		{
			CStr::CAppender Appender(EvalString);
			NJSON::fg_GenerateJSONString<'`', CBuildSystemParseContext, true>(Appender, EscapedString);
		}
		try
		{
			CBuildSystemRegistryParseContext Context(o_StringCache);
			Registry.f_ParseStrWithContext(Context, "Key " + EvalString);
			return fg_Move(Registry.f_GetChildIterator()->f_GetThisValue().m_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CEvalString>());
		}
		catch (CException const &_Exception)
		{
			DMibError("Failed to parse eval string:\n{}\n{}\n"_f << EscapedString << _Exception);
		}
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CEvalString &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CValue &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CRootValue &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CExpression &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CParam &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CFunctionCall &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CArray &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::COperator &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CDefine &o_Value, tf_FFunctor &&_fFunctor);
	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CTypeDefaulted &o_Value, tf_FFunctor &&_fFunctor);

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CEvalString &o_Value, tf_FFunctor &&_fFunctor)
	{
		for (auto &Token : o_Value.m_Tokens)
		{
			if (Token.f_IsExpression())
				fg_OperateOnStrings(Token.m_Token.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>().f_Get(), _fFunctor);
		}
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CExpression &o_Value, tf_FFunctor &&_fFunctor)
	{
		if (o_Value.f_IsParam())
			fg_OperateOnStrings(o_Value.m_Expression.f_GetAsType<CBuildSystemSyntax::CParam>(), _fFunctor);
		else if (o_Value.f_IsFunctionCall())
			fg_OperateOnStrings(o_Value.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>(), _fFunctor);
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CParam &o_Value, tf_FFunctor &&_fFunctor)
	{
		if (o_Value.f_IsJson() && o_Value.f_Json().f_IsString())
			_fFunctor(o_Value.f_Json().f_String(), o_Value.m_Param);
		else if (o_Value.f_IsEvalString())
			fg_OperateOnStrings(o_Value.m_Param.f_GetAsType<CBuildSystemSyntax::CEvalString>(), _fFunctor);
		else if (o_Value.f_IsExpression())
			fg_OperateOnStrings(o_Value.m_Param.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>().f_Get(), _fFunctor);
		else if (o_Value.f_IsArray())
			fg_OperateOnStrings(o_Value.m_Param.f_GetAsType<CBuildSystemSyntax::CArray>(), _fFunctor);
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CArray &o_Value, tf_FFunctor &&_fFunctor)
	{
		for (auto &Value : o_Value.m_Array)
			fg_OperateOnStrings(Value.f_Get(), _fFunctor);
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CFunctionCall &o_Value, tf_FFunctor &&_fFunctor)
	{
		for (auto &Param : o_Value.m_Params)
			fg_OperateOnStrings(Param, _fFunctor);
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::COperator &o_Value, tf_FFunctor &&_fFunctor)
	{
		fg_OperateOnStrings(o_Value.m_Right.f_Get(), _fFunctor);
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CTypeDefaulted &o_Value, tf_FFunctor &&_fFunctor)
	{
		fg_OperateOnStrings(o_Value.m_DefaultValue, _fFunctor);
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CDefine &o_Value, tf_FFunctor &&_fFunctor)
	{
		if (o_Value.m_Type.f_IsDefaulted())
			fg_OperateOnStrings(o_Value.m_Type.m_Type.f_GetAsType<CBuildSystemSyntax::CTypeDefaulted>(), _fFunctor);
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CValue &o_Value, tf_FFunctor &&_fFunctor)
	{
		if (o_Value.f_IsConstantString())
			_fFunctor(o_Value.f_ConstantString(), o_Value.m_Value);
		else if (o_Value.f_IsConstant())
			;
		else if (o_Value.f_IsArray())
			fg_OperateOnStrings(o_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CArray>(), _fFunctor);
		else if (o_Value.f_IsEvalString())
			fg_OperateOnStrings(o_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CEvalString>(), _fFunctor);
		else if (o_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CExpression>())
			fg_OperateOnStrings(o_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CExpression>(), _fFunctor);
		else if (o_Value.m_Value.f_IsOfType<CBuildSystemSyntax::COperator>())
			fg_OperateOnStrings(o_Value.m_Value.f_GetAsType<CBuildSystemSyntax::COperator>(), _fFunctor);
		else if (o_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CDefine>())
			fg_OperateOnStrings(o_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CDefine>(), _fFunctor);
	}

	template <typename tf_FFunctor>
	void fg_OperateOnStrings(CBuildSystemSyntax::CRootValue &o_Value, tf_FFunctor &&_fFunctor)
	{
		fg_OperateOnStrings(o_Value.m_Value, _fFunctor);
	}

	void fg_MakeEvalStrings(CStringCache &o_StringCache, CBuildSystemSyntax::CRootValue &o_Value)
	{
		fg_OperateOnStrings
			(
				o_Value
				, [&](CStr const &_String, auto &o_Value)
				{
					if (_String.f_Find("@(") >= 0)
						o_Value = fg_ParseEvalString(o_StringCache, _String);
				}
			)
		;
	}

	CBuildSystemData::CImportData CBuildSystem::fp_ExpandImportCMake(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const
	{
		auto &EntityData = _Entity.f_Data();

		CStr CmakeCacheDirectory = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_CacheDirectory);

		CStr LockDirectory = CmakeCacheDirectory;

		CStr TempDirectory = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_TempDirectory);

		if (LockDirectory.f_IsEmpty())
			LockDirectory = TempDirectory;

		bool bUpdateCache = f_EvaluateEntityPropertyBool(_Entity, gc_ConstKey_Import_CMake_UpdateCache, false);
		bool bVerbose = f_EvaluateEntityPropertyBool(_Entity, gc_ConstKey_Import_CMake_Verbose, false);
		bool bVerboseHash = f_EvaluateEntityPropertyBool(_Entity, gc_ConstKey_Import_CMake_VerboseHash, false);
		bool bDiffHash = f_EvaluateEntityPropertyBool(_Entity, gc_ConstKey_Import_CMake_DiffHash, false);

#ifdef DPlatformFamily_Windows
		auto fStartsWith = [&](auto &&_String, auto &&_Find)
			{
				return _String.f_StartsWithNoCase(_Find);
			}
		;
#else
		auto fStartsWith = [&](auto &&_String, auto &&_Find)
			{
				return _String.f_StartsWith(_Find);
			}
		;
#endif

		// Dependent variables
		CStr GeneratorVersion = "38";
		CStr GeneratorFullRebuildVersion = "4";

		CStr FullRebuildVersion = "{}-{}"_f << f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_FullRebuildVersion) << GeneratorFullRebuildVersion;
		TCVector<CStr> CacheExcludePatterns = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_Import_CMake_CacheExcludePatterns, TCVector<CStr>());
		CEJSONSorted CacheReplaceContents = f_EvaluateEntityProperty(_Entity, gc_ConstKey_Import_CMake_CacheReplaceContents).f_Move();
		CEJSONSorted CacheDuplicateLines = f_EvaluateEntityProperty(_Entity, gc_ConstKey_Import_CMake_CacheDuplicateLines).f_Move();
		CEJSONSorted CmakeEnvironmentContents = f_EvaluateEntityProperty(_Entity, gc_ConstKey_Import_CMake_Environment).f_Move();
		TCVector<CStr> CacheIgnoreInputs = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_Import_CMake_CacheIgnoreInputs, TCVector<CStr>());
		CEJSONSorted CmakeLanguages = f_EvaluateEntityProperty(_Entity, gc_ConstKey_Import_CMake_Languages).f_Move();
		CStr CmakeConfig = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_Config);
		CStr IntermediateName = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_IntermediateName);
		CStr IntermediateNamePrefix = "/{}"_f << IntermediateName;
		CStr IntermediateNamePrefixDirectory = "/{}/"_f << IntermediateName;
		TCVector<CStr> CmakeVariables = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_Import_CMake_Variables, TCVector<CStr>());
		TCVector<CStr> CmakeIncludeInHash = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_Import_CMake_IncludeInHash, TCVector<CStr>());
		TCVector<CStr> CmakeExcludeFromHash = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_Import_CMake_ExcludeFromHash, TCVector<CStr>());

		CStr HashContents = fg_Format("Config (Not checked): {}\n", f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_FullConfiguration));

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
				if (!CacheDuplicateLines.f_Array().f_IsEmpty())
					fAddStringHash(o_DependenciesHash, "{}"_f << CacheDuplicateLines, "Import.CMake_CacheDuplicateLines", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CmakeEnvironmentContents, "Import.CMake_Environment", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CacheIgnoreInputs, "Import.CMake_CacheIgnoreInputs", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CmakeLanguages, "Import.CMake_Languages", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, CmakeConfig, "Import.CMake_Config", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CmakeVariables, "Import.CMake_Variables", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << CmakeIncludeInHash, "Import.CMake_IncludeInHash", _bPerformExclude);
				fAddStringHash(o_DependenciesHash, "{}"_f << IntermediateName, "Import.CMake_IntermediateName", _bPerformExclude);
			}
		;

		CStr FileName = CFile::fs_GetExpandedPath(_Entity.f_GetKeyName(), CFile::fs_GetPath(EntityData.m_Position.m_File));

		CStr SharedTempDirectory = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_SharedTempDirectory, CStr());

		CStr Platform = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Platform);
		CStr Architecture = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Architecture);
		TCVector<CStr> CmakePaths = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_Import_CMake_Path, TCVector<CStr>());
		TCVector<CStr> CmakeVariablesWithPaths = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_Import_CMake_VariablesWithPaths, TCVector<CStr>());
		CStr CmakeSystemName = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_SystemName, CStr());
		CStr CmakeSystemProcessor = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_SystemProcessor, CStr());
		CStr CmakeCompiler = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_CCompiler, CStr());
		CStr CmakeCompilerTarget = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_CCompilerTarget, CStr());
		CStr CmakeCxxCompiler = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_CxxCompiler, CStr());
		CStr CmakeCxxCompilerTarget = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_CxxCompilerTarget, CStr());
		CEJSONSorted CmakeReplacePrefixes = f_EvaluateEntityProperty(_Entity, gc_ConstKey_Import_CMake_ReplacePrefixes).f_Move();

		auto fInitConfigHash = [&](CHash_SHA512 &o_DependenciesHash)
			{
				fInitHash(o_DependenciesHash, false);
				fAddStringHash(o_DependenciesHash, FileName, gc_ConstString_FileName.m_String, false);
				fAddStringHash(o_DependenciesHash, SharedTempDirectory, "Import.SharedTempDirectory", false);
				fAddStringHash(o_DependenciesHash, Platform, gc_ConstString_Platform.m_String, false);
				fAddStringHash(o_DependenciesHash, Architecture, gc_ConstString_Architecture.m_String, false);
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

		CCMakeGenerateState *pCMakeGenerateState;
		{
			CStr Hash;
			{
				DLock(mp_CMakeGenerateLock);
				auto pHash = mp_CMakeGenerated.f_FindEqual(LockDirectory);
				if (pHash)
					Hash = *pHash;
				pCMakeGenerateState = &mp_CMakeGenerateState[LockDirectory];
			}
			if (Hash)
				return fReturn(LockDirectory, Hash);
		}
		DLock(pCMakeGenerateState->m_Lock);
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
				DUnlock(pCMakeGenerateState->m_Lock);
				return fReturn(LockDirectory, Hash);
			}
		}

		if (pCMakeGenerateState->m_bTried)
		{
			fsp_ThrowError
				(
					EntityData.m_Position
					, "CMake generation previously failed"
				)
			;
		}

		pCMakeGenerateState->m_bTried = true;

		auto SetInvalidGenerated = g_OnScopeExit / [&]
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
					f_OutputConsole("{}: WARNING: Import cache out of date (CMake), but updating has been disabled with Import.CMake_UpdateCache\n"_f << CmakeCacheDirectory);
				}
				{
					DLock(mp_CMakeGenerateLock);
					mp_CMakeGenerated[LockDirectory] = ConfigHashString;
					mp_CMakeGeneratedContents[LockDirectory] = ConfigHashContents;
				}
				DUnlock(pCMakeGenerateState->m_Lock);
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

			f_OutputConsole("{}: Import cache out of date (CMake)\n"_f << CmakeCacheDirectory);
		}
		else
			f_OutputConsole("{}: Import cache missing (CMake)\n"_f << CmakeCacheDirectory);

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
				fg_AddStrSep(PrefixesString, "{}={}"_f << Entry[gc_ConstString_Find].f_String() << Entry[gc_ConstString_Replace].f_String(), ";");
			LaunchParams.m_Environment["CMAKE_MALTERLIB_REPLACEPREFIXES"] = PrefixesString;
		}
		LaunchParams.m_Environment["CMAKE_MALTERLIB_TEMPDIR"] = TempDirectory;
		for (auto &KeyValue : CmakeEnvironmentContents.f_Object())
			LaunchParams.m_Environment[KeyValue.f_Name()] = KeyValue.f_Value().f_String();
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
			{
				LaunchParams.m_Environment[fg_Format("CMAKE_MALTERLIB_LANGUAGE_{}", Laungage[gc_ConstString_CMakeLanguage].f_String())]
					= Laungage[gc_ConstString_MalterlibLanguage].f_String()
				;
			}
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

		//Params.f_Insert("--debug-find");
		//Params.f_Insert("--debug-output");

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
				CStr SysRoot = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_SysRoot, CStr());
				if (!SysRoot.f_IsEmpty())
				{
					Params.f_Insert("-DCMAKE_FIND_ROOT_PATH=" + SysRoot);
					Params.f_Insert("-DCMAKE_SYSROOT=" + SysRoot);
					LaunchParams.m_Environment["PKG_CONFIG_SYSROOT_DIR"] = SysRoot;
					LaunchParams.m_Environment["PKG_CONFIG_LIBDIR"] = "{}:{}"_f << (SysRoot / "usr/lib/pkgconfig") << (SysRoot / "usr/share/pkgconfig");
					LaunchParams.m_Environment["PKG_CONFIG_PATH"] = "";
				}

				CStr CompilerExternalToolchain = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_CExternalToolChain, CStr());
				if (!CompilerExternalToolchain.f_IsEmpty())
					Params.f_Insert("-DCMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN=" + CompilerExternalToolchain);

				CStr CxxCompilerExternalToolchain = f_EvaluateEntityPropertyString(_Entity, gc_ConstKey_Import_CMake_CxxExternalToolChain, CStr());
				if (!CxxCompilerExternalToolchain.f_IsEmpty())
					Params.f_Insert("-DCMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN=" + CxxCompilerExternalToolchain);
			}
		}

		CFile::fs_CreateDirectory(TempDirectory);

		CStr FullRebuildVersionFile = TempDirectory + "/MalterlibFullRebuildVersion";

		CStr LastFullRebuildVersion;
		if (CFile::fs_FileExists(FullRebuildVersionFile))
			LastFullRebuildVersion = CFile::fs_ReadStringFromFile(FullRebuildVersionFile, true);

		bool bCmakeAlwaysFullRebuild = f_EvaluateEntityPropertyBool(_Entity, gc_ConstKey_Import_CMake_AlwaysFullRebuild, false);

		if (bCmakeAlwaysFullRebuild || FullRebuildVersion != LastFullRebuildVersion || CFile::fs_FileExists(TempDirectory / "failed"))
		{
			CFile::fs_DeleteDirectoryRecursive(TempDirectory);
			CFile::fs_CreateDirectory(TempDirectory);
		}

		LaunchParams.m_bShowLaunched = false;

		CClock Clock{true};
		uint32 ExitCode = 0;

		CStr Output;
		//f_OutputConsole("Launching CMake: {}\n"_f << CProcessLaunchParams::fs_GetParams(Params), false);

		if
			(
				!CProcessLaunch::fs_LaunchBlock
				(
					CmakeExecutable
					, Params
					, [&](NStr::CStr const &_Output)
					{
						if (bVerbose)
							f_OutputConsole(_Output, false);
						Output += _Output;
					}
					, [&](NStr::CStr const &_Output)
					{
						if (bVerbose)
							f_OutputConsole(_Output, true);
						Output += _Output;
					}
					, ExitCode
					, LaunchParams
				)
			)
		{
			CFile::fs_Touch(TempDirectory / "failed");

			if (bVerbose)
				DMibError(fg_Format("Failed to launch cmake"));
			else
				DMibError(fg_Format("Failed to launch cmake: {}", Output));
		}

		if (ExitCode)
		{
			CFile::fs_Touch(TempDirectory / "failed");

			if (bVerbose)
				DMibError(fg_Format("cmake failed"));
			else
				DMibError(fg_Format("cmake failed: {}", Output));
		}

		f_OutputConsole("{}: Running CMake took {fe1} s\n"_f << CmakeCacheDirectory << Clock.f_GetTime());

		if (!CmakeCacheDirectory.f_IsEmpty())
		{
			auto Cleanup = g_OnScopeExit / [&, StartTime = Clock.f_GetTime()]
				{
					f_OutputConsole("{}: Creating CMake cache took {fe1} s\n"_f << CmakeCacheDirectory << (Clock.f_GetTime() - StartTime));
				}
			;

			CFile::fs_CreateDirectory(CmakeCacheDirectory);

			CFile::CFindFilesOptions FindOptions{TempDirectory + "/*", true};
			FindOptions.m_AttribMask = EFileAttrib_File;
			{
				for (auto &ExcludePattern : CacheExcludePatterns)
					FindOptions.m_ExcludePatterns.f_Insert(ExcludePattern);
			}

			struct CReplaceContents
			{
				CStr m_Find;
				CStr m_Replace;
				TCSet<CStr> m_FilePatterns;
				TCSet<CStr> m_ExcludeFilePatterns;
				bool m_bApplyToPaths = false;
			};

			TCVector<CReplaceContents> ReplaceContents;
			{
				for (auto &Replace : CacheReplaceContents.f_Array())
				{
					ReplaceContents.f_Insert
						(
							{
								Replace[gc_ConstString_Find].f_String()
								, Replace[gc_ConstString_Replace].f_String()
								, fg_ConvertContainer<TCSet<CStr>>(Replace[gc_ConstString_FilePatterns].f_StringArray())
								, fg_ConvertContainer<TCSet<CStr>>(Replace[gc_ConstString_ExcludeFilePatterns].f_StringArray())
								, Replace[gc_ConstString_ApplyToPaths].f_Boolean()
							}
						)
					;
				}
			}

			struct CDuplicateLine
			{
				CStr m_Match;
				CStr m_Search;
				CStr m_Replace;
				TCSet<CStr> m_FilePatterns;
				TCSet<CStr> m_ExcludeFilePatterns;
			};

			TCVector<CDuplicateLine> DuplicateLines;
			{
				for (auto &Replace : CacheDuplicateLines.f_Array())
				{
					DuplicateLines.f_Insert
						(
							{
								Replace[gc_ConstString_Match].f_String()
								, Replace[gc_ConstString_Find].f_String()
								, Replace[gc_ConstString_Replace].f_String()
								, fg_ConvertContainer<TCSet<CStr>>(Replace[gc_ConstString_FilePatterns].f_StringArray())
							}
						)
					;
				}
			}

			TCSet<CStr> IgnoreInputs;
			{
				for (auto &ToIgnore : CacheIgnoreInputs)
				{
					if (ToIgnore)
						IgnoreInputs[ToIgnore];
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

			auto fMatchFile = [](CStr const &_File, TCSet<CStr> const &_Patterns, TCSet<CStr> const &_ExcludePatterns) -> bool
				{
					auto fCheckPattern = [&](TCSet<CStr> const &_Patterns, bool _bDefault)
						{
							if (_Patterns.f_IsEmpty())
								return _bDefault;

							for (auto &Pattern : _Patterns)
							{
								if (fg_StrMatchWildcard(_File.f_GetStr(), Pattern.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
									return true;
							}

							return false;
						}
					;

					bool bIncluded = fCheckPattern(_Patterns, true);
					if (!bIncluded)
						return false;

					bool bExcluded = fCheckPattern(_ExcludePatterns, false);
					return !bExcluded;
				}
			;

			auto fReplacePath = [&](CStr const &_Path) -> CStr
				{
					CMultiReplace::CStringMap MultiReplaceMapPath;

					for (auto &Replace : ReplaceContents)
					{
						if (!Replace.m_bApplyToPaths)
							continue;

						if (fMatchFile(_Path, Replace.m_FilePatterns, Replace.m_ExcludeFilePatterns))
						{
							if (Replace.m_bApplyToPaths)
								MultiReplaceMapPath[Replace.m_Find] = Replace.m_Replace;
						}
					}

					CMultiReplace MultiReplacePath(fg_Move(MultiReplaceMapPath));
					return MultiReplacePath.f_Replace(_Path);
				}
			;

			CMultiReplace::CStringMap OutputFilesMultiReplaceMap;
			{
				CStr FileContents = CFile::fs_ReadStringFromFile(TempDirectory / "OutputFiles.list", true);
				ch8 const *pParse = FileContents;
				while (*pParse)
				{
					auto pLineStart = pParse;
					fg_ParseToEndOfLine(pParse);
					CStr Line(pLineStart, pParse - pLineStart);
					fg_ParseEndOfLine(pParse);

					Line = fReplacePath(Line);

					OutputFilesMultiReplaceMap[Line];
#ifdef DPlatformFamily_Windows
					OutputFilesMultiReplaceMap[Line.f_Replace("/", "\\")];
					OutputFilesMultiReplaceMap[Line.f_Replace("/", "\\\\")];
#endif
				}
			}
			CMultiReplace OutputFilesMultiReplace(fg_Move(OutputFilesMultiReplaceMap));

			CMultiReplace::CStringMap ProtectedFilesMultiReplaceMap;
			{
				CStr FileContents = CFile::fs_ReadStringFromFile(TempDirectory / "ProtectedFiles.list", true);
				ch8 const *pParse = FileContents;
				while (*pParse)
				{
					auto pLineStart = pParse;
					fg_ParseToEndOfLine(pParse);
					CStr Line(pLineStart, pParse - pLineStart);
					fg_ParseEndOfLine(pParse);

					Line = fReplacePath(Line);

					ProtectedFilesMultiReplaceMap[Line];
#ifdef DPlatformFamily_Windows
					ProtectedFilesMultiReplaceMap[Line.f_Replace("/", "\\")];
					ProtectedFilesMultiReplaceMap[Line.f_Replace("/", "\\\\")];
#endif
				}
			}
			CMultiReplace ProtectedFilesMultiReplace(fg_Move(ProtectedFilesMultiReplaceMap));

			TCSet<CStr> CppExtensions = {"h", "hpp", "hxx", "ipp", "cpp", "c", "cxx"};

			for (auto &File : FoundFiles)
			{
				auto StartTime = Clock.f_GetTime();
				CStr Extension = CFile::fs_GetExtension(File.m_Path);
				CStr FileDirectory = CFile::fs_GetPath(File.m_Path);
				bool bIsMHeader = Extension == "MHeader";
				bool bIsCpp = !!CppExtensions.f_FindEqual(Extension);

				CStr RelativePath = File.m_Path.f_Extract(PathPrefixLen);
				CMultiReplace::CStringMap MultiReplaceMap;
				{
					CMultiReplace::CStringMap MultiReplaceMapPath;

					for (auto &Replace : ReplaceContents)
					{
						if (fMatchFile(File.m_Path, Replace.m_FilePatterns, Replace.m_ExcludeFilePatterns))
						{
							MultiReplaceMap[Replace.m_Find] = Replace.m_Replace;
							if (Replace.m_bApplyToPaths)
								MultiReplaceMapPath[Replace.m_Find] = Replace.m_Replace;
						}
					}

					CMultiReplace MultiReplacePath(fg_Move(MultiReplaceMapPath));
					RelativePath = MultiReplacePath.f_Replace(RelativePath);
				}

				CMultiReplace MultiReplace(fg_Move(MultiReplaceMap));

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

				if (bIsMHeader)
				{
					RelativeSourceBare = CStr("@('{}'->MakeAbsolute())"_f << RelativeSourceBare);
					RelativeDestBare = CStr("@('{}'->MakeAbsolute())"_f << RelativeDestBare);
					RelativeBaseBare = CStr("@('{}'->MakeAbsolute())"_f << RelativeBaseBare);

					if (!RelativeSource.f_IsEmpty())
						RelativeSource = CStr("@('{}'->MakeAbsolute())/"_f << RelativeSource);
					else
						RelativeSource = "@('.'->MakeAbsolute())/";

					if (!RelativeDest.f_IsEmpty())
						RelativeDest = CStr("@('{}'->MakeAbsolute())/"_f << RelativeDest);
					else
						RelativeDest = "@('.'->MakeAbsolute())/";

					if (!RelativeBase.f_IsEmpty())
						RelativeBase = CStr("@('{}'->MakeAbsolute())/"_f << RelativeBase);
					else
						RelativeBase = "@('.'->MakeAbsolute())/";
				}
				else
				{
					if (!RelativeSource.f_IsEmpty())
						RelativeSource += "/";
					if (!RelativeDest.f_IsEmpty())
						RelativeDest += "/";
					if (!RelativeBase.f_IsEmpty())
						RelativeBase += "/";
				}

				CStr FileContents = CFile::fs_ReadStringFromFile(File.m_Path, true);

				if (Extension == "dependencies")
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

				FileContents = MultiReplace.f_Replace(FileContents);

				if (bIsCpp)
				{
					CStr NewFileContents;
					auto *pParse = FileContents.f_GetStr();
					while (*pParse)
					{
						auto pLineStart = pParse;
						fg_ParseToEndOfLine(pParse);
						CStr Line(pLineStart, pParse - pLineStart);
						fg_ParseEndOfLine(pParse);

						auto Cleanup = g_OnScopeExit / [&, pParse]
							{
								NewFileContents.f_AddStr(pLineStart, pParse - pLineStart);
							}
						;

						Line = Line.f_Trim();
						auto pParseLine = Line.f_GetStr();

						if (*pParseLine != '#')
							continue;

						++pParseLine;

						fg_ParseWhiteSpace(pParseLine);

						if (!fg_StrStartsWith(pParseLine, "include"))
							continue;

						pParseLine += 7;

						fg_ParseWhiteSpace(pParseLine);

						if (*pParseLine != '"')
							continue;

						++pParseLine;

						auto pStartFileName = pParseLine;

						while (*pParseLine && *pParseLine != '"')
							++pParseLine;

						if (*pParseLine != '"')
							continue;

						CStr FileName(pStartFileName, pParseLine - pStartFileName);
						CStr ExpandedFileName = CFile::fs_GetExpandedPath(FileName, FileDirectory);
						if (!fStartsWith(ExpandedFileName, TempDirectory))
							continue;

						CStr RelativePath = CFile::fs_MakePathRelative(ExpandedFileName, TempDirectory);

						Cleanup.f_Clear();

						NewFileContents += "#include \"{}\"\n"_f << RelativePath;
					}

					FileContents = fg_Move(NewFileContents);
				}

				if (!OutputFilesMultiReplace.f_IsEmpty())
				{
					FileContents = OutputFilesMultiReplace.f_Replace
						(
							FileContents
							, [&](CStr &o_String, ch8 const * &o_pParse, CStr const &_ToFind, CStr const &_ReplaceWith) -> bool
							{
								if (ProtectedFilesMultiReplace.f_StringMatches(o_pParse))
									return false;

								auto pParseEnd = o_pParse + _ToFind.f_GetLen();
								static constexpr auto pEndChars = "/\"' ,\t\r\n";
								if (!*pParseEnd || fg_StrFindChar(pEndChars, *pParseEnd) >= 0)
								{
									auto RelativePath = CFile::fs_MakePathRelative(_ToFind, TempDirectory);
									o_String += "{}(CMakeIntermediateDirectory)/{}/{}"_f << (bIsMHeader ? "@" : "#")<< IntermediateName << RelativePath;
									if (*pParseEnd)
									{
										o_String.f_AddChar(*pParseEnd);
										o_pParse = pParseEnd + 1;
									}
									else
										o_pParse = pParseEnd;

									return true;
								}

								return false;
							}
						)
					;
				}

				{
					for (auto &DuplicateLine : DuplicateLines)
					{
						if (!fMatchFile(File.m_Path, DuplicateLine.m_FilePatterns, DuplicateLine.m_ExcludeFilePatterns))
							continue;

						CStr NewContents;

						auto *pParse = FileContents.f_GetStr();

						while (*pParse)
						{
							auto pLineStart = pParse;
							fg_ParseToEndOfLine(pParse);
							CStr Line(pLineStart, pParse - pLineStart);
							fg_ParseEndOfLine(pParse);

							NewContents += Line;
							NewContents += "\n";

							if (fg_StrMatchWildcard(Line.f_GetStr(), DuplicateLine.m_Match.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							{
								NewContents += Line.f_Replace(DuplicateLine.m_Search, DuplicateLine.m_Replace);
								NewContents += "\n";
							}
						}

						FileContents = fg_Move(NewContents);
					}
				}

				{
					{
						CMultiReplace::CStringMap MultiReplaceMap;
						MultiReplaceMap[TempDirectoryFind] = RelativeDest;
						MultiReplaceMap[SourceBaseFind] = RelativeSource;
						MultiReplaceMap[BaseDirFind] = RelativeBase;
						MultiReplaceMap[TempDirectory] = RelativeDestBare;
						MultiReplaceMap[SourceBase] = RelativeSourceBare;
						MultiReplaceMap[BaseDir] = RelativeBaseBare;
#ifdef DPlatformFamily_Windows
						MultiReplaceMap[TempDirectoryFind.f_Replace("/", "\\")] = RelativeDest.f_Replace("/", "\\");
						MultiReplaceMap[SourceBaseFind.f_Replace("/", "\\")] = RelativeSource.f_Replace("/", "\\");
						MultiReplaceMap[BaseDirFind.f_Replace("/", "\\")] = RelativeBase.f_Replace("/", "\\");
						MultiReplaceMap[TempDirectory.f_Replace("/", "\\")] = RelativeDestBare.f_Replace("/", "\\");
						MultiReplaceMap[SourceBase.f_Replace("/", "\\")] = RelativeSourceBare.f_Replace("/", "\\");
						MultiReplaceMap[BaseDir.f_Replace("/", "\\")] = RelativeBaseBare.f_Replace("/", "\\");
						MultiReplaceMap[TempDirectoryFind.f_Replace("/", "\\\\")] = RelativeDest.f_Replace("/", "\\\\");
						MultiReplaceMap[SourceBaseFind.f_Replace("/", "\\\\")] = RelativeSource.f_Replace("/", "\\\\");
						MultiReplaceMap[BaseDirFind.f_Replace("/", "\\\\")] = RelativeBase.f_Replace("/", "\\\\");
						MultiReplaceMap[TempDirectory.f_Replace("/", "\\\\")] = RelativeDestBare.f_Replace("/", "\\\\");
						MultiReplaceMap[SourceBase.f_Replace("/", "\\\\")] = RelativeSourceBare.f_Replace("/", "\\\\");
						MultiReplaceMap[BaseDir.f_Replace("/", "\\\\")] = RelativeBaseBare.f_Replace("/", "\\\\");
#endif
						CMultiReplace MultiReplace(fg_Move(MultiReplaceMap));
						FileContents = MultiReplace.f_Replace(FileContents);
					}
				}

				if (bIsMHeader)
				{
					auto fGetStripped = [&](CStr const &_String)
						{
							return CFile::fs_GetExpandedPath(_String, DestPathDirectory);
						}
					;

					CBuildSystemRegistry Registry;
					CStr PatchedFileName = File.m_Path + ".patched";
					CFile::fs_WriteStringToFile(PatchedFileName, FileContents, false);

					CBuildSystemRegistryParseContext Context(mp_StringCache);
					Registry.f_ParseStrWithContext(Context, FileContents, PatchedFileName);
#ifdef DPlatformFamily_Windows
					TCMap<CStr, CBuildSystemSyntax::CValue, CCompare_TStrNoCase> RemappedOutputs;
#else
					TCMap<CStr, CBuildSystemSyntax::CValue> RemappedOutputs;
#endif
					auto fParseString = [&](CStr const &_String) -> CBuildSystemSyntax::CValue
						{
							return {fg_ParseEvalString(mp_StringCache, _String)};
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

									if (Identifier.f_NameConstantString() == gc_ConstString_Custom_WorkingDirectory.m_String)
									{
										if (!Value.m_Value.m_Value.f_IsOfType<CBuildSystemSyntax::CExpression>())
											continue;

										auto &Expression = Value.m_Value.m_Value.f_GetAsType<CBuildSystemSyntax::CExpression>();

										if (!Expression.m_Expression.f_IsOfType<CBuildSystemSyntax::CFunctionCall>())
											continue;

										auto &FunctionCall = Expression.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
										if (FunctionCall.m_Params.f_GetLen() != 1)
											continue;

										auto &Param = FunctionCall.m_Params[0];
										if (!Param.m_Param.f_IsOfType<CEJSONSorted>())
											continue;

										if (!Param.f_IsJson() || !Param.f_Json().f_IsString())
											continue;

										auto &ParamValue = Param.m_Param.f_GetAsType<CEJSONSorted>();
										WorkingDirectorySource = ParamValue.f_String();
									}

									if (Identifier.f_NameConstantString() == gc_ConstString_Custom_Outputs.m_String)
									{
										if (Child.f_GetThisValue().m_Value.f_IsArray())
										{
											for (auto &Output : Child.f_GetThisValue().m_Value.f_Array().m_Array)
											{
												if (!Output.f_Get().m_Value.f_IsOfType<CBuildSystemSyntax::CExpression>())
													continue;

												auto &Expression = Output.f_Get().m_Value.f_GetAsType<CBuildSystemSyntax::CExpression>();
												if (!Expression.m_Expression.f_IsOfType<CBuildSystemSyntax::CFunctionCall>())
													continue;

												auto &FunctionCall = Expression.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
												if (FunctionCall.m_Params.f_GetLen() != 1)
													continue;

												auto &Param = FunctionCall.m_Params[0];

												if (!Param.f_IsJson() || !Param.f_Json().f_IsString())
													continue;

												OriginalOutputs.f_Insert(&fg_RemoveQualifiers(Param));
											}
										}
									}

									if (Identifier.f_NameConstantString() == gc_ConstString_Custom_CommandLine.m_String)
									{
										for (auto &Token : Child.f_GetThisValue().m_Value.f_EvalString().m_Tokens)
										{
											if (Token.f_IsExpression())
											{
												auto &Expression = Token.f_Expression();
												if (!Expression.f_IsParam())
													continue;

												auto &Param0 = Expression.f_Param();
												if (!Param0.f_IsExpression())
													continue;

												auto &Expression0 = Param0.f_Expression();

												if (!Expression0.f_IsFunctionCall())
													continue;

												auto &FunctionCall0 = Expression0.f_FunctionCall();

												if (FunctionCall0.m_Params.f_GetLen() != 1)
													continue;

												auto &Param1 = FunctionCall0.m_Params[0];

												if (!Param1.f_IsExpression())
													continue;

												auto &Expression1 = Param1.f_Expression();

												if (!Expression1.f_IsFunctionCall())
													continue;

												auto &FunctionCall1 = Expression1.f_FunctionCall();

												if (FunctionCall1.m_Params.f_GetLen() != 1)
													continue;

												auto &Param = FunctionCall1.m_Params[0];

												if (!Param.f_IsJson() || !Param.f_Json().f_IsString())
													continue;

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
									auto &ParamString = pParam->f_Json().f_String();
									if (ParamString.f_StartsWith("@("))
										continue;

									CStr WorkingDirParam = CFile::fs_GetExpandedPath(ParamString, WorkingDirectory);
									CStr StrippedParam = fGetStripped(ParamString);
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
										auto NewValue = fParseString(fg_Format("`@(CMakeIntermediateDirectory)/{}/{}`", IntermediateName, ParamString));
										RemappedOutputs[StrippedParam] = NewValue;
										*OriginalOutputs[iOutput] = *pParam = CBuildSystemSyntax::CParam{NewValue.f_EvalString()};
									}
									else if (pWorkingDirOutput)
									{
										auto NewValue = fParseString(fg_Format("`@(CMakeIntermediateDirectory)/{}/{}/{}`", IntermediateName, RelativeWorkingDir, ParamString));
										RemappedOutputs[StrippedParam] = NewValue;
										*OriginalOutputs[iOutput] = *pParam = CBuildSystemSyntax::CParam{NewValue.f_EvalString()};
									}
								}
							}
						)
					;

					TCMap<CStr, CImportTarget> Targets;
					TCMap<CStr, TCLinkedList<CImportTarget *>> OutputToTarget;
					TCMap<CStr, TCLinkedList<CImportTarget::CDependency *>> TargetToDependencies;

					CImportTarget NextTarget;

					Registry.f_TransformFunc
						(
							[&](CBuildSystemRegistry &o_This)
							{
								fg_MakeEvalStrings(mp_StringCache, o_This.f_GetThisValue());

								do
								{
									auto &Name = o_This.f_GetName();

									if (!Name.f_IsKeyPrefixOperator())
										break;

									auto &PrefixOperator = Name.f_KeyPrefixOperator();

									if (PrefixOperator.m_Operator != CBuildSystemSyntax::CKeyPrefixOperator::EOperator_Entity)
										break;

									if (!PrefixOperator.m_Right.f_IsIdentifier())
										break;

									auto &Identifier = PrefixOperator.m_Right.f_Identifier();

									if (!Identifier.f_IsNameConstantString())
										break;

									auto &Value = o_This.f_GetThisValue();
									if (!Value.m_Value.f_IsConstantString())
										break;

									if (Identifier.f_NameConstantString() == gc_ConstString_Target.m_String)
									{
										auto &Target = Targets[Value.m_Value.f_ConstantString()] = fg_Move(NextTarget);
										Target.m_pBuildSystemRegistry = &o_This;
										Target.m_pNameValue = &Value.m_Value;
										Target.m_pBaseName = o_This.f_GetChildNoPath
											(
												CBuildSystemSyntax::CRootKey{CBuildSystemSyntax::CValue::fs_Identifier(mp_StringCache, "BaseName", EPropertyType_Target)}
											)
										;
									}
									else if (Identifier.f_NameConstantString() == gc_ConstString_Dependency.m_String)
									{
										auto &Dependency = NextTarget.m_Dependencies[Value.m_Value.f_ConstantString()];
										Dependency.m_pValue = &Value.m_Value;
										Dependency.m_pBuildSystemRegistry = &o_This;
									}
								}
								while (false)
									;

								do
								{
									auto &Name = o_This.f_GetName();

									if (!Name.f_IsValue())
										break;

									auto &KeyValue = Name.m_Value.f_GetAsType<CBuildSystemSyntax::CValue>();

									if (!KeyValue.f_IsIdentifier())
										break;

									auto &Identifier = KeyValue.f_Identifier();

									if (!Identifier.f_IsNameConstantString())
										break;

									auto &Value = o_This.f_GetThisValue();
									if (!Value.m_Value.f_IsArray())
										break;

									if (Identifier.f_NameConstantString() == gc_ConstString_Custom_Outputs.m_String)
									{
										if (Value.m_Value.f_IsArray())
										{
											for (auto &Entry : Value.m_Value.f_Array().m_Array)
												NextTarget.m_Outputs[CStr::fs_ToStr(Entry)];
										}
										else if (Value.m_Value.f_IsConstant() && Value.m_Value.f_Constant().f_IsArray())
										{
											for (auto &Entry : Value.m_Value.f_Constant().f_Array())
												NextTarget.m_Outputs[CStr::fs_ToStr(Entry)];
										}
									}
									else if (Identifier.f_NameConstantString() == gc_ConstString_Custom_Inputs.m_String)
									{
										if (Value.m_Value.f_IsArray())
										{
											for (auto &Entry : Value.m_Value.f_Array().m_Array)
												NextTarget.m_Inputs[CStr::fs_ToStr(Entry)];
										}
										else if (Value.m_Value.f_IsConstant() && Value.m_Value.f_Constant().f_IsArray())
										{
											for (auto &Entry : Value.m_Value.f_Constant().f_Array())
												NextTarget.m_Inputs[CStr::fs_ToStr(Entry)];
										}
									}
								}
								while (false)
									;

								do
								{
									auto Location = o_This.f_GetLocation();
									auto &Name = o_This.f_GetName();

									if (!Name.f_IsValue())
										break;

									auto &KeyValue = Name.m_Value.f_GetAsType<CBuildSystemSyntax::CValue>();

									if (!KeyValue.f_IsIdentifier())
										break;

									auto &Identifier = KeyValue.f_Identifier();

									if (!Identifier.f_IsNameConstantString())
										break;

									auto &Value = o_This.f_GetThisValue();

									if (Identifier.f_NameConstantString() == gc_ConstString_SearchPath.m_String)
									{
										if (!Value.m_Value.m_Value.f_IsOfType<CBuildSystemSyntax::COperator>())
											break;

										auto &Operator = Value.m_Value.m_Value.f_GetAsType<CBuildSystemSyntax::COperator>();

										if (!Operator.m_Right.f_Get().f_IsArray())
											break;

										auto &Array = Operator.m_Right.f_Get().m_Value.f_GetAsType<CBuildSystemSyntax::CArray>();

										NContainer::TCVector<NStorage::TCIndirection<CBuildSystemSyntax::CValue>> NewValues;

										for (auto &EntryIndirection : Array.m_Array)
										{
											auto AddEntryScope = g_OnScopeExit / [&]
												{
													NewValues.f_Insert(fg_Move(EntryIndirection));
												}
											;

											auto &Entry = EntryIndirection.f_Get();
											if (!Entry.f_IsExpression())
												continue;

											auto &Expression = Entry.m_Value.f_GetAsType<CBuildSystemSyntax::CExpression>();
											if (!Expression.f_IsFunctionCall())
												continue;

											auto &FunctionCall = Expression.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
											if (FunctionCall.m_Params.f_GetLen() != 1)
												continue;

											auto &Param = FunctionCall.m_Params[0];
											if (!Param.f_IsEvalString())
												continue;

											auto &EvalString = Param.m_Param.f_GetAsType<CBuildSystemSyntax::CEvalString>();

											if (EvalString.m_Tokens.f_GetLen() != 2)
												continue;

											auto &Token0 = EvalString.m_Tokens[0];
											auto &Token1 = EvalString.m_Tokens[1];

											if (!Token0.f_IsExpression() || !Token1.f_IsString())
												continue;

											auto &Token0Expression = Token0.m_Token.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>().f_Get();

											if (!Token0Expression.f_IsParam())
												continue;

											auto &Token0Param = Token0Expression.m_Expression.f_GetAsType<CBuildSystemSyntax::CParam>();
											if (Token0Param.f_IsIdentifier())
											{
												auto &Identifier = Token0Param.m_Param.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
												if
													(
														Identifier.m_Name
														!= gc_ConstString_CMakeIntermediateDirectory
													)
												{
													continue;
												}

												CStr FullPath;

												if (fStartsWith(Token1.f_String(), IntermediateNamePrefixDirectory))
													FullPath = CmakeCacheDirectory / Token1.f_String().f_Extract(IntermediateNamePrefixDirectory.f_GetLen());
												else if (Token1.f_String() == IntermediateNamePrefix)
													FullPath = CmakeCacheDirectory;
												else
													continue;

												NewValues.f_Insert(EntryIndirection);

												CStr RelativePath = CFile::fs_MakePathRelative(FullPath, DestPathDirectory);

												Expression = CBuildSystemSyntax::CExpression
													{
														CBuildSystemSyntax::CFunctionCall
														{
															{
																CBuildSystemSyntax::CParam
																{
																	CEJSONSorted(RelativePath)
																}
															}
															, CPropertyKey(mp_StringCache, EPropertyType_Property, gc_ConstString_MakeAbsolute)
															, true
															, true
														}
													}
												;
											}
											else if (Token0Param.f_IsExpression())
											{
												auto &Token0InnerExpression = Token0Param.m_Param.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>().f_Get();
												if (!Token0InnerExpression.f_IsFunctionCall())
													continue;

												auto &InnerFunctionCall = Token0InnerExpression.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();

												if (InnerFunctionCall.m_Params.f_GetLen() != 1)
													continue;

												auto &InnerParam = InnerFunctionCall.m_Params[0];
												if (!InnerParam.f_IsJson() || !InnerParam.f_Json().f_IsString())
													continue;

												auto &Path = InnerParam.f_Json().f_String();

												auto StrippedPath = fGetStripped(Path);
												if (fStartsWith(StrippedPath, CmakeCacheDirectory))
												{
													CStr FullPath = StrippedPath / Token1.f_String();
													auto RelativePath = CFile::fs_MakePathRelative(FullPath, CmakeCacheDirectory);

													NewValues.f_Insert(EntryIndirection);

													CBuildSystemSyntax::CIdentifier Identifier;
													Identifier.m_Name = gc_ConstString_CMakeIntermediateDirectory;

													Token0Expression.m_Expression = CBuildSystemSyntax::CParam{NStorage::TCIndirection<CBuildSystemSyntax::CIdentifier>{fg_Move(Identifier)}};
													Token1.m_Token = CStr("/{}{}"_f << IntermediateName << RelativePath);
												}
											}
										}

										Array.m_Array = fg_Move(NewValues);
									}
								}
								while (false)
									;

								if (o_This.f_GetThisValue().m_Value.f_IsConstantString())
								{
									auto &OriginalValue = o_This.f_GetThisValue().m_Value.f_ConstantString();

									CStr ExpandedPath = fGetStripped(OriginalValue);

									auto *pRemapped = RemappedOutputs.f_FindEqual(ExpandedPath);
									if (pRemapped)
										o_This.f_SetThisValue(CBuildSystemSyntax::CRootValue{*pRemapped});
									else
										fg_MakeEvalStrings(mp_StringCache, o_This.f_GetThisValue());
								}
							}
						)
					;

					if (CFile::fs_GetPath(File.m_Path) == TempDirectory)
					{
						for (auto &Target : Targets)
						{
							for (auto &Output : Target.m_Outputs)
								OutputToTarget[Output].f_Insert(&Target);
						}

						for (auto &Target : Targets)
						{
							TCSet<CStr> ResolvedInputs;
							TCSet<CStr> CheckedDependencies;

							auto fAddDependencies = [&](CImportTarget const &_Target, auto &&_fAddDependencies) -> void
								{
									ResolvedInputs += _Target.m_Outputs;

									for (auto &Dependency : _Target.m_Dependencies)
									{
										auto &DependencyName = _Target.m_Dependencies.fs_GetKey(Dependency);
										if (!CheckedDependencies(DependencyName).f_WasCreated())
											continue;

										auto *pDependency = Targets.f_FindEqual(DependencyName);
										if (!pDependency)
										{
											fsp_ThrowError
												(
													EntityData.m_Position
													, "Colud not resolve dependency {} in target {}"_f << DependencyName << Targets.fs_GetKey(Target)
												)
											;
										}
										// Only consider direct dependencies
										//_fAddDependencies(*pDependency, _fAddDependencies);
									}
								}
							;

							fAddDependencies(Target, fAddDependencies);

							for (auto &Input : Target.m_Inputs)
							{
								if (ResolvedInputs.f_FindEqual(Input))
									continue;

								auto *pDependencyTargets = OutputToTarget.f_FindEqual(Input);
								if (!pDependencyTargets)
									continue;

								for (auto &pDependencyTarget : *pDependencyTargets)
								{
									auto &TargetName = Targets.fs_GetKey(*pDependencyTarget);
									if (TargetName == Targets.fs_GetKey(Target))
										continue;

									auto pExistingDependency = Target.m_Dependencies.f_FindEqual(TargetName);
									if (pExistingDependency)
									{
										if (!pExistingDependency->m_bIsOrdered)
										{
											pExistingDependency->m_bIsOrdered = true;
											if
												(
													pExistingDependency->m_pBuildSystemRegistry->f_GetChildNoPath
													(
														{CBuildSystemSyntax::CValue::fs_Identifier(mp_StringCache, gc_ConstString_Indirect, EPropertyType_Dependency)}
													)
												)
											{
												pExistingDependency->m_pBuildSystemRegistry
													->f_CreateChildNoPath
													(
														{CBuildSystemSyntax::CValue::fs_Identifier(mp_StringCache, gc_ConstString_IndirectOrdered, EPropertyType_Dependency)}
													)
													->f_SetThisValue({true})
												;
												pExistingDependency->m_pBuildSystemRegistry
													->f_CreateChildNoPath({CBuildSystemSyntax::CValue::fs_Identifier(mp_StringCache, gc_ConstString_Link, EPropertyType_Dependency)})
													->f_SetThisValue({false})
												;
											}
										}
										continue;
									}

									fAddDependencies(*pDependencyTarget, fAddDependencies);

									f_OutputConsole
										(
											"{}: Adding missing dependency:\n"
											"	Target             : {}\n"
											"	Missing Dependency : {}\n"
											"	File               : {}\n"_f
											<< CmakeCacheDirectory
											<< Targets.fs_GetKey(Target)
											<< TargetName
											<< Input
										)
									;

									Target.m_Dependencies[TargetName];
									CBuildSystemSyntax::CRootKey Name = {CBuildSystemSyntax::CKeyPrefixOperator::fs_Entity(mp_StringCache, gc_ConstString_Dependency)};

									auto pNewDependency = Target.m_pBuildSystemRegistry->f_CreateChildNoPath(Name);
									pNewDependency->f_SetThisValue(CBuildSystemSyntax::CRootValue{TargetName});
									if (TargetName.f_StartsWith("Lib_"))
									{
										pNewDependency->f_CreateChildNoPath
											(
												{CBuildSystemSyntax::CValue::fs_Identifier(mp_StringCache, gc_ConstString_IndirectOrdered, EPropertyType_Dependency)}
											)
											->f_SetThisValue({true})
										;
										pNewDependency->f_CreateChildNoPath
											(
												{CBuildSystemSyntax::CValue::fs_Identifier(mp_StringCache, gc_ConstString_Link, EPropertyType_Dependency)}
											)
											->f_SetThisValue({false})
									;
									}

									break;
								}
							}
						}

						for (auto &Target : Targets)
						{
							for (auto &Dependency : Target.m_Dependencies)
							{
								auto &DependencyName = Target.m_Dependencies.fs_GetKey(Dependency);
								TargetToDependencies[DependencyName].f_Insert(&Dependency);
							}
						}

						TCSet<CStr, CCompare_TStrNoCase> TargetsNoCase;

						for (auto &Target : Targets)
						{
							auto &TargetName = Targets.fs_GetKey(Target);

							if (TargetsNoCase(TargetName).f_WasCreated())
								continue;

							CStr DisambiguatedName = TargetName;
							mint DisambiguateNumber = 1;
							while (TargetsNoCase.f_FindEqual(DisambiguatedName))
								DisambiguatedName = "{}{}"_f << TargetName << DisambiguateNumber;

							TargetsNoCase[DisambiguatedName];

							*Target.m_pNameValue = {DisambiguatedName};

							if (Target.m_pBaseName)
								Target.m_pBaseName->f_SetThisValue({CStr("{}{}"_f << Target.m_pBaseName->f_GetThisValue().m_Value.f_ConstantString() << DisambiguateNumber)});
							else
								DMibConOut2("Missing base name: {}\n", DisambiguatedName);


							f_OutputConsole
								(
									"{}: Disambiguating target only differing by case:\n"
									"	Old name: {}\n"
									"	New name: {}\n"_f
									<< CmakeCacheDirectory
									<< TargetName
									<< DisambiguatedName
								)
							;

							auto *pDependencies = TargetToDependencies.f_FindEqual(TargetName);
							if (pDependencies)
							{
								for (auto &pDependency : *pDependencies)
									*pDependency->m_pValue = {DisambiguatedName};
							}
						}
					}

					FileContents = Registry.f_GenerateStr();
				}
				else if (Extension == "dependencies")
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

			{
				CByteVector BinaryFileContents;
				CFile::fs_WriteStringToVector(BinaryFileContents, HashContents, false);
				f_WriteFile(BinaryFileContents, LastHashContentsFile);
				WrittenFiles[LastHashContentsFile];
			}

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

		CFile::fs_WriteStringToFile(FullRebuildVersionFile, FullRebuildVersion, false);

		{
			{
				DLock(mp_CMakeGenerateLock);
				SetInvalidGenerated.f_Clear();
				mp_CMakeGenerated[LockDirectory] = ConfigHashString;
				mp_CMakeGeneratedContents[LockDirectory] = ConfigHashContents;
			}
			DUnlock(pCMakeGenerateState->m_Lock);
			return fReturn(LockDirectory, ConfigHashString);
		}
	}

	void CBuildSystem::fp_ExpandImport(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const
	{
		CStr FileName = _Entity.f_GetKeyName();

		auto &EntityData = _Entity.f_Data();
		if (!f_EvalCondition(_Entity, EntityData.m_Condition, EntityData.m_DebugFlags & EPropertyFlag_TraceCondition))
			return;

		CBuildSystemData::CImportData Import = [&]()
			{
				if (CFile::fs_GetFile(FileName) == "CMakeLists.txt")
					return fp_ExpandImportCMake(_Entity, _Entity, _BuildSystemData);
				else
				{
					CBuildSystemData::CImportData Import;
					CBuildSystemPreprocessor Preprocessor(Import.m_Registry, _BuildSystemData.m_MutableSourceFiles, mp_FindCache, mp_Environment, mp_StringCache);
					Preprocessor.f_ReadFile(FileName);
					_BuildSystemData.m_MutableSourceFiles[FileName];
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

			_ParentEntity.f_CopyProperties(fg_Move(Import.m_RootEntity));

			CEntity *pInsertAfter = nullptr;
			for (auto iEntity = Import.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity;)
			{
				auto &Entity = *iEntity;
				auto Key = Entity.f_GetKey();
				++iEntity;

				if (auto pOtherEntity = _Entity.m_ChildEntitiesMap.f_FindEqual(Key))
					_Entity.m_ChildEntitiesMap.f_Remove(pOtherEntity);

				_Entity.m_ChildEntitiesMap.f_ExtractAndInsert(Entity.m_pParent->m_ChildEntitiesMap, &Entity);
				Entity.m_pParent = &_Entity;
				if (pInsertAfter)
					_Entity.m_ChildEntitiesOrdered.f_InsertAfter(Entity, pInsertAfter);
				else
					_Entity.m_ChildEntitiesOrdered.f_Insert(Entity);

				pInsertAfter = &Entity;

				if (Key.m_Type != EEntityType_Target && Key.m_Type != EEntityType_Workspace)
					fpr_EvaluateData(Entity);
			}
		}
	}
}
