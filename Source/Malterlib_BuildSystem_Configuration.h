// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_BuildSystem_Condition.h"

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
		auto operator <=> (CConfiguration const &_Right) const noexcept = default;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		NStr::CStr f_GetFullName() const;
		NStr::CStr f_GetFullSafeName() const;

		NStr::CStr m_Platform;
		NStr::CStr m_PlatformBase;
		NStr::CStr m_Configuration;
	};
}

#include "Malterlib_BuildSystem_Configuration.hpp"
