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
	constexpr NEncoding::EJSONDialectFlag gc_BuildSystemJSONParseFlags = NEncoding::EJSONDialectFlag_AllowUndefined | NEncoding::EJSONDialectFlag_AllowInvalidFloat;

	struct CBuildSystemRegistryValue : public NEncoding::CEJSON
	{
		using NEncoding::CEJSON::CEJSON;

		aint f_Cmp(CBuildSystemRegistryValue const &_Right) const;

		CBuildSystemRegistryValue() = default;
		CBuildSystemRegistryValue(CBuildSystemRegistryValue &&) = default;
		CBuildSystemRegistryValue(CBuildSystemRegistryValue const &) = default;

		CBuildSystemRegistryValue &operator = (CBuildSystemRegistryValue &&) = default;
		CBuildSystemRegistryValue &operator = (CBuildSystemRegistryValue const &) = default;

		CBuildSystemRegistryValue(NEncoding::CEJSON &&_Other);
		CBuildSystemRegistryValue(NEncoding::CEJSON const &_Other);
		CBuildSystemRegistryValue &operator = (NEncoding::CEJSON &&_Other);
		CBuildSystemRegistryValue &operator = (NEncoding::CEJSON const &_Other);
	};

	using CBuildSystemRegistry = NContainer::TCRegistry
		<
			CBuildSystemRegistryValue
			, CBuildSystemRegistryValue
			, NContainer::ERegistryFlag_PreserveOrder | NContainer::ERegistryFlag_DuplicateKeys | NContainer::ERegistryFlag_PreserveWhitspace | NContainer::ERegistryFlag_FullLocation
			, NStr::CStr
		>
	;

	NStr::CStr const &fg_RegistryNameStringForPath(CBuildSystemRegistryValue const &_Key);
}

#include "Malterlib_BuildSystem_Registry.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NBuildSystem;
#endif
