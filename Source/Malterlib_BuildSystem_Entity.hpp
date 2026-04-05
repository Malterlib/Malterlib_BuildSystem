// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem
{
	void CEntity::f_CheckParents() const
	{
		fpr_CheckParents();
	}

	template <typename tf_CStr>
	void CEntityKey::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}:{}") << fg_EntityTypeToStr(m_Type) << m_Name;
	}
}
