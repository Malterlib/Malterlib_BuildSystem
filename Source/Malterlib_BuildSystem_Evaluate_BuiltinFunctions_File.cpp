// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

namespace NMib::NBuildSystem
{
	NEncoding::CEJSON CBuildSystem::fp_BuiltinFunction_FindFiles
		(
			CEvalPropertyValueContext &_Context
			, NContainer::TCVector<NEncoding::CEJSON> &&_Params
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
				, fg_FunctionParam(g_String, "_Path")
				, fg_FunctionParam(g_String, "_Wildcard")
				, fg_FunctionParam(fg_Optional(g_StringArray), "_ExcludeWildcards", g_Optional)
			)
		;

		f_RegisterFunctions
			(
				{
					{
						"GeneratedFiles"
 						, CBuiltinFunction
						{
							fg_FunctionType(g_StringArray, fg_FunctionParam(g_String, "_Wildcard"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr Wildcard = _Params[0].f_String();

								TCVector<CEJSON> Return;
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
						}
					}
					,
					{
						"SourceFiles"
						, CBuiltinFunction
						{
							fg_FunctionType(g_StringArray, fg_FunctionParam(g_String, "_Wildcard"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr Wildcard = _Params[0].f_String();

								TCVector<CEJSON> Return;
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
						}
					}
					,
					{
						"ReadFile"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_FileName"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr FileName = _Params[0].f_String();

								CStr Ret = CFile::fs_ReadStringFromFile(FileName, true);

								_This.f_AddSourceFile(FileName);

								return fg_Move(Ret);
							}
						}
					}
					,
					{
						"FileExists"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_FileName"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CFile::fs_FileExists(_Params[0].f_String(), EFileAttrib_File);
							}
						}
					}
					,
					{
						"DirectoryExists"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_FileName"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CFile::fs_FileExists(_Params[0].f_String(), EFileAttrib_Directory);
							}
						}
					}
					,
					{
						"FileOrDirectoryExists"
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, "_FileName"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CFile::fs_FileExists(_Params[0].f_String());
							}
						}
					}
					,
					{
						"FindFilesIn"
						, CBuiltinFunction
						{
							FindFunctionType
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
 							{
								return _This.fp_BuiltinFunction_FindFiles(_Context, fg_Move(_Params), EFindFilesFunctionType_File);
							}
						}
					}
					,
					{
						"FindDirectoriesIn"
						, CBuiltinFunction
						{
							FindFunctionType
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _This.fp_BuiltinFunction_FindFiles(_Context, fg_Move(_Params), EFindFilesFunctionType_Directory);
							}
						}
					}
					,
					{
						"FindFilesRecursiveIn"
						, CBuiltinFunction
						{
							FindFunctionType
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _This.fp_BuiltinFunction_FindFiles(_Context, fg_Move(_Params), EFindFilesFunctionType_RecursiveFile);
							}
						}
					}
					,
					{
						"FindDirectoriesRecursiveIn"
						, CBuiltinFunction
						{
							FindFunctionType
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _This.fp_BuiltinFunction_FindFiles(_Context, fg_Move(_Params), EFindFilesFunctionType_RecursiveDirectory);
							}
						}
					}
				}
			)
		;
	}
}
