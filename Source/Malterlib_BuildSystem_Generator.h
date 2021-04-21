// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	class CBuildSystemGenerator
	{
	public:
		virtual ~CBuildSystemGenerator();
		virtual NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> f_GetValues(CBuildSystem const &_BuildSystem, NStr::CStr const &_OutputDir) = 0;
		virtual void f_Generate(CBuildSystem const &_BuildSystem, CBuildSystemData const &_BuildSystemData, NStr::CStr const &_OutputDir) = 0;
	};
	
	struct CGeneratorInterface
	{
		virtual ~CGeneratorInterface() = default;
		virtual bool f_GetBuiltin(NStr::CStr const &_Value, NStr::CStr &_Result) const = 0;
		virtual NStr::CStr f_GetExpandedPath(NStr::CStr const &_Path, NStr::CStr const &_Base) const = 0;
		virtual CSystemEnvironment f_GetBuildEnvironment(NStr::CStr const &_Platform, NStr::CStr const &_Architecture) const = 0;
	};
}
