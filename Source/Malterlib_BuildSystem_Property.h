// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	enum EPropertyType
	{
		EPropertyType_Invalid
		, EPropertyType_Property
		, EPropertyType_Compile
		, EPropertyType_Target
		, EPropertyType_Workspace
		, EPropertyType_Dependency
		, EPropertyType_Import
	};
	
	struct CPropertyKey
	{
		inline_always CPropertyKey();
		inline_always CPropertyKey(CStr const &_Name);
		inline_always CPropertyKey(EPropertyType _Type, CStr const &_Name);
		inline_always bool operator < (CPropertyKey const &_Right) const;

		EPropertyType m_Type;
		CStr m_Name;
	};
	
	struct CProperty
	{
		CProperty();
		~CProperty();
		
		inline_always EPropertyType f_GetType() const;
		inline_always CStr const &f_GetName() const;

		CProperty(CProperty const &_Other);
		CProperty &operator = (CProperty const &_Other);
		
		CPropertyKey m_Key;
		CCondition m_Condition;
		CStr m_Value;
		DMibListLinkDS_Link(CProperty, m_LinkEvalOrder);
		CFilePosition m_Position;
		CRegistryPreserveAndOrder_CStr const *m_pRegistry;
		CStr m_Debug;
	};
	
	enum EEvaluatedPropertyType
	{
		EEvaluatedPropertyType_Implicit
		, EEvaluatedPropertyType_Explicit
		, EEvaluatedPropertyType_External
	};
	
	struct CEvaluatedProperty
	{
		inline_always CEvaluatedProperty();
		
		CStr m_Value;
		EEvaluatedPropertyType m_Type;
		TCPointer<CProperty const> m_pProperty;
	};
	
	EPropertyType fg_PropertyTypeFromStr(CStr const &_String);
	CStr fg_PropertyTypeToStr(EPropertyType _Type);
}

#include "Malterlib_BuildSystem_Property.hpp"
