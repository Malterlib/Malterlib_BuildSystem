// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem::NXcode
{
	inline_small CStr const &CSolution::CCompare::operator() (CSolution const &_Node) const
	{
		return _Node.m_EntityName;
	}

	bool CElement::f_IsEmpty() const
	{
		if (m_bUseValues)
			return m_ValueSet.f_IsEmpty();

		return m_Value.f_IsEmpty();
	}

	CStr const &CElement::f_GetValue() const
	{
		if (m_bUseValues)
			CBuildSystem::fs_ThrowError(m_Positions, fg_Format("Trying to access value with array length {} as a single value", m_ValueSet.f_GetLen()));

		return m_Value;
	}

	inline TCVector<CStr> CElement::f_ValueArray() const
	{
		if (!m_bUseValues)
		{
			if (m_Value.f_IsEmpty())
				return {};

			CBuildSystem::fs_ThrowError(m_Positions, fg_Format("Trying to access single value as array", m_ValueSet.f_GetLen()));
		}

		return fs_ValueArray(m_ValueSet);
	}
	
	void CElement::f_SetValue(CStr const &_Value)
	{
		m_bUseValues = false;
		m_Value = _Value;
	}
	
	bool CElement::f_IsSameValue(CElement const &_Other) const
	{
		if (m_bUseValues != _Other.m_bUseValues)
			return false;

		if (m_bUseValues)
			return m_ValueSet == _Other.m_ValueSet;

		return m_Value == _Other.m_Value;
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

	inline_always CConfiguration const &CProjectDependency::CPerConfig::f_Configuration() const
	{
		return TCMap<CConfiguration, CPerConfig>::fs_GetKey(*this);
	}
 }
