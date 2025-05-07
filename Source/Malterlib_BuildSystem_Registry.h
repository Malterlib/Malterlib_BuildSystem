// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Encoding/EJson>
#include <Mib/Encoding/JsonParse>
#include <Mib/Encoding/JsonGenerate>
#include <Mib/Container/Registry>

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	using CBuildSystemRegistry = NContainer::TCRegistry
		<
			CBuildSystemSyntax::CRootKey
			, CBuildSystemSyntax::CRootValue
			, NContainer::ERegistryFlag_PreserveOrder | NContainer::ERegistryFlag_DuplicateKeys | NContainer::ERegistryFlag_PreserveWhitspace | NContainer::ERegistryFlag_FullLocation
			, NStr::CStr
		>
	;

	struct CBuildSystemRegistryParseContext : public CBuildSystemRegistry::CParseContext
	{
		CBuildSystemRegistryParseContext(CStringCache &_StringCache)
			: m_StringCache(_StringCache)
		{
		}

		CStringCache &m_StringCache;
		bool m_bParsingNamespace = false;
	};

	using CCustomRegistryKeyValue = NContainer::TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>;

	NStr::CStr const &fg_RegistryNameStringForPath(CBuildSystemSyntax::CRootKey const &_Key);
}

#include "Malterlib_BuildSystem_Registry.hpp"
#include "Malterlib_BuildSystem_Syntax.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NBuildSystem;
#endif
