// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	CBuildSystemSyntax::CRootValue CBuildSystemSyntax::CIdentifier::f_RootValue() &&
	{
		return {CBuildSystemSyntax::CValue{CBuildSystemSyntax::CExpression{CBuildSystemSyntax::CParam{fg_Move(*this)}}}};
	}

	CBuildSystemSyntax::CRootKey CBuildSystemSyntax::CIdentifier::f_RootKey() &&
	{
		return {CBuildSystemSyntax::CValue{CBuildSystemSyntax::CExpression{CBuildSystemSyntax::CParam{fg_Move(*this)}}}};
	}

	EPropertyType CBuildSystemSyntax::CIdentifier::f_PropertyTypeConstant() const
	{
		DMibRequire(f_IsPropertyTypeConstant());
		return m_PropertyType.f_GetAsType<EPropertyType>();
	}

	bool CBuildSystemSyntax::CIdentifier::f_IsPropertyTypeConstant() const
	{
		return m_PropertyType.f_IsOfType<EPropertyType>();
	}

	bool CBuildSystemSyntax::CIdentifier::f_IsNameConstantString() const
	{
		return m_Name.f_IsOfType<CStringAndHash>();
	}

	uint32 CBuildSystemSyntax::CIdentifier::f_NameConstantStringHash() const
	{
		DMibRequire(f_IsNameConstantString());
		return m_Name.f_GetAsType<CStringAndHash>().m_Hash;
	}

	CStr const &CBuildSystemSyntax::CIdentifier::f_NameConstantString() const
	{
		DMibRequire(f_IsNameConstantString());
		return m_Name.f_GetAsType<CStringAndHash>().m_String;
	}

	CPropertyKey CBuildSystemSyntax::CIdentifier::f_PropertyKeyConstant() const
	{
		DMibRequire(m_EntityType == EEntityType_Invalid);

		auto &ConstantName = m_Name.f_GetAsType<CStringAndHash>();

		return CPropertyKey(CAssertAddedToStringCache(), f_PropertyTypeConstant(), ConstantName.m_String, ConstantName.m_Hash);
	}

	CPropertyKeyReference CBuildSystemSyntax::CIdentifier::f_PropertyKeyReferenceConstant(EPropertyType _PropertyType) const
	{
		DMibRequire(m_EntityType == EEntityType_Invalid);

		auto &ConstantName = m_Name.f_GetAsType<CStringAndHash>();

		return CPropertyKeyReference(CAssertAddedToStringCache(), _PropertyType, ConstantName.m_String, ConstantName.m_Hash);
	}

	CPropertyKeyReference CBuildSystemSyntax::CIdentifier::f_PropertyKeyReferenceConstant() const
	{
		DMibRequire(m_EntityType == EEntityType_Invalid);

		auto &ConstantName = m_Name.f_GetAsType<CStringAndHash>();

		return CPropertyKeyReference(CAssertAddedToStringCache(), f_PropertyTypeConstant(), ConstantName.m_String, ConstantName.m_Hash);
	}

	NEncoding::CEJSONSorted CBuildSystemSyntax::CIdentifierReference::f_ToJSON() const
	{
		CEJSONSorted Return;

		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_IdentifierReference;
		Object[gc_ConstString_Identifier] = m_Identifier.f_ToJSON().f_ToJSON();

		return Return;
	}
	
	NEncoding::CEJSONSorted CBuildSystemSyntax::CIdentifier::f_ToJSON() const
	{
		CEJSONSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_Identifier;
		auto &Name = Object[gc_ConstString_Name];

		switch (m_Name.f_GetTypeID())
		{
		case 0: Name = m_Name.f_Get<0>().m_String; break;
		case 1: Name = m_Name.f_Get<1>().f_ToJSONArray(true).f_ToJSON(); break;
		default: DMibNeverGetHere;
		}

		auto &EntityType = Object[gc_ConstString_EntityType];
		if (m_EntityType == EEntityType_Invalid)
			EntityType = "";
		else
			EntityType = fg_EntityTypeToStr(m_EntityType);

		auto &PropertyType = Object[gc_ConstString_PropertyType];
		if (m_bEmptyPropertyType || (f_IsPropertyTypeConstant() && f_PropertyTypeConstant() == EPropertyType_Invalid))
			PropertyType = "";
		else
		{
			switch (m_PropertyType.f_GetTypeID())
			{
			case 0: PropertyType = fg_PropertyTypeToStr(m_PropertyType.f_Get<0>()); break;
			case 1: PropertyType = m_PropertyType.f_Get<1>().f_ToJSONArray(true).f_ToJSON(); break;
			default: DMibNeverGetHere;
			}
		}

		return Return;
	}
	
	auto CBuildSystemSyntax::CIdentifier::fs_FromJSON(CStringCache &o_StringCache, CEJSONSorted const &_JSON, CFilePosition const &_Position) -> CIdentifier
	{
		CIdentifier Return;

		if (!_JSON.f_IsUserType())
			CBuildSystem::fs_ThrowError(_Position, "Invalid type for identifier");

		auto &Token = _JSON.f_UserType();
		if (Token.m_Type != gc_ConstString_BuildSystemToken.m_String)
			CBuildSystem::fs_ThrowError(_Position, "Invalid type for identifier");

		auto pEntityType = Token.m_Value.f_GetMember(gc_ConstString_EntityType, EJSONType_String);
		if (!pEntityType)
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid EntityType member");

		auto pName = Token.m_Value.f_GetMember(gc_ConstString_Name);
		if (!pName)
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid Name member");

		if (pName->f_IsString())
			Return.m_Name = CStringAndHash(o_StringCache, pName->f_String(), pName->f_String().f_Hash());
		else if (pName->f_IsArray())
			Return.m_Name = CEvalString::fs_FromJSON(o_StringCache, CEJSONSorted::fs_FromJSON(*pName), _Position);
		else
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid Name member");

		if (!pEntityType->f_String().f_IsEmpty())
		{
			Return.m_EntityType = fg_EntityTypeFromStr(pEntityType->f_String());
			if (Return.m_EntityType == EEntityType_Invalid)
				CBuildSystem::fs_ThrowError(_Position, "Unknown entity type: {}"_f << pEntityType->f_String());
		}

		auto pPropertyType = Token.m_Value.f_GetMember(gc_ConstString_PropertyType);
		if (!pPropertyType)
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid PropertyType member");

		if (pPropertyType->f_IsString())
		{
			Return.m_bEmptyPropertyType = pPropertyType->f_String().f_IsEmpty();
			auto PropertyType = fg_PropertyTypeFromStr(pPropertyType->f_String());
			Return.m_PropertyType = PropertyType;
			if (PropertyType == EPropertyType_Invalid)
				CBuildSystem::fs_ThrowError(_Position, "Unknown property type: {}"_f << *pPropertyType);
		}
		else if (pPropertyType->f_IsArray())
			Return.m_PropertyType = CEvalString::fs_FromJSON(o_StringCache, CEJSONSorted::fs_FromJSON(*pPropertyType), _Position);
		else
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid PropertyType member");

		return Return;
	}
}
