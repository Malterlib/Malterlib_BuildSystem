// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	auto CBuildSystemSyntax::CArray::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position, bool _bAppendAllowed) -> CArray
	{
		CArray Array;
		for (auto &Element : _JSON.f_Array())
			Array.m_Array.f_Insert(CValue::fs_FromJSON(Element, _Position, _bAppendAllowed));

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

	auto CBuildSystemSyntax::CObject::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position, bool _bAppendAllowed) -> CObject
	{
		CObject Object;
		for (auto &Member : _JSON.f_Object())
		{
			CObjectKey Key;
			auto &Name = Member.f_Name();
			bool bVerifyAppendObject = false;
			switch (EJSONStringType(Name.f_GetUserData()))
			{
			case EJSONStringType_DoubleQuote:
			case EJSONStringType_SingleQuote:
				{
					Key.m_Key = Name;
				}
				break;
			case EJSONStringType_NoQuote:
				{
					if (Name == "<<")
					{
						Key.m_Key = CAppendObject();

						bVerifyAppendObject = true;
					}
					else if (Name == "...")
						CBuildSystem::fs_ThrowError(_Position, "Ellipsis object keys are only supported in types");
					else
						Key.m_Key = Name;
				}
				break;
			case EJSONStringType_Custom:
				{
					TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext ParseContext;
					ParseContext.m_FileName = _Position.m_File;
					ParseContext.m_StartLine = _Position.m_Line;
					ParseContext.m_StartColumn = _Position.m_Column;
					ParseContext.m_StartCharacter = _Position.m_Character;

					ParseContext.m_pStartParse = (uch8 const *)Name.f_GetStr();
					auto pParse = ParseContext.m_pStartParse;

					Key.m_Key = CEvalString::fs_FromJSON(CEJSON::fs_FromJSON(ParseContext.f_ParseEvalStringToken(pParse))["Value"], _Position);
				}
				break;
			}

			DMibCheck(Name.f_GetUserData() <= EJSONStringType_Custom);

			auto &ObjectValue = Object.m_Object[Key];
			Object.m_ObjectSorted.f_Insert(ObjectValue);

			CValue &Value = ObjectValue.m_Value = CValue::fs_FromJSON(Member.f_Value(), _Position, _bAppendAllowed);

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
						if (ArrayValue.m_Value.f_IsOfType<CObject>() || ArrayValue.m_Value.f_IsOfType<CExpression>() || ArrayValue.m_Value.f_IsOfType<CExpressionAppend>())
							;
						else
							CBuildSystem::fs_ThrowError(_Position, "Append object array only supports objects expressions or append expressions");
					}
				}
				else
					CBuildSystem::fs_ThrowError(_Position, "Append object only supports objects, arrays or expressions");
			}
		}

		return Object;
	}
}
