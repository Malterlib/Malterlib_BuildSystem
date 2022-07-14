// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/BuildSystem/Registry>

namespace NMib::NBuildSystem
{
	template <typename tf_CStr>
	void CBuildSystemSyntax::CEvalStringToken::f_Format(tf_CStr &o_Str) const
	{
		m_Token.f_Visit
			(
				[&](auto const &_Value)
				{
					using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
					if constexpr (NTraits::TCIsSame<CValueType, NStorage::TCIndirection<CExpression>>::mc_Value)
					{
						o_Str += "@";
						o_Str += typename tf_CStr::CFormat("{}") << _Value;
					}
					else
						o_Str += _Value.f_EscapeStrNoQuotes("\\`");
				}
			)
		;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CEvalString::f_Format(tf_CStr &o_Str, bool _bAddQuotes) const
	{
		if (_bAddQuotes)
			o_Str += "`";

		for (auto &Token : m_Tokens)
			o_Str += typename tf_CStr::CFormat("{}") << Token;

		if (_bAddQuotes)
			o_Str += "`";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CWildcardString::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "~";
		if (m_String.f_IsOfType<CEvalString>())
			o_Str += typename tf_CStr::CFormat("{}") << m_String.f_GetAsType<CEvalString>();
		else
			CBuildSystemSyntax::fs_FormatString(o_Str, m_String.f_GetAsType<NStr::CStr>());
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CParam::f_Format(tf_CStr &o_Str) const
	{
		m_Param.f_Visit
			(
				[&](auto const &_Value)
				{
					if constexpr (NTraits::TCIsSameDereferencedUnqualified<decltype(_Value), NEncoding::CEJSON>::mc_Value)
						o_Str += typename tf_CStr::CFormat("{}") << _Value.f_ToString(nullptr, gc_BuildSystemJSONParseFlags);
					else
						o_Str += typename tf_CStr::CFormat("{}") << _Value;
				}
			)
		;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CFunctionCall::f_Format(tf_CStr &o_Str) const
	{
		auto iParam = m_Params.f_GetIterator();

		if (m_bPostFunction)
		{
			DMibCheck(!!iParam);
			if (iParam)
			{
				o_Str += typename tf_CStr::CFormat("{}") << *iParam;
				++iParam;
			}
			o_Str += "->";
		}

		if (!m_bEmptyPropertyType)
		{
			o_Str += fg_PropertyTypeToStr(m_PropertyType);
			o_Str += ".";
		}
		o_Str += m_Name;
		o_Str += "(";

		bool bFirst = true;
		for (; iParam; ++iParam)
		{
			if (bFirst)
				bFirst = false;
			else
				o_Str += ", ";


			o_Str += typename tf_CStr::CFormat("{}") << *iParam;
		}

		o_Str += ")";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CTernary::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}") << m_Conditional;
		o_Str += " ? ";
		o_Str += typename tf_CStr::CFormat("{}") << m_Left;
		o_Str += " : ";
		o_Str += typename tf_CStr::CFormat("{}") << m_Right;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CBinaryOperator::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("({}") << m_Left;
		switch (m_Operator)
		{
		case EOperator_LessThan: o_Str += " < "; break;
		case EOperator_LessThanEqual: o_Str += " <= "; break;
		case EOperator_GreaterThan: o_Str += " > "; break;
		case EOperator_GreaterThanEqual: o_Str += ">= "; break;
		case EOperator_Equal: o_Str += " == "; break;
		case EOperator_NotEqual: o_Str += " != "; break;
		case EOperator_MatchEqual: o_Str += " <==> "; break;
		case EOperator_MatchNotEqual: o_Str += " <!=> "; break;
		case EOperator_Add: o_Str += " + "; break;
		case EOperator_Subtract: o_Str += " - "; break;
		case EOperator_Divide: o_Str += " / "; break;
		case EOperator_Multiply: o_Str += " * "; break;
		case EOperator_Modulus: o_Str += " % "; break;
		case EOperator_BitwiseLeftShift: o_Str += " << "; break;
		case EOperator_BitwiseRightShift: o_Str += " >> "; break;
		case EOperator_BitwiseAnd: o_Str += " & "; break;
		case EOperator_BitwiseXor: o_Str += " ^ "; break;
		case EOperator_BitwiseOr: o_Str += " | "; break;
		case EOperator_And: o_Str += " && "; break;
		case EOperator_Or: o_Str += " || "; break;
		}
		o_Str += typename tf_CStr::CFormat("{})") << m_Right;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CPrefixOperator::f_Format(tf_CStr &o_Str) const
	{
		switch (m_Operator)
		{
		case EOperator_LogicalNot: o_Str += "!"; break;
		case EOperator_BitwiseNot: o_Str += "~"; break;
		case EOperator_UnaryPlus: o_Str += "+"; break;
		case EOperator_UnaryMinus: o_Str += "-"; break;
		}
		o_Str += typename tf_CStr::CFormat("{}") << m_Right;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CExpression::f_Format(tf_CStr &o_Str) const
	{
		if (m_bParen)
			o_Str += "(";
		m_Expression.f_Visit
			(
				[&](auto const &_Value)
				{
					o_Str += typename tf_CStr::CFormat("{}") << _Value;
				}
			)
		;
		if (m_bParen)
			o_Str += ")";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CExpressionAppend::f_Format(tf_CStr &o_Str) const
	{
		CExpression::f_Format(o_Str);
		o_Str += "...";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CJSONSubscript::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "[";
		m_Index.f_Visit
			(
				[&](auto const &_Value)
				{
					o_Str += typename tf_CStr::CFormat("{}") << _Value;
				}
			)
		;
		o_Str += "]";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CJSONAccessorEntry::f_Format(tf_CStr &o_Str) const
	{
		m_Accessor.f_Visit
			(
				[&](auto const &_Value)
				{
					using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
					if constexpr (NTraits::TCIsSame<CValueType, CExpression>::mc_Value)
						o_Str += "@";

					o_Str += typename tf_CStr::CFormat("{}") << _Value;
				}
			)
		;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CJSONAccessor::fs_FormatAccessors(tf_CStr &o_Str, NContainer::TCVector<CJSONAccessorEntry> const &_Accessors)
	{
		o_Str += "<";
		bool bFirst = true;
		for (auto &Accessor : _Accessors)
		{
			if (!bFirst && !Accessor.m_Accessor.f_IsOfType<CJSONSubscript>())
				o_Str += ".";

			o_Str += typename tf_CStr::CFormat("{}") << Accessor;

			bFirst = false;
		}
		o_Str += ">";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CJSONAccessor::f_Format(tf_CStr &o_Str) const
 	{
		o_Str += typename tf_CStr::CFormat("{}") << m_Param;
		fs_FormatAccessors(o_Str, m_Accessors);
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CIdentifier::f_Format(tf_CStr &o_Str) const
	{
		if (m_EntityType != EEntityType_Invalid)
		{
			o_Str += fg_EntityTypeToStr(m_EntityType);
			o_Str += ":";
		}

		if (!m_bEmptyPropertyType)
		{
			o_Str += fg_PropertyTypeToStr(m_PropertyType);
			o_Str += ".";
		}

		m_Name.f_Visit
			(
				[&](auto const &_Value)
				{
					using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
					typename tf_CStr::CAppender Appender(o_Str);

					if constexpr (NTraits::TCIsSame<CValueType, NStr::CStr>::mc_Value)
						NContainer::TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_GenerateIdentifier(Appender, _Value);
					else
					{
						for (auto &Token : _Value.m_Tokens)
						{
							Token.m_Token.f_Visit
								(
									[&](auto const &_Value)
									{
										using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
										if constexpr (NTraits::TCIsSame<CValueType, NStr::CStr>::mc_Value)
											NContainer::TCRegistry_CustomKeyValue<CBuildSystemSyntax::CRootKey, CBuildSystemSyntax::CRootValue>::fs_GenerateIdentifier(Appender, _Value);
										else
											o_Str += typename tf_CStr::CFormat("@{}") << _Value;
									}
								)
							;
						}
					}
				}
			)
		;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CArray::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{vs}") << m_Array;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CAppendObject::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "<<";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CObjectKey::f_Format(tf_CStr &o_Str) const
	{
		m_Key.f_Visit
			(
				[&](auto const &_Value)
				{
					if constexpr (NTraits::TCIsSameDereferencedUnqualified<decltype(_Value), NStr::CStr>::mc_Value)
						CBuildSystemSyntax::fs_FormatKeyString(o_Str, _Value);
					else
						o_Str += typename tf_CStr::CFormat("{}") << _Value;
				}
			)
		;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CObject::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "{";
		bool bFirst = true;
		for (auto &Value : m_ObjectSorted)
		{
			if (bFirst)
				bFirst = false;
			else
				o_Str += ", ";

			o_Str += typename tf_CStr::CFormat("{}: {}") << m_Object.fs_GetKey(Value) << Value.m_Value;
		}
		o_Str += "}";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CDefaultType::f_Format(tf_CStr &o_Str) const
	{
		switch (m_Type)
		{
		case EType_Any:
			o_Str += "any";
			return;
		case EType_Void:
			o_Str += "void";
			return;
		case EType_String:
			o_Str += "string";
			return;
		case EType_Integer:
			o_Str += "int";
			return;
		case EType_FloatingPoint:
			o_Str += "float";
			return;
		case EType_Boolean:
			o_Str += "bool";
			return;
		case EType_Date:
			o_Str += "date";
			return;
		case EType_Binary:
			o_Str += "binary";
			return;
		}
		DMibNeverGetHere;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CArrayType::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("[{}]") << m_Type.f_Get();
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CUserType::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("type({})") << m_Name;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CClassType::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "{";

		bool bAdded = false;

		for (auto &Member : m_MembersSorted)
		{
			if (bAdded)
				o_Str += ", ";

			auto Key = m_Members.fs_GetKey(Member);
			if (Member.m_bOptional)
			{
				auto UserData = Key.f_GetUserData();
				Key += "?";
				Key.f_SetUserData(UserData);
			}

			CBuildSystemSyntax::fs_FormatKeyString(o_Str, Key);

			o_Str += typename tf_CStr::CFormat(": {}") << Member.m_Type.f_Get();

			bAdded = true;
		}

		if (m_OtherKeysType)
		{
			if (bAdded)
				o_Str += ", ";
			o_Str += typename tf_CStr::CFormat("...: {}") << m_OtherKeysType.f_Get().f_Get();
		}

		o_Str += "}";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CType::f_Format(tf_CStr &o_Str) const
	{
		m_Type.f_Visit
			(
				[&](auto const &_Value)
				{
					o_Str += typename tf_CStr::CFormat("{}") << _Value;
				}
			)
		;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CTypeDefaulted::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}") << m_Type.f_Get();
		o_Str += " = ";
		o_Str += typename tf_CStr::CFormat("{}") << m_DefaultValue;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::COneOf::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "one_of(";
		bool bFirst = true;
		for (auto &OneOf : m_OneOf)
		{
			if (bFirst)
				bFirst = false;
			else
				o_Str += ", ";

			OneOf.f_Visit
				(
					[&](auto const &_Value)
					{
						if constexpr (NTraits::TCIsSameDereferencedUnqualified<decltype(_Value), NEncoding::CEJSON>::mc_Value)
						{
							typename tf_CStr::CAppender Appender(o_Str);
							NEncoding::NJSON::fg_GenerateJSONValue<CBuildSystemParseContext>(Appender, _Value.f_ToJSON(), 0, nullptr, gc_BuildSystemJSONParseFlags);
						}
						else
							o_Str += typename tf_CStr::CFormat("{}") << _Value;
					}
				)
			;
		}
		o_Str += ")";
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CFunctionParameter::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}") << m_Type.f_Get();

		switch (m_ParamType)
		{
		case EParamType_None: break;
		case EParamType_Optional: o_Str += "?"; break;
		case EParamType_Ellipsis: o_Str += "..."; break;
		}

		o_Str += typename tf_CStr::CFormat(" {}") << m_Name;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CFunctionType::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "function(";

		bool bFirst = true;
		for (auto &Param : m_Parameters)
		{
			if (bFirst)
				bFirst = false;
			else
				o_Str += ", ";

			o_Str += typename tf_CStr::CFormat("{}") << Param;
		}

		o_Str += ") ";
		o_Str += typename tf_CStr::CFormat("{}") << m_Return.f_Get();
	}


	template <typename tf_CStr>
	void CBuildSystemSyntax::CDefine::f_Format(tf_CStr &o_Str) const
	{
		o_Str += "define ";
		o_Str += typename tf_CStr::CFormat("{}") << m_Type;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::COperator::f_Format(tf_CStr &o_Str) const
	{
		switch (m_Operator)
		{
		case EOperator_LessThan: o_Str += "< "; break;
		case EOperator_LessThanEqual: o_Str += "<= "; break;
		case EOperator_GreaterThan: o_Str += "> "; break;
		case EOperator_GreaterThanEqual: o_Str += ">= "; break;
		case EOperator_Equal: o_Str += "== "; break;
		case EOperator_NotEqual: o_Str += "!= "; break;
		case EOperator_MatchEqual: o_Str += "<==> "; break;
		case EOperator_MatchNotEqual: o_Str += "<!=> "; break;
		case EOperator_Append: o_Str += "=+ "; break;
		case EOperator_Prepend: o_Str += "+= "; break;
		}
		o_Str += typename tf_CStr::CFormat("{}") << m_Right;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CValue::f_Format(tf_CStr &o_Str) const
	{
		m_Value.f_Visit
			(
				[&](auto const &_Value)
				{
					if constexpr (NTraits::TCIsSameDereferencedUnqualified<decltype(_Value), NEncoding::CEJSON>::mc_Value)
					{
						typename tf_CStr::CAppender Appender(o_Str);
						NEncoding::NJSON::fg_GenerateJSONValue<CBuildSystemParseContext>(Appender, _Value.f_ToJSON(), 0, nullptr, gc_BuildSystemJSONParseFlags);
					}
					else
						o_Str += typename tf_CStr::CFormat("{}") << _Value;
				}
			)
		;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CRootValue::f_Format(tf_CStr &o_Str) const
	{
		if (!m_Accessors.f_IsEmpty())
		{
			o_Str += "#";
			CJSONAccessor::fs_FormatAccessors(o_Str, m_Accessors);
			o_Str += " ";
		}

		o_Str += typename tf_CStr::CFormat("{}") << m_Value;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CRootKey::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}") << m_Value;
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CKeyLogicalOperator::f_Format(tf_CStr &o_Str) const
	{
		switch (m_Operator)
		{
		case EOperator_And: o_Str += "&"; break;
		case EOperator_Or: o_Str += "|"; break;
		case EOperator_Not: o_Str += "!"; break;
		}
	}

	template <typename tf_CStr>
	void CBuildSystemSyntax::CKeyPrefixOperator::f_Format(tf_CStr &o_Str) const
	{
		switch (m_Operator)
		{
		case EOperator_Entity: o_Str += "%"; break;
		case EOperator_ConfigurationTuple: o_Str += "*"; break;
		case EOperator_Equal: o_Str += "!!"; break;
		case EOperator_NotEqual: o_Str += "!"; break;
		case EOperator_Pragma: o_Str += "#"; break;
		}
		o_Str += typename tf_CStr::CFormat("{}") << m_Right;
	}
}
