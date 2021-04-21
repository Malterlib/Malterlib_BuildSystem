// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	NStr::CStr const &CBuildSystemConfiguration::f_GetName() const
	{
		return NContainer::TCMap<NStr::CStr, CBuildSystemConfiguration>::fs_GetKey(*this);
	}
	
	NStr::CStr const &CConfigurationType::f_GetName() const
	{
		return NContainer::TCMap<NStr::CStr, CConfigurationType>::fs_GetKey(*this);
	}
}
