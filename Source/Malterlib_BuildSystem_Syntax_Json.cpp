// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Encoding/JsonParse>

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	NEncoding::CEJsonSorted CBuildSystemSyntax::CArray::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &Array = Return.f_Array();
		for (auto &Value : m_Array)
			Array.f_Insert(Value.f_Get().f_ToJson());
		return Return;
	}

	auto CBuildSystemSyntax::CArray::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed)
		-> NStorage::TCVariant<NEncoding::CEJsonSorted, CArray>
	{
		CArray Array;
		bool bAllConstant = true;
		for (auto &Element : _Json.f_Array())
		{
			auto &Value = Array.m_Array.f_Insert(CValue::fs_FromJson(o_StringCache, Element, _Position, _bAppendAllowed));
			if (!Value.f_Get().f_IsConstant())
				bAllConstant = false;
		}

		if (bAllConstant)
		{
			NEncoding::CEJsonSorted Return;
			auto &ReturnArray = Return.f_Array();

			for (auto &Entry : Array.m_Array)
				ReturnArray.f_Insert(fg_Move(Entry.f_Get().m_Value.f_GetAsType<NEncoding::CEJsonSorted>()));

			return Return;
		}

		return Array;
	}

	CBuildSystemSyntax::CObject::CObject() = default;
	CBuildSystemSyntax::CObject::CObject(CObject &&_Other) = default;

	CBuildSystemSyntax::CObject::CObject(CObject const &_Other)
	{
		for (auto &Value : _Other.m_ObjectSorted)
		{
			auto &Key = _Other.m_Object.fs_GetKey(Value);
			auto &NewValue = m_Object[Key];
			NewValue.m_Value = Value.m_Value;
			m_ObjectSorted.f_Insert(NewValue);
		}
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CObject::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &Object = Return.f_Object();
		for (auto &Value : m_ObjectSorted)
		{
			auto &Key = m_Object.fs_GetKey(Value);
			CStr OutKey;
			switch (Key.m_Key.f_GetTypeID())
			{
			case 0:
				{
					OutKey = Key.m_Key.f_Get<0>();
					DMibFastCheck(OutKey.f_GetUserData() == Key.m_Key.f_Get<0>().f_GetUserData());
				}
				break;
			case 1:
				{
					Key.m_Key.f_Get<1>().f_Format(OutKey, false);
					OutKey.f_SetUserData(EJsonStringType_Custom);
				}
				break;
			case 2:
				{
					OutKey = gc_ConstString_Symbol_AppendObjectNoQuote;
				}
				break;
			}

			Object[OutKey] = Value.m_Value.f_Get().f_ToJson();
		}
		return Return;
	}

	auto CBuildSystemSyntax::CObject::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed)
		-> NStorage::TCVariant<NEncoding::CEJsonSorted, CObject>
	{
		CObject Object;
		bool bAllConstant = true;
		for (auto &Member : _Json.f_Object())
		{
			CObjectKey Key;
			auto &Name = Member.f_Name();
			bool bVerifyAppendObject = false;
			switch (EJsonStringType(Name.f_GetUserData()))
			{
			case EJsonStringType_DoubleQuote:
			case EJsonStringType_SingleQuote:
				{
					Key.m_Key = Name;
				}
				break;
			case EJsonStringType_NoQuote:
				{
					if (Name == gc_ConstString_Symbol_AppendObject.m_String)
					{
						Key.m_Key = CAppendObject();

						bAllConstant = false;

						bVerifyAppendObject = true;
					}
					else if (Name == gc_ConstString_Symbol_Ellipsis.m_String)
						CBuildSystem::fs_ThrowError(_Position, "Ellipsis object keys are only supported in types");
					else
						Key.m_Key = Name;
				}
				break;
			case EJsonStringType_Custom:
				{
					bAllConstant = false;

					TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::CJsonParseContext ParseContext;
					ParseContext.m_FileName = _Position.m_File;
					ParseContext.m_StartLine = _Position.m_Line;
					ParseContext.m_StartColumn = _Position.m_Column;
					ParseContext.m_StartCharacter = _Position.m_Character;

					ParseContext.m_pStartParse = (uch8 const *)Name.f_GetStr();
					auto pParse = ParseContext.m_pStartParse;

					Key.m_Key = CEvalString::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(ParseContext.f_ParseEvalStringToken(pParse))[gc_ConstString_Value], _Position);
				}
				break;
			}

			DMibCheck(Name.f_GetUserData() <= EJsonStringType_Custom);

			auto &ObjectValue = Object.m_Object[Key];
			Object.m_ObjectSorted.f_Insert(ObjectValue);

			CValue &Value = ObjectValue.m_Value = CValue::fs_FromJson(o_StringCache, Member.f_Value(), _Position, _bAppendAllowed);

			if (!Value.f_IsConstant())
				bAllConstant = false;

			if (bVerifyAppendObject)
			{
				if (Value.m_Value.f_IsOfType<CObject>() || Value.m_Value.f_IsOfType<CExpression>())
					;
				else if (Value.m_Value.f_IsOfType<CArray>())
				{
					auto &Array = Value.m_Value.f_GetAsType<CArray>();
					for (auto &Object : Array.m_Array)
					{
						CValue &ArrayValue = Object;
						if
							(
								ArrayValue.m_Value.f_IsOfType<CObject>()
								|| ArrayValue.m_Value.f_IsOfType<CExpression>()
								|| ArrayValue.m_Value.f_IsOfType<CExpressionAppend>()
								|| (ArrayValue.f_IsConstant() && ArrayValue.f_Constant().f_IsObject())
							)
						{
						}
						else
							CBuildSystem::fs_ThrowError(_Position, "Append object array only supports objects expressions or append expressions");
					}
				}
				else if (Value.f_IsConstant() && Value.f_Constant().f_IsArray())
				{
					auto &Array = Value.f_Constant().f_Array();
					for (auto &Object : Array)
					{
						if (Object.f_IsObject())
							;
						else
							CBuildSystem::fs_ThrowError(_Position, "Append object array only supports objects expressions or append expressions");
					}
				}
				else
					CBuildSystem::fs_ThrowError(_Position, "Append object only supports objects, arrays or expressions");
			}
		}

		if (bAllConstant)
		{
			NEncoding::CEJsonSorted Return;
			auto &ReturnObject = Return.f_Object();

			for (auto &Entry : Object.m_Object)
			{
				auto &Key = Object.m_Object.fs_GetKey(Entry);
				ReturnObject[Key.m_Key.f_GetAsType<NStr::CStr>()] = fg_Move(Entry.m_Value.f_Get().m_Value.f_GetAsType<NEncoding::CEJsonSorted>());
			}

			return Return;
		}

		return Object;
	}
}
