// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CBuildSystemConfiguration
	{
		inline_always NStr::CStr const &f_GetName() const;
		
		NStr::CStr m_Description;
		CCondition m_Condition;
		CFilePosition m_Position;
	};

	struct CConfigurationType
	{
		inline_always NStr::CStr const &f_GetName() const;

		NContainer::TCMap<NStr::CStr, CBuildSystemConfiguration> m_Configurations;
	};
	
	struct CConfigurationTuple
	{
		NStr::CStr m_Type;
		NStr::CStr m_Name;
		CFilePosition m_Position;
	};

	struct align_cacheline CConfiguration
	{
		bool operator < (CConfiguration const &_Right) const;
		bool operator == (CConfiguration const &_Right) const;

		NStr::CStr f_GetFullName() const;
		NStr::CStr f_GetFullSafeName() const;

		NStr::CStr m_Platform;
		NStr::CStr m_PlatformBase;
		NStr::CStr m_Configuration;
	};
}

#include "Malterlib_BuildSystem_Configuration.hpp"
