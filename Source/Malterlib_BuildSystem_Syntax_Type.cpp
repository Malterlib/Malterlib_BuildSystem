// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_Any{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Any}};
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_Void{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Void}};
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_String{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_String}};
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_Integer{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Integer}};
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_FloatingPoint{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_FloatingPoint}};
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_Boolean{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Boolean}};
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_Date{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Date}};
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_Binary{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Binary}};
	CBuildSystemSyntax::CType const CBuildSystemSyntax::CDefaultType::ms_IdentifierReference{CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Identifier}};

	NEncoding::CEJsonSorted CBuildSystemSyntax::CDefaultType::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_DefaultType;
		auto &TypeName = Object[gc_ConstString_TypeName];

		switch (m_Type)
		{
		case EType_Any: TypeName = gc_ConstString_any; break;
		case EType_Void: TypeName = gc_ConstString_void; break;
		case EType_String: TypeName = gc_ConstString_string; break;
		case EType_Integer: TypeName = gc_ConstString_int; break;
		case EType_FloatingPoint: TypeName = gc_ConstString_float; break;
		case EType_Boolean: TypeName = gc_ConstString_bool; break;
		case EType_Date: TypeName = gc_ConstString_date; break;
		case EType_Binary: TypeName = gc_ConstString_binary; break;
		case EType_Identifier: TypeName = gc_ConstString_identifier; break;
		default: DMibNeverGetHere;
		}

		return Return;
	}

	auto CBuildSystemSyntax::CDefaultType::fs_FromJson(CJsonSorted const &_Json, CFilePosition const &_Position) -> CDefaultType
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "DefaultType token should be an object");

		auto *pTypeName = _Json.f_GetMember(gc_ConstString_TypeName, EJsonType_String);
		if (!pTypeName)
			CBuildSystem::fs_ThrowError(_Position, "DefautType does not have a valid TypeName member");

		auto &TypeName = pTypeName->f_String();

		if (TypeName == gc_ConstString_int.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Integer};
		else if (TypeName == gc_ConstString_string.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_String};
		else if (TypeName == gc_ConstString_float.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_FloatingPoint};
		else if (TypeName == gc_ConstString_bool.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Boolean};
		else if (TypeName == gc_ConstString_date.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Date};
		else if (TypeName == gc_ConstString_binary.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Binary};
		else if (TypeName == gc_ConstString_any.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Any};
		else if (TypeName == gc_ConstString_void.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Void};
		else if (TypeName == gc_ConstString_identifier.m_String)
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Identifier};
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid type name for DefautType: '{}'"_f << TypeName);

		return {};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CTypeDefaulted::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_TypeDefaulted;
		Object[gc_ConstString_InnerType] = m_Type.f_Get().f_ToJson().f_ToJson();
		Object[gc_ConstString_DefaultValue] = m_DefaultValue.f_ToJson().f_ToJson();

		return Return;
	}

	auto CBuildSystemSyntax::CTypeDefaulted::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> CTypeDefaulted
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted token should be an object");

		auto *pType = _Json.f_GetMember(gc_ConstString_InnerType);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted does not have a valid Type member");

		auto *pDefault = _Json.f_GetMember(gc_ConstString_DefaultValue);
		if (!pDefault)
			CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted does not have a valid DefaultValue member");

		CStr Type;
		if (pDefault->f_IsUserType())
		{
			auto &UserType = pDefault->f_UserType();
			if (UserType.m_Type != gc_ConstString_BuildSystemToken.m_String)
				CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted does not have valid DefaultValue member");

			auto pType = UserType.m_Value.f_GetMember(gc_ConstString_Type, EJsonType_String);

			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted does not have valid Type member");

			Type = pType->f_String();
		}

		return CTypeDefaulted{{CType::fs_FromJson(o_StringCache, *pType, _Position)}, CParam::fs_FromJson(o_StringCache, *pDefault, _Position, Type, false)};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CUserType::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_Type;
		Object[gc_ConstString_TypeName] = m_Name;

		return Return;
	}

	auto CBuildSystemSyntax::CUserType::fs_FromJson(NEncoding::CJsonSorted const &_Json, CFilePosition const &_Position) -> CUserType
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "UserType token should be an object");

		auto *pTypeName = _Json.f_GetMember(gc_ConstString_TypeName, EJsonType_String);
		if (!pTypeName)
			CBuildSystem::fs_ThrowError(_Position, "UserType does not have a valid TypeName member");

		return CUserType{pTypeName->f_String()};
	}

	CBuildSystemSyntax::CClassType::CMember::CMember() = default;
	CBuildSystemSyntax::CClassType::CMember::CMember(CMember &&_Other) = default;

	CBuildSystemSyntax::CClassType::CMember::CMember(CMember const &_Other)
		: m_Type(_Other.m_Type)
		, m_bOptional(_Other.m_bOptional)
	{
	}

	CBuildSystemSyntax::CClassType::CMember::CMember(CType const &_Type, bool _bOptional)
		: m_Type(_Type)
		, m_bOptional(_bOptional)
	{
	}

	CBuildSystemSyntax::CClassType::CClassType() = default;
	CBuildSystemSyntax::CClassType::CClassType(CClassType &&_Other) = default;

	CBuildSystemSyntax::CClassType::CClassType(CClassType const &_Other)
		: m_OtherKeysType(_Other.m_OtherKeysType)
	{
		for (auto &Value : _Other.m_MembersSorted)
		{
			auto &Key = _Other.m_Members.fs_GetKey(Value);
			auto &NewValue = m_Members[Key];
			NewValue.m_Type = Value.m_Type;
			NewValue.m_bOptional = Value.m_bOptional;
			m_MembersSorted.f_Insert(NewValue);
		}
	}

	CBuildSystemSyntax::CClassType::CClassType(NContainer::TCVector<NStorage::TCTuple<NStr::CStr, CMember>> const &_Members, NStorage::TCOptional<CType> const &_OtherKeysType)
	{
		if (_OtherKeysType)
			m_OtherKeysType = *_OtherKeysType;

		for (auto &Member : _Members)
		{
			auto &NewMember = *m_Members(fg_Get<0>(Member), fg_Get<1>(Member));
			m_MembersSorted.f_Insert(NewMember);
		}
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CClassType::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &Object = Return.f_Object();

		for (auto &Member : m_MembersSorted)
		{
			auto &Key = m_Members.fs_GetKey(Member);
			if (Member.m_bOptional)
			{
				CStr OutKey = Key + gc_ConstString_Symbol_Optional.m_String;
				OutKey.f_SetUserData(Key.f_GetUserData());
				Object[OutKey] = Member.m_Type.f_Get().f_ToJson();
			}
			else
				Object[Key] = Member.m_Type.f_Get().f_ToJson();
		}

		if (m_OtherKeysType)
			Object[gc_ConstString_Symbol_EllipsisNoQuote] = m_OtherKeysType->f_Get().f_ToJson();

		return Return;
	}

	auto CBuildSystemSyntax::CClassType::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> CClassType
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "ClassType token should be an object");

		CClassType ClassType;
		for (auto &Member : _Json.f_Object())
		{
			auto Name = Member.f_Name();
			switch (EJsonStringType(Name.f_GetUserData()))
			{
			case EJsonStringType_NoQuote:
				{
					if (Name == gc_ConstString_Symbol_AppendObject.m_String)
						CBuildSystem::fs_ThrowError(_Position, "Append object cannot be used in types");
					else if (Name == gc_ConstString_Symbol_Ellipsis.m_String)
					{
						ClassType.m_OtherKeysType = CType::fs_FromJson(o_StringCache, Member.f_Value(), _Position);
						continue;
					}
				}
				break;
			case EJsonStringType_Custom:
				CBuildSystem::fs_ThrowError(_Position, "Eval string as key not supported in types");
				break;
			case EJsonStringType_DoubleQuote:
			case EJsonStringType_SingleQuote:
					break;
			}

			bool bOptional = Name.f_EndsWith(gc_ConstString_Symbol_Optional.m_String);
			if (bOptional)
			{
				auto UserData = Name.f_GetUserData();
				Name = Name.f_Left(Name.f_GetLen() - 1);
				Name.f_SetUserData(UserData);
			}

			auto &ClassMember = ClassType.m_Members[Name];
			ClassMember.m_bOptional = bOptional;
			ClassMember.m_Type = CType::fs_FromJson(o_StringCache, Member.f_Value(), _Position);
			ClassType.m_MembersSorted.f_Insert(ClassMember);
		}

		return ClassType;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CArrayType::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &Array = Return.f_Array();
		Array.f_Insert(m_Type.f_Get().f_ToJson());
		return Return;
	}

	auto CBuildSystemSyntax::CArrayType::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> CArrayType
	{
		if (!_Json.f_IsArray())
			CBuildSystem::fs_ThrowError(_Position, "ArrayType token should be an array");

		auto &Array = _Json.f_Array();
		if (Array.f_GetLen() != 1)
			CBuildSystem::fs_ThrowError(_Position, "Array definitions should have one entry");

		return CBuildSystemSyntax::CArrayType{CBuildSystemSyntax::CType::fs_FromJson(o_StringCache, Array[0], _Position)};
	}

	bool CBuildSystemSyntax::CType::f_IsOptional() const
	{
		auto pType = this;
		while (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CTypeDefaulted>())
			pType = &pType->m_Type.f_GetAsType<CBuildSystemSyntax::CTypeDefaulted>().m_Type.f_Get();
		return pType->m_bOptional;
	}

	bool CBuildSystemSyntax::CType::f_IsDefaulted() const
	{
		return m_Type.f_IsOfType<CBuildSystemSyntax::CTypeDefaulted>();
	}

	bool CBuildSystemSyntax::CType::f_IsFunction() const
	{
		return m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>();
	}

	bool CBuildSystemSyntax::CType::f_IsAny(TCFunctionNoAlloc<CType const *(CType const *_pType)> const &_fGetCanonical) const
	{
		auto *pType = this;
		if (_fGetCanonical)
			pType = _fGetCanonical(pType);

		if (pType->m_Type.f_IsOfType<CDefaultType>() && pType->m_Type.f_GetAsType<CDefaultType>().m_Type == CDefaultType::EType_Any)
			return true;
		if (pType->m_Type.f_IsOfType<COneOf>())
		{
			for (auto &Type : m_Type.f_GetAsType<COneOf>().m_OneOf)
			{
				if (!Type.f_IsOfType<NStorage::TCIndirection<CType>>())
					continue;
				auto pCanonicalType = &Type.f_GetAsType<NStorage::TCIndirection<CType>>().f_Get();
				if (_fGetCanonical)
					pCanonicalType = _fGetCanonical(pCanonicalType);
				if (pCanonicalType->m_Type.f_IsOfType<CDefaultType>() && pCanonicalType->m_Type.f_GetAsType<CDefaultType>().m_Type == CDefaultType::EType_Any)
					return true;
			}
		}

		return false;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CType::f_ToJson() const
	{
		auto fGetType = [&]() -> CEJsonSorted
			{
				switch (m_Type.f_GetTypeID())
				{
				case 0: return m_Type.f_Get<0>().f_ToJson();
				case 1: return m_Type.f_Get<1>().f_ToJson();
				case 2: return m_Type.f_Get<2>().f_ToJson();
				case 3: return m_Type.f_Get<3>().f_ToJson();
				case 4: return m_Type.f_Get<4>().f_ToJson();
				case 5: return m_Type.f_Get<5>().f_ToJson();
				case 6: return m_Type.f_Get<6>().f_ToJson();
				}
				DNeverGetHere;
				return {};
			}
		;

		if (m_bOptional)
		{
			CEJsonSorted Return;
			auto &UserType = Return.f_UserType();
			UserType.m_Type = gc_ConstString_BuildSystemToken;

			auto &Object = UserType.m_Value.f_Object();
			Object[gc_ConstString_Type] = gc_ConstString_Optional;
			Object[gc_ConstString_Param] = fGetType().f_ToJson();

			return Return;
		}
		else
			return fGetType();
	}

	auto CBuildSystemSyntax::CType::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> CType
	{
		if (_Json.f_IsUserType())
		{
			auto &UserType = _Json.f_UserType();
			if (UserType.m_Type != gc_ConstString_BuildSystemToken.m_String)
				CBuildSystem::fs_ThrowError(_Position, "Define does not have valid Define member");

			auto pType = UserType.m_Value.f_GetMember(gc_ConstString_Type, EJsonType_String);

			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Define does not have valid Define Type member");

			auto &Type = pType->f_String();
			bool bOptional = false;

			if (Type == gc_ConstString_Optional.m_String)
			{
				auto pParam = UserType.m_Value.f_GetMember(gc_ConstString_Param);

				if (!pParam)
					CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Param member");

				auto Type = CBuildSystemSyntax::CType::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(*pParam), _Position);
				Type.m_bOptional = true;

				return Type;
			}
			else if (Type == gc_ConstString_DefaultType.m_String)
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::CDefaultType::fs_FromJson(UserType.m_Value, _Position), bOptional};
			else if (Type == gc_ConstString_Type.m_String)
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::CUserType::fs_FromJson(UserType.m_Value, _Position), bOptional};
			else if (Type == gc_ConstString_OneOf.m_String)
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(UserType.m_Value), _Position), bOptional};
			else if (Type == gc_ConstString_FunctionType.m_String)
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::CFunctionType::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(UserType.m_Value), _Position), bOptional};
			else if (Type == gc_ConstString_TypeDefaulted.m_String)
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::CTypeDefaulted::fs_FromJson(o_StringCache, CEJsonSorted::fs_FromJson(UserType.m_Value), _Position), bOptional};
			else
				CBuildSystem::fs_ThrowError(_Position, "Invalid type user type: {}"_f << Type);
		}
		else if (_Json.f_IsArray())
			return CBuildSystemSyntax::CType{CBuildSystemSyntax::CArrayType::fs_FromJson(o_StringCache, _Json, _Position)};
		else if (_Json.f_IsObject())
			return CBuildSystemSyntax::CType{CBuildSystemSyntax::CClassType::fs_FromJson(o_StringCache, _Json, _Position)};
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid type: {jp}"_f << _Json);

		return {};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::COneOf::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_OneOf;
		auto &OutArray = Object[gc_ConstString_OneOfList].f_Array();
		for (auto &Value : m_OneOf)
		{
			switch (Value.f_GetTypeID())
			{
			case 0: OutArray.f_Insert(Value.f_Get<0>().f_ToJson()); break;
			case 1: OutArray.f_Insert(Value.f_Get<1>().f_Get().f_ToJson().f_ToJson()); break;
			default: DNeverGetHere;
			}
		}

		return Return;
	}

	auto CBuildSystemSyntax::COneOf::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> COneOf
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "OneOf token should be an object");

		auto *pOneOfList = _Json.f_GetMember(gc_ConstString_OneOfList, EJsonType_Array);
		if (!pOneOfList)
			CBuildSystem::fs_ThrowError(_Position, "OneOf does not have a valid OneOfList member");

		COneOf Return;

		for (auto &OneOf : pOneOfList->f_Array())
		{
			if (OneOf.f_IsUserType() || OneOf.f_IsArray() || OneOf.f_IsObject())
				Return.m_OneOf.f_Insert(CBuildSystemSyntax::CType::fs_FromJson(o_StringCache, OneOf, _Position));
			else
				Return.m_OneOf.f_Insert(OneOf);
		}

		return Return;
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CFunctionParameter::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = m_Type.f_Get().f_ToJson();
		Object[gc_ConstString_Name] = m_Name;
		Object[gc_ConstString_Ellipsis] = m_ParamType == EParamType_Ellipsis;

		return Return;
	}

	auto CBuildSystemSyntax::CFunctionParameter::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> CFunctionParameter
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token should be an object");

		auto pType = _Json.f_GetMember(gc_ConstString_Type);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token does not have a valid Type member");

		auto pName = _Json.f_GetMember(gc_ConstString_Name, EJsonType_String);
		if (!pName)
			CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token does not have a valid Name member");

		auto pEllipsis = _Json.f_GetMember(gc_ConstString_Ellipsis, EJsonType_Boolean);
		if (!pEllipsis)
			CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token does not have a valid Ellipsis member");

		EParamType ParamType;
		if (pEllipsis->f_Boolean())
			ParamType = EParamType_Ellipsis;
		else
			ParamType = EParamType_None;

		auto Type = CBuildSystemSyntax::CType::fs_FromJson(o_StringCache, *pType, _Position);
		if (Type.f_IsOptional())
		{
			if (ParamType == EParamType_Ellipsis)
				CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token cannot be both optional and ellipsis");

			ParamType = EParamType_Optional;
		}

		return {fg_Move(Type), pName->f_String(), ParamType};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CFunctionType::f_ToJson() const
	{
		CEJsonSorted Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = gc_ConstString_BuildSystemToken;

		auto &Object = UserType.m_Value.f_Object();
		Object[gc_ConstString_Type] = gc_ConstString_FunctionType;
		Object[gc_ConstString_ReturnType] = m_Return.f_Get().f_ToJson().f_ToJson();

		auto &OutParameters = Object[gc_ConstString_Parameters].f_Array();
		for (auto &Parameter : m_Parameters)
			OutParameters.f_Insert(Parameter.f_ToJson().f_ToJson());

		return Return;
	}

	auto CBuildSystemSyntax::CFunctionType::fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position) -> CFunctionType
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "FunctionType token should be an object");

		auto *pReturnType = _Json.f_GetMember(gc_ConstString_ReturnType);
		if (!pReturnType)
			CBuildSystem::fs_ThrowError(_Position, "Function does not have a valid ReturnType member");

		auto *pParameters = _Json.f_GetMember(gc_ConstString_Parameters, EJsonType_Array);
		if (!pParameters)
			CBuildSystem::fs_ThrowError(_Position, "Function does not have a valid Parameters member");

		TCVector<CFunctionParameter> Parameters;
		for (auto &Parameter : pParameters->f_Array())
			Parameters.f_Insert(CBuildSystemSyntax::CFunctionParameter::fs_FromJson(o_StringCache, Parameter, _Position));

		return {CBuildSystemSyntax::CType::fs_FromJson(o_StringCache, *pReturnType, _Position), fg_Move(Parameters)};
	}

	NEncoding::CEJsonSorted CBuildSystemSyntax::CDefine::f_ToJson() const
	{
		if (m_Type.f_IsFunction())
			return m_Type.f_ToJson();
		else
		{
			CEJsonSorted Return;
			auto &UserType = Return.f_UserType();
			UserType.m_Type = gc_ConstString_BuildSystemToken;

			auto &Object = UserType.m_Value.f_Object();
			Object[gc_ConstString_Type] = gc_ConstString_Define;
			Object[gc_ConstString_Define] = m_Type.f_ToJson().f_ToJson();
			if (m_bLegacy)
				Object[gc_ConstString_Legacy] = m_bLegacy;

			return Return;
		}
	}

	auto CBuildSystemSyntax::CDefine::fs_FromJson(CStringCache &o_StringCache, CEJsonSorted const &_Json, CFilePosition const &_Position) -> CDefine
	{
		if (!_Json.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "Define token should be an object");

		auto &Value = _Json;
		DMibRequire(Value.f_GetMember(gc_ConstString_Type, EJsonType_String) && Value.f_GetMember(gc_ConstString_Type, EJsonType_String)->f_String() == gc_ConstString_Define.m_String);

		auto pDefine = Value.f_GetMember(gc_ConstString_Define);
		if (!pDefine)
			CBuildSystem::fs_ThrowError(_Position, "Define does not have valid Define member");

		bool bLegacy = false;
		if (auto pValue = Value.f_GetMember(gc_ConstString_Legacy))
			bLegacy = pValue->f_Boolean();

		return CBuildSystemSyntax::CDefine{CBuildSystemSyntax::CType::fs_FromJson(o_StringCache, *pDefine, _Position), bLegacy};
	}
}
