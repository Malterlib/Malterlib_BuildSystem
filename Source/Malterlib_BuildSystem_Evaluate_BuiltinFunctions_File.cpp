// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

namespace NMib::NBuildSystem
{
	NEncoding::CEJSONSorted CBuildSystem::fp_BuiltinFunction_FindFiles
		(
			CEvalPropertyValueContext &_Context
			, NContainer::TCVector<NEncoding::CEJSONSorted> &&_Params
			, EFindFilesFunctionType _Function
		) const
	{
		CStr WholePath = mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
		WholePath = WholePath / _Params[1].f_String();

		CFindOptions FindOptions(WholePath);

		switch (_Function)
		{
		case EFindFilesFunctionType_File:
			FindOptions.m_Attribs = EFileAttrib_File;
			break;
		case EFindFilesFunctionType_Directory:
			FindOptions.m_Attribs = EFileAttrib_Directory;
			break;
		case EFindFilesFunctionType_RecursiveFile:
			FindOptions.m_Attribs = EFileAttrib_File;
			FindOptions.m_bRecursive = true;
			break;
		case EFindFilesFunctionType_RecursiveDirectory:
			FindOptions.m_Attribs = EFileAttrib_Directory;
			FindOptions.m_bRecursive = true;
			break;
		}

		if (_Params.f_GetLen() > 2 && _Params[2].f_IsValid())
		{
			for (auto &Path : _Params[2].f_StringArray())
				FindOptions.m_Exclude[Path];
		}

		auto Files = mp_FindCache.f_FindFiles(FindOptions, true);

		TCVector<CStr> Return;
		for (auto &File : Files)
			Return.f_Insert(File.m_Path);

		return Return;
	}

	void CBuildSystem::fp_RegisterBuiltinFunctions_File()
	{
		auto FindFunctionType = fg_FunctionType
			(
				g_StringArray
				, fg_FunctionParam(g_String, gc_ConstString__Path)
				, fg_FunctionParam(g_String, gc_ConstString__Wildcard)
				, fg_FunctionParam(fg_Optional(g_StringArray), gc_ConstString__ExcludeWildcards, g_Optional)
			)
		;

		f_RegisterFunctions
			(
				{
					{
						gc_ConstKey_Builtin_GeneratedFiles.m_Name
						, CBuiltinFunction
						{
							fg_FunctionType(g_StringArray, fg_FunctionParam(g_String, gc_ConstString__Wildcard))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								CStr Wildcard = _Params[0].f_String();

								TCVector<CEJSONSorted> Return;
								{
									DMibLock(_This.mp_GeneratedFilesLock);

									for (auto iFile = _This.mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
									{
										if (iFile->m_bGeneral)
										{
											if (NStr::fg_StrMatchWildcard(iFile.f_GetKey().f_GetStr(), Wildcard.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
												Return.f_Insert(iFile.f_GetKey());
										}
									}
								}

								return fg_Move(Return);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_SourceFiles.m_Name
						, CBuiltinFunction
						{
							fg_FunctionType(g_StringArray, fg_FunctionParam(g_String, gc_ConstString__Wildcard))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								CStr Wildcard = _Params[0].f_String();

								TCVector<CEJSONSorted> Return;
								{
									DMibLockRead(_This.mp_SourceFilesLock);

									for (auto iFile = _This.mp_SourceFiles.f_GetIterator(); iFile; ++iFile)
									{
										if (NStr::fg_StrMatchWildcard(iFile->f_GetStr(), Wildcard.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
											Return.f_Insert(*iFile);
									}
								}

								return fg_Move(Return);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_ReadFile.m_Name
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__FileName))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								CStr FileName = _Params[0].f_String();

								CStr Ret = _This.f_ReadFile(FileName);
								_This.f_AddSourceFile(FileName);

								return fg_Move(Ret);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_FileExists.m_Name
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__FileName))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.mp_FindCache.f_FileExists(_Params[0].f_String(), EFileAttrib_File);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_LinkExists.m_Name
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__FileName))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.mp_FindCache.f_FileExists(_Params[0].f_String(), EFileAttrib_Link);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_ResolveSymbolicLink.m_Name
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__FileName))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.mp_FindCache.f_ResolveSymbolicLink(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_DirectoryExists.m_Name
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__FileName))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.mp_FindCache.f_FileExists(_Params[0].f_String(), EFileAttrib_Directory);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_FileOrDirectoryExists.m_Name
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__FileName))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.mp_FindCache.f_FileExists(_Params[0].f_String(), EFileAttrib_Directory | EFileAttrib_File);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_FindFilesIn.m_Name
						, CBuiltinFunction
						{
							FindFunctionType
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_FindFiles(_Context, fg_Move(_Params), EFindFilesFunctionType_File);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_FindDirectoriesIn.m_Name
						, CBuiltinFunction
						{
							FindFunctionType
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_FindFiles(_Context, fg_Move(_Params), EFindFilesFunctionType_Directory);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_FindFilesRecursiveIn.m_Name
						, CBuiltinFunction
						{
							FindFunctionType
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_FindFiles(_Context, fg_Move(_Params), EFindFilesFunctionType_RecursiveFile);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstKey_Builtin_FindDirectoriesRecursiveIn.m_Name
						, CBuiltinFunction
						{
							FindFunctionType
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
								return _This.fp_BuiltinFunction_FindFiles(_Context, fg_Move(_Params), EFindFilesFunctionType_RecursiveDirectory);
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
