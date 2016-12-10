// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CBuildSystemConfiguration
	{
		inline_always CStr const &f_GetName() const;
		
		CStr m_Description;
		CCondition m_Condition;
		CFilePosition m_Position;
	};

	struct CConfigurationType
	{
		inline_always CStr const &f_GetName() const;

		TCMap<CStr, CBuildSystemConfiguration> m_Configurations;
	};
	
	struct CConfigurationTuple
	{
		CStr m_Type;
		CStr m_Name;
		CFilePosition m_Position;
	};

	struct align_cacheline CConfiguration
	{
		bool operator < (CConfiguration const &_Right) const;
		bool operator == (CConfiguration const &_Right) const;

		CStr f_GetFullName() const;
		
		CStr m_Platform;
		CStr m_PlatformBase;
		CStr m_Configuration;
	};
}

#include "Malterlib_BuildSystem_Configuration.hpp"
