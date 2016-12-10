// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem::NVisualStudio
{
	CStr const &CSolution::CCompare::operator() (CSolution const &_Node) const
	{
		return _Node.m_EntityName;
	}

	CGeneratorInstance::CValueProperties::CValueProperties()
		: m_Validity(EPropertyValidity_Any)
	{
	}
	
	bool CGeneratorInstance::CConfigValue::operator < (CConfigValue const &_Right) const
	{
		if (m_Parent < _Right.m_Parent)
			return true;
		else if (m_Parent > _Right.m_Parent)
			return false;
		if (m_Entity < _Right.m_Entity)
			return true;
		else if (m_Entity > _Right.m_Entity)
			return false;
		if (m_Property < _Right.m_Property)
			return true;
		else if (m_Property > _Right.m_Property)
			return false;

		return m_Value < _Right.m_Value;
	}
	
	bool CGeneratorInstance::CThreadLocal::f_FileExists(CStr const &_Path)
	{
		auto pExists = m_FileExistsCache.f_FindEqual(_Path);
		if (pExists)
			return *pExists;
		bool bExists = CFile::fs_FileExists(_Path, EFileAttrib_File);;
		m_FileExistsCache[_Path] = bExists;
		return bExists;
	}
	
	void CGeneratorInstance::CThreadLocal::f_CreateDirectory(CStr const &_Path)
	{
		auto Mapped = m_CreateDirectoryCache(_Path);
		if (Mapped.f_WasCreated())
		{
			CFile::fs_CreateDirectory(_Path);
		}
	}
		
	template <typename tf_CSet0, typename tf_CSet1>
	bool CGeneratorInstance::fp_IsSameConfig(tf_CSet0 const &_Configs, tf_CSet1 const &_AllConfigs) const
	{
		for (auto iConfig = _Configs.f_GetIterator(); iConfig; ++iConfig)
		{
			if (!_AllConfigs.f_FindEqual(iConfig.f_GetKey()))
				return false;
		}
		for (auto iConfig = _AllConfigs.f_GetIterator(); iConfig; ++iConfig)
		{
			if (!_Configs.f_FindEqual(iConfig.f_GetKey()))
				return false;
		}

		return true;
	}
}
