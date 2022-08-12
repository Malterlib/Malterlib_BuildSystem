// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	CGenerateSettings const &CBuildSystem::f_GetGenerateSettings() const
	{
		return mp_GenerateOptions.m_Settings;
	}

	CGenerateOptions const &CBuildSystem::f_GetGenerateOptions() const
	{
		return mp_GenerateOptions;
	}

	CBuildSystem::CEvaluationContext::CEvaluationContext(CEvaluatedProperties *_pEvaluatedProperties)
		: m_pEvaluatedProperties(_pEvaluatedProperties)
	{
	}

	CBuildSystem::CChangePropertiesScope::CChangePropertiesScope(CEvaluationContext &_Context, CEvaluatedProperties *_pNewProperties)
		: m_Context(_Context)
	{
		m_pOldProperties = _Context.m_pEvaluatedProperties;
		_Context.m_pEvaluatedProperties = _pNewProperties;
	}

	CBuildSystem::CChangePropertiesScope::~CChangePropertiesScope()
	{
		m_Context.m_pEvaluatedProperties = m_pOldProperties;
	}

	template <typename tf_CStr>
	void CBuildSystem::CTypeConformError::f_Format(tf_CStr &o_Str) const
	{
		if (!m_ErrorPath.f_IsEmpty())
		{
			o_Str += "For type path:";
			for (auto &PathComponent : m_ErrorPath)
			{
				o_Str += "\n    ";
				o_Str += PathComponent;
			}
			o_Str += "\n\n    ";
		}

		o_Str += m_Error;
	}
}
