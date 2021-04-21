// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	CEJSON CBuildSystem::fp_EvaluatePropertyValueObject(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CObject const &_Value) const
	{
		CEJSON ObjectReturn;
		auto &Object = ObjectReturn.f_Object();

		for (auto &Value : _Value.m_ObjectSorted)
		{
			auto &Key = _Value.m_Object.fs_GetKey(Value);

			switch (Key.m_Key.f_GetTypeID())
			{
			case 0:
				{
					Object[Key.m_Key.f_GetAsType<CStr>()] = fp_EvaluatePropertyValue(_Context, Value.m_Value.f_Get(), nullptr);
				}
				break;
			case 1:
				{
					Object[fp_EvaluatePropertyValueEvalString(_Context, Key.m_Key.f_GetAsType<CBuildSystemSyntax::CEvalString>())]
						= fp_EvaluatePropertyValue(_Context, Value.m_Value.f_Get(), nullptr)
					;
				}
				break;
			case 2:
				{
					auto fApplyObject = [&](CEJSON const &_Object)
						{
							if (!_Object.f_IsObject())
								fsp_ThrowError(_Context, "Append object expects object arguments");

							for (auto iObject = _Object.f_Object().f_SortedIterator(); iObject; ++iObject)
								Object[iObject->f_Name()] = iObject->f_Value();
						}
					;

					auto &ValueObject = Value.m_Value.f_Get().m_Value;
					if (ValueObject.f_IsOfType<CBuildSystemSyntax::CArray>())
					{
						auto Array = fp_EvaluatePropertyValueArray(_Context, ValueObject.f_GetAsType<CBuildSystemSyntax::CArray>());
						for (auto &Entry : Array.f_Array())
						{
							if (!Entry.f_IsValid())
								continue;
							if (!Entry.f_IsObject())
								fsp_ThrowError(_Context, "Append object array expected arrays members to evalutate to objects");

							fApplyObject(Entry);
						}
					}
					else if (ValueObject.f_IsOfType<CBuildSystemSyntax::CObject>())
						fApplyObject(fp_EvaluatePropertyValueObject(_Context, ValueObject.f_GetAsType<CBuildSystemSyntax::CObject>()));
					else if (ValueObject.f_IsOfType<CBuildSystemSyntax::CExpression>())
					{
						auto ExpressionResult = fp_EvaluatePropertyValueExpression(_Context, ValueObject.f_GetAsType<CBuildSystemSyntax::CExpression>());

						if (ExpressionResult.f_IsArray())
						{
							for (auto &Entry : ExpressionResult.f_Array())
							{
								if (!Entry.f_IsObject())
									fsp_ThrowError(_Context, "Append object expression array expected arrays members to evalutate to objects");

								fApplyObject(Entry);
							}
						}
						else
							fApplyObject(ExpressionResult);
					}
					else
						fsp_ThrowError(_Context, "Append object only supports objects, arrays or expressions evaluating to objects or arrays");
				}
				break;
			default:
				{
					DMibNeverGetHere;
				}
				break;
			}
		}

		return ObjectReturn;
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueArray(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CArray const &_Value) const
	{
		CEJSON Return;

		auto &ReturnArray = Return.f_Array();

		for (auto &Entry : _Value.m_Array)
		{
			if (Entry.f_Get().m_Value.f_IsOfType<CBuildSystemSyntax::CExpressionAppend>())
			{
				CBuildSystemSyntax::CExpression const &Expression = Entry.f_Get().m_Value.f_GetAsType<CBuildSystemSyntax::CExpressionAppend>();
				CEJSON ToAppend = fp_EvaluatePropertyValueExpression(_Context, Expression);
				if (ToAppend.f_IsArray())
					ReturnArray.f_Insert(ToAppend.f_Array());
				else if (!ToAppend.f_IsValid())
					; // Undefined values are ignored
				else
					fsp_ThrowError(_Context, "Append expressions expects an array to expand. {} resulted in : {}"_f << Expression << ToAppend);
			}
			else
				ReturnArray.f_Insert(fp_EvaluatePropertyValue(_Context, Entry.f_Get(), nullptr));
		}

		return Return;
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueWildcardString(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CWildcardString const &_Value) const
	{
		CStr WildcardValue;
		if (_Value.m_String.f_IsOfType<CStr>())
			WildcardValue = _Value.m_String.f_GetAsType<CStr>();
		else if (_Value.m_String.f_IsOfType<CBuildSystemSyntax::CEvalString>())
			WildcardValue = fp_EvaluatePropertyValueEvalString(_Context, _Value.m_String.f_GetAsType<CBuildSystemSyntax::CEvalString>());
		else
			DMibNeverGetHere;

		return CEJSONUserType{"Wildcard", fg_Move(WildcardValue)};
	}

	CStr CBuildSystem::fp_EvaluatePropertyValueEvalString(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CEvalString const &_Value) const
	{
		CStr ReturnString;

		for (auto &Token : _Value.m_Tokens)
		{
			if (Token.m_Token.f_IsOfType<CStr>())
				ReturnString += Token.m_Token.f_GetAsType<CStr>();
			else if (Token.m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>())
			{
				auto &ExpressionToken = Token.m_Token.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>();
				auto Expression = fp_EvaluatePropertyValueExpression(_Context, ExpressionToken);
				if (!Expression.f_IsString())
				{
					fsp_ThrowError
						(
							_Context
							, "Expressions in eval strings needs to evaluate to strings.\n\tExpression: {}\n\tValue: {}\n\tEvaluated string: {}\n"_f
							<< ExpressionToken
							<< Expression.f_ToString(nullptr, EJSONDialectFlag_AllowUndefined)
							<< ReturnString
						)
					;
				}

				ReturnString += Expression.f_String();
			}
			else
				DMibNeverGetHere;
		}

		return ReturnString;
	}

	void CBuildSystem::fp_ApplyAccessors
		(
			CEvalPropertyValueContext &_Context
			, NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors
			, NFunction::TCFunctionNoAlloc<void (NStr::CStr const &_Member)> const &_fApplyMemberName
			, NFunction::TCFunctionNoAlloc<void (int64 _Index)> const &_fApplyArrayIndex
		) const
	{
		for (auto &Accessor : _Accessors)
		{
			if (Accessor.m_Accessor.f_IsOfType<CStr>())
				_fApplyMemberName(Accessor.m_Accessor.f_GetAsType<CStr>());
			else if (Accessor.m_Accessor.f_IsOfType<CBuildSystemSyntax::CExpression>())
			{
				auto ExpressionResult = fp_EvaluatePropertyValueExpression(_Context, Accessor.m_Accessor.f_GetAsType<CBuildSystemSyntax::CExpression>());
				if (!ExpressionResult.f_IsString())
				{
					fsp_ThrowError
						(
							_Context
							, "Expression {} does not evalutate to a string value for member access"_f << Accessor.m_Accessor.f_GetAsType<CBuildSystemSyntax::CExpression>()
						)
					;
				}
				_fApplyMemberName(ExpressionResult.f_String());
			}
			else if (Accessor.m_Accessor.f_IsOfType<CBuildSystemSyntax::CJSONSubscript>())
			{
				auto &Subscript = Accessor.m_Accessor.f_GetAsType<CBuildSystemSyntax::CJSONSubscript>();
				if (Subscript.m_Index.f_IsOfType<uint32>())
					_fApplyArrayIndex(Subscript.m_Index.f_GetAsType<uint32>());
				else if (Subscript.m_Index.f_IsOfType<CBuildSystemSyntax::CExpression>())
				{
					auto ExpressionResult = fp_EvaluatePropertyValueExpression(_Context, Subscript.m_Index.f_GetAsType<CBuildSystemSyntax::CExpression>());
					if (!ExpressionResult.f_IsInteger())
					{
						fsp_ThrowError
							(
								_Context
								, "Expression {} does not evalutate to a integer value for array subscript access"_f
								<< Accessor.m_Accessor.f_GetAsType<CBuildSystemSyntax::CExpression>()
							)
						;
					}

					_fApplyArrayIndex(ExpressionResult.f_Integer());
				}
				else
					DMibNeverGetHere;
			}
			else
				DMibNeverGetHere;
		}
	}

	CBuildSystemSyntax::CType const *CBuildSystem::f_GetCanonicalDefaultedType
		(
			CEntity const &_Entity
			, CBuildSystemSyntax::CType const *_pType
			, CFilePosition &o_TypePosition
		) const
	{
		auto pType = _pType;
		while (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CUserType>())
		{
			auto &UserTypeName = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CUserType>().m_Name;
			auto pUserType = fp_GetUserTypeWithPositionForProperty(_Entity, UserTypeName);
			if (!pUserType)
				fsp_ThrowError(o_TypePosition, "Could not find user type of name '{}'"_f << UserTypeName);
			pType = &pUserType->m_Type;
			o_TypePosition = pUserType->m_Position;
		}
		return pType;
	}

	CBuildSystemSyntax::CType const *CBuildSystem::fp_GetCanonicalType
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CType const *_pType
			, CFilePosition &o_TypePosition
		) const
	{
		auto pType = _pType;
		while (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CUserType>() || pType->m_Type.f_IsOfType<CBuildSystemSyntax::CTypeDefaulted>())
		{
			if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CUserType>())
			{
				auto &UserTypeName = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CUserType>().m_Name;
				auto pUserType = fp_GetUserTypeWithPositionForProperty(_Context.m_Context, UserTypeName);

				if (!pUserType)
					fsp_ThrowError(_Context, o_TypePosition, "Could not find user type of name '{}'"_f << UserTypeName);

				pType = &pUserType->m_Type;
				o_TypePosition = pUserType->m_Position;
			}
			else
				pType = &pType->m_Type.f_GetAsType<CBuildSystemSyntax::CTypeDefaulted>().m_Type.f_Get();
		}
		return pType;
	}

	struct CBuildSystem::CApplyAccessorsHelper
	{
		CBuildSystem const *m_pThis;
		CEvalPropertyValueContext &m_Context;
		CFilePosition &m_TypePosition;
		CBuildSystemSyntax::CType const *m_pType;

		CBuildSystemSyntax::CType const *f_GetCanonical(CBuildSystemSyntax::CType const *_pType) const
		{
			return m_pThis->fp_GetCanonicalType(m_Context, _pType, m_TypePosition);
		}

		template <typename tf_CType>
		tf_CType const &f_GetType(CBuildSystemSyntax::CType const *_pType, ch8 const *_pTypeName) const
		{
			auto pType = f_GetCanonical(_pType);

			if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
				pType = f_GetCanonical(&pType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>().m_Return.f_Get());

			if (pType->m_Type.f_IsOfType<tf_CType>())
				return pType->m_Type.f_GetAsType<tf_CType>();

			if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::COneOf>())
			{
				auto &OneOf = pType->m_Type.f_GetAsType<CBuildSystemSyntax::COneOf>();
				for (auto &Type : OneOf.m_OneOf)
				{
					if (!Type.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>())
						continue;
					auto pCanonicalType = f_GetCanonical(&Type.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>().f_Get());
					if (pCanonicalType->m_Type.f_IsOfType<tf_CType>())
						return pCanonicalType->m_Type.f_GetAsType<tf_CType>();
				}
			}
			fsp_ThrowError(m_Context, "Could not apply accessor to type. Type '{}' does not have a {}"_f << *_pType << _pTypeName);
		}

		auto f_GetCanonicalFunctor() const
		{
			return [this](CBuildSystemSyntax::CType const *_pType)
				{
					return f_GetCanonical(_pType);
				}
			;
		}
	};

	CBuildSystemSyntax::CType const *CBuildSystem::fp_ApplyAccessorsToType
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CType const *_pType
			, NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors
			, CFilePosition &o_TypePosition
		) const
	{

		CApplyAccessorsHelper Vars{this, _Context, o_TypePosition, _pType};

		fp_ApplyAccessors
			(
				_Context
				, _Accessors
				, [&Vars](CStr const &_MemberName)
				{
					auto SaveTypePosition = Vars.m_TypePosition;
					//CDefaultType, CUserType, CClassType, CArrayType, COneOf, CFunctionType
					if (Vars.m_pType->f_IsAny(Vars.f_GetCanonicalFunctor()))
					{
						Vars.m_pType = &CBuildSystemSyntax::CDefaultType::ms_Any;
						return;
					}
					Vars.m_TypePosition = SaveTypePosition;

					auto &ClassType = Vars.f_GetType<CBuildSystemSyntax::CClassType>(Vars.m_pType, "class type");

					auto pMember = ClassType.m_Members.f_FindEqual(_MemberName);
					if (pMember)
						Vars.m_pType = &pMember->m_Type.f_Get();
					else
					{
						if (ClassType.m_OtherKeysType)
							Vars.m_pType = &ClassType.m_OtherKeysType.f_Get().f_Get();
						else
							fsp_ThrowError(Vars.m_Context, "No member named '{}' in type '{}'"_f << _MemberName << *Vars.m_pType);
					}
				}
				, [&Vars](int64 _Index)
				{
					auto SaveTypePosition = Vars.m_TypePosition;
					if (Vars.m_pType->f_IsAny(Vars.f_GetCanonicalFunctor()))
					{
						Vars.m_pType = &CBuildSystemSyntax::CDefaultType::ms_Any;
						return;
					}
					Vars.m_TypePosition = SaveTypePosition;

					auto &ArrayType = Vars.f_GetType<CBuildSystemSyntax::CArrayType>(Vars.m_pType, "array type");

					Vars.m_pType = &ArrayType.m_Type.f_Get();
				}
			)
		;

		return Vars.m_pType;
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::COperator const &_Value, CWritePropertyContext *_pWriteContext) const
	{
		if (_Value.m_Operator != CBuildSystemSyntax::COperator::EOperator_Append && _Value.m_Operator != CBuildSystemSyntax::COperator::EOperator_Prepend)
			fsp_ThrowError(_Context, "Operator expressions can only be used in conditions");

		bool bPrepend = _Value.m_Operator == CBuildSystemSyntax::COperator::EOperator_Prepend;
		ch8 const *pOperatorName = bPrepend ? "prepend += operator" : "append =+ operator";

		if (!_pWriteContext)
			fsp_ThrowError(_Context, "{cc} can only be used at root"_f << pOperatorName);

		auto &Property = _pWriteContext->m_Property;

		CBuildSystemSyntax::CIdentifier Identifier;
		Identifier.m_PropertyType = Property.m_Type;
		Identifier.m_Name = Property.m_Name;

 		auto *pTypeWithPosition = fp_GetTypeForProperty(_Context.m_OriginalContext, Property);

		if (!pTypeWithPosition)
			fsp_ThrowError(_Context, "Found no type for {}"_f << Property);

		auto TypePosition = pTypeWithPosition->m_Position;

		auto *pType = fp_GetCanonicalType(_Context, &pTypeWithPosition->m_Type, TypePosition);

		if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
			pType = fp_GetCanonicalType(_Context, &pType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>().m_Return.f_Get(), TypePosition);

		if (!pType)
			fsp_ThrowError(_Context, "Could not resolve type for {}"_f << Property);

		CEJSON OriginalValue;
		if (_pWriteContext->m_pWriteDestination)
		{
			if (!_pWriteContext->m_pAccessors)
				fsp_ThrowError(_Context, "No accessors specified");

			pType = fp_ApplyAccessorsToType(_Context, pType, *_pWriteContext->m_pAccessors, TypePosition);

			OriginalValue = fg_Move(*_pWriteContext->m_pWriteDestination);
		}
		else
			OriginalValue = fp_EvaluatePropertyValueIdentifier(_Context, Identifier);

		CEJSON Right = fp_EvaluatePropertyValue(_Context, _Value.m_Right, nullptr);

		auto fApplyObject = [&]()
			{
				auto &OriginalObject = OriginalValue.f_Object();

				for (auto &Member : Right.f_Object())
				{
					if (Member.f_Name().f_GetUserData() == EJSONStringType_NoQuote)
					{
						auto *pObject = &OriginalValue;
						for (auto &Component : Member.f_Name().f_Split("."))
						{
							if (pObject->f_IsValid() && !pObject->f_IsObject())
								fsp_ThrowError(_Context, "Encountered non object subobject of wrong type while appending '{}'"_f << Member.f_Name());

							pObject = &(*pObject)[Component];
						}
						*pObject = Member.f_Value();
					}
					else
						OriginalObject[Member.f_Name()] = Member.f_Value();
				}
			}
		;

		if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CArrayType>())
		{
			if (OriginalValue.f_IsValid() && !OriginalValue.f_IsArray())
				fsp_ThrowError(_Context, "Expected original value to be an array for {}. Value: {}"_f << pOperatorName << OriginalValue);

			auto &OriginalArray = OriginalValue.f_Array();

			bool bHandled = false;
			if (Right.f_IsArray())
			{
				auto TempRight = Right;
				if (fp_DoesValueConformToType(_Context, *pType, TypePosition, TempRight, EDoesValueConformToTypeFlag_CanApplyDefault))
				{
					bHandled = true;

					if (bPrepend)
					{
						TempRight.f_Array().f_Insert(fg_Move(OriginalArray));
						OriginalArray = fg_Move(TempRight.f_Array());
					}
					else
						OriginalArray.f_Insert(fg_Move(TempRight.f_Array()));
				}
			}

			if (!bHandled)
			{
				auto TempRight = Right;
				if (fp_DoesValueConformToType(_Context, pType->m_Type.f_GetAsType<CBuildSystemSyntax::CArrayType>().m_Type.f_Get(), TypePosition, TempRight, EDoesValueConformToTypeFlag_CanApplyDefault))
				{
					bHandled = true;

					if (bPrepend)
						OriginalArray.f_InsertFirst(fg_Move(TempRight));
					else
						OriginalArray.f_Insert(fg_Move(TempRight));
				}
			}

			if (!bHandled)
			{
				CTypeConformError ConformErrorArray;
				CTypeConformError ConformErrorElement;
				fp_DoesValueConformToType(_Context, *pType, TypePosition, Right, EDoesValueConformToTypeFlag_None, &ConformErrorArray);
				fp_DoesValueConformToType(_Context, pType->m_Type.f_GetAsType<CBuildSystemSyntax::CArrayType>().m_Type.f_Get(), TypePosition, Right, EDoesValueConformToTypeFlag_None, &ConformErrorElement);

				fsp_ThrowError
					(
						_Context
						, "Value to append does not conform to the type for {}: {}\n"
						"As array: {}\n"
						"As array element: {}\n"
						"Value: {}"_f
						<< Property
						<< *pType
						<< ConformErrorArray
						<< ConformErrorElement
						<< Right
					)
				;
			}
		}
		else if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CClassType>())
		{
			if (!Right.f_IsObject())
				fsp_ThrowError(_Context, "Can only append objects to objects with {}"_f << pOperatorName);

			if (!OriginalValue.f_IsObject() && OriginalValue.f_IsValid())
				fsp_ThrowError(_Context, "Expected original value to be an object for {}"_f << pOperatorName);

			fApplyObject();
		}
		else if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>())
		{
			auto &Type = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>();
			switch (Type.m_Type)
			{
			case CBuildSystemSyntax::CDefaultType::EType_Void:
			case CBuildSystemSyntax::CDefaultType::EType_Boolean:
			case CBuildSystemSyntax::CDefaultType::EType_Date:
				fsp_ThrowError(_Context, "{cc} is not supported for this type. Type is: {}"_f << pOperatorName << *pType);
				break;
			case CBuildSystemSyntax::CDefaultType::EType_Any:
				{
					if (OriginalValue.f_EType() == NEncoding::EEJSONType_Array)
					{
						if (Right.f_EType() == NEncoding::EEJSONType_Array)
						{
							if (bPrepend)
							{
								Right.f_Array().f_Insert(fg_Move(OriginalValue.f_Array()));
								OriginalValue = fg_Move(Right);
							}
							else
								OriginalValue.f_Array().f_Insert(fg_Move(Right.f_Array()));
						}
						else
						{
							if (bPrepend)
								OriginalValue.f_Array().f_InsertFirst(fg_Move(Right));
							else
								OriginalValue.f_Array().f_Insert(fg_Move(Right));
						}
						break;
					}

					if (OriginalValue.f_IsValid() == Right.f_IsValid())
					{
						if (!Right.f_IsValid())
							break; // undefined and undefined don't do anything

						if (OriginalValue.f_EType() != Right.f_EType())
						{
							fsp_ThrowError
								(
									_Context
									, "{cc} on any type needs left and right to be of same type. Type is: {} != {}"_f
									<< pOperatorName
									<< fg_EJSONTypeToString(OriginalValue.f_EType())
									<< fg_EJSONTypeToString(Right.f_EType())
								)
							;
						}
					}

					switch (Right.f_EType())
					{
					case NEncoding::EEJSONType_String:
						{
							if (bPrepend)
							{
								Right.f_String() += OriginalValue.f_String();
								OriginalValue = fg_Move(Right);
							}
							else
								OriginalValue.f_String() += Right.f_String();
						}
						break;
					case NEncoding::EEJSONType_Binary:
						{
							if (bPrepend)
							{
								Right.f_Binary().f_Insert(fg_Move(OriginalValue.f_Binary()));
								OriginalValue = fg_Move(Right);
							}
							else
								OriginalValue.f_Binary().f_Insert(fg_Move(Right.f_Binary()));
						}
						break;
					case NEncoding::EEJSONType_Integer: OriginalValue.f_Integer() += Right.f_Integer(); break;
					case NEncoding::EEJSONType_Float: OriginalValue.f_Float() += Right.f_Float(); break;
					case NEncoding::EEJSONType_Object: fApplyObject(); break;

					case NEncoding::EEJSONType_Array:
					case NEncoding::EEJSONType_Date:
					case NEncoding::EEJSONType_Null:
					case NEncoding::EEJSONType_Boolean:
					case NEncoding::EEJSONType_UserType:
					case NEncoding::EEJSONType_Invalid:
						fsp_ThrowError
							(
								_Context
								, "{cc} on any type does not support appending JSON type: {}"_f << pOperatorName << fg_EJSONTypeToString(OriginalValue.f_EType())
							)
						;
						break;
					}
				}
				break;
			case CBuildSystemSyntax::CDefaultType::EType_String:
				{
					if (!Right.f_IsValid())
						break;

					if (OriginalValue.f_IsValid() && OriginalValue.f_EType() != NEncoding::EEJSONType_String)
						fsp_ThrowError(_Context, "Expected original value to be a string for {}"_f << pOperatorName);
					if (Right.f_EType() != NEncoding::EEJSONType_String)
						fsp_ThrowError(_Context, "Expected right value to be a string for {}"_f << pOperatorName);

					if (bPrepend)
					{
						Right.f_String() += OriginalValue.f_String();
						OriginalValue = fg_Move(Right);
					}
					else
						OriginalValue.f_String() += Right.f_String();
				}
				break;
			case CBuildSystemSyntax::CDefaultType::EType_Integer:
				{
					if (!Right.f_IsValid())
						break;

					if (OriginalValue.f_IsValid() && OriginalValue.f_EType() != NEncoding::EEJSONType_Integer)
						fsp_ThrowError(_Context, "Expected original value to be an integer for {}"_f << pOperatorName);
					if (Right.f_EType() != NEncoding::EEJSONType_Integer)
						fsp_ThrowError(_Context, "Expected right value to be an integer for {}"_f << pOperatorName);

					OriginalValue.f_Integer() += Right.f_Integer();
				}
				break;
			case CBuildSystemSyntax::CDefaultType::EType_FloatingPoint:
				{
					if (!Right.f_IsValid())
						break;

					if (OriginalValue.f_IsValid() && OriginalValue.f_EType() != NEncoding::EEJSONType_Float)
						fsp_ThrowError(_Context, "Expected original value to be a float for {}"_f << pOperatorName);
					if (Right.f_EType() != NEncoding::EEJSONType_Float)
						fsp_ThrowError(_Context, "Expected right value to be a float for {}"_f << pOperatorName);

					OriginalValue.f_Float() += Right.f_Float();
				}
				break;
			case CBuildSystemSyntax::CDefaultType::EType_Binary:
				{
					if (!Right.f_IsValid())
						break;

					if (OriginalValue.f_IsValid() && OriginalValue.f_EType() != NEncoding::EEJSONType_Binary)
						fsp_ThrowError(_Context, "Expected original value to be binary for {}"_f << pOperatorName);
					if (Right.f_EType() != NEncoding::EEJSONType_Binary)
						fsp_ThrowError(_Context, "Expected right value to be binary for {}"_f << pOperatorName);

					if (bPrepend)
					{
						Right.f_Binary().f_Insert(fg_Move(OriginalValue.f_Binary()));
						OriginalValue = fg_Move(Right);
					}
					else
						OriginalValue.f_Binary().f_Insert(fg_Move(Right.f_Binary()));
				}
				break;
			}
		}
		else
			fsp_ThrowError(_Context, "Append += operator is only supported for array and class types. Current type is: {}"_f << *pType);

		return OriginalValue;
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueDefine(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CDefine const &_Value) const
	{
		fsp_ThrowError(_Context, "Define expressions can only be used at root");
		return {};
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueExpressionAppend(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CExpressionAppend const &_Value) const
	{
		fsp_ThrowError(_Context, "Append expressions can only be used in arrays or as function parameters");
		return {};
	}

	CTypeWithPosition const *CBuildSystem::fp_GetUserTypeWithPositionForProperty(CEntity const &_Entity, NStr::CStr const &_UserType) const
	{
		for (auto *pEntity = &_Entity; pEntity; pEntity = pEntity->m_pParent)
		{
			auto pType = pEntity->f_Data().m_UserTypes.f_FindEqual(_UserType);
			if (pType)
				return pType;
		}

		return nullptr;
	}

	CTypeWithPosition const *CBuildSystem::fp_GetTypeForProperty(CEntity const &_Entity, CPropertyKey const &_VariableName) const
	{
		auto pBuiltin = mp_BuiltinVariablesDefinitions.f_FindEqual(_VariableName);
		if (pBuiltin)
			return &*pBuiltin;

		for (auto *pEntity = &_Entity; pEntity; pEntity = pEntity->m_pParent)
		{
			auto pType = pEntity->f_Data().m_VariableDefinitions.f_FindEqual(_VariableName);
			if (pType)
				return &pType->m_Type;
		}

		return nullptr;
	}

	namespace
	{
		CStr fg_IndentString(CStr const &_String, ch8 const *_pIdent)
		{
			CStr Return;
			for (auto &Line : _String.f_SplitLine())
				fg_AddStrSep(Return, "{}{}"_f << _pIdent << Line, "\n");
			return Return;
		}
	}

	bool CBuildSystem::fp_DoesValueConformToType
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CType const &_Type
			, CFilePosition const &_TypePosition
			, CEJSON &o_Value
			, EDoesValueConformToTypeFlag _Flags
			, CTypeConformError *o_pError
		) const
	{
		auto fCheckType = [&]() -> bool
			{
				if (_Type.m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>())
				{
					auto &SpecificType = _Type.m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>();
					switch (SpecificType.m_Type)
					{
					case CBuildSystemSyntax::CDefaultType::EType_Any: return true;
					case CBuildSystemSyntax::CDefaultType::EType_Void:
						{
							if (o_pError && o_Value.f_IsValid())
								o_pError->m_Error = "Expected void value (undefined)";

							return !o_Value.f_IsValid();
						}
					case CBuildSystemSyntax::CDefaultType::EType_String:
						{
							if (o_pError && !o_Value.f_IsString())
								o_pError->m_Error = "Expected a string";

							return o_Value.f_IsString();
						}
					case CBuildSystemSyntax::CDefaultType::EType_Integer:
						{
							if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString())
							{
								try
								{
									o_Value = o_Value.f_AsInteger();
								}
								catch (CException const &_Exception)
								{
									if (o_pError)
										o_pError->m_Error = _Exception.f_GetErrorStr();
									return false;
								}
							}

							if (o_pError && !o_Value.f_IsInteger())
								o_pError->m_Error = "Expected an integer";

							return o_Value.f_IsInteger();
						}
					case CBuildSystemSyntax::CDefaultType::EType_FloatingPoint:
						{
							if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString())
							{
								try
								{
									o_Value = o_Value.f_AsFloat();
								}
								catch (CException const &_Exception)
								{
									if (o_pError)
										o_pError->m_Error = _Exception.f_GetErrorStr();
									return false;
								}
							}

							if (o_pError && !o_Value.f_IsFloat())
								o_pError->m_Error = "Expected a float";

							return o_Value.f_IsFloat();
						}
					case CBuildSystemSyntax::CDefaultType::EType_Boolean:
						{
							if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString())
							{
								try
								{
									o_Value = o_Value.f_AsBoolean();
								}
								catch (CException const &_Exception)
								{
									if (o_pError)
										o_pError->m_Error = _Exception.f_GetErrorStr();
									return false;
								}
							}

							if (o_pError && !o_Value.f_IsBoolean())
								o_pError->m_Error = "Expected a boolean";

							return o_Value.f_IsBoolean();
						}
					case CBuildSystemSyntax::CDefaultType::EType_Date:
						{
							if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString())
							{
								try
								{
									CStr ParseValue = o_Value.f_String();
									uch8 const *pParse = (uch8 const *)ParseValue.f_GetStr();

									TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::CEJSONParseContext Context;
									Context.m_pStartParse = pParse;
									o_Value = Context.f_ParseDate(pParse, false);
								}
								catch (CException const &_Exception)
								{
									if (o_pError)
										o_pError->m_Error = _Exception.f_GetErrorStr();
									return false;
								}
							}

							if (o_pError && !o_Value.f_IsDate())
								o_pError->m_Error = "Expected a date";

							return o_Value.f_IsDate();
						}
					case CBuildSystemSyntax::CDefaultType::EType_Binary:
						{
							if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString())
							{
								try
								{
									CStr ParseValue = o_Value.f_String();
									uch8 const *pParse = (uch8 const *)ParseValue.f_GetStr();

									TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::CEJSONParseContext Context;
									Context.m_pStartParse = pParse;
									o_Value = Context.f_ParseBinary(pParse, false);
								}
								catch (CException const &_Exception)
								{
									if (o_pError)
										o_pError->m_Error = _Exception.f_GetErrorStr();
									return false;
								}
							}

							if (o_pError && !o_Value.f_IsBinary())
								o_pError->m_Error = "Expected a binary";

							return o_Value.f_IsBinary();
						}
					}
				}
				else if (_Type.m_Type.f_IsOfType<CBuildSystemSyntax::CUserType>())
				{
					auto &SpecificType = _Type.m_Type.f_GetAsType<CBuildSystemSyntax::CUserType>();

					auto *pType = fp_GetUserTypeWithPositionForProperty(_Context.m_Context, SpecificType.m_Name);
					if (!pType)
						fsp_ThrowError(_Context, "Could not find user type of name '{}'"_f << SpecificType.m_Name);

					if (o_pError)
						o_pError->m_ErrorPath.f_Insert("type({})"_f << SpecificType.m_Name);

					if (!fp_DoesValueConformToType(_Context, pType->m_Type, pType->m_Position, o_Value, _Flags, o_pError))
						return false;

					if (o_pError)
						o_pError->m_ErrorPath.f_PopBack();

					return true;
				}
				else if (_Type.m_Type.f_IsOfType<CBuildSystemSyntax::CTypeDefaulted>())
				{
					auto &SpecificType = _Type.m_Type.f_GetAsType<CBuildSystemSyntax::CTypeDefaulted>();

					if (_Flags & EDoesValueConformToTypeFlag_CanApplyDefault)
					{
						if (!o_Value.f_IsValid())
						{
							CEvalPropertyValueContext Context{_Context.m_Context, _Context.m_OriginalContext, _TypePosition, _Context.m_EvalContext, &_Context};
							o_Value = fp_EvaluatePropertyValueParam(Context, SpecificType.m_DefaultValue);
						}

						if (o_pError)
							o_pError->m_ErrorPath.f_Insert(" = {}"_f << SpecificType.m_DefaultValue);

						if (!fp_DoesValueConformToType(_Context, SpecificType.m_Type.f_Get(), _TypePosition, o_Value, _Flags, o_pError))
							return false;

						if (o_pError)
							o_pError->m_ErrorPath.f_PopBack();

						return true;
					}

					return fp_DoesValueConformToType(_Context, SpecificType.m_Type.f_Get(), _TypePosition, o_Value, _Flags, o_pError);
				}
				else if (_Type.m_Type.f_IsOfType<CBuildSystemSyntax::CClassType>())
				{
					if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString())
					{
						try
						{
							CStr ToParse = o_Value.f_String().f_Trim();
							if (ToParse.f_StartsWith("{"))
							{
								_Flags &= EDoesValueConformToTypeFlag_ConvertFromString;
								o_Value = CEJSON::fs_FromString(ToParse);
							}
							else
							{
								CEJSON NewValue;

								auto &NewValueObject = NewValue.f_Object();
								for (auto &Element : o_Value.f_String().f_Split(";"))
								{
									auto iAssign = Element.f_FindChar('=');
									if (iAssign < 0)
										DMibError("Expected object member in the form Key=Value, got: {}"_f << Element);

									NewValueObject[Element.f_Left(iAssign)] = Element.f_Extract(iAssign + 1);
								}

								o_Value = fg_Move(NewValue);
							}
						}
						catch (CException const &_Exception)
						{
							if (o_pError)
								o_pError->m_Error = _Exception.f_GetErrorStr();
							return false;
						}
					}

					if (!o_Value.f_IsObject())
					{
						if (o_pError)
							o_pError->m_Error = "Expected an object";

						return false;
					}

					auto &Object = o_Value.f_Object();

					TCSet<CStr> CheckedMembers;

					if (o_pError)
						o_pError->m_ErrorPath.f_Insert();

					auto &SpecificType = _Type.m_Type.f_GetAsType<CBuildSystemSyntax::CClassType>();
					for (auto &Member : SpecificType.m_Members)
					{
						auto &MemberName = SpecificType.m_Members.fs_GetKey(Member);
						auto pObjectMember = Object.f_GetMember(MemberName);
						if (!pObjectMember)
						{
							if (_Flags & EDoesValueConformToTypeFlag_CanApplyDefault)
							{
								CEJSON MaybeDefaulted;
								if (fp_DoesValueConformToType(_Context, Member.m_Type.f_Get(), _TypePosition, MaybeDefaulted, _Flags, o_pError))
								{
									Object[MemberName] = fg_Move(MaybeDefaulted);
									CheckedMembers[MemberName];
									continue;
								}
							}

							if (!Member.m_bOptional)
							{
								if (o_pError)
								{
									o_pError->m_ErrorPath.f_PopBack();
									o_pError->m_Error = "Object is missing '{}'"_f << MemberName;
								}
								return false;
							}
						}

						if (pObjectMember)
						{
							if (o_pError)
								o_pError->m_ErrorPath.f_GetLast() = MemberName;

							if (!fp_DoesValueConformToType(_Context, Member.m_Type.f_Get(), _TypePosition, *pObjectMember, _Flags, o_pError))
								return false;
						}

						CheckedMembers[MemberName];
					}

					if (SpecificType.m_OtherKeysType)
					{
						for (auto &Member : Object)
						{
							if (!CheckedMembers.f_FindEqual(Member.f_Name()))
							{
								if (o_pError)
									o_pError->m_ErrorPath.f_GetLast() = "...({})"_f << Member.f_Name();

								if (!fp_DoesValueConformToType(_Context, SpecificType.m_OtherKeysType.f_Get().f_Get(), _TypePosition, Member.f_Value(), _Flags, o_pError))
									return false;

							}
						}

						if (o_pError)
							o_pError->m_ErrorPath.f_PopBack();
					}
					else
					{
						if (o_pError)
							o_pError->m_ErrorPath.f_PopBack();

						for (auto &Member : Object)
						{
							if (!CheckedMembers.f_FindEqual(Member.f_Name()))
							{
								if (o_pError)
									o_pError->m_Error = "Object has extra member '{}'"_f << Member.f_Name();
								return false;
							}
						}
					}

					return true;
				}
				else if (_Type.m_Type.f_IsOfType<CBuildSystemSyntax::CArrayType>())
				{
					if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString())
					{
						try
						{
							CStr ToParse = o_Value.f_String().f_Trim();
							if (ToParse.f_StartsWith("["))
							{
								_Flags &= EDoesValueConformToTypeFlag_ConvertFromString;
								o_Value = CEJSON::fs_FromString(ToParse);
							}
							else
							{
								CEJSON NewValue;

								auto &NewValueArray = NewValue.f_Array();
								for (auto &Element : o_Value.f_String().f_Split(";"))
									NewValueArray.f_Insert(fg_Move(Element));

								o_Value = fg_Move(NewValue);
							}
						}
						catch (CException const &_Exception)
						{
							if (o_pError)
								o_pError->m_Error = _Exception.f_GetErrorStr();
							return false;
						}
					}

					if (!o_Value.f_IsArray())
					{
						if (o_pError)
							o_pError->m_Error = "Expected an array";

						return false;
					}

					auto &SpecificType = _Type.m_Type.f_GetAsType<CBuildSystemSyntax::CArrayType>();

					if (o_pError)
						o_pError->m_ErrorPath.f_Insert();

					mint iElement = 0;
					for (auto &ElementValue : o_Value.f_Array())
					{
						if (o_pError)
							o_pError->m_ErrorPath.f_GetLast() = "[{}]"_f << iElement;

						if (!fp_DoesValueConformToType(_Context, SpecificType.m_Type.f_Get(), _TypePosition, ElementValue, _Flags, o_pError))
							return false;

						++iElement;
					}

					if (o_pError)
						o_pError->m_ErrorPath.f_PopBack();

					return true;
				}
				else if (_Type.m_Type.f_IsOfType<CBuildSystemSyntax::COneOf>())
				{
					auto &SpecificType = _Type.m_Type.f_GetAsType<CBuildSystemSyntax::COneOf>();
					TCVector<CTypeConformError> ConformErrors;
					CStr NonMatchingValues;
					for (auto &TypeOrValue : SpecificType.m_OneOf)
					{
						if (TypeOrValue.f_IsOfType<CEJSON>())
						{
							auto &ExpectedValue = TypeOrValue.f_GetAsType<CEJSON>();

							CStr ConversionError;

							if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString() && o_Value.f_EType() != ExpectedValue.f_EType())
							{
								try
								{
									switch (ExpectedValue.f_EType())
									{
									case EEJSONType_Integer:
										{
											CEJSON ConvertedValue = o_Value.f_AsInteger();
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJSONType_Float:
										{
											CEJSON ConvertedValue = o_Value.f_AsFloat();
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJSONType_Boolean:
										{
											CEJSON ConvertedValue = o_Value.f_AsBoolean();
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJSONType_Date:
										{
											CStr ParseValue = o_Value.f_String();
											uch8 const *pParse = (uch8 const *)ParseValue.f_GetStr();

											TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::CEJSONParseContext Context;
											Context.m_pStartParse = pParse;

											CEJSON ConvertedValue = Context.f_ParseDate(pParse, false);
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJSONType_Binary:
										{
											CStr ParseValue = o_Value.f_String();
											uch8 const *pParse = (uch8 const *)ParseValue.f_GetStr();

											TCRegistry_CustomValue<NBuildSystem::CBuildSystemRegistryValue>::CEJSONParseContext Context;
											Context.m_pStartParse = pParse;

											CEJSON ConvertedValue = Context.f_ParseBinary(pParse, false);
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJSONType_String:
									case EEJSONType_Invalid:
									case EEJSONType_Null:
									case EEJSONType_UserType:
									case EEJSONType_Object:
									case EEJSONType_Array:
										break;
									}
								}
								catch (CException const &_Exception)
								{
									if (o_pError)
										ConversionError = _Exception.f_GetErrorStr();
								}
							}

							if (o_Value == ExpectedValue)
								return true;

							if (o_pError)
							{
								CStr Error = ExpectedValue.f_ToString(nullptr, gc_BuildSystemJSONParseFlags);
								if (ConversionError)
									Error = "{} ({})"_f << fg_TempCopy(Error) << ConversionError;

								fg_AddStrSep(NonMatchingValues, Error, ", ");
							}
						}
						else
						{
							if (_Flags & (EDoesValueConformToTypeFlag_CanApplyDefault | EDoesValueConformToTypeFlag_ConvertFromString))
							{
								auto &Type = TypeOrValue.f_GetAsType<TCIndirection<CBuildSystemSyntax::CType>>().f_Get();
								auto Value = o_Value;
								if (o_pError)
								{
									CTypeConformError Error;
									if (fp_DoesValueConformToType(_Context, Type, _TypePosition, Value, _Flags, &Error))
									{
										o_Value = fg_Move(Value);
										return true;
									}
									ConformErrors.f_Insert(Error);
								}
								else if (fp_DoesValueConformToType(_Context, Type, _TypePosition, Value, _Flags))
								{
									o_Value = fg_Move(Value);
									return true;
								}
							}
							else
							{
								auto &Type = TypeOrValue.f_GetAsType<TCIndirection<CBuildSystemSyntax::CType>>().f_Get();
								if (o_pError)
								{
									CTypeConformError Error;
									if (fp_DoesValueConformToType(_Context, Type, _TypePosition, o_Value, _Flags, &Error))
										return true;
									ConformErrors.f_Insert(Error);
								}
								else if (fp_DoesValueConformToType(_Context, Type, _TypePosition, o_Value, _Flags))
									return true;
							}
						}
					}

					if (o_pError)
					{
						CStr Error = "No match found in one_of";
						if (NonMatchingValues)
							Error += "\n    None of values match: {}"_f << NonMatchingValues;
						if (!ConformErrors.f_IsEmpty())
						{
							Error += "\n    Value does not conform to any of types: {}"_f << NonMatchingValues;
							for (auto &ConformError : ConformErrors)
							{
								Error += "\n";
								Error += fg_IndentString("{}"_f << ConformError, "        ");
							}
						}

						o_pError->m_Error = fg_Move(Error);
					}

					return false;
				}
				else if (_Type.m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
				{
					auto &SpecificType = _Type.m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

					if (o_pError)
						o_pError->m_ErrorPath.f_Insert("{}"_f << SpecificType);

					if (!fp_DoesValueConformToType(_Context, SpecificType.m_Return.f_Get(), _TypePosition, o_Value, _Flags, o_pError))
						return false;

					if (o_pError)
						o_pError->m_ErrorPath.f_PopBack();

					return true;
				}
				else
					DMibNeverGetHere;

				if (o_pError)
					o_pError->m_Error = "Internal error";

				return false;
			}
		;

		if (!fCheckType())
		{
			if (_Type.f_IsOptional() && !o_Value.f_IsValid())
				return true;
			
			return false;
		}
		return true;
	}

	void CBuildSystem::fp_CheckValueConformToPropertyType
		(
			CEvalPropertyValueContext &_Context
			, CPropertyKey const &_Property
			, CEJSON &o_Value
			, CFilePosition const &_Position
			, EDoesValueConformToTypeFlag _Flags
		) const
	{
		auto *pType = fp_GetTypeForProperty(_Context.m_OriginalContext, _Property);

		if (!pType)
			fsp_ThrowError(_Context, _Position, "Found no type for {}"_f << _Property);

		fp_CheckValueConformToType
			(
				_Context
				, pType->m_Type
				, o_Value
				, _Position
				, pType->m_Position
				, [&]() -> CStr
				{
					return "{}"_f << _Property;
				}
				, _Flags
			)
		;
	}

	void CBuildSystem::fp_CheckValueConformToType
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CType const &_Type
			, CEJSON &o_Value
			, CFilePosition const &_Position
			, CFilePosition const &_TypePosition
			, NFunction::TCFunctionNoAlloc<CStr ()> const &_fGetErrorContext
			, EDoesValueConformToTypeFlag _Flags
		) const
	{
		CFilePosition TypePosition = _TypePosition;
		auto pCanonicalType = fp_GetCanonicalType(_Context, &_Type, TypePosition);
		if (pCanonicalType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
		{
			auto &FunctionType = pCanonicalType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();
			if (_Context.m_EvalContext.m_pCallingFunction != &FunctionType)
			{
				TCVector<CBuildSystemError> Errors;
				if (TypePosition.f_IsValid())
					Errors.f_Insert({TypePosition, "See definition of function type"});
				if (_TypePosition != TypePosition && _TypePosition.f_IsValid())
					Errors.f_Insert({TypePosition, "See definition of function type (original)"});
				fsp_ThrowError(_Context, _Position, "Trying to access a function as a variable, you need to call it", Errors);
			}
		}

		if (!fp_DoesValueConformToType(_Context, _Type, TypePosition, o_Value, _Flags | EDoesValueConformToTypeFlag_CanApplyDefault, nullptr))
		{
			CTypeConformError Error;
			fp_DoesValueConformToType(_Context, _Type, TypePosition, o_Value, _Flags | EDoesValueConformToTypeFlag_CanApplyDefault, &Error);

			if (pCanonicalType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
			{
				auto &FunctionType = pCanonicalType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();
				pCanonicalType = fp_GetCanonicalType(_Context, &FunctionType.m_Return.f_Get(), TypePosition);
			}

			TCVector<CBuildSystemError> Errors;

			if (TypePosition.f_IsValid())
				Errors.f_Insert({TypePosition, "See definition of type"});

			if (_TypePosition != TypePosition && _TypePosition.f_IsValid())
				Errors.f_Insert({_TypePosition, "See definition of type (original)"});

			fsp_ThrowError
				(
					_Context
					, _Position
					, "For {} value:\n{}\nDoes not conform to type:\n{}\nError: {}"_f
					<< _fGetErrorContext()
					<< fg_IndentString(o_Value.f_ToString("\t", gc_BuildSystemJSONParseFlags).f_Trim(), "    ")
					<< fg_IndentString("{}"_f << *pCanonicalType, "    ")
					<< fg_IndentString("{}"_f << Error, "    ").f_Trim()
					, Errors
				)
			;
		}
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueAccessors(CEvalPropertyValueContext &_Context, CEJSON &&_Value, TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors) const
	{
		CEJSON Return = fg_Move(_Value);

		fp_ApplyAccessors
			(
				_Context
				, _Accessors
				, [&](CStr const &_MemberName)
				{
					if (!Return.f_IsObject())
					{
						fsp_ThrowError
							(
								_Context
								, "JSON value '{}' is not an object so cannot apply member name '{}'"_f
								<< Return.f_ToString(nullptr, EJSONDialectFlag_AllowUndefined)
								<< _MemberName
							)
						;
					}

					auto pMember = Return.f_GetMember(_MemberName);
					if (!pMember)
						Return = CEJSON();
					else
						Return = fg_Move(*pMember);
				}
				, [&](int64 _Index)
				{
					if (!Return.f_IsArray())
						fsp_ThrowError(_Context, "JSON value '{}' is not an array so cannot apply subscript index"_f << Return.f_ToString(nullptr, EJSONDialectFlag_AllowUndefined));

					auto &Array = Return.f_Array();

					if (_Index >= int64(TCLimitsInt<smint>::mc_Max) || _Index < 0 || _Index >= int64(Array.f_GetLen()))
						fsp_ThrowError(_Context, "Index {} is out of range for array that has {} elements"_f << _Index << Array.f_GetLen());

					Return = fg_Move(Array[_Index]);
				}
			)
		;

		return Return;
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueJSONAccessor(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CJSONAccessor const &_Value) const
	{
		return fp_EvaluatePropertyValueAccessors(_Context, fp_EvaluatePropertyValueParam(_Context, _Value.m_Param), _Value.m_Accessors);
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueIdentifier(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CIdentifier const &_Value) const
	{
		CStr PropertyName;
		if (_Value.f_IsNameConstantString())
			PropertyName = _Value.f_NameConstantString();
		else
			PropertyName = fp_EvaluatePropertyValueEvalString(_Context, _Value.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>());

		auto pOriginalContext = &_Context.m_OriginalContext;
		if (_Value.m_EntityType != EEntityType_Invalid)
		{
			while (pOriginalContext && pOriginalContext->f_GetKey().m_Type != _Value.m_EntityType)
				pOriginalContext = pOriginalContext->m_pParent;

			if (!pOriginalContext)
			{
				fsp_ThrowError
					(
						_Context
						, "No entity with type '{}' found in parent entities for path: {}"_f << fg_EntityTypeToStr(_Value.m_EntityType) << _Context.m_OriginalContext.f_GetPath()
					)
				;
			}
		}

		DMibCheck(_Value.m_PropertyType != EPropertyType_Invalid);

		CPropertyKey Key;
		Key.m_Name = PropertyName;
		Key.m_Type = _Value.m_PropertyType;

		if (Key.m_Type == EPropertyType_Builtin)
		{
			if (Key.m_Name == "GeneratedFiles")
			{
				TCVector<CEJSON> Ret;
				{
					DMibLock(mp_GeneratedFilesLock);
					for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
					{
						if (iFile->m_bGeneral)
							Ret.f_Insert(iFile.f_GetKey());
					}
				}
				return fg_Move(Ret);
			}
			else if (Key.m_Name == "SourceFiles")
			{
				TCVector<CEJSON> Ret;
				{
					DMibLockRead(mp_SourceFilesLock);
					for (auto iFile = mp_SourceFiles.f_GetIterator(); iFile; ++iFile)
						Ret.f_Insert(*iFile);
				}
				return fg_Move(Ret);
			}
			else if (Key.m_Name == "BuildSystemSourceAbsolute")
				return mp_FileLocation;
			else if (Key.m_Name == "BuildSystemSource")
				return mp_FileLocationFile;
			else if (Key.m_Name == "GeneratorStateFile")
				return mp_GeneratorStateFileName;
			else if (Key.m_Name == "BasePathAbsolute")
				return f_GetBaseDir();
			else if (Key.m_Name == "MToolExe")
			{
				CStr Ret = CFile::fs_GetProgramDirectory() / "MTool";
				#ifdef DPlatformFamily_Windows
					Ret += ".exe";
				#endif
				return fg_Move(Ret);
			}
			else if (Key.m_Name == "MalterlibExe")
			{
				CStr Ret = CFile::fs_GetProgramDirectory() / "mib";
				#ifdef DPlatformFamily_Windows
					Ret += ".exe";
				#endif
				return fg_Move(Ret);
			}
			else
			{
				CStr Ret;
				if (!mp_GeneratorInterface->f_GetBuiltin(Key.m_Name, Ret))
					fsp_ThrowError(_Context, CStr::CFormat("Unrecognized builtin '{}'") << Key.m_Name);
				return fg_Move(Ret);
			}
		}
		else if (Key.m_Type == EPropertyType_This)
		{
			auto &ContextKey = pOriginalContext->f_GetKey();

			if (ContextKey.m_Type == EEntityType_Root)
				fsp_ThrowError(_Context, "Cannot access this on root entity");

			if (Key.m_Name == "Identity")
			{
				if (!ContextKey.m_Name.f_IsConstantString())
					fsp_ThrowError(_Context, "Identity can only be gotten from entity with constant string name. Path: {}"_f << pOriginalContext->f_GetPath());

				return pOriginalContext->f_GetKeyName();
			}
			else if (Key.m_Name == "IdentityAsAbsolutePath")
			{
				if (!ContextKey.m_Name.f_IsConstantString())
					fsp_ThrowError(_Context, "Identity can only be gotten from entity with constant string name. Path: {}"_f << pOriginalContext->f_GetPath());

				return CFile::fs_GetExpandedPath(pOriginalContext->f_GetKeyName(), CFile::fs_GetPath(pOriginalContext->f_Data().m_Position.m_File));
			}
			else if (Key.m_Name == "IdentityPath")
				return pOriginalContext->f_Data().m_Position.m_File;
			else if (Key.m_Name == "Type")
				return fg_EntityTypeToStr(ContextKey.m_Type);
			else
				fsp_ThrowError(_Context, CStr::CFormat("Unrecognized entity (this) accessor '{}'") << Key.m_Name);
		}
		else if (pOriginalContext == &_Context.m_OriginalContext)
		{
			for (auto *pEvaluatedProperties = _Context.m_EvalContext.m_pEvaluatedProperties; pEvaluatedProperties; pEvaluatedProperties = pEvaluatedProperties->m_pParentProperties)
			{
				auto pValue = pEvaluatedProperties->m_Properties.f_FindEqual(Key);
				if (pValue)
					return pValue->m_Value;
			}

			CProperty const *pFromProperty = nullptr;
			return fp_EvaluateEntityProperty(_Context.m_OriginalContext, _Context.m_OriginalContext, Key, _Context.m_EvalContext, pFromProperty, _Context.m_Position, &_Context);
		}
		else
		{
			CEvaluatedProperty * pValue;
			{
				pValue = pOriginalContext->m_EvaluatedProperties.m_Properties.f_FindEqual(Key);
				if (pValue)
					return pValue->m_Value;
			}

			pValue = pOriginalContext->m_EvaluatedProperties.m_Properties.f_FindEqual(Key);
			if (!pValue)
			{
				CProperty const *pFromProperty = nullptr;
				CChangePropertiesScope ChangeProperties(_Context.m_EvalContext, &pOriginalContext->m_EvaluatedProperties);
				return fp_EvaluateEntityProperty(*pOriginalContext, *pOriginalContext, Key, _Context.m_EvalContext, pFromProperty, _Context.m_Position, &_Context);
			}
			else
				return pValue->m_Value;
		}
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueParam(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CParam const &_Value) const
	{
		switch (_Value.m_Param.f_GetTypeID())
		{
			case 0: return _Value.m_Param.f_Get<0>();
			case 1: return fp_EvaluatePropertyValueObject(_Context, _Value.m_Param.f_Get<1>());
			case 2: return fp_EvaluatePropertyValueArray(_Context, _Value.m_Param.f_Get<2>());
			case 3: return fp_EvaluatePropertyValueIdentifier(_Context, _Value.m_Param.f_Get<3>().f_Get());
			case 4: return fp_EvaluatePropertyValueEvalString(_Context, _Value.m_Param.f_Get<4>());
			case 5: return fp_EvaluatePropertyValueWildcardString(_Context, _Value.m_Param.f_Get<5>());
			case 6: return fp_EvaluatePropertyValueExpression(_Context, _Value.m_Param.f_Get<6>().f_Get());
			case 7: return fp_EvaluatePropertyValueExpressionAppend(_Context, _Value.m_Param.f_Get<7>().f_Get());
			case 8: return fp_EvaluatePropertyValueTernary(_Context, _Value.m_Param.f_Get<8>().f_Get());
			case 9: return fp_EvaluatePropertyValueBinaryOperator(_Context, _Value.m_Param.f_Get<9>().f_Get());
			case 10: return fp_EvaluatePropertyValuePrefixOperator(_Context, _Value.m_Param.f_Get<10>().f_Get());
			default: DNeverGetHere; break;
		}
		return {};
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueExpression(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CExpression const &_Value) const
	{
		if (_Value.m_Expression.f_IsOfType<CBuildSystemSyntax::CFunctionCall>())
			return fp_EvaluatePropertyValueFunctionCall(_Context, _Value.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>());
		else if (_Value.m_Expression.f_IsOfType<CBuildSystemSyntax::CParam>())
			return fp_EvaluatePropertyValueParam(_Context, _Value.m_Expression.f_GetAsType<CBuildSystemSyntax::CParam>());
		else if (_Value.m_Expression.f_IsOfType<TCIndirection<CBuildSystemSyntax::CJSONAccessor>>())
			return fp_EvaluatePropertyValueJSONAccessor(_Context, _Value.m_Expression.f_GetAsType<TCIndirection<CBuildSystemSyntax::CJSONAccessor>>().f_Get());
 		else
			DMibNeverGetHere;

		return {};
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueTernary(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CTernary const &_Value) const
	{
		CEJSON Conditional = fp_EvaluatePropertyValueParam(_Context, _Value.m_Conditional);
		if (!Conditional.f_IsBoolean())
			fsp_ThrowError(_Context, "Expected conditional to evalute to a boolean");

		if (Conditional.f_Boolean())
			return fp_EvaluatePropertyValueParam(_Context, _Value.m_Left);
		else
			return fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValue(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CRootValue const &_Value, CPropertyKey const *_pProperty) const
	{
		if (!_Value.m_Accessors.f_IsEmpty())
		{
			if (!_pProperty)
				fsp_ThrowError(_Context, "Accessors are only supported at root");

			CBuildSystemSyntax::CIdentifier Identifier;
			Identifier.m_PropertyType = _pProperty->m_Type;
			Identifier.m_Name = _pProperty->m_Name;

			CEJSON Return = fp_EvaluatePropertyValueIdentifier(_Context, Identifier);
			CEJSON *pWriteDestination = &Return;

			fp_ApplyAccessors
				(
					_Context
					, _Value.m_Accessors
					, [&](CStr const &_MemberName)
					{
						auto pMember = pWriteDestination->f_GetMember(_MemberName);
						if (!pMember)
							fsp_ThrowError(_Context, "No member named '{}' in JSON object"_f << _MemberName);

						pWriteDestination = pMember;
					}
					, [&](int64 _Index)
					{
						if (!pWriteDestination->f_IsArray())
							fsp_ThrowError(_Context, "JSON object is not an array so cannot apply subscript index");

						auto &Array = pWriteDestination->f_Array();

						if (_Index >= int64(TCLimitsInt<smint>::mc_Max) || _Index < 0 || _Index >= int64(Array.f_GetLen()))
							fsp_ThrowError(_Context, "Index {} is out of range for array that has {} elements"_f << _Index << Array.f_GetLen());

						pWriteDestination = &Array[_Index];
					}
				)
			;

			CWritePropertyContext WriteContext{*_pProperty, pWriteDestination, &_Value.m_Accessors};
			*pWriteDestination = fp_EvaluatePropertyValue(_Context, _Value.m_Value, &WriteContext);

			return Return;
		}

		CWritePropertyContext WriteContext{*_pProperty};
		return fp_EvaluatePropertyValue(_Context, _Value.m_Value, &WriteContext);
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValue(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CValue const &_Value, CWritePropertyContext *_pWriteContext) const
	{
		switch (_Value.m_Value.f_GetTypeID())
		{
		case 0: return _Value.m_Value.f_Get<0>();
		case 1: return fp_EvaluatePropertyValueObject(_Context, _Value.m_Value.f_Get<1>());
		case 2: return fp_EvaluatePropertyValueArray(_Context, _Value.m_Value.f_Get<2>());
		case 3: return fp_EvaluatePropertyValueWildcardString(_Context, _Value.m_Value.f_Get<3>());
		case 4: return fp_EvaluatePropertyValueEvalString(_Context, _Value.m_Value.f_Get<4>());
		case 5: return fp_EvaluatePropertyValueExpression(_Context, _Value.m_Value.f_Get<5>());
		case 6: return fp_EvaluatePropertyValueExpressionAppend(_Context, _Value.m_Value.f_Get<6>());
		case 7: return fp_EvaluatePropertyValueOperator(_Context, _Value.m_Value.f_Get<7>(), _pWriteContext);
		case 8: return fp_EvaluatePropertyValueDefine(_Context, _Value.m_Value.f_Get<8>());
		default: DMibNeverGetHere; return {};
		}
	}

	void CBuildSystem::fp_EvaluatePropertyValueFunctionCallCollectParams
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CFunctionCall const &_FunctionCall
			, CBuildSystemSyntax::CFunctionType const &_FunctionType
			, TCFunctionNoAlloc<void (CEJSON &&_Param, CBuildSystemSyntax::CFunctionParameter const &_FunctionParam, bool _bEllipsis)> const &_fConsumeParam
			, CFilePosition const &_TypePosition
		) const
	{
		TCVector<CEJSON> EvalParams;
		for (auto &Param : _FunctionCall.m_Params)
		{
			if (Param.m_Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>())
			{
				auto &Expression = Param.m_Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>().f_Get();
				CEJSON ToAppend = fp_EvaluatePropertyValueExpression(_Context, Expression);
				if (ToAppend.f_IsArray())
					EvalParams.f_Insert(ToAppend.f_Array());
				else if (!ToAppend.f_IsValid())
					; // Undefined values are ignored
				else
					fsp_ThrowError(_Context, "Append expressions expects an array to expand. {} resulted in : {}"_f << Expression << ToAppend);
			}
			else
				EvalParams.f_Insert(fp_EvaluatePropertyValueParam(_Context, Param));
		}

		{
			auto iParamType = _FunctionType.m_Parameters.f_GetIterator();

			mint iParam = 0;
			for (auto &EvalParam : EvalParams)
			{
				if (!iParamType)
					fsp_ThrowError(_Context, "Too many parameters for function '{}' with type: {}"_f << _FunctionCall.m_Name << _FunctionType);

				fp_CheckValueConformToType
					(
						_Context
						, iParamType->m_Type.f_Get()
						, EvalParam
						, _Context.m_Position
						, _TypePosition
						, [&]() -> CStr
						{
							return "In call to function '{}' parameter {} ({})"_f << _FunctionCall.m_Name << iParamType->m_Name << iParam;
						}
						, EDoesValueConformToTypeFlag_None
					)
				;

				if (iParamType->m_ParamType == CBuildSystemSyntax::CFunctionParameter::EParamType_Ellipsis)
					_fConsumeParam(fg_Move(EvalParam), *iParamType, true);
				else
				{
					_fConsumeParam(fg_Move(EvalParam), *iParamType, false);
					++iParamType;
				}

				++iParam;
			}

			for (; iParamType; ++iParamType)
			{
				if (iParamType->m_ParamType == CBuildSystemSyntax::CFunctionParameter::EParamType_Ellipsis)
					break;
				if (iParamType->m_ParamType == CBuildSystemSyntax::CFunctionParameter::EParamType_Optional)
				{
					CEJSON Param; // Undefined

					fp_CheckValueConformToType
						(
							_Context
							, iParamType->m_Type.f_Get()
							, Param
							, _Context.m_Position
							, _TypePosition
							, [&]() -> CStr
							{
								return "parameter {} ({})"_f << iParamType->m_Name << iParam;
							}
							, EDoesValueConformToTypeFlag_None
						)
					;

					_fConsumeParam(fg_Move(Param), *iParamType, false);
				}
				else
					fsp_ThrowError(_Context, "Missing parameters for function '{}' with type: {}"_f << _FunctionCall.m_Name << _FunctionType);
				++iParam;
			}
		}
	}

	NEncoding::CEJSON CBuildSystem::fp_EvaluatePropertyValueFunctionCallBuiltin
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CFunctionCall const &_FunctionCall
			, CBuiltinFunction const *_pFunction
		) const
	{
		TCVector<CEJSON> Params;
		TCVector<CEJSON> *pEllipsis = nullptr;
		fp_EvaluatePropertyValueFunctionCallCollectParams
			(
				_Context
				, _FunctionCall
				, _pFunction->m_Type
				, [&](CEJSON &&_Param, CBuildSystemSyntax::CFunctionParameter const &_FunctionParam, bool _bEllipsis)
				{
					if (_bEllipsis)
					{
						if (!pEllipsis)
							pEllipsis = &Params.f_Insert(EJSONType_Array).f_Array();
						pEllipsis->f_Insert(fg_Move(_Param));
					}
					else
						Params.f_Insert(fg_Move(_Param));
				}
				, {}
			)
		;

		return _pFunction->m_fFunction(*this, _Context, fg_Move(Params));
	}

	CEJSON CBuildSystem::fp_EvaluatePropertyValueFunctionCall(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CFunctionCall const &_FunctionCall) const
	{
		if (_FunctionCall.m_PropertyType == EPropertyType_Property)
		{
			auto pFunction = fg_RemoveQualifiers(mp_BuiltinFunctions).f_FindEqual(_FunctionCall.m_Name);
			if (pFunction)
				return fp_EvaluatePropertyValueFunctionCallBuiltin(_Context, _FunctionCall, pFunction);
		}

		CPropertyKey PropertyKey(_FunctionCall.m_PropertyType, _FunctionCall.m_Name);

		auto *pTypeWithPosition = fp_GetTypeForProperty(_Context.m_OriginalContext, PropertyKey);
		if (!pTypeWithPosition)
			fsp_ThrowError(_Context, "No such function: {}"_f << PropertyKey);

		auto TypePosition = pTypeWithPosition->m_Position;

		CBuildSystemSyntax::CType const *pType = fp_GetCanonicalType(_Context, &pTypeWithPosition->m_Type, TypePosition);

		if (!pType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
			fsp_ThrowError(_Context, "Type is a variable, not a fuction: {}"_f << PropertyKey);

		auto &FunctionType = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

		CEvaluatedProperties TempProperties;
		TempProperties.m_pParentProperties = _Context.m_EvalContext.m_pEvaluatedProperties;

		TCVector<CEJSON> *pEllipsis = nullptr;
		fp_EvaluatePropertyValueFunctionCallCollectParams
			(
				_Context
				, _FunctionCall
				, FunctionType
				, [&](CEJSON &&_Param, CBuildSystemSyntax::CFunctionParameter const &_FunctionParam, bool _bEllipsis)
				{
					if (_bEllipsis)
					{
						if (!pEllipsis)
							pEllipsis = &(TempProperties.m_Properties[CPropertyKey(_FunctionParam.m_Name)].m_Value = EJSONType_Array).f_Array();
						pEllipsis->f_Insert(fg_Move(_Param));
					}
					else
						TempProperties.m_Properties[CPropertyKey(_FunctionParam.m_Name)].m_Value = fg_Move(_Param);
				}
				, TypePosition
			)
		;

		CChangePropertiesScope ChangeProperties(_Context.m_EvalContext, &TempProperties);

		auto pOldFunction = _Context.m_EvalContext.m_pCallingFunction;
		_Context.m_EvalContext.m_pCallingFunction = &FunctionType;
		auto Cleanup = g_OnScopeExit > [&]
			{
				_Context.m_EvalContext.m_pCallingFunction = pOldFunction;
			}
		;

		CPropertyKey TemplateKey;
		TemplateKey.m_Name = _FunctionCall.m_Name;
		TemplateKey.m_Type = _FunctionCall.m_PropertyType;

		CProperty const *pProperty;

		return fp_EvaluateEntityProperty
			(
				_Context.m_OriginalContext
				, _Context.m_OriginalContext
				, TemplateKey
				, _Context.m_EvalContext
				, pProperty
				, _Context.m_Position
				, &_Context
			)
		;
	}
}
