// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	class CBuildSystemGenerator
	{
	public:
		virtual ~CBuildSystemGenerator();
		virtual void f_Generate(CBuildSystem const &_BuildSystem, CBuildSystemData const &_BuildSystemData, CStr const &_OutputDir) = 0;
	};
	
	struct CGeneratorInterface
	{
		virtual bool f_GetBuiltin(CStr const &_Value, CStr &_Result) const = 0;
		virtual CStr f_GetExpandedPath(CStr const &_Path, CStr const& _Base) const = 0;
		virtual CSystemEnvironment f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const = 0;
	};
}
