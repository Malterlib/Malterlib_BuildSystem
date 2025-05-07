// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_RegisterBuiltinFunctions_Compare()
	{
		f_RegisterFunctions
			(
				{
					{
						gc_ConstString_GreaterThan
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, gc_ConstString__Left)
								, fg_FunctionParam(g_Any, gc_ConstString__Right)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0] > _Params[1];
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_GreaterThanEqual
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, gc_ConstString__Left)
								, fg_FunctionParam(g_Any, gc_ConstString__Right)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0] >= _Params[1];
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_LessThan
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, gc_ConstString__Left)
								, fg_FunctionParam(g_Any, gc_ConstString__Right)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0] < _Params[1];
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_LessThanEqual
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, gc_ConstString__Left)
								, fg_FunctionParam(g_Any, gc_ConstString__Right)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0] <= _Params[1];
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Equal
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, gc_ConstString__Left)
								, fg_FunctionParam(g_Any, gc_ConstString__Right)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0] == _Params[1];
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_NotEqual
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, gc_ConstString__Left)
								, fg_FunctionParam(g_Any, gc_ConstString__Right)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0] != _Params[1];
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Not
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Boolean, gc_ConstString__Boolean)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return !_Params[0].f_Boolean();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_And
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Boolean, gc_ConstString__Left)
								, fg_FunctionParam(g_Boolean, gc_ConstString__Right)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_Boolean() && _Params[1].f_Boolean();
							}
							, DMibBuildSystemFilePosition
						}
					}
					,
					{
						gc_ConstString_Or
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Boolean, gc_ConstString__Left)
								, fg_FunctionParam(g_Boolean, gc_ConstString__Right)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								return _Params[0].f_Boolean() || _Params[1].f_Boolean();
							}
							, DMibBuildSystemFilePosition
						}
					}
				}
			)
		;
	}
}
