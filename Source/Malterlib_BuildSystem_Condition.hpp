// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem
{
	template <typename tf_CStr>
	void CCondition::f_FormatRecursive(tf_CStr &o_Str, umint _Depth) const
	{
		switch (m_Type)
		{
		case EConditionType_Root:
		case EConditionType_Or:
		case EConditionType_And:
		case EConditionType_Not:
			{
				o_Str += tf_CStr::CFormat("{sj*}{}\n")
					<< ""
					<< _Depth*3
					<< CCondition::fs_ConditionTypeToStr(m_Type)
				;
				o_Str += tf_CStr::CFormat("{sj*}{{\n")
					<< ""
					<< _Depth*3
				;

				for (auto &Child : m_Children)
					Child.f_FormatRecursive(o_Str, _Depth + 1);

				o_Str += tf_CStr::CFormat("{sj*}}\n")
					<< ""
					<< _Depth*3
				;
				break;
			}
		default:
			{
				o_Str += tf_CStr::CFormat("{sj*}{} {} {}\n")
					<< ""
					<< _Depth*3
					<< m_Left
					<< CCondition::fs_ConditionTypeToStr(m_Type)
					<< m_Right
				;
				break;
			}
		}
	}

	template <typename tf_CStr>
	void CCondition::f_Format(tf_CStr &o_Str) const
	{
		f_FormatRecursive(o_Str, 0);
	}
}
