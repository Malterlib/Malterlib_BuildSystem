// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_BuildSystem_Property.h"

namespace NMib::NBuildSystem
{
	CProperty::CProperty() = default;
	CProperty::~CProperty() = default;

	CProperty::CProperty(CProperty const &_Other)
		: m_Key(_Other.m_Key)
		, m_Condition(_Other.m_Condition)
		, m_Value(_Other.m_Value)
		, m_Position(_Other.m_Position)
		, m_Debug(_Other.m_Debug)
		, m_pRegistry(_Other.m_pRegistry)
	{
	}
	
	CProperty &CProperty::operator = (CProperty const &_Other)
	{
		m_Key = _Other.m_Key;
		m_Condition = _Other.m_Condition;
		m_Value = _Other.m_Value;
		m_Position = _Other.m_Position;
		m_Debug = _Other.m_Debug;
		m_pRegistry = _Other.m_pRegistry;
		return *this;
	}

	EPropertyType fg_PropertyTypeFromStr(CStr const &_String)
	{
		if (_String == "Property" || _String == "") return EPropertyType_Property;
		else if (_String == "Compile") return EPropertyType_Compile;
		else if (_String == "Target") return EPropertyType_Target;
		else if (_String == "Workspace") return EPropertyType_Workspace;
		else if (_String == "Dependency") return EPropertyType_Dependency;
		else if (_String == "Import") return EPropertyType_Import;
		else if (_String == "Repository") return EPropertyType_Repository;
		else if (_String == "Group") return EPropertyType_Group;
		else return EPropertyType_Invalid;
	}

	CStr fg_PropertyTypeToStr(EPropertyType _Type)
	{
		switch (_Type)
		{
		case EPropertyType_Property: return "Property";
		case EPropertyType_Compile: return "Compile";
		case EPropertyType_Target: return "Target";
		case EPropertyType_Workspace: return "Workspace";
		case EPropertyType_Dependency: return "Dependency";
		case EPropertyType_Import: return "Import";
		case EPropertyType_Repository: return "Repository";
		case EPropertyType_Group: return "Group";
		default: DMibNeverGetHere; return CStr();
		}
	}
}
