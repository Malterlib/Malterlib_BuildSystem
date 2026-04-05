// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Generator_Ninja.h"

namespace NMib::NBuildSystem::NNinja
{
	CStr const &CProject::f_GetName() const
	{
		return TCMap<CStr, CProject>::fs_GetKey(*this);
	}

	CProject::CProject(CWorkspace *_pWorkspace)
		: m_pWorkspace(_pWorkspace)
	{
	}

	CStr const &CWorkspace::f_GetName() const
	{
		return m_Name;
	}

	CGeneratorThreadLocalConfigScope::CGeneratorThreadLocalConfigScope(TCThreadLocal<CGeneratorThreadLocal> &_ThreadLocal, CConfigResultTarget *_pConfigResult)
		: m_pNewConfigResult(_pConfigResult)
		, m_ThreadLocal(_ThreadLocal)
	{
		auto &ThreadLocal = *m_ThreadLocal;
		m_pOldConfigResult = ThreadLocal.m_pCurrentConfigResult;
		ThreadLocal.m_pCurrentConfigResult = _pConfigResult;
	}

	CGeneratorThreadLocalConfigScope::~CGeneratorThreadLocalConfigScope()
	{
		auto &ThreadLocal = *m_ThreadLocal;
		ThreadLocal.m_pCurrentConfigResult = m_pOldConfigResult;
	}

	void CGeneratorThreadLocalConfigScope::f_Suspend() noexcept
	{
		auto &ThreadLocal = *m_ThreadLocal;
		ThreadLocal.m_pCurrentConfigResult = m_pOldConfigResult;
	}

	void CGeneratorThreadLocalConfigScope::f_ResumeNoExcept() noexcept
	{
		auto &ThreadLocal = *m_ThreadLocal;
		m_pOldConfigResult = ThreadLocal.m_pCurrentConfigResult;
		ThreadLocal.m_pCurrentConfigResult = m_pNewConfigResult;
	}
}
