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
						gc_ConstString_ReadWindowsRegistry
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_String
								, fg_FunctionParam(g_String, gc_ConstString__Root)
								, fg_FunctionParam(g_String, gc_ConstString__Key)
								, fg_FunctionParam(g_String, gc_ConstString__ValueName)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSONSorted> &&_Params) -> CEJSONSorted
							{
		#ifdef DPlatformFamily_Windows
								if (_Params.f_GetLen() != 3 || !_Params[0].f_IsString() || !_Params[1].f_IsString() || !_Params[2].f_IsString())
									fs_ThrowError(_Context, "Explode takes three string parameters: <Root> <Key> <ValueName>");

								using ERegRoot = NMib::NPlatform::CWin32_Registry::ERegRoot;
								ERegRoot RegRoot;

								auto const &Root = _Params[0].f_String();

								if (Root == gc_ConstString_LocalMachine.m_String)
									RegRoot = ERegRoot::ERegRoot_LocalMachine;
								else if (Root == gc_ConstString_CurrentUser.m_String)
									RegRoot = ERegRoot::ERegRoot_CurrentUser;
								else if (Root == gc_ConstString_Classes.m_String)
									RegRoot = ERegRoot::ERegRoot_Classes;
								else if (Root == gc_ConstString_Win64_LocalMachine.m_String)
									RegRoot = ERegRoot::ERegRoot_Win64_LocalMachine;
								else if (Root == gc_ConstString_Win64_CurrentUser.m_String)
									RegRoot = ERegRoot::ERegRoot_Win64_CurrentUser;
								else if (Root == gc_ConstString_Win64_Classes.m_String)
									RegRoot = ERegRoot::ERegRoot_Win64_Classes;
								else 
									fs_ThrowError(_Context, "Unknown root: {}"_f << Root);

								NMib::NPlatform::CWin32_Registry Registry{RegRoot};
								if (Registry.f_ValueExists(_Params[1].f_String(), _Params[2].f_String()))
									return Registry.f_Read_Str(_Params[1].f_String(), _Params[2].f_String());

								return {};
		#else
								return {};
		#endif
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
