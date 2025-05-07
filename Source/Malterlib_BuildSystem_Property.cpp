// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_BuildSystem_Property.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	inline_never COrdering_Strong fg_CompareStringNoInline(NStr::CStr const &_Left, NStr::CStr const &_Right)
	{
		return _Left <=> _Right;
	}

	CPropertyKey::CPropertyKey() = default;

	CPropertyKey::CPropertyKey(CStringCache &o_StringCache, NStr::CStr const &_Name)
		: CPropertyKey(o_StringCache, EPropertyType_Property, _Name, _Name.f_Hash())
	{
	}

	CPropertyKey::CPropertyKey(CStringCache &o_StringCache, EPropertyType _Type, NStr::CStr const &_Name)
		: CPropertyKey(o_StringCache, _Type, _Name, _Name.f_Hash())
	{
	}

	CPropertyKey::CPropertyKey(CStringCache &o_StringCache, EPropertyType _Type, NStr::CStr const &_Name, uint32 _Hash)
		: m_TypeAndHash(_Type, _Hash)
		, m_Name(o_StringCache.f_AddString(_Name, _Hash))
	{
	}

	CPropertyKey::CPropertyKey(CAssertAddedToStringCache _Dummy, EPropertyType _Type, NStr::CStr const &_Name, uint32 _Hash)
		: m_TypeAndHash(_Type, _Hash)
		, m_Name(_Name)
	{
	}

	CPropertyKey::CPropertyKey(CPropertyKey const &_Other) = default;
	CPropertyKey::CPropertyKey(CPropertyKey &&_Other) = default;

	CPropertyKey &CPropertyKey::operator = (CPropertyKey const &_Other)
	{
		m_TypeAndHash = _Other.m_TypeAndHash;
		fg_RemoveQualifiers(m_Name) = _Other.m_Name;

		return *this;
	}

	CPropertyKey &CPropertyKey::operator = (CPropertyKey &&_Other)
	{
		m_TypeAndHash = _Other.m_TypeAndHash;
		fg_RemoveQualifiers(m_Name) = fg_Move(fg_RemoveQualifiers(_Other.m_Name));

		return *this;
	}

	CPropertyKeyReference::CPropertyKeyReference(CPropertyKeyTypeAndHash const &_TypeAndHash, NStr::CStr const &_Name)
		: m_TypeAndHash(_TypeAndHash)
		, m_Name(_Name)
	{
	}

	CPropertyKeyReference::CPropertyKeyReference(CAssertAddedToStringCache _Dummy, EPropertyType _Type, NStr::CStr const &_Name, uint32 _Hash)
		: m_TypeAndHash(_Type, _Hash)
		, m_Name(_Name)
	{
	}

	CPropertyKeyReference CPropertyKey::f_Reference() const
	{
		return CPropertyKeyReference(m_TypeAndHash, m_Name);
	}

	CPropertyKey::CPropertyKey(CPropertyKeyReference const &_Other)
		: m_TypeAndHash(_Other.m_TypeAndHash)
		, m_Name(_Other.m_Name)
	{
	}

	CProperty::CProperty() = default;
	CProperty::~CProperty() = default;

	CProperty::CProperty(CProperty const &_Other)
		: m_pCondition(_Other.m_pCondition)
		, m_Value(_Other.m_Value)
		, m_Position(_Other.m_Position)
		, m_Flags(_Other.m_Flags)
		, m_pRegistry(_Other.m_pRegistry)
	{
	}
	
	CProperty &CProperty::operator = (CProperty const &_Other)
	{
		m_pCondition = _Other.m_pCondition;
		m_Value = _Other.m_Value;
		m_Position = _Other.m_Position;
		m_Flags = _Other.m_Flags;
		m_pRegistry = _Other.m_pRegistry;
		return *this;
	}

	EPropertyType fg_PropertyTypeFromStr(CStr const &_String)
	{
		if (_String == gc_ConstString_Property.m_String || _String == gc_ConstString_Empty) return EPropertyType_Property;
		else if (_String == gc_ConstString_Compile.m_String) return EPropertyType_Compile;
		else if (_String == gc_ConstString_Target.m_String) return EPropertyType_Target;
		else if (_String == gc_ConstString_Workspace.m_String) return EPropertyType_Workspace;
		else if (_String == gc_ConstString_Dependency.m_String) return EPropertyType_Dependency;
		else if (_String == gc_ConstString_GenerateFile.m_String) return EPropertyType_GenerateFile;
		else if (_String == gc_ConstString_Import.m_String) return EPropertyType_Import;
		else if (_String == gc_ConstString_Repository.m_String) return EPropertyType_Repository;
		else if (_String == gc_ConstString_CreateTemplate.m_String) return EPropertyType_CreateTemplate;
		else if (_String == gc_ConstString_Group.m_String) return EPropertyType_Group;
		else if (_String == gc_ConstString_this.m_String) return EPropertyType_This;
		else if (_String == gc_ConstString_Builtin.m_String) return EPropertyType_Builtin;
		else if (_String == gc_ConstString_Type.m_String) return EPropertyType_Type;
		else if (_String == gc_ConstString_GeneratorSetting.m_String) return EPropertyType_GeneratorSetting;

		else return EPropertyType_Invalid;
	}

	CStr fg_PropertyTypeToStr(EPropertyType _Type)
	{
		switch (_Type)
		{
		case EPropertyType_Property: return gc_ConstString_Property;
		case EPropertyType_Compile: return gc_ConstString_Compile;
		case EPropertyType_Target: return gc_ConstString_Target;
		case EPropertyType_Workspace: return gc_ConstString_Workspace;
		case EPropertyType_Dependency: return gc_ConstString_Dependency;
		case EPropertyType_Import: return gc_ConstString_Import;
		case EPropertyType_Repository: return gc_ConstString_Repository;
		case EPropertyType_CreateTemplate: return gc_ConstString_CreateTemplate;
		case EPropertyType_Group: return gc_ConstString_Group;
		case EPropertyType_This: return gc_ConstString_this;
		case EPropertyType_Builtin: return gc_ConstString_Builtin;
		case EPropertyType_Type: return gc_ConstString_Type;
		case EPropertyType_GenerateFile: return gc_ConstString_GenerateFile;
		case EPropertyType_GeneratorSetting: return gc_ConstString_GeneratorSetting;
		default: DMibNeverGetHere; return CStr();
		}
	}

	void CBuildSystem::f_CheckPropertyTypeValue
		(
			CPropertyKeyReference const &_PropertyKey
			, CEJsonSorted const &_Value
			, EEJsonType _ExpectedType
			, CBuildSystemUniquePositions const &_Positions
			, bool _bOptional
		) const
	{
		if ((_Value.f_IsValid() || !_bOptional) && _Value.f_EType() != _ExpectedType)
		{
			fs_ThrowError
				(
					_Positions
					, "Wrong type for '{}'. Got {}, expected {}"_f
					<< _PropertyKey
					<< fg_EJsonTypeToString(_Value.f_EType())
					<< fg_EJsonTypeToString(_ExpectedType)
				)
			;
		}
	}
}
