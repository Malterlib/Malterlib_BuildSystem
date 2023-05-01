// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsRegistry>
#endif
#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_RegisterFunctions(TCMap<CStr, CBuiltinFunction> &&_Functions)
	{
		mp_BuiltinFunctions += fg_Move(_Functions);
	}

	void CBuildSystem::fp_RegisterBuiltinFunctions()
	{
		fp_RegisterBuiltinFunctions_Execute();
		fp_RegisterBuiltinFunctions_List();
		fp_RegisterBuiltinFunctions_Windows();
		fp_RegisterBuiltinFunctions_File();
		fp_RegisterBuiltinFunctions_Misc();
		fp_RegisterBuiltinFunctions_Color();
		fp_RegisterBuiltinFunctions_Compare();
		fp_RegisterBuiltinFunctions_Path();
		fp_RegisterBuiltinFunctions_Property();
		fp_RegisterBuiltinFunctions_String();
	}

	CBuildSystemSyntax::CFunctionParameter::EParamType g_Optional = CBuildSystemSyntax::CFunctionParameter::EParamType_Optional;
	CBuildSystemSyntax::CFunctionParameter::EParamType g_Ellipsis = CBuildSystemSyntax::CFunctionParameter::EParamType_Ellipsis;

	CBuildSystemSyntax::CType g_Any{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Any}};
	CBuildSystemSyntax::CType g_Void{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Void}};
	CBuildSystemSyntax::CType g_String{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_String}};
	CBuildSystemSyntax::CType g_Integer{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Integer}};
	CBuildSystemSyntax::CType g_FloatingPoint{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_FloatingPoint}};
	CBuildSystemSyntax::CType g_Boolean{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Boolean}};
	CBuildSystemSyntax::CType g_Date{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Date}};
	CBuildSystemSyntax::CType g_Binary{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Binary}};
	CBuildSystemSyntax::CType g_Identifier{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Identifier}};

	CBuildSystemSyntax::CType g_StringArray{CBuildSystemSyntax::CArrayType{CBuildSystemSyntax::CType{g_String}}};
	CBuildSystemSyntax::CType g_AnyArray{CBuildSystemSyntax::CArrayType{CBuildSystemSyntax::CType{g_Any}}};
	CBuildSystemSyntax::CType g_ObjectWithAny{CBuildSystemSyntax::CClassType{{}, {CBuildSystemSyntax::CType{g_Any}}}};

	CBuildSystemSyntax::CType g_StringArrayDefaultedEmpty = fg_Defaulted(g_StringArray, _[_]);
	CBuildSystemSyntax::CType g_AnyArrayDefaultedEmpty = fg_Defaulted(g_AnyArray, _[_]);

	CBuildSystemSyntax::CType g_Position
		{
			CBuildSystemSyntax::CClassType
			(
				{
					{gc_ConstString_File, g_String}
					, {gc_ConstString_Line, g_Integer}
					, {gc_ConstString_Column, g_Integer}
					, {gc_ConstString_Identifier, g_String}
					, {gc_ConstString_Message, g_String}
				}
				, {}
			)
		}
	;

	CBuildSystemSyntax::CType fg_Array(CBuildSystemSyntax::CType::CVariant &&_Type)
	{
		return CBuildSystemSyntax::CType{CBuildSystemSyntax::CArrayType{CBuildSystemSyntax::CType{fg_Move(_Type)}}};
	}

	CBuildSystemSyntax::CType fg_Array(CBuildSystemSyntax::CType &&_Type)
	{
		return CBuildSystemSyntax::CType{CBuildSystemSyntax::CArrayType{fg_Move(_Type)}};
	}

	CBuildSystemSyntax::CType fg_Defaulted(CBuildSystemSyntax::CType const &_Type, CEJSONSorted &&_Default)
	{
		return CBuildSystemSyntax::CType{CBuildSystemSyntax::CBuildSystemSyntax::CTypeDefaulted{{_Type}, fg_Move(_Default)}};
	}

	CBuildSystemSyntax::CType fg_Optional(CBuildSystemSyntax::CType const &_Type)
	{
		return CBuildSystemSyntax::CType{_Type.m_Type, true};
	}
}
