// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"

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

	auto CBuildSystemSyntax::CDefaultType::fs_FromJSON(CJSON const &_JSON, CFilePosition const &_Position) -> CDefaultType
	{
		if (!_JSON.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "DefaultType token should be an object");

		auto *pTypeName = _JSON.f_GetMember("TypeName", EJSONType_String);
		if (!pTypeName)
			CBuildSystem::fs_ThrowError(_Position, "DefautType does not have a valid TypeName member");

		auto &TypeName = pTypeName->f_String();

		if (TypeName == "int")
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Integer};
		else if (TypeName == "string")
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_String};
		else if (TypeName == "float")
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_FloatingPoint};
		else if (TypeName == "bool")
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Boolean};
		else if (TypeName == "date")
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Date};
		else if (TypeName == "binary")
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Binary};
		else if (TypeName == "any")
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Any};
		else if (TypeName == "void")
			return CBuildSystemSyntax::CDefaultType{CBuildSystemSyntax::CDefaultType::EType_Void};
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid type name for DefautType: '{}'"_f << TypeName);

		return {};
	}


	auto CBuildSystemSyntax::CTypeDefaulted::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> CTypeDefaulted
	{
		if (!_JSON.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted token should be an object");

		auto *pType = _JSON.f_GetMember("InnerType");
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted does not have a valid Type member");

		auto *pDefault = _JSON.f_GetMember("DefaultValue");
		if (!pDefault)
			CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted does not have a valid DefaultValue member");

		CStr Type;
		if (pDefault->f_IsUserType())
		{
			auto &UserType = pDefault->f_UserType();
			if (UserType.m_Type != "BuildSystemToken")
				CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted does not have valid DefaultValue member");

			auto pType = UserType.m_Value.f_GetMember("Type", EJSONType_String);

			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "TypeDefaulted does not have valid Type member");

			Type = pType->f_String();
		}

		return CTypeDefaulted{{CType::fs_FromJSON(*pType, _Position)}, CParam::fs_FromJSON(*pDefault, _Position, Type, false)};
	}

	auto CBuildSystemSyntax::CUserType::fs_FromJSON(NEncoding::CJSON const &_JSON, CFilePosition const &_Position) -> CUserType
	{
		if (!_JSON.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "UserType token should be an object");

		auto *pTypeName = _JSON.f_GetMember("TypeName", EJSONType_String);
		if (!pTypeName)
			CBuildSystem::fs_ThrowError(_Position, "UserType does not have a valid TypeName member");

		return CUserType{pTypeName->f_String()};
	}

	auto CBuildSystemSyntax::CClassType::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> CClassType
	{
		if (!_JSON.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "ClassType token should be an object");

		CClassType ClassType;
		for (auto &Member : _JSON.f_Object())
		{
			auto Name = Member.f_Name();
			switch (EJSONStringType(Name.f_GetUserData()))
			{
			case EJSONStringType_NoQuote:
				{
					if (Name == "<<")
						CBuildSystem::fs_ThrowError(_Position, "Append object cannot be used in types");
					else if (Name == "...")
					{
						ClassType.m_OtherKeysType = CType::fs_FromJSON(Member.f_Value(), _Position);
						continue;
					}
				}
				break;
			case EJSONStringType_Custom:
				CBuildSystem::fs_ThrowError(_Position, "Eval string as key not supported in types");
				break;
			case EJSONStringType_DoubleQuote:
			case EJSONStringType_SingleQuote:
					break;
			}

			bool bOptional = Name.f_EndsWith("?");
			if (bOptional)
			{
				auto UserData = Name.f_GetUserData();
				Name = Name.f_Left(Name.f_GetLen() - 1);
				Name.f_SetUserData(UserData);
			}

			auto &ClassMember = ClassType.m_Members[Name];
			ClassMember.m_bOptional = bOptional;
			ClassMember.m_Type = CType::fs_FromJSON(Member.f_Value(), _Position);
		}

		return ClassType;
	}

	auto CBuildSystemSyntax::CArrayType::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> CArrayType
	{
		if (!_JSON.f_IsArray())
			CBuildSystem::fs_ThrowError(_Position, "ArrayType token should be an array");

		auto &Array = _JSON.f_Array();
		if (Array.f_GetLen() != 1)
			CBuildSystem::fs_ThrowError(_Position, "Array definitions should have one entry");

		return CBuildSystemSyntax::CArrayType{CBuildSystemSyntax::CType::fs_FromJSON(Array[0], _Position)};
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

	auto CBuildSystemSyntax::CType::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> CType
	{
		if (_JSON.f_IsUserType())
		{
			auto &UserType = _JSON.f_UserType();
			if (UserType.m_Type != "BuildSystemToken")
				CBuildSystem::fs_ThrowError(_Position, "Define does not have valid Define member");

			auto pType = UserType.m_Value.f_GetMember("Type", EJSONType_String);

			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Define does not have valid Define Type member");

			auto &Type = pType->f_String();
			bool bOptional = false;

			if (Type == "Optional")
			{
				auto pParam = UserType.m_Value.f_GetMember("Param");

				if (!pParam)
					CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Param member");

				auto Type = CBuildSystemSyntax::CType::fs_FromJSON(CEJSON::fs_FromJSON(*pParam), _Position);
				Type.m_bOptional = true;

				return Type;
			}
			else if (Type == "DefaultType")
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::CDefaultType::fs_FromJSON(UserType.m_Value, _Position), bOptional};
			else if (Type == "Type")
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::CUserType::fs_FromJSON(UserType.m_Value, _Position), bOptional};
			else if (Type == "OneOf")
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::COneOf::fs_FromJSON(CEJSON::fs_FromJSON(UserType.m_Value), _Position), bOptional};
			else if (Type == "FunctionType")
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::CFunctionType::fs_FromJSON(CEJSON::fs_FromJSON(UserType.m_Value), _Position), bOptional};
			else if (Type == "TypeDefaulted")
				return CBuildSystemSyntax::CType{CBuildSystemSyntax::CTypeDefaulted::fs_FromJSON(CEJSON::fs_FromJSON(UserType.m_Value), _Position), bOptional};
			else
				CBuildSystem::fs_ThrowError(_Position, "Invalid type user type: {}"_f << Type);
		}
		else if (_JSON.f_IsArray())
			return CBuildSystemSyntax::CType{CBuildSystemSyntax::CArrayType::fs_FromJSON(_JSON, _Position)};
		else if (_JSON.f_IsObject())
			return CBuildSystemSyntax::CType{CBuildSystemSyntax::CClassType::fs_FromJSON(_JSON, _Position)};
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid type: {jp}"_f << _JSON);

		return {};
	}

	auto CBuildSystemSyntax::COneOf::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> COneOf
	{
		if (!_JSON.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "OneOf token should be an object");

		auto *pOneOfList = _JSON.f_GetMember("OneOfList", EJSONType_Array);
		if (!pOneOfList)
			CBuildSystem::fs_ThrowError(_Position, "OneOf does not have a valid OneOfList member");

		COneOf Return;

		for (auto &OneOf : pOneOfList->f_Array())
		{
			if (OneOf.f_IsUserType() || OneOf.f_IsArray() || OneOf.f_IsObject())
				Return.m_OneOf.f_Insert(CBuildSystemSyntax::CType::fs_FromJSON(OneOf, _Position));
			else
				Return.m_OneOf.f_Insert(OneOf);
		}

		return Return;
	}

	auto CBuildSystemSyntax::CFunctionParameter::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> CFunctionParameter
	{
		if (!_JSON.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token should be an object");

		auto pType = _JSON.f_GetMember("Type");
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token does not have a valid Type member");

		auto pName = _JSON.f_GetMember("Name", EJSONType_String);
		if (!pName)
			CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token does not have a valid Name member");

		auto pEllipsis = _JSON.f_GetMember("Ellipsis", EJSONType_Boolean);
		if (!pEllipsis)
			CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token does not have a valid Ellipsis member");

		EParamType ParamType;
		if (pEllipsis->f_Boolean())
			ParamType = EParamType_Ellipsis;
		else
			ParamType = EParamType_None;

		auto Type = CBuildSystemSyntax::CType::fs_FromJSON(*pType, _Position);
		if (Type.f_IsOptional())
		{
			if (ParamType == EParamType_Ellipsis)
				CBuildSystem::fs_ThrowError(_Position, "FunctionParameter token cannot be both optional and ellipsis");

			ParamType = EParamType_Optional;
		}

		return {fg_Move(Type), pName->f_String(), ParamType};
	}

	auto CBuildSystemSyntax::CFunctionType::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> CFunctionType
	{
		if (!_JSON.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "FunctionType token should be an object");

		auto *pReturnType = _JSON.f_GetMember("ReturnType");
		if (!pReturnType)
			CBuildSystem::fs_ThrowError(_Position, "Function does not have a valid ReturnType member");

		auto *pParameters = _JSON.f_GetMember("Parameters", EJSONType_Array);
		if (!pParameters)
			CBuildSystem::fs_ThrowError(_Position, "Function does not have a valid Parameters member");

		TCVector<CFunctionParameter> Parameters;
		for (auto &Parameter : pParameters->f_Array())
			Parameters.f_Insert(CBuildSystemSyntax::CFunctionParameter::fs_FromJSON(Parameter, _Position));

		return {CBuildSystemSyntax::CType::fs_FromJSON(*pReturnType, _Position), fg_Move(Parameters)};
	}

	auto CBuildSystemSyntax::CDefine::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position) -> CDefine
	{
		if (!_JSON.f_IsObject())
			CBuildSystem::fs_ThrowError(_Position, "Define token should be an object");

		auto &Value = _JSON;
		DMibRequire(Value.f_GetMember("Type", EJSONType_String) && Value.f_GetMember("Type", EJSONType_String)->f_String() == "Define");

		auto pDefine = Value.f_GetMember("Define");
		if (!pDefine)
			CBuildSystem::fs_ThrowError(_Position, "Define does not have valid Define member");

		return CBuildSystemSyntax::CDefine{CBuildSystemSyntax::CType::fs_FromJSON(*pDefine, _Position)};
	}
}
