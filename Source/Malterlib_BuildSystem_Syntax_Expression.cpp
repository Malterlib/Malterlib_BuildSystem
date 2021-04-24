// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	NEncoding::CEJSON CBuildSystemSyntax::CKeyLogicalOperator::f_ToJSON() const
	{
		CEJSON Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = "BuildSystemToken";
		UserType.m_Value["Type"] = "KeyLogicalOperator";
		auto &Operator = UserType.m_Value["Operator"];

		switch (m_Operator)
		{
		case EOperator_And: Operator = "&"; break;
		case EOperator_Or: Operator = "|"; break;
		case EOperator_Not: Operator = "!"; break;
		}

		return Return;
	}

	CBuildSystemSyntax::CKeyLogicalOperator CBuildSystemSyntax::CKeyLogicalOperator::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position)
	{
 		auto &UserType = _JSON.f_UserType();
		DMibRequire(UserType.m_Type == "BuildSystemToken");

		auto &Value = UserType.m_Value;
		auto *pType = UserType.m_Value.f_GetMember("Type");
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Missing Type member for PrefixOperator token");

		auto &Type = pType->f_String();

		if (Type != "KeyLogicalOperator")
			CBuildSystem::fs_ThrowError(_Position, "'{}' is not a valid type for Param"_f << Type);

		auto pOperator = Value.f_GetMember("Operator");
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "Missing Operator member for PrefixOperator token");

		CBuildSystemSyntax::CKeyLogicalOperator OutputOperator;

		auto Operator = pOperator->f_String();
		if (Operator == "&")
			OutputOperator.m_Operator = EOperator_And;
		else if (Operator == "|")
			OutputOperator.m_Operator = EOperator_Or;
		else if (Operator == "!")
			OutputOperator.m_Operator = EOperator_Not;
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid operator '{}' for KeyLogicalOperator token"_f << Operator);

		return OutputOperator;
	}

	auto CBuildSystemSyntax::CKeyPrefixOperator::fs_TypeFromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position) -> EOperator
	{
 		auto &UserType = _JSON.f_UserType();
		DMibRequire(UserType.m_Type == "BuildSystemToken");

		auto &Value = UserType.m_Value;
		auto *pType = UserType.m_Value.f_GetMember("Type");
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Missing Type member for PrefixOperator token");

		auto &Type = pType->f_String();

		if (Type != "KeyPrefixOperator")
			CBuildSystem::fs_ThrowError(_Position, "'{}' is not a valid type for Param"_f << Type);

		auto pOperator = Value.f_GetMember("Operator");
		if (!pOperator)
			CBuildSystem::fs_ThrowError(_Position, "Missing Operator member for PrefixOperator token");

		CBuildSystemSyntax::CKeyPrefixOperator OutputOperator;

		auto Operator = pOperator->f_String();
		if (Operator == "!!")
			return EOperator_Equal;
		else if (Operator == "!")
			return EOperator_NotEqual;
		else if (Operator == "%")
			return EOperator_Entity;
		else if (Operator == "*")
			return EOperator_ConfigurationTuple;
		else if (Operator == "#")
			return EOperator_Pragma;
		else
			CBuildSystem::fs_ThrowError(_Position, "Invalid operator '{}' for PrefixOperator token"_f << Operator);
	}

	NEncoding::CEJSON CBuildSystemSyntax::CKeyPrefixOperator::f_ToJSON() const
	{
		CEJSON Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = "BuildSystemToken";
		UserType.m_Value["Type"] = "KeyPrefixOperator";
		auto &Operator = UserType.m_Value["Operator"];

		switch (m_Operator)
		{
		case EOperator_Equal: Operator = "!!"; break;
		case EOperator_NotEqual: Operator = "!"; break;
		case EOperator_Entity: Operator = "%"; break;
		case EOperator_ConfigurationTuple: Operator = "*"; break;
		case EOperator_Pragma: Operator = "#"; break;
		}

		UserType.m_Value["Right"] = m_Right.f_ToJSON().f_ToJSON();

		return Return;
	}

	CBuildSystemSyntax::CKeyPrefixOperator CBuildSystemSyntax::CKeyPrefixOperator::fs_FromJSON(EOperator _Operator, NEncoding::CEJSON const &_JSON, CFilePosition const &_Position)
	{
 		auto &UserType = _JSON.f_UserType();
		DMibRequire(UserType.m_Type == "BuildSystemToken");

		CBuildSystemSyntax::CKeyPrefixOperator OutputOperator;
		OutputOperator.m_Operator = _Operator;

		auto &Value = UserType.m_Value;
		auto pRight = Value.f_GetMember("Right");
		if (!pRight)
			CBuildSystem::fs_ThrowError(_Position, "Missing Right member for PrefixOperator token");

		auto Right = CEJSON::fs_FromJSON(*pRight);
		OutputOperator.m_Right = CBuildSystemSyntax::CValue::fs_FromJSON(Right, _Position, false);

		return OutputOperator;
	}

	CBuildSystemSyntax::CKeyPrefixOperator CBuildSystemSyntax::CKeyPrefixOperator::fs_FromJSON(NEncoding::CEJSON const &_JSON, CFilePosition const &_Position)
	{
		return fs_FromJSON(CKeyPrefixOperator::fs_TypeFromJSON(_JSON, _Position), _JSON, _Position);
	}

	bool CBuildSystemSyntax::CParam::f_IsBinaryOperator() const
	{
		if (m_Param.f_IsOfType<NStorage::TCIndirection<CBinaryOperator>>())
			return true;

		if (m_Param.f_IsOfType<NStorage::TCIndirection<CExpression>>())
		{
			auto &Expression = m_Param.f_GetAsType<NStorage::TCIndirection<CExpression>>().f_Get();
			if (!Expression.m_bParen && Expression.m_Expression.f_IsOfType<CParam>())
				return Expression.m_Expression.f_GetAsType<CParam>().f_IsBinaryOperator();
		}

		return false;
	}

	CBuildSystemSyntax::CBinaryOperator &CBuildSystemSyntax::CParam::f_BinaryOperator()
	{
		if (m_Param.f_IsOfType<NStorage::TCIndirection<CExpression>>())
		{
			auto &Expression = m_Param.f_GetAsType<NStorage::TCIndirection<CExpression>>().f_Get();
			if (!Expression.m_bParen && Expression.m_Expression.f_IsOfType<CParam>())
				return Expression.m_Expression.f_GetAsType<CParam>().f_BinaryOperator();
		}

		return m_Param.f_GetAsType<NStorage::TCIndirection<CBinaryOperator>>();
	}

	CBuildSystemSyntax::CBinaryOperator const &CBuildSystemSyntax::CParam::f_BinaryOperator() const
	{
		if (m_Param.f_IsOfType<NStorage::TCIndirection<CExpression>>())
		{
			auto &Expression = m_Param.f_GetAsType<NStorage::TCIndirection<CExpression>>().f_Get();
			if (!Expression.m_bParen && Expression.m_Expression.f_IsOfType<CParam>())
				return Expression.m_Expression.f_GetAsType<CParam>().f_BinaryOperator();
		}

		return m_Param.f_GetAsType<NStorage::TCIndirection<CBinaryOperator>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsJson() const
	{
		return m_Param.f_IsOfType<NEncoding::CEJSON>();
	}

	NEncoding::CEJSON const &CBuildSystemSyntax::CParam::f_Json() const
	{
		return m_Param.f_GetAsType<NEncoding::CEJSON>();
	}

	bool CBuildSystemSyntax::CParam::f_IsObject() const
	{
		return m_Param.f_IsOfType<CObject>();
	}

	CBuildSystemSyntax::CObject const &CBuildSystemSyntax::CParam::f_Object() const
	{
		return m_Param.f_GetAsType<CObject>();
	}

	bool CBuildSystemSyntax::CParam::f_IsArray() const
	{
		return m_Param.f_IsOfType<CArray>();
	}

	CBuildSystemSyntax::CArray const &CBuildSystemSyntax::CParam::f_Array() const
	{
		return m_Param.f_GetAsType<CArray>();
	}

	bool CBuildSystemSyntax::CParam::f_IsIdentifier() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CIdentifier>>();
	}

	CBuildSystemSyntax::CIdentifier const &CBuildSystemSyntax::CParam::f_Identifier() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CIdentifier>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsEvalString() const
	{
		return m_Param.f_IsOfType<CEvalString>();
	}

	CBuildSystemSyntax::CEvalString const &CBuildSystemSyntax::CParam::f_EvalString() const
	{
		return m_Param.f_GetAsType<CEvalString>();
	}

	bool CBuildSystemSyntax::CParam::f_IsWildcardString() const
	{
		return m_Param.f_IsOfType<CWildcardString>();
	}

	CBuildSystemSyntax::CWildcardString const &CBuildSystemSyntax::CParam::f_WildcardString() const
	{
		return m_Param.f_GetAsType<CWildcardString>();
	}

	bool CBuildSystemSyntax::CParam::f_IsExpression() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CExpression>>();
	}

	CBuildSystemSyntax::CExpression const &CBuildSystemSyntax::CParam::f_Expression() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CExpression>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsExpressionAppend() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CExpressionAppend>>();
	}

	CBuildSystemSyntax::CExpressionAppend const &CBuildSystemSyntax::CParam::f_ExpressionAppend() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CExpressionAppend>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsTernary() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CTernary>>();
	}

	CBuildSystemSyntax::CTernary const &CBuildSystemSyntax::CParam::f_Ternary() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CTernary>>();
	}

	bool CBuildSystemSyntax::CParam::f_IsPrefixOperator() const
	{
		return m_Param.f_IsOfType<NStorage::TCIndirection<CPrefixOperator>>();
	}

	CBuildSystemSyntax::CPrefixOperator const &CBuildSystemSyntax::CParam::f_PrefixOperator() const
	{
		return m_Param.f_GetAsType<NStorage::TCIndirection<CPrefixOperator>>();
	}

	NEncoding::CEJSON CBuildSystemSyntax::CParam::f_ToJSON() const
	{
		switch (m_Param.f_GetTypeID())
		{
		case 0: return m_Param.f_Get<0>();
		case 1: return m_Param.f_Get<1>().f_ToJSON();
		case 2: return m_Param.f_Get<2>().f_ToJSON();
		case 3: return m_Param.f_Get<3>().f_Get().f_ToJSON();
		case 4: return m_Param.f_Get<4>().f_ToJSON();
		case 5: return m_Param.f_Get<5>().f_ToJSON();
		case 6: return m_Param.f_Get<6>().f_Get().f_ToJSON();
		case 7: return m_Param.f_Get<7>().f_Get().f_ToJSON();
		case 8: return m_Param.f_Get<8>().f_Get().f_ToJSON();
		case 9: return m_Param.f_Get<9>().f_Get().f_ToJSON();
		case 10: return m_Param.f_Get<10>().f_Get().f_ToJSON();
		}

		DNeverGetHere;
		return {};
	}

	auto CBuildSystemSyntax::CParam::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position, NStr::CStr const &_Type, bool _bAppendAllowed) -> CParam
	{
		if (!_JSON.f_IsUserType())
		{
			if (_JSON.f_IsArray())
				return CParam{CArray::fs_FromJSON(_JSON, _Position, false)};
			else if (_JSON.f_IsObject())
				return CParam{CObject::fs_FromJSON(_JSON, _Position, false)};
			else
				return CParam{_JSON};
		}

 		auto &UserType = _JSON.f_UserType();
		DMibRequire(UserType.m_Type == "BuildSystemToken");

		auto &Value = UserType.m_Value;

		if (_Type == "Identifier")
			return CParam{CIdentifier::fs_FromJSON(_JSON, _Position)};
		else if (_Type == "EvalString")
		{
			auto pValue = Value.f_GetMember("Value");
			if (!pValue)
				CBuildSystem::fs_ThrowError(_Position, "Missing Value member for EvalString token"_f << _Type);
			return CParam{CEvalString::fs_FromJSON(CEJSON::fs_FromJSON(*pValue), _Position)};
		}
		else if (_Type == "WildcardString")
			return CParam{CWildcardString::fs_FromJSON(CEJSON::fs_FromJSON(Value), _Position)};
		else if (_Type == "JSONAccessor")
			return CParam{NStorage::TCIndirection<CExpression>(CExpression{NStorage::TCIndirection<CJSONAccessor>(CJSONAccessor::fs_FromJSON(CEJSON::fs_FromJSON(Value), _Position))})};
		else if (_Type == "PostFunction" || _Type == "Function")
			return CParam{NStorage::TCIndirection<CExpression>(CExpression{CFunctionCall::fs_FromJSON(Value, _Position, _Type)})};
		else if (_Type == "Expression")
		{
			auto pParam = Value.f_GetMember("Param");
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Param member");

			auto pParen = Value.f_GetMember("Paren", EJSONType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "Expression token does not have valid Paren member");

			return CParam{NStorage::TCIndirection<CExpression>(CExpression::fs_FromJSON(CEJSON::fs_FromJSON(*pParam), _Position, pParen->f_Boolean()))};
		}
		else if (_Type == "AppendExpression")
		{
			if (!_bAppendAllowed)
				CBuildSystem::fs_ThrowError(_Position, "AppendExpression is only allowed as function parameter");

			auto pParam = Value.f_GetMember("Param");
			if (!pParam)
				CBuildSystem::fs_ThrowError(_Position, "AppendExpression token does not have valid Param member");

			auto pParen = Value.f_GetMember("Paren", EJSONType_Boolean);
			if (!pParen)
				CBuildSystem::fs_ThrowError(_Position, "AppendExpression does not have valid Paren member");

			return CParam{NStorage::TCIndirection<CExpressionAppend>(CExpressionAppend::fs_FromJSON(CEJSON::fs_FromJSON(*pParam), _Position, pParen->f_Boolean()))};
		}
		else if (_Type == "Ternary")
			return CParam{NStorage::TCIndirection<CTernary>(CTernary::fs_FromJSON(Value, _Position))};
		else if (_Type == "PrefixOperator")
			return CParam{NStorage::TCIndirection<CPrefixOperator>(CPrefixOperator::fs_FromJSON(Value, _Position))};
		else if (_Type == "BinaryOperator")
		{
			auto Operator = CBinaryOperator::fs_FromJSON(Value, _Position);

			if (Operator.m_Right.f_IsBinaryOperator())
			{
				auto fOperatorPrecedence = [](CBinaryOperator::EOperator _Operator) -> uint32
					{
						switch (_Operator)
						{
						case CBinaryOperator::EOperator_LessThan:
						case CBinaryOperator::EOperator_LessThanEqual:
						case CBinaryOperator::EOperator_GreaterThan:
						case CBinaryOperator::EOperator_GreaterThanEqual:
							return 9;

						case CBinaryOperator::EOperator_Equal:
						case CBinaryOperator::EOperator_NotEqual:
						case CBinaryOperator::EOperator_MatchEqual:
						case CBinaryOperator::EOperator_MatchNotEqual:
							return 10;

						case CBinaryOperator::EOperator_Add:
						case CBinaryOperator::EOperator_Subtract:
							return 6;

						case CBinaryOperator::EOperator_Divide:
						case CBinaryOperator::EOperator_Multiply:
						case CBinaryOperator::EOperator_Modulus:
							return 5;

						case CBinaryOperator::EOperator_BitwiseLeftShift:
						case CBinaryOperator::EOperator_BitwiseRightShift:
							return 7;

						case CBinaryOperator::EOperator_BitwiseAnd: return 11;
						case CBinaryOperator::EOperator_BitwiseXor: return 12;
						case CBinaryOperator::EOperator_BitwiseOr: return 13;
						case CBinaryOperator::EOperator_And: return 14;
						case CBinaryOperator::EOperator_Or: return 15;
						}
						return 0;
					}
				;

				auto RightBinary = fg_Move(Operator.m_Right.f_BinaryOperator());

				auto Prededence = fOperatorPrecedence(Operator.m_Operator);
				auto PrededenceRightBinary = fOperatorPrecedence(RightBinary.m_Operator);
				if (PrededenceRightBinary >= Prededence)
				{
					auto Left1 = fg_Move(Operator.m_Left);

					auto OperatorType = Operator.m_Operator;

					Operator = fg_Move(RightBinary);
					auto *pDestination = &Operator;
					while (pDestination->m_Left.f_IsBinaryOperator())
					{
						auto &Next = pDestination->m_Left.f_BinaryOperator();
						auto PrededenceRightBinary = fOperatorPrecedence(Next.m_Operator);
						if (PrededenceRightBinary < Prededence)
							break;
						pDestination = &pDestination->m_Left.f_BinaryOperator();
					}

					auto Left2 = fg_Move(pDestination->m_Left);

					pDestination->m_Left.m_Param = NStorage::TCIndirection<CBinaryOperator>
						(
							CBinaryOperator
							{
								OperatorType
								, fg_Move(Left1)
								, fg_Move(Left2)
							}
						)
					;
				}
				else
					Operator.m_Right.m_Param = NStorage::TCIndirection<CBinaryOperator>(fg_Move(RightBinary));
			}

			return CParam{NStorage::TCIndirection<CBinaryOperator>(fg_Move(Operator))};
		}
		else
			CBuildSystem::fs_ThrowError(_Position, "'{}' is not a valid type for Param"_f << _Type);

		return {};
	}

	NEncoding::CEJSON CBuildSystemSyntax::CTernary::f_ToJSON() const
	{
		CEJSON Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = "BuildSystemToken";
		UserType.m_Value["Type"] = "Ternary";
		UserType.m_Value["Conditional"] = m_Conditional.f_ToJSON().f_ToJSON();
		UserType.m_Value["Left"] = m_Left.f_ToJSON().f_ToJSON();
		UserType.m_Value["Right"] = m_Right.f_ToJSON().f_ToJSON();

		return Return;
	}

	auto CBuildSystemSyntax::CTernary::fs_FromJSON(CJSON const &_JSON, CFilePosition const &_Position) -> CTernary
	{
		CTernary Ternary;

		auto pConditional = _JSON.f_GetMember("Conditional");
		if (!pConditional)
			CBuildSystem::fs_ThrowError(_Position, "Ternary token does not have valid Conditional member");

		auto pLeft = _JSON.f_GetMember("Left");
		if (!pLeft)
			CBuildSystem::fs_ThrowError(_Position, "Ternary token does not have valid Left member");

		auto pRight = _JSON.f_GetMember("Right");
		if (!pRight)
			CBuildSystem::fs_ThrowError(_Position, "Ternary token does not have valid Right member");

		auto fParseParam = [&](CEJSON const &_JSON) -> CParam
			{
				if (!_JSON.f_IsUserType())
					return CParam::fs_FromJSON(_JSON, _Position, {}, true);

				auto &UserType = _JSON.f_UserType();
				auto &Value = UserType.m_Value;

				auto pType = Value.f_GetMember("Type", EJSONType_String);
				if (!pType)
					CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

				return CParam::fs_FromJSON(_JSON, _Position, pType->f_String(), true);
			}
		;

		Ternary.m_Conditional = fParseParam(CEJSON::fs_FromJSON(*pConditional));
		Ternary.m_Left = fParseParam(CEJSON::fs_FromJSON(*pLeft));
		Ternary.m_Right = fParseParam(CEJSON::fs_FromJSON(*pRight));

		return Ternary;
	}

	NEncoding::CEJSON CBuildSystemSyntax::CFunctionCall::f_ToJSON() const
	{
		CEJSON Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = "BuildSystemToken";
		UserType.m_Value["Type"] = m_bPostFunction ? "PostFunction" : "Function";
		UserType.m_Value["Name"] = m_Name;
		UserType.m_Value["PropertyType"] = m_bEmptyPropertyType ? CStr("") : fg_PropertyTypeToStr(m_PropertyType);

		auto &Params = UserType.m_Value["Params"].f_Array();
		for (auto &Param : m_Params)
			Params.f_Insert(Param.f_ToJSON().f_ToJSON());

		return Return;
	}

	auto CBuildSystemSyntax::CFunctionCall::fs_FromJSON(CJSON const &_JSON, CFilePosition const &_Position, NStr::CStr const &_Type) -> CFunctionCall
	{
		CFunctionCall FunctionCall;

		FunctionCall.m_bPostFunction = _Type == "PostFunction";

		auto pName = _JSON.f_GetMember("Name", EJSONType_String);
		if (!pName)
			CBuildSystem::fs_ThrowError(_Position, "Function token does not have valid Name member");

		auto pPropertyType = _JSON.f_GetMember("PropertyType", EJSONType_String);
		if (!pPropertyType)
			CBuildSystem::fs_ThrowError(_Position, "Function token does not have valid PropertyType member");

		FunctionCall.m_Name = pName->f_String();
		FunctionCall.m_bEmptyPropertyType = pPropertyType->f_String().f_IsEmpty();
		FunctionCall.m_PropertyType = fg_PropertyTypeFromStr(pPropertyType->f_String());
		if (FunctionCall.m_PropertyType == EPropertyType_Invalid)
			CBuildSystem::fs_ThrowError(_Position, "Unknown property type '{}'"_f << pPropertyType->f_String());

		auto pParams = _JSON.f_GetMember("Params", EJSONType_Array);
		if (!pParams)
			CBuildSystem::fs_ThrowError(_Position, "Function token does not have valid Params member");

		auto ParamsEJSON = CEJSON::fs_FromJSON(*pParams);
		auto &Params = ParamsEJSON.f_Array();

		if (FunctionCall.m_bPostFunction && Params.f_GetLen() < 1)
			CBuildSystem::fs_ThrowError(_Position, "Post function call needs at least one parmeter");

		for (auto &Param : Params)
		{
			if (!Param.f_IsUserType())
			{
				FunctionCall.m_Params.f_Insert(CParam::fs_FromJSON(Param, _Position, {}, true));
				continue;
			}

			auto &UserType = Param.f_UserType();
			auto &Value = UserType.m_Value;

			auto pType = Value.f_GetMember("Type", EJSONType_String);
			if (!pType)
				CBuildSystem::fs_ThrowError(_Position, "Param does not have valid Type member");

			FunctionCall.m_Params.f_Insert(CParam::fs_FromJSON(Param, _Position, pType->f_String(), true));
		}

		return FunctionCall;
	}

	bool CBuildSystemSyntax::CExpression::f_IsParam() const
	{
		return m_Expression.f_IsOfType<CExpression>();
	}

	CBuildSystemSyntax::CParam const &CBuildSystemSyntax::CExpression::f_Param() const
	{
		return m_Expression.f_GetAsType<CParam>();
	}

	bool CBuildSystemSyntax::CExpression::f_IsFunctionCall() const
	{
		return m_Expression.f_IsOfType<CFunctionCall>();
	}

	CBuildSystemSyntax::CFunctionCall const &CBuildSystemSyntax::CExpression::f_FunctionCall() const
	{
		return m_Expression.f_GetAsType<CFunctionCall>();
	}

	bool CBuildSystemSyntax::CExpression::f_IsJsonAccessor() const
	{
		return m_Expression.f_IsOfType<NStorage::TCIndirection<CJSONAccessor>>();
	}

	CBuildSystemSyntax::CJSONAccessor const &CBuildSystemSyntax::CExpression::f_JsonAccessor() const
	{
		return m_Expression.f_GetAsType<NStorage::TCIndirection<CJSONAccessor>>().f_Get();
	}

	NEncoding::CEJSON CBuildSystemSyntax::CExpression::f_ToJSON(bool _bAppendExpression) const
	{
		CEJSON Return;
		auto &UserType = Return.f_UserType();
		UserType.m_Type = "BuildSystemToken";
		UserType.m_Value["Type"] = _bAppendExpression ? "AppendExpression" : "Expression";
		UserType.m_Value["Paren"] = m_bParen;

		auto &Param = UserType.m_Value["Param"];
		switch (m_Expression.f_GetTypeID())
		{
		case 0: Param = m_Expression.f_Get<0>().f_ToJSON().f_ToJSON(); break;
		case 1: Param = m_Expression.f_Get<1>().f_ToJSON().f_ToJSON(); break;
		case 2: Param = m_Expression.f_Get<2>().f_Get().f_ToJSON().f_ToJSON(); break;
		default: DMibNeverGetHere;
		}

		return Return;
	}

	NEncoding::CEJSON CBuildSystemSyntax::CExpression::f_ToJSONRaw() const
	{
		switch (m_Expression.f_GetTypeID())
		{
		case 0: return m_Expression.f_Get<0>().f_ToJSON();
		case 1: return m_Expression.f_Get<1>().f_ToJSON();
		case 2: return m_Expression.f_Get<2>().f_Get().f_ToJSON();
		default: DMibNeverGetHere;
		}

		return {};
	}

	auto CBuildSystemSyntax::CExpression::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position, bool _bParen) -> CExpression
	{
		if (!_JSON.f_IsUserType())
			return CExpression{CParam::fs_FromJSON(_JSON, _Position, {}, false)};

		auto &UserType = _JSON.f_UserType();
		if (UserType.m_Type != "BuildSystemToken")
			CBuildSystem::fs_ThrowError(_Position, "Invalid type for expression");

		auto &Value = UserType.m_Value;

		auto pType = Value.f_GetMember("Type", EJSONType_String);
		if (!pType)
			CBuildSystem::fs_ThrowError(_Position, "Expression does not have valid Type member");

		if (pType->f_String() == "Function" || pType->f_String() == "PostFunction")
			return CExpression{CFunctionCall::fs_FromJSON(Value, _Position, pType->f_String()), _bParen};
		else if (pType->f_String() == "JSONAccessor")
			return CExpression{NStorage::TCIndirection<CJSONAccessor>(CJSONAccessor::fs_FromJSON(CEJSON::fs_FromJSON(Value), _Position)), _bParen};
		else
			return CExpression{CParam::fs_FromJSON(_JSON, _Position, pType->f_String(), false), _bParen};
	}

	NEncoding::CEJSON CBuildSystemSyntax::CExpressionAppend::f_ToJSON() const
	{
		return static_cast<CExpression const &>(*this).f_ToJSON(true);
	}
	
	auto CBuildSystemSyntax::CExpressionAppend::fs_FromJSON(CEJSON const &_JSON, CFilePosition const &_Position, bool _bParen) -> CExpressionAppend
	{
		CExpressionAppend Return;
		static_cast<CExpression &>(Return) = CExpression::fs_FromJSON(_JSON, _Position, _bParen);
		return Return;
	}
}
