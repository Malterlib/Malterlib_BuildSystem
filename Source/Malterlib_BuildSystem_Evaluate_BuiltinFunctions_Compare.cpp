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
						"GreaterThan"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, "_Left")
								, fg_FunctionParam(g_Any, "_Right")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0] > _Params[1];
							}
						}
					}
					,
					{
						"GreaterThanEqual"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, "_Left")
								, fg_FunctionParam(g_Any, "_Right")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0] >= _Params[1];
							}
						}
					}
					,
					{
						"LessThan"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, "_Left")
								, fg_FunctionParam(g_Any, "_Right")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0] < _Params[1];
							}
						}
					}
					,
					{
						"LessThanEqual"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, "_Left")
								, fg_FunctionParam(g_Any, "_Right")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0] <= _Params[1];
							}
						}
					}
					,
					{
						"Equal"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, "_Left")
								, fg_FunctionParam(g_Any, "_Right")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0] == _Params[1];
							}
						}
					}
					,
					{
						"NotEqual"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Any, "_Left")
								, fg_FunctionParam(g_Any, "_Right")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0] != _Params[1];
							}
						}
					}
					,
					{
						"Not"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Boolean, "_Boolean")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return !_Params[0].f_Boolean();
							}
						}
					}
					,
					{
						"And"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Boolean, "_Left")
								, fg_FunctionParam(g_Boolean, "_Right")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_Boolean() && _Params[1].f_Boolean();
							}
						}
					}
					,
					{
						"Or"
						, CBuiltinFunction
						{
							fg_FunctionType
							(
								g_Boolean
								, fg_FunctionParam(g_Boolean, "_Left")
								, fg_FunctionParam(g_Boolean, "_Right")
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJSON> &&_Params) -> CEJSON
							{
								return _Params[0].f_Boolean() || _Params[1].f_Boolean();
							}
						}
					}
				}
			)
		;
	}
}
