// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#include <Mib/CommandLine/AnsiEncodingParse>

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_RegisterBuiltinFunctions_Color()
	{
		TCMap<CStr, CBuiltinFunction> Functions;

		{
			auto fAddFunction = [&](CStr const &_Name, auto &&_fRun)
				{
					Functions
						[
							_Name
							, fg_FunctionType(g_String)
							, [fRun = fg_Move(_fRun)](CBuildSystem const &_This, auto &_Context, auto &&_Params) -> CEJsonSorted
							{
								CAnsiEncoding Encoding(_This.f_AnsiFlags());
								return fRun(Encoding);
							}
							, DMibBuildSystemFilePosition
						]
					;
				}
			;

			fAddFunction(gc_Str<"ColorDefault">, [](auto &&_Enc){return _Enc.f_Default();});
			fAddFunction(gc_Str<"ColorStatusNormal">, [](auto &&_Enc){return _Enc.f_StatusNormal();});
			fAddFunction(gc_Str<"ColorStatusWarning">, [](auto &&_Enc){return _Enc.f_StatusWarning();});
			fAddFunction(gc_Str<"ColorStatusError">, [](auto &&_Enc){return _Enc.f_StatusError();});
			fAddFunction(gc_Str<"ColorBold">, [](auto &&_Enc){return _Enc.f_Bold();});
			fAddFunction(gc_Str<"ColorNotBold">, [](auto &&_Enc){return _Enc.f_NotBold();});
			fAddFunction(gc_Str<"ColorItalic">, [](auto &&_Enc){return _Enc.f_Italic();});
			fAddFunction(gc_Str<"ColorNotItalic">, [](auto &&_Enc){return _Enc.f_NotItalic();});

			fAddFunction(gc_Str<"ColorSyntaxNumber">, [](auto &&_Enc){return _Enc.f_SyntaxColor(NCommandLine::CAnsiEncoding::ESyntaxColor_Number);});
			fAddFunction(gc_Str<"ColorSyntaxString">, [](auto &&_Enc){return _Enc.f_SyntaxColor(NCommandLine::CAnsiEncoding::ESyntaxColor_String);});
			fAddFunction(gc_Str<"ColorSyntaxConstant">, [](auto &&_Enc){return _Enc.f_SyntaxColor(NCommandLine::CAnsiEncoding::ESyntaxColor_Constant);});
		}

		f_RegisterFunctions(fg_Move(Functions));

		f_RegisterFunctions
			(
				{
					{
						gc_Str<"ColorEnabled">
						, CBuiltinFunction
						{
							fg_FunctionType(g_Boolean)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CAnsiEncoding Encoding(_This.f_AnsiFlags());
								return Encoding.f_Color();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_Str<"ColorForeground16">
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Integer, gc_Str<"_ColorID">))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CAnsiEncoding Encoding(_This.f_AnsiFlags());
								return Encoding.f_Foreground16(fg_Clamp(_Params[0].f_Integer(), 0, 15));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_Str<"ColorBackground16">
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Integer, gc_Str<"_ColorID">))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CAnsiEncoding Encoding(_This.f_AnsiFlags());
								return Encoding.f_Background16(fg_Clamp(_Params[0].f_Integer(), 0, 15));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_Str<"ColorForeground256">
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Integer, gc_Str<"_ColorID">))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CAnsiEncoding Encoding(_This.f_AnsiFlags());
								return Encoding.f_Foreground256(fg_Clamp(_Params[0].f_Integer(), 0, 255));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_Str<"ColorBackground256">
						, CBuiltinFunction
						{
							fg_FunctionType(g_String, fg_FunctionParam(g_Integer, gc_Str<"_ColorID">))
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CAnsiEncoding Encoding(_This.f_AnsiFlags());
								return Encoding.f_Background256(fg_Clamp(_Params[0].f_Integer(), 0, 255));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_Str<"ColorForegroundRGB">
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_Integer, gc_Str<"_ColorR">), fg_FunctionParam(g_Integer, gc_Str<"_ColorG">), fg_FunctionParam(g_Integer, gc_Str<"_ColorB">)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CAnsiEncoding Encoding(_This.f_AnsiFlags());
								return Encoding.f_ForegroundRGB(fg_Clamp(_Params[0].f_Integer(), 0, 255), fg_Clamp(_Params[1].f_Integer(), 0, 255), fg_Clamp(_Params[2].f_Integer(), 0, 255));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_Str<"ColorBackgroundRGB">
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_Integer, gc_Str<"_ColorR">), fg_FunctionParam(g_Integer, gc_Str<"_ColorG">), fg_FunctionParam(g_Integer, gc_Str<"_ColorB">)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								CAnsiEncoding Encoding(_This.f_AnsiFlags());
								return Encoding.f_BackgroundRGB(fg_Clamp(_Params[0].f_Integer(), 0, 255), fg_Clamp(_Params[1].f_Integer(), 0, 255), fg_Clamp(_Params[2].f_Integer(), 0, 255));
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_Str<"ColorStrip">
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_Str<"_String">)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return CAnsiEncodingParse::fs_StripEncoding(_Params[0].f_String());
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;

	}
}
