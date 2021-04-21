// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_RegisterBuiltinFunctions_Path()
	{
		f_RegisterFunctions
			(
				{
					{
						"RelativeBase"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr WholePath = _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
								return CFile::fs_MakePathRelative(WholePath, _This.mp_BaseDir);
							}
						}
					}
					,
					{
						"GetLastPaths"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"), fg_FunctionParam(g_Integer, "_NumPaths"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
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
						}
					}
					,
					{
						"RemoveStartPaths"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"), fg_FunctionParam(g_Integer, "_NumPaths"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr Path = _Params[0].f_String();

								int32 nPaths = _Params[1].f_Integer();

								CStr Value = CFile::fs_GetMalterlibPath(Path);
								while (nPaths--)
									fg_GetStrSep(Value, "/");

								return Value;
							}
						}
					}
					,
					{
						"GetPath"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CFile::fs_GetPath(_Params[0].f_String());
							}
						}
					}
					,
					{
						"GetFile"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CFile::fs_GetFile(_Params[0].f_String());
							}
						}
					}
					,
					{
						"GetExtension"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CFile::fs_GetExtension(_Params[0].f_String());
							}
						}
					}
					,
					{
						"GetFileNoExt"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CFile::fs_GetFileNoExt(_Params[0].f_String());
							}
						}
					}
					,
					{
						"GetDrive"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return CFile::fs_GetDrive(_Params[0].f_String());
							}
						}
					}
					,
					{
						"AppendPath"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"), fg_FunctionParam(g_String, "p_Paths", g_Ellipsis))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr OutputPath = _Params[0].f_String();

								for (auto &AppendPath : _Params[1].f_Array())
									OutputPath = CFile::fs_AppendPath(OutputPath, AppendPath.f_String());

								return OutputPath;
							}
						}
					}
					,
					{
						"MakeRelative"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"), fg_FunctionParam(g_String, "_Base"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								CStr WholePath = _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
								CStr WholePathBase = _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[1].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
								return CFile::fs_MakePathRelative(WholePath, WholePathBase);
							}
						}
					}
					,
					{
						"MakeAbsolute"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"), fg_FunctionParam(fg_Optional(g_String), "_Base", g_Optional))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								if (_Params[1].f_IsValid())
									return _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), _Params[1].f_String());
								else
									return _This.mp_GeneratorInterface->f_GetExpandedPath(_Params[0].f_String(), CFile::fs_GetPath(_Context.m_Position.m_File));
							}
						}
					}
					,
					{
						"WindowsPath"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_String().f_ReplaceChar('/', '\\');
							}
						}
					}
					,
					{
						"UnixPath"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_String().f_ReplaceChar('\\', '/');
 							}
						}
					}
					,
					{
						"NativePath"
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_String, "_Path"))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
#ifdef DPlatformFamily_Windows
								return _Params[0].f_String().f_ReplaceChar('/', '\\');
#else
								return _Params[0].f_String().f_ReplaceChar('\\', '/');
#endif
							}
						}
					}
				}
			)
		;
	}
}
