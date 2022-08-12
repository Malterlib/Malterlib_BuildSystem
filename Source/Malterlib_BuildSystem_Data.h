// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/BuildSystem/Registry>

#ifdef DMibDebug
//#define DMibBuildSystem_DebugReferences
//#define DMibBuildSystem_DebugReferencesAdvanced
#endif

#include "Malterlib_BuildSystem_DataForwardDeclare.h"
#include "Malterlib_BuildSystem_FilePosition.h"
#include "Malterlib_BuildSystem_Condition.h"
#include "Malterlib_BuildSystem_Property.h"
#include "Malterlib_BuildSystem_Entity.h"
#include "Malterlib_BuildSystem_Configuration.h"

namespace NMib::NBuildSystem
{
	struct CBuildSystemData
	{
		CBuildSystemData();
		~CBuildSystemData();

		CBuildSystemData(CBuildSystemData const &_Other);

		void f_Assign(CBuildSystemData const &_Other);

		NContainer::TCMap<NStr::CStr, CConfigurationType> m_ConfigurationTypes;
		CEntity m_RootEntity{nullptr};

		NContainer::TCSet<NStr::CStr> m_MutableSourceFiles;

		struct CImportData
		{
			CImportData();
			CImportData(CImportData const &_Right);

			CEntity m_RootEntity{nullptr};
			CBuildSystemRegistry m_Registry;
		};
	};

	struct CDependenciesBackup
	{
		struct CEntityBackup
		{
			CEntityKey m_Key;
			CEntity m_Entity;
		};
		NContainer::TCMap<NContainer::TCVector<CEntityKey>, NContainer::TCLinkedList<CEntityBackup>> m_Backup;
	};
}

#include "Malterlib_BuildSystem_Entity.hpp"
