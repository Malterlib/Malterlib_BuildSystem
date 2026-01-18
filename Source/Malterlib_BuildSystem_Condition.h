// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_BuildSystem_FilePosition.h"
#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	enum EConditionType
	{
		EConditionType_Root
		, EConditionType_MatchEqual
		, EConditionType_MatchNotEqual
		, EConditionType_CompareEqual
		, EConditionType_CompareNotEqual
		, EConditionType_CompareLessThan
		, EConditionType_CompareLessThanEqual
		, EConditionType_CompareGreaterThan
		, EConditionType_CompareGreaterThanEqual
		, EConditionType_Or
		, EConditionType_And
		, EConditionType_Not
	};

	struct CCondition
	{
		bool f_NeedPerFile() const;
		static bool fs_TryParseCondition(CBuildSystemRegistry const &_Registry, CCondition &_ParentCondition, bool _bRoot = true);
		static void fs_ParseCondition(CBuildSystemRegistry const &_Registry, CCondition &_ParentCondition, bool _bRoot = true);
		bool f_SimpleEval(NContainer::TCMap<NStr::CStr, NStr::CStr> const &_Values) const;
		bool f_IsCompare() const;
		bool f_IsDefault() const;

		static NStr::CStr const &fs_ConditionTypeToStr(EConditionType _Type);

		template <typename tf_CStr>
		void f_FormatRecursive(tf_CStr &o_Str, mint _Depth) const;
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		COrdering_Partial operator <=> (CCondition const &_Right) const;
		bool operator == (CCondition const &_Right) const = default;

		NContainer::TCLinkedList<CCondition> m_Children;

		CBuildSystemSyntax::CValue m_Left;
		CBuildSystemSyntax::CValue m_Right;
		EConditionType m_Type = EConditionType_Root;
		CFilePosition m_Position;
	};
}

#include "Malterlib_BuildSystem_Condition.hpp"
