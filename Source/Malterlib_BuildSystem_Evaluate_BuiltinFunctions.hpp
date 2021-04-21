// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	template <typename tf_CType>
	CBuildSystemSyntax::CFunctionParameter fg_FunctionParam(tf_CType &&_Type, CStr &&_Name, CBuildSystemSyntax::CFunctionParameter::EParamType _ParamType)
	{
		return CBuildSystemSyntax::CFunctionParameter
			{
				CBuildSystemSyntax::CType{fg_Forward<tf_CType>(_Type)}
				, fg_Move(_Name)
				, _ParamType
			}
		;
	}

	template <typename ...tfp_CParameter>
	CBuildSystemSyntax::CFunctionType fg_FunctionType(CBuildSystemSyntax::CType const &_ReturnType, tfp_CParameter && ...p_Params)
	{
		return CBuildSystemSyntax::CFunctionType
			{
				_ReturnType
				, fg_CreateVector<CBuildSystemSyntax::CFunctionParameter>(fg_Forward<tfp_CParameter>(p_Params)...)
			}
		;
	}

	template <typename ...tfp_CParameter>
	CBuildSystemSyntax::CType fg_OneOf(tfp_CParameter && ...p_Params)
	{
		return
			{
				CBuildSystemSyntax::COneOf
				{
					fg_CreateVector<CBuildSystemSyntax::COneOf::CVariant>(fg_Forward<tfp_CParameter>(p_Params)...)
				}
			}
		;
	}
}
