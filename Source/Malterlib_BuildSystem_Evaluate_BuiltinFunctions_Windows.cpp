// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsRegistry>
#endif

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_RegisterBuiltinFunctions_Windows()
	{
		f_RegisterFunctions
			(
				{
					{
						"ReadWindowsRegistry"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, "_Root")
								, fg_FunctionParam(g_String, "_Key")
								, fg_FunctionParam(g_String, "_ValueName")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
		#ifdef DPlatformFamily_Windows
								if (_Params.f_GetLen() != 3 || !_Params[0].f_IsString() || !_Params[1].f_IsString() || !_Params[2].f_IsString())
									fsp_ThrowError(_Context, "Explode takes three string parameters: <Root> <Key> <ValueName>");

								using ERegRoot = NMib::NPlatform::CWin32_Registry::ERegRoot;
								ERegRoot RegRoot;

								auto const &Root = _Params[0].f_String();

								if (Root == "LocalMachine")
									RegRoot = ERegRoot::ERegRoot_LocalMachine;
								else if (Root == "CurrentUser")
									RegRoot = ERegRoot::ERegRoot_CurrentUser;
								else if (Root == "Classes")
									RegRoot = ERegRoot::ERegRoot_Classes;
								else if (Root == "Win64_LocalMachine")
									RegRoot = ERegRoot::ERegRoot_Win64_LocalMachine;
								else if (Root == "Win64_CurrentUser")
									RegRoot = ERegRoot::ERegRoot_Win64_CurrentUser;
								else if (Root == "Win64_Classes")
									RegRoot = ERegRoot::ERegRoot_Win64_Classes;

								NMib::NPlatform::CWin32_Registry Registry{RegRoot};
								if (Registry.f_ValueExists(_Params[1].f_String(), _Params[2].f_String()))
									return Registry.f_Read_Str(_Params[1].f_String(), _Params[2].f_String());

								return {};
		#else
								return {};
		#endif
							}
						}
					}
				}
			)
		;
	}
}
