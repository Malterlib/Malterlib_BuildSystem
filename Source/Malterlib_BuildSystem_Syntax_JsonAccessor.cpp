// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	NEncoding::CEJsonSorted CBuildSystemSyntax::CJsonAccessorEntry::f_AccessorToJson() const
	{
		switch (m_Accessor.f_GetTypeID())
		{
		case 0: return m_Accessor.f_Get<0>();
		case 1: return CEJsonSorted::fs_FromJson(m_Accessor.f_Get<1>().f_ToJson().f_UserType().m_Value);
		case 2:
			{
				CEJsonSorted Return;
				auto &OutObject = Return.f_Object();
				OutObject[gc_ConstString_Type] = gc_ConstString_Subscript;
				auto &OutArgument = OutObject[gc_ConstString_Argument];

				auto &Entry = m_Accessor.f_Get<2>();
				switch (Entry.m_Index.f_GetTypeID())
				{
				case 0: OutArgument = Entry.m_Index.f_Get<0>(); break;
				case 1: OutArgument = Entry.m_Index.f_Get<1>().f_ToJson(); break;
				default: DMibNeverGetHere;
				};

				return Return;
			}
			break;
		default: DMibNeverGetHere;
		}

		return {};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CJsonAccessorEntry::f_ToJson() const
	{
		NEncoding::CEJsonSorted Return;
		Return[gc_ConstString_OptionalChaining] = m_bOptional;
		Return[gc_ConstString_Accessor] = f_AccessorToJson();
		return Return;
	}

	auto CBuildSystemSyntax::CJsonAccessorEntry::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position) -> CJsonAccessorEntry
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "JsonAccessorEntry is not an object");

		auto pOptionalChain = _Json.f_GetMember(gc_ConstString_OptionalChaining, EJsonType_Boolean);
		if (!pOptionalChain)
			CBuildSystem::fs_ThrowError(_Position, "JsonAccessorEntry does not contain OptionalChain property");

		auto pAccessor = _Json.f_GetMember(gc_ConstString_Accessor);
		if (!pAccessor)
			CBuildSystem::fs_ThrowError(_Position, "JsonAccessorEntry does not contain Accessor property");

		return {.m_Accessor = fs_AccessorFromJson(o_StringCache, *pAccessor, _Position), .m_bOptional = pOptionalChain->f_Boolean()};
	}

	auto CBuildSystemSyntax::CJsonAccessorEntry::fs_AccessorFromJson
		(
			CStringCache &o_StringCache
			, NEncoding::CEJsonSorted const &_Json
			, CFilePosition const &_Position
		) -> NStorage::TCVariant<NStr::CStr, CExpression, CJsonSubscript>
	{
		if (_Json.f_IsString())
			return {_Json.f_String()};
		else if (_Json.f_IsObject())
		{
			auto pType = _Json.f_GetMember(gc_ConstString_Type, EJsonType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Type member");

			if (pType->f_String() == gc_ConstString_Subscript.m_String)
			{
				auto pArgument = _Json.f_GetMember(gc_ConstString_Argument);
				if (!pArgument)
					CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Argument member");

				if (pArgument->f_IsInteger())
					return {CJsonSubscript{uint32(pArgument->f_Integer())}};
				else if (pArgument->f_IsUserType())
				{
					auto &UserType = pArgument->f_UserType();
					if (UserType.m_Type != gc_ConstString_BuildSystemToken.m_String)
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression argument does not have valid type");

					auto Value = CEJsonSorted::fs_FromJson(UserType.m_Value);

					auto pType = Value.f_GetMember(gc_ConstString_Type, EJsonType_String);
					if (!pType || pType->f_String() != gc_ConstString_Expression.m_String)
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression argument does not have valid type");

					auto pParam = Value.f_GetMember(gc_ConstString_Param);
					if (!pParam)
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression token does not have valid Param member");

					auto pParen = Value.f_GetMember(gc_ConstString_Paren, EJsonType_Boolean);
					if (!pParen)
						CBuildSystem::fs_ThrowError(_Position, "Accessor exprossion token does not have valid Paren member");

					return {CJsonSubscript{CExpression::fs_FromJson(o_StringCache, *pParam, _Position, pParen->f_Boolean())}};
				}
				else
					CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Argument member");
			}
			else if (pType->f_String() == gc_ConstString_Expression.m_String)
			{
				auto pParam = _Json.f_GetMember(gc_ConstString_Param);
				if (!pParam)
					CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Param member");

				auto pParen = _Json.f_GetMember(gc_ConstString_Paren, EJsonType_Boolean);
				if (!pParen)
					CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Paren member");

				return {CExpression::fs_FromJson(o_StringCache, *pParam, _Position, pParen->f_Boolean())};
			}
			else
				CBuildSystem::fs_ThrowError(_Position, "Unknown type for accessor: {}"_f << pType->f_String());
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid accessor type");

		return {};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CJsonAccessor::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_JsonAccessor;
		Object[gc_ConstString_Param] = m_Param.f_ToJson().f_ToJson();

		auto &Accessors = Object[gc_ConstString_Accessors].f_Array();

		for (auto &Accessor : m_Accessors)
			Accessors.f_Insert(Accessor.f_ToJson().f_ToJson());

		return Return;
	}

	auto CBuildSystemSyntax::CJsonAccessor::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> CJsonAccessor
	{
		auto pParam = _Json.f_GetMember(gc_ConstString_Param);
		if (!pParam)
			CBuildSystem::fs_ThrowError(_Position, "JsonAccessor token does not have valid Param member");

		auto &Param = *pParam;

		CJsonAccessor Return;

		if (!Param.f_IsUserType())
			Return.m_Param = CParam::fs_FromJson(o_StringCache, Param, _Position, {}, true);
		else
		{
			auto &UserType = Param.f_UserType();
			auto &Value = UserType.m_Value.f_Object();

			auto pType = Value.f_GetMember(gc_ConstString_Type, EJsonType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

			Return.m_Param = CParam::fs_FromJson(o_StringCache, Param, _Position, pType->f_String(), true);
		}

		auto pAccessors = _Json.f_GetMember(gc_ConstString_Accessors, EJsonType_Array);
		if (!pAccessors)
			CBuildSystem::fs_ThrowError(_Position, "JsonAccessor token does not have valid Accessors member");

		for (auto &Accessor : pAccessors->f_Array())
			Return.m_Accessors.f_Insert(CJsonAccessorEntry::fs_FromJson(o_StringCache, Accessor, _Position));

		return Return;
	}
}
