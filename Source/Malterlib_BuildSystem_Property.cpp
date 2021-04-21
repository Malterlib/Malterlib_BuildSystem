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
		, m_bNeedPerFile(_Other.m_bNeedPerFile)
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
		m_bNeedPerFile = _Other.m_bNeedPerFile;
		return *this;
	}

	EPropertyType fg_PropertyTypeFromStr(CStr const &_String)
	{
		if (_String == "Property" || _String == "") return EPropertyType_Property;
		else if (_String == "Compile") return EPropertyType_Compile;
		else if (_String == "Target") return EPropertyType_Target;
		else if (_String == "Workspace") return EPropertyType_Workspace;
		else if (_String == "Dependency") return EPropertyType_Dependency;
		else if (_String == "GenerateFile") return EPropertyType_GenerateFile;
		else if (_String == "Import") return EPropertyType_Import;
		else if (_String == "Repository") return EPropertyType_Repository;
		else if (_String == "CreateTemplate") return EPropertyType_CreateTemplate;
		else if (_String == "Group") return EPropertyType_Group;
		else if (_String == "this") return EPropertyType_This;
		else if (_String == "Builtin") return EPropertyType_Builtin;
		else if (_String == "Type") return EPropertyType_Type;

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
		case EPropertyType_CreateTemplate: return "CreateTemplate";
		case EPropertyType_Group: return "Group";
		case EPropertyType_This: return "this";
		case EPropertyType_Builtin: return "Builtin";
		case EPropertyType_Type: return "Type";
		case EPropertyType_GenerateFile: return "GenerateFile";
		default: DMibNeverGetHere; return CStr();
		}
	}

	CPropertyKey CPropertyKey::fs_FromString(CStr const &_String, CFilePosition const &_Position)
	{
		if (auto iPoint = _String.f_FindChar('.'); iPoint >= 0)
		{
			CStr PropertyType = _String.f_Left(iPoint);

			EPropertyType Type = fg_PropertyTypeFromStr(_String.f_Left(iPoint));
			if (PropertyType.f_IsEmpty() || Type == EPropertyType_Invalid)
				CBuildSystem::fs_ThrowError(_Position, CStr::CFormat("Unrecognized property '{}'") << PropertyType);

			return {Type, _String.f_Extract(iPoint + 1)};
		}

		return {EPropertyType_Property, _String};
	}

	void CBuildSystem::f_CheckPropertyTypeValue
		(
			EPropertyType _PropType
			, CStr const &_Property
			, CEJSON const &_Value
			, EEJSONType _ExpectedType
			, CFilePosition const &_Position
			, bool _bOptional
		) const
	{
		if ((_Value.f_IsValid() || !_bOptional) && _Value.f_EType() != _ExpectedType)
		{
			fs_ThrowError
				(
					_Position
					, "Wrong type for '{}.{}'. Got {}, expected {}"_f
					<< fg_PropertyTypeToStr(_PropType)
					<< _Property
					<< fg_EJSONTypeToString(_Value.f_EType())
					<< fg_EJSONTypeToString(_ExpectedType)
				)
			;
		}
	}
}
