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

	COrdering_Strong CGeneratorInstance::CConfigValue::operator <=> (CConfigValue const &_Right) const
	{
		return fg_TupleReferences(m_Parent, m_Entity, m_Property, m_Value) <=> fg_TupleReferences(_Right.m_Parent, _Right.m_Entity, _Right.m_Property, _Right.m_Value);
	}

	template <typename tf_CSet0, typename tf_CSet1>
	bool NVisualStudio::CGeneratorInstance::fp_IsSameConfig(tf_CSet0 const &_Configs, tf_CSet1 const &_AllConfigs) const
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

#include "Malterlib_BuildSystem_Generator_VisualStudio_GeneratorSettings.hpp"