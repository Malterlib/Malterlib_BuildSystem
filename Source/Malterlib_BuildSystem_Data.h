// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

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
		
		TCMap<CStr, CConfigurationType> m_ConfigurationTypes;
		CEntity m_RootEntity = nullptr;

		TCSet<CStr> m_SourceFiles;
		struct CImportData
		{
			CEntity m_RootEntity = nullptr;
			CRegistryPreserveAndOrder_CStr m_Registry;
		};
		TCLinkedList<CImportData> m_Imports;
	};

	struct CDependenciesBackup
	{
		TCMap<TCVector<CEntityKey>, TCLinkedList<CEntity>> m_Backup;
	};
}

#include "Malterlib_BuildSystem_Entity.hpp"
