// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_BuildSystem_FilePosition.h"

namespace NMib::NBuildSystem
{
	enum EConditionType
	{
		EConditionType_Root
		, EConditionType_Compare
		, EConditionType_CompareNot
		, EConditionType_Or
		, EConditionType_And
		, EConditionType_Not
	};

	struct CCondition
	{
		inline_always CCondition();
		bool f_NeedPerFile() const;
		static void fs_ParseCondition(CRegistryPreserveAndOrder_CStr &_Registry, CCondition &_ParentCondition, bool _bRoot = true);
		bool f_SimpleEval(TCMap<CStr, CStr> const &_Values) const;

		TCLinkedList<CCondition> m_Children;
		
		CStr m_Subject;
		CStr m_Value;
		EConditionType m_Type;
		CFilePosition m_Position;
	};
}

#include "Malterlib_BuildSystem_Condition.hpp"
