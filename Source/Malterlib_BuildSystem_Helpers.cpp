// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Helpers.h"

namespace NMib::NBuildSystem
{
	CStr fg_EscapeXcodeProjectVar(CStr const &_Var)
	{
		if (_Var.f_FindChars(" @`!\"#$%&\'()*+,-./:;[{<\\|=]}>^~?_") >= 0)
			return _Var.f_EscapeStr();
		return _Var;
	}
}
