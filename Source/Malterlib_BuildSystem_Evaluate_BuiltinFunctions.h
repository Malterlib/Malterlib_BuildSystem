// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	template <typename tf_CType>
	CBuildSystemSyntax::CFunctionParameter fg_FunctionParam
		(
			tf_CType &&_Type
			, CStr const &_Name
			, CBuildSystemSyntax::CFunctionParameter::EParamType _ParamType = CBuildSystemSyntax::CFunctionParameter::EParamType_None
		)
	;

	extern CBuildSystemSyntax::CFunctionParameter::EParamType g_Optional;
	extern CBuildSystemSyntax::CFunctionParameter::EParamType g_Ellipsis;

	extern CBuildSystemSyntax::CType g_Any;
	extern CBuildSystemSyntax::CType g_Void;
	extern CBuildSystemSyntax::CType g_String;
	extern CBuildSystemSyntax::CType g_Integer;
	extern CBuildSystemSyntax::CType g_FloatingPoint;
	extern CBuildSystemSyntax::CType g_Boolean;
	extern CBuildSystemSyntax::CType g_Date;
	extern CBuildSystemSyntax::CType g_Binary;
	extern CBuildSystemSyntax::CType g_Identifier;
	extern CBuildSystemSyntax::CType g_StringArray;
	extern CBuildSystemSyntax::CType g_AnyArray;
	extern CBuildSystemSyntax::CType g_ObjectWithAny;
	extern CBuildSystemSyntax::CType g_Position;

	extern CBuildSystemSyntax::CType g_StringArrayDefaultedEmpty;
	extern CBuildSystemSyntax::CType g_AnyArrayDefaultedEmpty;

	template <typename ...tfp_CParameter>
	CBuildSystemSyntax::CFunctionType fg_FunctionType(CBuildSystemSyntax::CType const &_ReturnType, tfp_CParameter && ...p_Params);

	CBuildSystemSyntax::CType fg_Array(CBuildSystemSyntax::CType::CVariant &&_Type);
	CBuildSystemSyntax::CType fg_Array(CBuildSystemSyntax::CType &&_Type);
	CBuildSystemSyntax::CType fg_Defaulted(CBuildSystemSyntax::CType const &_Type, NEncoding::CEJsonSorted &&_Default);
	CBuildSystemSyntax::CType fg_UserType(CStr const &_Name);
	CBuildSystemSyntax::CType fg_Optional(CBuildSystemSyntax::CType const &_Type);
	template <typename ...tfp_CParameter>
	CBuildSystemSyntax::CType fg_OneOf(tfp_CParameter && ...p_Params);
}

#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.hpp"
