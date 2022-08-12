// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	NEncoding::CEJSONSorted CBuildSystemSyntax::CJSONAccessorEntry::f_ToJSON() const
	{
		switch (m_Accessor.f_GetTypeID())
		{
		case 0: return m_Accessor.f_Get<0>();
		case 1: return CEJSONSorted::fs_FromJSON(m_Accessor.f_Get<1>().f_ToJSON().f_UserType().m_Value);
		case 2:
			{
				CEJSONSorted Return;
				auto &OutObject = Return.f_Object();
				OutObject[gc_ConstString_Type] = gc_ConstString_Subscript;
				auto &OutArgument = OutObject[gc_ConstString_Argument];

				auto &Entry = m_Accessor.f_Get<2>();
				switch (Entry.m_Index.f_GetTypeID())
				{
				case 0: OutArgument = Entry.m_Index.f_Get<0>(); break;
				case 1: OutArgument = Entry.m_Index.f_Get<1>().f_ToJSON(); break;
				default: DMibNeverGetHere;
				};

				return Return;
			}
			break;
		default: DMibNeverGetHere;
		}

		return {};
	}

	auto CBuildSystemSyntax::CJSONAccessorEntry::fs_FromJSON(CStringCache &o_StringCache, CEJSONSorted const &_JSON, CFilePosition const &_Position) -> CJSONAccessorEntry
	{
		if (_JSON.f_IsString())
			return {_JSON.f_String()};
		else if (_JSON.f_IsObject())
		{
			auto pType = _JSON.f_GetMember(gc_ConstString_Type, EJSONType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Type member");

			if (pType->f_String() == gc_ConstString_Subscript.m_String)
			{
				auto pArgument = _JSON.f_GetMember(gc_ConstString_Argument);
				if (!pArgument)
					CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Argument member");

				if (pArgument->f_IsInteger())
					return {CJSONSubscript{uint32(pArgument->f_Integer())}};
				else if (pArgument->f_IsUserType())
				{
					auto &UserType = pArgument->f_UserType();
					if (UserType.m_Type != gc_ConstString_BuildSystemToken.m_String)
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression argument does not have valid type");

					auto Value = CEJSONSorted::fs_FromJSON(UserType.m_Value);

					auto pType = Value.f_GetMember(gc_ConstString_Type, EJSONType_String);
					if (!pType || pType->f_String() != gc_ConstString_Expression.m_String)
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression argument does not have valid type");

					auto pParam = Value.f_GetMember(gc_ConstString_Param);
					if (!pParam)
						CBuildSystem::fs_ThrowError(_Position, "Accessor expression token does not have valid Param member");

					auto pParen = Value.f_GetMember(gc_ConstString_Paren, EJSONType_Boolean);
					if (!pParen)
						CBuildSystem::fs_ThrowError(_Position, "Accessor exprossion token does not have valid Paren member");

					return {CJSONSubscript{CExpression::fs_FromJSON(o_StringCache, *pParam, _Position, pParen->f_Boolean())}};
				}
				else
					CBuildSystem::fs_ThrowError(_Position, "Accessor does not have valid Argument member");
			}
			else if (pType->f_String() == gc_ConstString_Expression.m_String)
			{
				auto pParam = _JSON.f_GetMember(gc_ConstString_Param);
				if (!pParam)
					CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Param member");

				auto pParen = _JSON.f_GetMember(gc_ConstString_Paren, EJSONType_Boolean);
				if (!pParen)
					CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Paren member");

				return {CExpression::fs_FromJSON(o_StringCache, *pParam, _Position, pParen->f_Boolean())};
			}
			else
				CBuildSystem::fs_ThrowError(_Position, "Unknown type for accessor: {}"_f << pType->f_String());
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid accessor type");

		return {};
	}

	NEncoding::CEJSONSorted CBuildSystemSyntax::CJSONAccessor::f_ToJSON() const
	{
		CEJSONSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_JSONAccessor;
		Object[gc_ConstString_Param] = m_Param.f_ToJSON().f_ToJSON();

		auto &Accessors = Object[gc_ConstString_Accessors].f_Array();

		for (auto &Accessor : m_Accessors)
			Accessors.f_Insert(Accessor.f_ToJSON().f_ToJSON());

		return Return;
	}

	auto CBuildSystemSyntax::CJSONAccessor::fs_FromJSON(CStringCache &o_StringCache, NEncoding::CEJSONSorted const &_JSON, CFilePosition const &_Position) -> CJSONAccessor
	{
		auto pParam = _JSON.f_GetMember(gc_ConstString_Param);
		if (!pParam)
			CBuildSystem::fs_ThrowError(_Position, "JSONAccessor token does not have valid Param member");

		auto &Param = *pParam;

		CJSONAccessor Return;

		if (!Param.f_IsUserType())
			Return.m_Param = CParam::fs_FromJSON(o_StringCache, Param, _Position, {}, true);
		else
		{
			auto &UserType = Param.f_UserType();
			auto &Value = UserType.m_Value.f_Object();

			auto pType = Value.f_GetMember(gc_ConstString_Type, EJSONType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

			Return.m_Param = CParam::fs_FromJSON(o_StringCache, Param, _Position, pType->f_String(), true);
		}

		auto pAccessors = _JSON.f_GetMember(gc_ConstString_Accessors, EJSONType_Array);
		if (!pAccessors)
			CBuildSystem::fs_ThrowError(_Position, "JSONAccessor token does not have valid Accessors member");

		for (auto &Accessor : pAccessors->f_Array())
			Return.m_Accessors.f_Insert(CJSONAccessorEntry::fs_FromJSON(o_StringCache, Accessor, _Position));

		return Return;
	}
}
