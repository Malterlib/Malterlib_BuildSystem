// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	auto CBuildSystemSyntax::CJSONAccessorEntry::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position) -> CJSONAccessorEntry
	{
		if (_JSON.f_IsString())
			return {_JSON.f_String()};
		else if (_JSON.f_IsObject())
		{
			auto pType = _JSON.f_GetMember("Type", EJSONType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Type member");

			if (pType->f_String() == "Subscript")
			{
				auto pArgument = _JSON.f_GetMember("Argument");
				if (!pArgument)
					CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Argument member");

				if (pArgument->f_IsInteger())
					return {CJSONSubscript{uint32(pArgument->f_Integer())}};
				else if (pArgument->f_IsUserType())
				{
					auto &UserType = pArgument->f_UserType();
					if (UserType.m_Type != "BuildSystemToken")
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression argument does not have valid type");

					auto Value = CEJSON::fs_FromJSON(UserType.m_Value);

					auto pType = Value.f_GetMember("Type", EJSONType_String);
					if (!pType || pType->f_String() != "Expression")
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression argument does not have valid type");

					auto pParam = Value.f_GetMember("Param");
					if (!pParam)
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression token does not have valid Param member");

					auto pParen = Value.f_GetMember("Paren", EJSONType_Boolean);
					if (!pParen)
						CBuildSystem::fs_ThrowError(_Position, "Accessor exprossion token does not have valid Paren member");

					return {CJSONSubscript{CExpression::fs_FromJSON(*pParam, _Position, pParen->f_Boolean())}};
				}
				else
					CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Argument member");
			}
			else if (pType->f_String() == "Expression")
			{
				auto pParam = _JSON.f_GetMember("Param");
				if (!pParam)
					CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Param member");

				auto pParen = _JSON.f_GetMember("Paren", EJSONType_Boolean);
				if (!pParen)
					CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Paren member");

				return {CExpression::fs_FromJSON(*pParam, _Position, pParen->f_Boolean())};
			}
			else
				CBuildSystem::fs_ThrowError(_Position, "Unknown type for accessor: {}"_f << pType->f_String());
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid accessor type");

		return {};
	}

	auto CBuildSystemSyntax::CJSONAccessor::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> CJSONAccessor
	{
		auto pParam = _JSON.f_GetMember("Param");
		if (!pParam)
			CBuildSystem::fs_ThrowError(_Position, "JSONAccessor token does not have valid Param member");

		auto &Param = *pParam;

		CJSONAccessor Return;

		if (!Param.f_IsUserType())
			Return.m_Param = CParam::fs_FromJSON(Param, _Position, {}, true);
		else
		{
			auto &UserType = Param.f_UserType();
			auto &Value = UserType.m_Value;

			auto pType = Value.f_GetMember("Type", EJSONType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

			Return.m_Param = CParam::fs_FromJSON(Param, _Position, pType->f_String(), true);
		}

		auto pAccessors = _JSON.f_GetMember("Accessors", EJSONType_Array);
		if (!pAccessors)
			CBuildSystem::fs_ThrowError(_Position, "JSONAccessor token does not have valid Accessors member");

		for (auto &Accessor : pAccessors->f_Array())
			Return.m_Accessors.f_Insert(CJSONAccessorEntry::fs_FromJSON(Accessor, _Position));

		return Return;
	}
}
