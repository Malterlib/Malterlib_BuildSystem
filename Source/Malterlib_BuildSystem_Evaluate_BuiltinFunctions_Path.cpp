// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsFilePath>

namespace
{
	void fg_ShortenPath(CStr &o_Path)
	{
		if (o_Path.f_GetLen() > 200)
		{
			CStr EndPath;
			while (!CFile::fs_FileExists(o_Path))
			{
				EndPath = CFile::fs_GetFile(o_Path) / EndPath;
				o_Path = CFile::fs_GetPath(o_Path);
			}

			if (o_Path)
				o_Path = NFile::NPlatform::fg_ConvertToShortWindowsPath<CWStr, CStr>(o_Path, false) / EndPath;
			else
				o_Path = EndPath;
		}
	}
}
#endif

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_RegisterBuiltinFunctions_Path()
	{
		f_RegisterFunctions
			(
				{
					{
						gc_ConstString_RelativeBase
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr WholePath = _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
								return CFile::fs_MakePathRelative(WholePath, _This.f_GetBaseDir());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_GetLastPaths
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path), fg_FunctionParam(g_Integer, gc_ConstString__NumPaths))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr Path = _Params[0].f_String();

								int32 nPaths = _Params[1].f_Integer();
								CStr Value;
								while (nPaths--)
								{
									if (Value.f_IsEmpty())
										Value = CFile::fs_GetFile(Path);
									else
										Value = CFile::fs_GetFile(Path) / Value;
									Path = CFile::fs_GetPath(Path);
								}

								return Value;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_RemoveStartPaths
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"), fg_FunctionParam(g_Integer, "_NumPaths"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr Path = _Params[0].f_String();

								int32 nPaths = _Params[1].f_Integer();

								CStr Value = CFile::fs_GetMalterlibPath(Path);
								while (nPaths--)
									fg_GetStrSep(Value, "/");

								return Value;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_GetPath
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return CFile::fs_GetPath(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_GetFile
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return CFile::fs_GetFile(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_GetExtension
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return CFile::fs_GetExtension(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_GetFileNoExt
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return CFile::fs_GetFileNoExt(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_GetDrive
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return CFile::fs_GetDrive(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_AppendPath
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path), fg_FunctionParam(g_String, gc_ConstString_p_Paths, g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr OutputPath = _Params[0].f_String();

								for (auto &AppendPath : _Params[1].f_Array())
									OutputPath = CFile::fs_AppendPath(OutputPath, AppendPath.f_String());

								return OutputPath;
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_MakeRelative
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path), fg_FunctionParam(g_String, gc_ConstString__Base))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CStr WholePath = _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
								CStr WholePathBase = _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[1].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
								return CFile::fs_MakePathRelative(WholePath, WholePathBase);
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_MakeAbsolute
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path), fg_FunctionParam(fg_Optional(g_String), gc_ConstString__Base, g_Optional))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								if (_Params[1].f_IsValid())
									return _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), _Params[1].f_String());
								else
									return _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_IsAbsolute
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return CFile::fs_IsPathAbsolute(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_WindowsPath
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_ReplaceChar('/', '\\');
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_UnixPath
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_String().f_ReplaceChar('\\', '/');
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_NativePath
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
#ifdef DPlatformFamily_Windows
								return _Params[0].f_String().f_ReplaceChar('/', '\\');
#else
								return _Params[0].f_String().f_ReplaceChar('\\', '/');
#endif
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_ShortenPath
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, gc_ConstString__Path))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto Return = fg_Move(_Params[0].f_String());
#ifdef DPlatformFamily_Windows
								fg_ShortenPath(Return);
#endif
								return fg_Move(Return);
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
