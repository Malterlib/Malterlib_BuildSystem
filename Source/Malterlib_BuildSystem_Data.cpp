// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_BuildSystem_Data.h"

namespace NMib::NBuildSystem
{
	CBuildSystemData::CBuildSystemData() = default;
	CBuildSystemData::~CBuildSystemData() = default;

	CBuildSystemData::CBuildSystemData(CBuildSystemData const &_Right)
		: m_ConfigurationTypes(_Right.m_ConfigurationTypes)
		, m_RootEntity(_Right.m_RootEntity, nullptr, EEntityCopyFlag_CopyChildren | EEntityCopyFlag_CopyExternal)
		, m_MutableSourceFiles(_Right.m_MutableSourceFiles)
	{
		DMibRequire(_Right.m_RootEntity.m_pParent == nullptr);
		DMibRequire(_Right.m_RootEntity.f_GetKey() == CEntityKey{});
	}

	CBuildSystemData::CImportData::CImportData() = default;

	CBuildSystemData::CImportData::CImportData(CImportData const &_Right)
		: m_Registry(_Right.m_Registry)
		, m_RootEntity(_Right.m_RootEntity, nullptr, EEntityCopyFlag_CopyChildren)
	{
		DMibRequire(_Right.m_RootEntity.m_pParent == nullptr);
		DMibRequire(_Right.m_RootEntity.f_GetKey() == CEntityKey{});
	}

	void CBuildSystemData::f_Assign(CBuildSystemData const &_Other)
	{
		m_ConfigurationTypes = _Other.m_ConfigurationTypes;
		m_RootEntity.f_Assign(_Other.m_RootEntity);
		m_MutableSourceFiles = _Other.m_MutableSourceFiles;
	}
}
