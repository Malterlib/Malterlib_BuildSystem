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

	bool CBuildSystemSyntax::CIdentifier::f_IsNameConstantString() const
	{
		return m_Name.f_IsOfType<CStr>();
	}

	CStr const &CBuildSystemSyntax::CIdentifier::f_NameConstantString() const
	{
		DMibRequire(f_IsNameConstantString());
		return m_Name.f_GetAsType<CStr>();
	}

	NEncoding::CEJSON CBuildSystemSyntax::CIdentifier::f_ToJSON() const
	{
		CEJSON Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = "BuildSystemToken";
		UserType.m_Value["Type"] = "Identifier";
		auto &Name = UserType.m_Value["Name"];

		switch (m_Name.f_GetTypeID())
		{
		case 0: Name = m_Name.f_Get<0>(); break;
		case 1: Name = m_Name.f_Get<1>().f_ToJSONArray(true).f_ToJSON(); break;
		default: DMibNeverGetHere;
		}

		auto &EntityType = UserType.m_Value["EntityType"];
		if (m_EntityType == EEntityType_Invalid)
			EntityType = "";
		else
			EntityType = fg_EntityTypeToStr(m_EntityType);

		auto &PropertyType = UserType.m_Value["PropertyType"];
		if (m_bEmptyPropertyType || m_PropertyType == EPropertyType_Invalid)
			PropertyType = "";
		else
			PropertyType = fg_PropertyTypeToStr(m_PropertyType);

		return Return;
	}
	
	auto CBuildSystemSyntax::CIdentifier::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position) -> CIdentifier
	{
		CIdentifier Return;

		if (!_JSON.f_IsUserType())
			CBuildSystem::fs_ThrowError(_Position, "Invalid type for identifier");

		auto &Token = _JSON.f_UserType();
		if (Token.m_Type != "BuildSystemToken")
			CBuildSystem::fs_ThrowError(_Position, "Invalid type for identifier");

		auto pEntityType = Token.m_Value.f_GetMember("EntityType", EJSONType_String);
		if (!pEntityType)
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid EntityType member");

		auto pPropertyType = Token.m_Value.f_GetMember("PropertyType", EJSONType_String);
		if (!pPropertyType)
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid PropertyType member");

		auto pName = Token.m_Value.f_GetMember("Name");
		if (!pName)
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid Name member");

		if (pName->f_IsString())
			Return.m_Name = pName->f_String();
		else if (pName->f_IsArray())
			Return.m_Name = CEvalString::fs_FromJSON(CEJSON::fs_FromJSON(*pName), _Position);
		else
			CBuildSystem::fs_ThrowError(_Position, "Identifier token does not have valid Name member");

		if (!pEntityType->f_String().f_IsEmpty())
		{
			Return.m_EntityType = fg_EntityTypeFromStr(pEntityType->f_String());
			if (Return.m_EntityType == EEntityType_Invalid)
				CBuildSystem::fs_ThrowError(_Position, "Unknown entity type: {}"_f << pEntityType->f_String());
		}

		Return.m_bEmptyPropertyType = pPropertyType->f_String().f_IsEmpty();
		Return.m_PropertyType = fg_PropertyTypeFromStr(pPropertyType->f_String());
		if (Return.m_PropertyType == EPropertyType_Invalid)
			CBuildSystem::fs_ThrowError(_Position, "Unknown property type: {}"_f << pPropertyType->f_String());

		return Return;
	}
}
