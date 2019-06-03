// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Encoding/EJSON>
#include <Mib/Encoding/JSONParse>
#include <Mib/Encoding/JSONGenerate>
#include <Mib/Container/Registry>

namespace NMib::NBuildSystem
{
	struct CBuildSystemRegistryValue : public NEncoding::CEJSON
	{
	};

	using CBuildSystemRegistry = NContainer::TCRegistry
		<
			NStr::CStr
			, CBuildSystemRegistryValue
			, NContainer::ERegistryFlag_PreserveOrder | NContainer::ERegistryFlag_DuplicateKeys | NContainer::ERegistryFlag_PreserveWhitspace
		>
	;
}

#include "Malterlib_BuildSystem_Registry.hpp"
