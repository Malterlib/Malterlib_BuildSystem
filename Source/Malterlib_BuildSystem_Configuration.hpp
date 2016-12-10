// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	CStr const &CBuildSystemConfiguration::f_GetName() const
	{
		return TCMap<CStr, CBuildSystemConfiguration>::fs_GetKey(*this);
	}
	
	CStr const &CConfigurationType::f_GetName() const
	{
		return TCMap<CStr, CConfigurationType>::fs_GetKey(*this);
	}
}
