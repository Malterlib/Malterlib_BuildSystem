// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

#include <Mib/Cryptography/UUID>

namespace NMib::NBuildSystem
{
	CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueObject(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CObject const &_Value) const
	{
		CEJsonSorted ObjectReturn;
		auto &Object = ObjectReturn.f_Object();

		for (auto &Value : _Value.m_ObjectSorted)
		{
			auto &Key = _Value.m_Object.fs_GetKey(Value);

			switch (Key.m_Key.f_GetTypeID())
			{
			case 0:
				{
					Object[Key.m_Key.f_GetAsType<CStr>()] = fp_EvaluatePropertyValue(_Context, Value.m_Value.f_Get(), nullptr).f_Move();
				}
				break;
			case 1:
				{
					Object[fp_EvaluatePropertyValueEvalString(_Context, Key.m_Key.f_GetAsType<CBuildSystemSyntax::CEvalString>())]
						= fp_EvaluatePropertyValue(_Context, Value.m_Value.f_Get(), nullptr).f_Move()
					;
				}
				break;
			case 2:
				{
					auto fApplyObject = [&](CEJsonSorted &&_Object)
						{
							if (!_Object.f_IsObject())
								fs_ThrowError(_Context, "Append object expects object arguments");

							for (auto iObject = _Object.f_Object().f_OrderedIterator(); iObject; ++iObject)
								Object[iObject->f_Name()] = fg_Move(iObject->f_Value());
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
								fs_ThrowError(_Context, "Append object array expected arrays members to evalutate to objects");

							fApplyObject(fg_Move(Entry));
						}
					}
					else if (ValueObject.f_IsOfType<CBuildSystemSyntax::CObject>())
						fApplyObject(fp_EvaluatePropertyValueObject(_Context, ValueObject.f_GetAsType<CBuildSystemSyntax::CObject>()));
					else if (ValueObject.f_IsOfType<CBuildSystemSyntax::CExpression>())
					{
						auto ExpressionResult = fp_EvaluatePropertyValueExpression(_Context, ValueObject.f_GetAsType<CBuildSystemSyntax::CExpression>());
						auto &ExpressionResultRef = ExpressionResult.f_Get();

						if (ExpressionResultRef.f_IsArray())
						{
							for (auto &Entry : ExpressionResult.f_MoveArray())
							{
								if (!Entry.f_IsObject())
									fs_ThrowError(_Context, "Append object expression array expected arrays members to evalutate to objects");

								fApplyObject(fg_Move(Entry));
							}
						}
						else
							fApplyObject(ExpressionResult.f_Move());
					}
					else
						fs_ThrowError(_Context, "Append object only supports objects, arrays or expressions evaluating to objects or arrays");
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

	CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueArray(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CArray const &_Value) const
	{
		CEJsonSorted Return;

		auto &ReturnArray = Return.f_Array();

		for (auto &Entry : _Value.m_Array)
		{
			if (Entry.f_Get().m_Value.f_IsOfType<CBuildSystemSyntax::CExpressionAppend>())
			{
				CBuildSystemSyntax::CExpression const &Expression = Entry.f_Get().m_Value.f_GetAsType<CBuildSystemSyntax::CExpressionAppend>();
				auto ToAppend = fp_EvaluatePropertyValueExpression(_Context, Expression);
				auto &ToAppendRef = ToAppend.f_Get();

				if (ToAppendRef.f_IsArray())
					ReturnArray.f_Insert(ToAppend.f_MoveArray());
				else if (!ToAppendRef.f_IsValid())
					; // Undefined values are ignored
				else
					fs_ThrowError(_Context, "Append expressions expects an array to expand. {} resulted in : {}"_f << Expression << ToAppendRef);
			}
			else
				ReturnArray.f_Insert(fp_EvaluatePropertyValue(_Context, Entry.f_Get(), nullptr).f_Move());
		}

		return Return;
	}

	CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueWildcardString(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CWildcardString const &_Value) const
	{
		CStr WildcardValue;
		if (_Value.m_String.f_IsOfType<CStr>())
			WildcardValue = _Value.m_String.f_GetAsType<CStr>();
		else if (_Value.m_String.f_IsOfType<CBuildSystemSyntax::CEvalString>())
			WildcardValue = fp_EvaluatePropertyValueEvalString(_Context, _Value.m_String.f_GetAsType<CBuildSystemSyntax::CEvalString>());
		else
			DMibNeverGetHere;

		return CEJsonUserTypeSorted{"Wildcard", fg_Move(WildcardValue)};
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
				auto &ExpressionRef = Expression.f_Get();
				if (!ExpressionRef.f_IsString())
				{
					fs_ThrowError
						(
							_Context
							, "Expressions in eval strings needs to evaluate to strings.\n\tExpression: {}\n\tValue: {}\n\tEvaluated string: {}\n"_f
							<< ExpressionToken
							<< ExpressionRef.f_ToString(nullptr, EJsonDialectFlag_AllowUndefined)
							<< ReturnString
						)
					;
				}

				ReturnString += ExpressionRef.f_String();
			}
			else
				DMibNeverGetHere;
		}

		return ReturnString;
	}

	void CBuildSystem::fp_ApplyAccessors
		(
			CEvalPropertyValueContext &_Context
			, NContainer::TCVector<CBuildSystemSyntax::CJsonAccessorEntry> const &_Accessors
			, NFunction::TCFunctionNoAlloc<void (NStr::CStr const &_Member, bool _bOptionalChain)> const &_fApplyMemberName
			, NFunction::TCFunctionNoAlloc<void (int64 _Index, bool _bOptionalChain)> const &_fApplyArrayIndex
		) const
	{
		for (auto &Accessor : _Accessors)
		{
			if (Accessor.m_Accessor.f_IsOfType<CStr>())
				_fApplyMemberName(Accessor.m_Accessor.f_GetAsType<CStr>(), Accessor.m_bOptional);
			else if (Accessor.m_Accessor.f_IsOfType<CBuildSystemSyntax::CExpression>())
			{
				auto ExpressionResult = fp_EvaluatePropertyValueExpression(_Context, Accessor.m_Accessor.f_GetAsType<CBuildSystemSyntax::CExpression>());
				auto ExpressionResultRef = ExpressionResult.f_Get();
				if (!ExpressionResultRef.f_IsString())
				{
					fs_ThrowError
						(
							_Context
							, "Expression {} does not evalutate to a string value for member access"_f << Accessor.m_Accessor.f_GetAsType<CBuildSystemSyntax::CExpression>()
						)
					;
				}
				_fApplyMemberName(ExpressionResultRef.f_String(), Accessor.m_bOptional);
			}
			else if (Accessor.m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>())
			{
				auto &Subscript = Accessor.m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>();
				if (Subscript.m_Index.f_IsOfType<uint32>())
					_fApplyArrayIndex(Subscript.m_Index.f_GetAsType<uint32>(), Accessor.m_bOptional);
				else if (Subscript.m_Index.f_IsOfType<CBuildSystemSyntax::CExpression>())
				{
					auto ExpressionResult = fp_EvaluatePropertyValueExpression(_Context, Subscript.m_Index.f_GetAsType<CBuildSystemSyntax::CExpression>());
					auto &ExpressionResultRef = ExpressionResult.f_Get();
					if (!ExpressionResultRef.f_IsInteger())
					{
						fs_ThrowError
							(
								_Context
								, "Expression {} does not evalutate to a integer value for array subscript access"_f
								<< Accessor.m_Accessor.f_GetAsType<CBuildSystemSyntax::CExpression>()
							)
						;
					}

					_fApplyArrayIndex(ExpressionResultRef.f_Integer(), Accessor.m_bOptional);
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
			CEntity &_Entity
			, CBuildSystemSyntax::CType const *_pType
			, CFilePosition &o_TypePosition
		) const
	{
		auto &EntityData = _Entity.f_Data();
		CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
		CBuildSystemUniquePositions Positions;
		CEvalPropertyValueContext Context{_Entity, _Entity, EntityData.m_Position, EvalContext, nullptr, f_EnablePositions(&Positions)};

		CStr Namespace;

		auto pType = _pType;
		while (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CUserType>())
		{
			auto &UserTypeName = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CUserType>().m_Name;
			auto pUserType = fp_GetUserTypeWithPositionForProperty(Context, UserTypeName, Namespace);
			if (!pUserType)
			{
				Positions.f_AddPositionFirst(o_TypePosition, gc_ConstString_Type, gc_ConstString_Type);
				fs_ThrowError(Positions, "Could not find user type of name '{}'"_f << UserTypeName);
			}
			pType = &pUserType->m_Type;
			o_TypePosition = pUserType->m_Position;
		}
		return pType;
	}

	CBuildSystemSyntax::CType const *CBuildSystem::fp_GetCanonicalType
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CType const *_pType
			, CFilePosition const *&o_pTypePosition
		) const
	{
		auto pType = _pType;

		CStr Namespace;

		while (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CUserType>() || pType->m_Type.f_IsOfType<CBuildSystemSyntax::CTypeDefaulted>())
		{
			if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CUserType>())
			{
				auto &UserTypeName = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CUserType>().m_Name;

				auto *pUserType = fp_GetUserTypeWithPositionForProperty(_Context, UserTypeName, Namespace);
				if (!pUserType)
					fs_ThrowError(_Context, *o_pTypePosition, "Could not find user type of name '{}'"_f << UserTypeName);

				pType = &pUserType->m_Type;
				o_pTypePosition = &pUserType->m_Position;
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
		CFilePosition const *&m_pTypePosition;
		CBuildSystemSyntax::CType const *m_pType;

		CBuildSystemSyntax::CType const *f_GetCanonical(CBuildSystemSyntax::CType const *_pType) const
		{
			return m_pThis->fp_GetCanonicalType(m_Context, _pType, m_pTypePosition);
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
			fs_ThrowError(m_Context, "Could not apply accessor to type. Type '{}' does not have a {}"_f << *_pType << _pTypeName);
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
			, NContainer::TCVector<CBuildSystemSyntax::CJsonAccessorEntry> const &_Accessors
			, CFilePosition const *o_pTypePosition
		) const
	{

		CApplyAccessorsHelper Vars{this, _Context, o_pTypePosition, _pType};

		fp_ApplyAccessors
			(
				_Context
				, _Accessors
				, [&Vars](CStr const &_MemberName, bool _bOptionalChain)
				{
					auto pSaveTypePosition = Vars.m_pTypePosition;
					//CDefaultType, CUserType, CClassType, CArrayType, COneOf, CFunctionType
					if (Vars.m_pType->f_IsAny(Vars.f_GetCanonicalFunctor()))
					{
						Vars.m_pType = &CBuildSystemSyntax::CDefaultType::ms_Any;
						return;
					}
					Vars.m_pTypePosition = pSaveTypePosition;

					auto &ClassType = Vars.f_GetType<CBuildSystemSyntax::CClassType>(Vars.m_pType, "class type");

					auto pMember = ClassType.m_Members.f_FindEqual(_MemberName);
					if (pMember)
						Vars.m_pType = &pMember->m_Type.f_Get();
					else
					{
						if (ClassType.m_OtherKeysType)
							Vars.m_pType = &ClassType.m_OtherKeysType.f_Get().f_Get();
						else
							fs_ThrowError(Vars.m_Context, "No member named '{}' in type '{}'"_f << _MemberName << *Vars.m_pType);
					}
				}
				, [&Vars](int64 _Index, bool _bOptionalChain)
				{
					auto pSaveTypePosition = Vars.m_pTypePosition;
					if (Vars.m_pType->f_IsAny(Vars.f_GetCanonicalFunctor()))
					{
						Vars.m_pType = &CBuildSystemSyntax::CDefaultType::ms_Any;
						return;
					}
					Vars.m_pTypePosition = pSaveTypePosition;

					auto &ArrayType = Vars.f_GetType<CBuildSystemSyntax::CArrayType>(Vars.m_pType, "array type");

					Vars.m_pType = &ArrayType.m_Type.f_Get();
				}
			)
		;

		return Vars.m_pType;
	}

	CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::COperator const &_Value, CWritePropertyContext *_pWriteContext) const
	{
		if (_Value.m_Operator != CBuildSystemSyntax::COperator::EOperator_Append && _Value.m_Operator != CBuildSystemSyntax::COperator::EOperator_Prepend)
			fs_ThrowError(_Context, "Operator expressions can only be used in conditions");

		bool bPrepend = _Value.m_Operator == CBuildSystemSyntax::COperator::EOperator_Prepend;
		ch8 const *pOperatorName = bPrepend ? "prepend += operator" : "append =+ operator";

		if (!_pWriteContext)
			fs_ThrowError(_Context, "{cc} can only be used at root"_f << pOperatorName);

		auto &Property = _pWriteContext->m_Property;

		CBuildSystemSyntax::CIdentifier Identifier;
		Identifier.m_PropertyType = Property.f_GetType();
		Identifier.m_Name = Property.f_GetStringAndHash();

		auto *pTypeWithPosition = fp_GetTypeForProperty(_Context, Property.f_Reference());

		if (!pTypeWithPosition)
			fs_ThrowError(_Context, "Found no type for {}"_f << Property);

		auto *pTypePosition = &pTypeWithPosition->m_Position;

		auto *pType = fp_GetCanonicalType(_Context, &pTypeWithPosition->m_Type, pTypePosition);

		auto fGetCanonicalType = [&]
			(
#ifndef DCompiler_Workaround_Apple_clang
				this
#endif
				auto &&_fThis
				, CBuildSystemSyntax::CType const *_pType
			) -> CBuildSystemSyntax::CType const *
			{
#ifdef DCompiler_Workaround_Apple_clang
#define _fThis(...) _fThis(_fThis, __VA_ARGS__)
#endif
				auto pType = fp_GetCanonicalType(_Context, _pType, pTypePosition);

				if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
					pType = fp_GetCanonicalType(_Context, &pType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>().m_Return.f_Get(), pTypePosition);

				if (pType && pType->m_Type.f_IsOfType<CBuildSystemSyntax::COneOf>())
				{
					for (auto &OneType : pType->m_Type.f_GetAsType<CBuildSystemSyntax::COneOf>().m_OneOf)
					{
						if (!OneType.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>())
							continue;

						auto &Type = OneType.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>().f_Get();

						auto *pCanonicalType = _fThis(&Type);

						if (pCanonicalType->m_Type.template f_IsOfType<CBuildSystemSyntax::CArrayType>())
							return pCanonicalType;
						else if (pCanonicalType->m_Type.template f_IsOfType<CBuildSystemSyntax::CClassType>())
							return pCanonicalType;
						else if (pCanonicalType->m_Type.template f_IsOfType<CBuildSystemSyntax::CDefaultType>())
							return pCanonicalType;
					}
				}

				if (!pType)
					fs_ThrowError(_Context, "Could not resolve type for {}"_f << Property);

				return pType;
			}
		;

#ifdef DCompiler_Workaround_Apple_clang
#define fGetCanonicalType(...) fGetCanonicalType(fGetCanonicalType, __VA_ARGS__)
#endif
		pType = fGetCanonicalType(pType);

		TCOptional<CValuePotentiallyByRef> OriginalEvaluatedValue;
		CEJsonSorted *pOriginalValue;

		if (_pWriteContext->m_pWriteDestination)
		{
			if (!_pWriteContext->m_pAccessors)
				fs_ThrowError(_Context, "No accessors specified");

			pType = fGetCanonicalType(fp_ApplyAccessorsToType(_Context, pType, *_pWriteContext->m_pAccessors, pTypePosition));

			pOriginalValue = _pWriteContext->m_pWriteDestination;
		}
		else
		{
			OriginalEvaluatedValue = fp_EvaluatePropertyValueIdentifier(_Context, Identifier, true);
			pOriginalValue = OriginalEvaluatedValue->f_MakeMutable();
		}

		CEJsonSorted &OriginalValue = *pOriginalValue;

		auto Right = fp_EvaluatePropertyValue(_Context, _Value.m_Right, nullptr);
		auto &RightRef = Right.f_Get();

		auto fApplyObject = [&]()
			{
				auto &OriginalObject = OriginalValue.f_Object();

				for (auto &Member : RightRef.f_Object())
				{
					if (Member.f_Name().f_GetUserData() == EJsonStringType_NoQuote)
					{
						auto *pObject = &OriginalValue;
						for (auto &Component : Member.f_Name().f_Split("."))
						{
							if (pObject->f_IsValid() && !pObject->f_IsObject())
								fs_ThrowError(_Context, "Encountered non object subobject of wrong type while appending '{}'"_f << Member.f_Name());

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
				fs_ThrowError(_Context, "Expected original value to be an array for {}. Value: {}"_f << pOperatorName << OriginalValue);

			auto &OriginalArray = OriginalValue.f_Array();

			bool bHandled = false;
			if (RightRef.f_IsArray())
			{
				auto TempRight = RightRef;
				if (fp_DoesValueConformToType(_Context, *pType, *pTypePosition, TempRight, EDoesValueConformToTypeFlag_CanApplyDefault, {}))
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
				auto TempRight = RightRef;
				if 
				(
					fp_DoesValueConformToType
					(
						_Context
						, pType->m_Type.f_GetAsType<CBuildSystemSyntax::CArrayType>().m_Type.f_Get()
						, *pTypePosition
						, TempRight
						, EDoesValueConformToTypeFlag_CanApplyDefault
					 	, {}
					)
				)
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
				auto TempRight1 = RightRef;
				auto TempRight2 = RightRef;
				fp_DoesValueConformToType(_Context, *pType, *pTypePosition, TempRight1, EDoesValueConformToTypeFlag_None, {}, &ConformErrorArray);
				fp_DoesValueConformToType
					(
						_Context
						, pType->m_Type.f_GetAsType<CBuildSystemSyntax::CArrayType>().m_Type.f_Get()
						, *pTypePosition
						, TempRight2
						, EDoesValueConformToTypeFlag_None
						, {}
						, &ConformErrorElement
					)
				;

				fs_ThrowError
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
						<< RightRef
					)
				;
			}
		}
		else if (pType->m_Type.f_IsOfType<CBuildSystemSyntax::CClassType>())
		{
			if (!RightRef.f_IsObject())
				fs_ThrowError(_Context, "Can only append objects to objects with {}"_f << pOperatorName);

			if (!OriginalValue.f_IsObject() && OriginalValue.f_IsValid())
				fs_ThrowError(_Context, "Expected original value to be an object for {}"_f << pOperatorName);

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
			case CBuildSystemSyntax::CDefaultType::EType_Identifier:
				fs_ThrowError(_Context, "{cc} is not supported for this type. Type is: {}"_f << pOperatorName << *pType);
				break;
			case CBuildSystemSyntax::CDefaultType::EType_Any:
				{
					if (OriginalValue.f_EType() == NEncoding::EEJsonType_Array)
					{
						if (RightRef.f_EType() == NEncoding::EEJsonType_Array)
						{
							if (bPrepend)
							{
								auto NewValue = Right.f_Move();
								NewValue.f_Array().f_Insert(fg_Move(OriginalValue.f_Array()));
								OriginalValue = fg_Move(NewValue);
							}
							else
								OriginalValue.f_Array().f_Insert(Right.f_MoveArray());
						}
						else
						{
							if (bPrepend)
								OriginalValue.f_Array().f_InsertFirst(Right.f_Move());
							else
								OriginalValue.f_Array().f_Insert(Right.f_Move());
						}
						break;
					}

					if (OriginalValue.f_IsValid() == RightRef.f_IsValid())
					{
						if (!RightRef.f_IsValid())
							break; // undefined and undefined don't do anything

						if (OriginalValue.f_EType() != RightRef.f_EType())
						{
							fs_ThrowError
								(
									_Context
									, "{cc} on any type needs left and right to be of same type. Type is: {} != {}"_f
									<< pOperatorName
									<< fg_EJsonTypeToString(OriginalValue.f_EType())
									<< fg_EJsonTypeToString(RightRef.f_EType())
								)
							;
						}
					}

					switch (RightRef.f_EType())
					{
					case NEncoding::EEJsonType_String:
						{
							if (bPrepend)
							{
								auto NewValue = Right.f_Move();
								NewValue.f_String() += OriginalValue.f_String();
								OriginalValue = fg_Move(NewValue);
							}
							else
								OriginalValue.f_String() += RightRef.f_String();
						}
						break;
					case NEncoding::EEJsonType_Binary:
						{
							if (bPrepend)
							{
								auto NewValue = Right.f_Move();
								NewValue.f_Binary().f_Insert(fg_Move(OriginalValue.f_Binary()));
								OriginalValue = fg_Move(NewValue);
							}
							else
								OriginalValue.f_Binary().f_Insert(fg_Move(Right.f_Move().f_Binary()));
						}
						break;
					case NEncoding::EEJsonType_Integer: OriginalValue.f_Integer() += RightRef.f_Integer(); break;
					case NEncoding::EEJsonType_Float: OriginalValue.f_Float() += RightRef.f_Float(); break;
					case NEncoding::EEJsonType_Object: fApplyObject(); break;

					case NEncoding::EEJsonType_Array:
					case NEncoding::EEJsonType_Date:
					case NEncoding::EEJsonType_Null:
					case NEncoding::EEJsonType_Boolean:
					case NEncoding::EEJsonType_UserType:
					case NEncoding::EEJsonType_Invalid:
						fs_ThrowError
							(
								_Context
								, "{cc} on any type does not support appending Json type: {}"_f << pOperatorName << fg_EJsonTypeToString(OriginalValue.f_EType())
							)
						;
						break;
					}
				}
				break;
			case CBuildSystemSyntax::CDefaultType::EType_String:
				{
					if (!RightRef.f_IsValid())
						break;

					if (OriginalValue.f_IsValid() && OriginalValue.f_EType() != NEncoding::EEJsonType_String)
						fs_ThrowError(_Context, "Expected original value to be a string for {}"_f << pOperatorName);
					if (RightRef.f_EType() != NEncoding::EEJsonType_String)
						fs_ThrowError(_Context, "Expected right value to be a string for {}"_f << pOperatorName);

					if (bPrepend)
					{
						auto NewValue = Right.f_Move();
						NewValue.f_String() += OriginalValue.f_String();
						OriginalValue = fg_Move(NewValue);
					}
					else
						OriginalValue.f_String() += RightRef.f_String();
				}
				break;
			case CBuildSystemSyntax::CDefaultType::EType_Integer:
				{
					if (!RightRef.f_IsValid())
						break;

					if (OriginalValue.f_IsValid() && OriginalValue.f_EType() != NEncoding::EEJsonType_Integer)
						fs_ThrowError(_Context, "Expected original value to be an integer for {}"_f << pOperatorName);
					if (RightRef.f_EType() != NEncoding::EEJsonType_Integer)
						fs_ThrowError(_Context, "Expected right value to be an integer for {}"_f << pOperatorName);

					OriginalValue.f_Integer() += RightRef.f_Integer();
				}
				break;
			case CBuildSystemSyntax::CDefaultType::EType_FloatingPoint:
				{
					if (!RightRef.f_IsValid())
						break;

					if (OriginalValue.f_IsValid() && OriginalValue.f_EType() != NEncoding::EEJsonType_Float)
						fs_ThrowError(_Context, "Expected original value to be a float for {}"_f << pOperatorName);
					if (RightRef.f_EType() != NEncoding::EEJsonType_Float)
						fs_ThrowError(_Context, "Expected right value to be a float for {}"_f << pOperatorName);

					OriginalValue.f_Float() += RightRef.f_Float();
				}
				break;
			case CBuildSystemSyntax::CDefaultType::EType_Binary:
				{
					if (!RightRef.f_IsValid())
						break;

					if (OriginalValue.f_IsValid() && OriginalValue.f_EType() != NEncoding::EEJsonType_Binary)
						fs_ThrowError(_Context, "Expected original value to be binary for {}"_f << pOperatorName);
					if (RightRef.f_EType() != NEncoding::EEJsonType_Binary)
						fs_ThrowError(_Context, "Expected right value to be binary for {}"_f << pOperatorName);

					if (bPrepend)
					{
						auto NewValue = Right.f_Move();
						NewValue.f_Binary().f_Insert(fg_Move(OriginalValue.f_Binary()));
						OriginalValue = fg_Move(NewValue);
					}
					else
						OriginalValue.f_Binary().f_Insert(fg_Move(Right.f_Move().f_Binary()));
				}
				break;
			}
		}
		else
			fs_ThrowError(_Context, "Append += operator is only supported for array, class types and built in types. Current type is: {}"_f << *pType);

		_pWriteContext->m_bTypeAlreadyChecked = true;

		return fg_Move(OriginalValue);
	}

	CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueDefine(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CDefine const &_Value) const
	{
		fs_ThrowError(_Context, "Define expressions can only be used at root");
		return {};
	}

	CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueExpressionAppend(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CExpressionAppend const &_Value) const
	{
		fs_ThrowError(_Context, "Append expressions can only be used in arrays or as function parameters");
		return {};
	}

	CTypeWithPosition const *CBuildSystem::fp_GetUserTypeWithPositionForProperty(CEvalPropertyValueContext &_Context, NStr::CStr const &_UserType, NStr::CStr &o_Namespace) const
	{
		for (auto *pEntity = &_Context.m_Context; pEntity; pEntity = pEntity->m_pParent)
		{
			NContainer::TCLinkedList<CTypeWithConditions> const *pTypes;
			CStr Namespace;

			if (o_Namespace)
			{
				CStr FullName = "{}::{}"_f << o_Namespace << _UserType;
				pTypes = pEntity->f_Data().m_UserTypes.f_FindEqual(FullName);

				if (!pTypes)
				{
					auto Namespaces = o_Namespace.f_Split("::");
					Namespaces.f_PopBack();
					Namespace = CStr::fs_Join(Namespaces, "::");

					while (!Namespaces.f_IsEmpty())
					{
						CStr FullName = "{}::{}"_f << Namespace << _UserType;
						pTypes = pEntity->f_Data().m_UserTypes.f_FindEqual(FullName);
						if (pTypes)
							break;

						Namespaces.f_PopBack();
						Namespace = CStr::fs_Join(Namespaces, "::");
					}

					if (!pTypes)
						pTypes = pEntity->f_Data().m_UserTypes.f_FindEqual(_UserType);
				}
				else
					Namespace = o_Namespace;
			}
			else
				pTypes = pEntity->f_Data().m_UserTypes.f_FindEqual(_UserType);

			if (!pTypes)
				continue;

			for (auto &Type : *pTypes)
			{
				if
					(
						!Type.m_pConditions
						|| fpr_EvalCondition
						(
							_Context.m_Context
							, _Context.m_OriginalContext
							, *Type.m_pConditions
							, _Context.m_EvalContext
							, Type.m_DebugFlags
							, &_Context
							, _Context.m_pStorePositions
						)
					)
				{
					if (auto iLast = _UserType.f_FindReverse("::"); iLast >= 0)
					{
						if (Namespace)
						{
							Namespace += "::";
							Namespace += _UserType.f_Left(iLast);
						}
						else
							Namespace = _UserType.f_Left(iLast);
					}

					if (Namespace.f_GetStr() != o_Namespace.f_GetStr())
						o_Namespace = Namespace;

					return &Type.m_Type;
				}
			}
		}

		return nullptr;
	}

	CTypeWithPosition const *CBuildSystem::fp_GetTypeForProperty(CBuildSystem::CEvalPropertyValueContext &_Context, CPropertyKeyReference const &_VariableName) const
	{
		auto *pOverriddenType = _Context.m_EvalContext.m_OverriddenTypes.f_FindEqual(_VariableName);
		if (pOverriddenType)
			return pOverriddenType;

		auto pBuiltin = mp_BuiltinVariablesDefinitions.f_FindEqual(_VariableName);
		if (pBuiltin)
			return &*pBuiltin;

		for (auto *pEntity = &_Context.m_OriginalContext; pEntity; pEntity = pEntity->m_pParent)
		{
			auto pDefinitions = pEntity->f_Data().m_VariableDefinitions.f_FindEqual(_VariableName);
			if (pDefinitions)
			{
				for (auto &Definition : *pDefinitions)
				{
					if
						(
							!Definition.m_pConditions
							|| fpr_EvalCondition
							(
								_Context.m_Context
								, _Context.m_OriginalContext
								, *Definition.m_pConditions
								, _Context.m_EvalContext
								, Definition.m_DebugFlags
								, &_Context
								, _Context.m_pStorePositions
							)
						)
					{
						return &Definition.m_Type;
					}
				}
			}
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
			, CEJsonSorted &o_Value
			, EDoesValueConformToTypeFlag _Flags
			, CStr const &_Namespace
			, CTypeConformError *o_pError
			, NFunction::TCFunctionNoAlloc<CStr ()> const *_pGetErrorContext
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

									NBuildSystem::CCustomRegistryKeyValue::CEJsonParseContext Context;
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

									NBuildSystem::CCustomRegistryKeyValue::CEJsonParseContext Context;
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
					case CBuildSystemSyntax::CDefaultType::EType_Identifier:
						{
							if (!o_Value.f_IsObject())
							{
								if (o_pError)
									o_pError->m_Error = "Expected an object for identifier";
								return false;
							}

							bool bFoundType = false;
							bool bFoundName = false;

							auto &Object = o_Value.f_Object();
							for (auto &Member : Object)
							{
								if (Member.f_Name() == gc_ConstString_Type.m_String)
								{
									bFoundType = true;
									if (!Member.f_Value().f_IsString())
									{
										if (o_pError)
											o_pError->m_Error = "Expected a string for Type";
										return false;
									}
								}
								else if (Member.f_Name() == gc_ConstString_Name.m_String)
								{
									bFoundName = true;
									if (!Member.f_Value().f_IsString())
									{
										if (o_pError)
											o_pError->m_Error = "Expected a string for Name";
										return false;
									}
								}
								else if (Member.f_Name() == gc_ConstString_EntityType.m_String)
								{
									if (!Member.f_Value().f_IsString())
									{
										if (o_pError)
											o_pError->m_Error = "Expected a string for EntityType";
										return false;
									}
								}
								else
								{
									if (o_pError)
										o_pError->m_Error = "Unexpected member '{}'"_f << Member.f_Name();
									return false;
								}
							}

							if (!bFoundType)
							{
								if (o_pError)
									o_pError->m_Error = "Missing Type member for identifier";
								return false;
							}

							if (!bFoundName)
							{
								if (o_pError)
									o_pError->m_Error = "Missing Name member for identifier";
								return false;
							}

							return true;
						}
					}
				}
				else if (_Type.m_Type.f_IsOfType<CBuildSystemSyntax::CUserType>())
				{
					auto &SpecificType = _Type.m_Type.f_GetAsType<CBuildSystemSyntax::CUserType>();

					CStr Namespace = _Namespace;
					auto *pType = fp_GetUserTypeWithPositionForProperty(_Context, SpecificType.m_Name, Namespace);
					if (!pType)
						fs_ThrowError(_Context, "Could not find user type of name '{}'"_f << SpecificType.m_Name);

					if (o_pError)
						o_pError->m_ErrorPath.f_Insert("type({})"_f << SpecificType.m_Name);

					if (!fp_DoesValueConformToType(_Context, pType->m_Type, pType->m_Position, o_Value, _Flags, Namespace, o_pError, _pGetErrorContext))
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
						bool bDefaulted = !o_Value.f_IsValid();
						if (bDefaulted)
						{
							CBuildSystemUniquePositions::CPosition *pStoredPosition = nullptr;
							if (_Context.m_pStorePositions && _TypePosition.f_IsValid())
								pStoredPosition = _Context.m_pStorePositions->f_AddPosition(_TypePosition, "Apply default");

							CEvalPropertyValueContext Context{_Context.m_Context, _Context.m_OriginalContext, _TypePosition, _Context.m_EvalContext, &_Context, _Context.m_pStorePositions};
							o_Value = fp_EvaluatePropertyValueParam(Context, SpecificType.m_DefaultValue).f_Move();

							if (pStoredPosition)
								pStoredPosition->f_AddValue(o_Value, f_EnableValues());
						}

						if (o_pError)
							o_pError->m_ErrorPath.f_Insert(" = {}"_f << SpecificType.m_DefaultValue);

						if (!fp_DoesValueConformToType(_Context, SpecificType.m_Type.f_Get(), _TypePosition, o_Value, _Flags, _Namespace, o_pError, _pGetErrorContext))
						{
							if (bDefaulted)
								fs_ThrowError(_Context, "Defaulted value '{}' does not conform to type '{}' it's defaulting"_f << o_Value << SpecificType.m_Type.f_Get());

							return false;
						}

						if (o_pError)
							o_pError->m_ErrorPath.f_PopBack();

						return true;
					}

					return fp_DoesValueConformToType(_Context, SpecificType.m_Type.f_Get(), _TypePosition, o_Value, _Flags, _Namespace, o_pError, _pGetErrorContext);
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
								o_Value = CEJsonSorted::fs_FromString(ToParse);
							}
							else
							{
								CEJsonSorted NewValue;

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
								CEJsonSorted MaybeDefaulted;
								if (fp_DoesValueConformToType(_Context, Member.m_Type.f_Get(), _TypePosition, MaybeDefaulted, _Flags, _Namespace, o_pError, _pGetErrorContext))
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

							if (!fp_DoesValueConformToType(_Context, Member.m_Type.f_Get(), _TypePosition, *pObjectMember, _Flags, _Namespace, o_pError, _pGetErrorContext))
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
									o_pError->m_ErrorPath.f_GetLast() = "... = {}"_f << Member.f_Name();

								if
								(
									!fp_DoesValueConformToType
									(
										_Context
										, SpecificType.m_OtherKeysType.f_Get().f_Get()
										, _TypePosition
										, Member.f_Value()
										, _Flags
										, _Namespace
										, o_pError
										, _pGetErrorContext
									)
								)
								{
									return false;
								}
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
								o_Value = CEJsonSorted::fs_FromString(ToParse);
							}
							else
							{
								CEJsonSorted NewValue;

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

						if (!fp_DoesValueConformToType(_Context, SpecificType.m_Type.f_Get(), _TypePosition, ElementValue, _Flags, _Namespace, o_pError, _pGetErrorContext))
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
						if (TypeOrValue.f_IsOfType<CEJsonSorted>())
						{
							auto &ExpectedValue = TypeOrValue.f_GetAsType<CEJsonSorted>();

							CStr ConversionError;

							if ((_Flags & EDoesValueConformToTypeFlag_ConvertFromString) && o_Value.f_IsString() && o_Value.f_EType() != ExpectedValue.f_EType())
							{
								try
								{
									switch (ExpectedValue.f_EType())
									{
									case EEJsonType_Integer:
										{
											CEJsonSorted ConvertedValue = o_Value.f_AsInteger();
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJsonType_Float:
										{
											CEJsonSorted ConvertedValue = o_Value.f_AsFloat();
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJsonType_Boolean:
										{
											CEJsonSorted ConvertedValue = o_Value.f_AsBoolean();
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJsonType_Date:
										{
											CStr ParseValue = o_Value.f_String();
											uch8 const *pParse = (uch8 const *)ParseValue.f_GetStr();

											NBuildSystem::CCustomRegistryKeyValue::CEJsonParseContext Context;
											Context.m_pStartParse = pParse;

											CEJsonSorted ConvertedValue = Context.f_ParseDate(pParse, false);
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJsonType_Binary:
										{
											CStr ParseValue = o_Value.f_String();
											uch8 const *pParse = (uch8 const *)ParseValue.f_GetStr();

											NBuildSystem::CCustomRegistryKeyValue::CEJsonParseContext Context;
											Context.m_pStartParse = pParse;

											CEJsonSorted ConvertedValue = Context.f_ParseBinary(pParse, false);
											if (ConvertedValue == ExpectedValue)
												return true;
										}
										break;
									case EEJsonType_String:
									case EEJsonType_Invalid:
									case EEJsonType_Null:
									case EEJsonType_UserType:
									case EEJsonType_Object:
									case EEJsonType_Array:
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
								CStr Error = ExpectedValue.f_ToString(nullptr, gc_BuildSystemJsonParseFlags);
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
									if (fp_DoesValueConformToType(_Context, Type, _TypePosition, Value, _Flags, _Namespace, &Error, _pGetErrorContext))
									{
										o_Value = fg_Move(Value);
										return true;
									}
									ConformErrors.f_Insert(Error);
								}
								else if (fp_DoesValueConformToType(_Context, Type, _TypePosition, Value, _Flags, _Namespace, nullptr, _pGetErrorContext))
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
									if (fp_DoesValueConformToType(_Context, Type, _TypePosition, o_Value, _Flags, _Namespace, &Error, _pGetErrorContext))
										return true;
									ConformErrors.f_Insert(Error);
								}
								else if (fp_DoesValueConformToType(_Context, Type, _TypePosition, o_Value, _Flags, _Namespace, nullptr, _pGetErrorContext))
									return true;
							}
						}
					}

					if (o_pError)
					{
						CStr Error = "No match found in one_of:";
						if (NonMatchingValues)
							Error += "\n\n    None of values match: {}"_f << NonMatchingValues;

						if (!ConformErrors.f_IsEmpty())
						{
							Error += "\n\n    Value does not conform to any of the types. See errors:";
							for (auto &ConformError : ConformErrors)
							{
								Error += "\n\n";
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

					if (!fp_DoesValueConformToType(_Context, SpecificType.m_Return.f_Get(), _TypePosition, o_Value, _Flags, _Namespace, o_pError, _pGetErrorContext))
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
			{
				if (_Context.m_pStorePositions && _TypePosition.f_IsValid())
				{
					if (_pGetErrorContext && *_pGetErrorContext)
						_Context.m_pStorePositions->f_AddPosition(_TypePosition, "Optional was not set for '{}'"_f << (*_pGetErrorContext)())->f_AddValue({}, f_EnableValues());
					else
						_Context.m_pStorePositions->f_AddPosition(_TypePosition, gc_ConstString_Optional)->f_AddValue({}, f_EnableValues());
				}
				return true;
			}
			
			return false;
		}
		return true;
	}

	void CBuildSystem::fp_CheckValueConformToPropertyType
		(
			CEvalPropertyValueContext &_Context
			, CPropertyKeyReference const &_Property
			, CEJsonSorted &o_Value
			, CFilePosition const &_Position
			, EDoesValueConformToTypeFlag _Flags
		) const
	{
		CTypeWithPosition const *pType;
		CBuildSystemUniquePositions TypePositions;

		TypePositions.m_pParentPositions = _Context.m_pStorePositions;
		_Context.m_pStorePositions = f_EnablePositions(&TypePositions);

		auto CleanupPositions = g_OnScopeExit / [&]
			{
				_Context.m_pStorePositions = TypePositions.m_pParentPositions;
			}
		;

		{
			pType = fp_GetTypeForProperty(_Context, _Property);

			if (!pType)
			{
				if (!_Context.m_EvalContext.m_bFailUndefinedTypeCheck)
					return;

				TypePositions.f_AddPositionFirst(_Position, gc_ConstString_Context, gc_ConstString_Context);

				fs_ThrowError(_Context, TypePositions, "Found no type for {}"_f << _Property);
			}
		}
		
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

		if (TypePositions.m_pParentPositions)
			TypePositions.m_pParentPositions->f_AddPositions(TypePositions);
	}

	void CBuildSystem::fp_CheckValueConformToType
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CType const &_Type
			, CEJsonSorted &o_Value
			, CFilePosition const &_Position
			, CFilePosition const &_TypePosition
			, NFunction::TCFunctionNoAlloc<CStr ()> const &_fGetErrorContext
			, EDoesValueConformToTypeFlag _Flags
		) const
	{
		CFilePosition const *pTypePosition = &_TypePosition;
		auto pCanonicalType = fp_GetCanonicalType(_Context, &_Type, pTypePosition);
		if (pCanonicalType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
		{
			auto &FunctionType = pCanonicalType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();
			if (_Context.m_EvalContext.m_pCallingFunction != &FunctionType)
			{
				TCVector<CBuildSystemError> Errors;
				if (pTypePosition->f_IsValid())
					Errors.f_Insert({CBuildSystemUniquePositions(*pTypePosition, gc_ConstString_Type), "See definition of function type"});
				if (_TypePosition != *pTypePosition && _TypePosition.f_IsValid())
					Errors.f_Insert({CBuildSystemUniquePositions(*pTypePosition, gc_ConstString_Type), "See definition of function type (original)"});
				fs_ThrowError(_Context, _Position, "Trying to access a function as a variable, you need to call it", Errors);
			}
		}

		if (!fp_DoesValueConformToType(_Context, _Type, *pTypePosition, o_Value, _Flags | EDoesValueConformToTypeFlag_CanApplyDefault, {}, nullptr, &_fGetErrorContext))
		{
			CTypeConformError Error;
			fp_DoesValueConformToType(_Context, _Type, *pTypePosition, o_Value, _Flags | EDoesValueConformToTypeFlag_CanApplyDefault, {}, &Error);

			if (pCanonicalType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
			{
				auto &FunctionType = pCanonicalType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();
				pCanonicalType = fp_GetCanonicalType(_Context, &FunctionType.m_Return.f_Get(), pTypePosition);
			}

			TCVector<CBuildSystemError> Errors;

			if (pTypePosition->f_IsValid())
				Errors.f_Insert({CBuildSystemUniquePositions(*pTypePosition, gc_ConstString_Type), "See definition of type"});

			if (_TypePosition != *pTypePosition && _TypePosition.f_IsValid())
				Errors.f_Insert({CBuildSystemUniquePositions(_TypePosition, gc_ConstString_Type), "See definition of type (original)"});

			fs_ThrowError
				(
					_Context
					, _Position
					, "For {}\n    Value:\n\n{}\n\n    Does not conform to type:\n\n{}\n\n        {}\n"_f
					<< _fGetErrorContext()
					<< fg_IndentString(o_Value.f_ToString("\t", gc_BuildSystemJsonParseFlags).f_Trim(), "        ")
					<< fg_IndentString("{}"_f << *pCanonicalType, "        ")
					<< fg_IndentString("{}"_f << Error, "        ").f_Trim()
					, Errors
				)
			;
		}
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluatePropertyValueAccessors(CEvalPropertyValueContext &_Context, CValuePotentiallyByRef &&_Value, TCVector<CBuildSystemSyntax::CJsonAccessorEntry> const &_Accessors) const
	{
		auto *pValue = &_Value.f_Get();

		fp_ApplyAccessors
			(
				_Context
				, _Accessors
				, [&](CStr const &_MemberName, bool _bOptionalChain)
				{
					if (!pValue || !pValue->f_IsValid())
					{
						if (_bOptionalChain)
							return;

						fs_ThrowError
							(
								_Context
								, "JSON value is undefined so cannot apply member name '{}'"_f
								<< _MemberName
							)
						;
					}

					if (!pValue->f_IsObject())
					{
						fs_ThrowError
							(
								_Context
								, "JSON value '{}' is not an object so cannot apply member name '{}'"_f
								<< pValue->f_ToString(nullptr, EJsonDialectFlag_AllowUndefined)
								<< _MemberName
							)
						;
					}

					pValue = pValue->f_GetMember(_MemberName);
				}
				, [&](int64 _Index, bool _bOptionalChain)
				{
					if (!pValue || !pValue->f_IsValid())
					{
						if (_bOptionalChain)
							return;

						fs_ThrowError
							(
								_Context
								, "JSON value is undefined so cannot apply cannot apply subscript index"
							)
						;
					}

					if (!pValue->f_IsArray())
						fs_ThrowError(_Context, "JSON value '{}' is not an array so cannot apply subscript index"_f << pValue->f_ToString(nullptr, EJsonDialectFlag_AllowUndefined));

					auto &Array = pValue->f_Array();

					if (_Index >= int64(TCLimitsInt<smint>::mc_Max) || _Index < 0 || _Index >= int64(Array.f_GetLen()))
						fs_ThrowError(_Context, "Index {} is out of range for array that has {} elements"_f << _Index << Array.f_GetLen());

					pValue = &Array[_Index];
				}
			)
		;

		if (pValue)
			return _Value.f_GetSubObject(*pValue);

		return CEJsonSorted{};
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluatePropertyValueJsonAccessor(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CJsonAccessor const &_Value) const
	{
		return fp_EvaluatePropertyValueAccessors(_Context, fp_EvaluatePropertyValueParam(_Context, _Value.m_Param), _Value.m_Accessors);
	}

	NEncoding::CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueIdentifierReference(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CIdentifierReference const &_Value) const
	{
		CStr const *pPropertyName;
		CStr PropertyNameHolder;
		if (_Value.m_Identifier.f_IsNameConstantString())
			pPropertyName = &_Value.m_Identifier.f_NameConstantString();
		else
		{
			PropertyNameHolder = fp_EvaluatePropertyValueEvalString(_Context, _Value.m_Identifier.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>());
			pPropertyName = &PropertyNameHolder;
		}

		CStr const &PropertyName = *pPropertyName;

		EPropertyType PropertyType = EPropertyType_Invalid;
		if (_Value.m_Identifier.f_IsPropertyTypeConstant())
		{
			PropertyType = _Value.m_Identifier.f_PropertyTypeConstant();
			DMibCheck(PropertyType != EPropertyType_Invalid);
		}
		else
		{
			auto PropertyTypeString = fp_EvaluatePropertyValueEvalString(_Context, _Value.m_Identifier.m_PropertyType.f_GetAsType<CBuildSystemSyntax::CEvalString>());
			PropertyType = fg_PropertyTypeFromStr(PropertyTypeString);
			if (PropertyType == EPropertyType_Invalid)
				fs_ThrowError(_Context, "Invalid property type '{}'"_f << PropertyTypeString);
		}

		NEncoding::CEJsonSorted Return;
		auto &Object = Return.f_Object();
		Object[gc_ConstString_Type] = fg_PropertyTypeToStr(PropertyType);
		Object[gc_ConstString_Name] = PropertyName;

		if (_Value.m_Identifier.m_EntityType != EEntityType_Invalid)
			Object[gc_ConstString_EntityType] = fg_EntityTypeToStr(_Value.m_Identifier.m_EntityType);

		return Return;
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluatePropertyValueIdentifier(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CIdentifier const &_Value, bool _bMoveCache) const
	{
		CStr const *pPropertyName;
		CStr PropertyNameHolder;
		uint32 PropertyNameHash;
		
		if (_Value.f_IsNameConstantString())
		{
			pPropertyName = &_Value.f_NameConstantString();
			PropertyNameHash = _Value.f_NameConstantStringHash();
		}
		else
		{
			PropertyNameHolder = fp_EvaluatePropertyValueEvalString(_Context, _Value.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>());
			PropertyNameHash = PropertyNameHolder.f_Hash();
			PropertyNameHolder = mp_StringCache.f_AddString(PropertyNameHolder, PropertyNameHash);
			pPropertyName = &PropertyNameHolder;
		}

		CStr const &PropertyName = *pPropertyName;

		auto pOriginalContext = &_Context.m_OriginalContext;
		if (_Value.m_EntityType != EEntityType_Invalid)
		{
			while (pOriginalContext && pOriginalContext->f_GetKey().m_Type != _Value.m_EntityType)
				pOriginalContext = pOriginalContext->m_pParent;

			if (!pOriginalContext)
			{
				fs_ThrowError
					(
						_Context
						, "No entity with type '{}' found in parent entities for path: {}"_f << fg_EntityTypeToStr(_Value.m_EntityType) << _Context.m_OriginalContext.f_GetPath()
					)
				;
			}
		}

		EPropertyType PropertyType = EPropertyType_Invalid;
		if (_Value.f_IsPropertyTypeConstant())
		{
			PropertyType = _Value.f_PropertyTypeConstant();
			DMibCheck(PropertyType != EPropertyType_Invalid);
		}
		else
		{
			auto PropertyTypeString = fp_EvaluatePropertyValueEvalString(_Context, _Value.m_PropertyType.f_GetAsType<CBuildSystemSyntax::CEvalString>());
			PropertyType = fg_PropertyTypeFromStr(PropertyTypeString);
			if (PropertyType == EPropertyType_Invalid)
				fs_ThrowError(_Context, "Invalid property type '{}'"_f << PropertyTypeString);
		}

		if (PropertyType == EPropertyType_Builtin)
		{
			if (PropertyName == gc_ConstKey_Builtin_GeneratedFiles.m_Name)
			{
				TCVector<CEJsonSorted> Ret;
				{
					DMibLock(mp_GeneratedFilesLock);
					for (auto iFile = mp_GeneratedFiles.f_GetIterator(); iFile; ++iFile)
					{
						if (iFile->m_bGeneral)
							Ret.f_Insert(iFile.f_GetKey());
					}
				}

				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.GeneratedFiles")->f_AddValue(Ret, f_EnableValues());

				return CEJsonSorted(fg_Move(Ret));
			}
			else if (PropertyName == gc_ConstKey_Builtin_SourceFiles.m_Name)
			{
				TCVector<CEJsonSorted> Ret;
				{
					DMibLockRead(mp_SourceFilesLock);
					for (auto &File : mp_SourceFiles.f_Keys())
						Ret.f_Insert(File);
				}

				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.SourceFiles")->f_AddValue(Ret, f_EnableValues());

				return CEJsonSorted(fg_Move(Ret));
			}
			else if (PropertyName == gc_ConstKey_Builtin_BuildSystemSourceAbsolute.m_Name)
			{
				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BuildSystemSourceAbsolute")->f_AddValue(mp_FileLocation, f_EnableValues());

				return &mp_FileLocation;
			}
			else if (PropertyName == gc_ConstKey_Builtin_BuildSystemSource.m_Name)
			{
				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BuildSystemSource")->f_AddValue(mp_FileLocationFile, f_EnableValues());
				
				return &mp_FileLocationFile;
			}
			else if (PropertyName == gc_ConstKey_Builtin_GeneratorStateFile.m_Name)
			{
				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.GeneratorStateFile")->f_AddValue(mp_GeneratorStateFileName, f_EnableValues());
				
				return &mp_GeneratorStateFileName;
			}
			else if (PropertyName == gc_ConstKey_Builtin_BasePathAbsolute.m_Name)
			{
				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePathAbsolute")->f_AddValue(f_GetBaseDir(), f_EnableValues());
				
				return &mp_BaseDir;
			}
			else if (PropertyName == gc_ConstKey_Builtin_MToolExe.m_Name)
			{
				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.MToolExe")->f_AddValue(mp_MToolExe, f_EnableValues());

				return &mp_MToolExe;
			}
			else if (PropertyName == gc_ConstKey_Builtin_CMakeRoot.m_Name)
			{
				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.CMakeRoot")->f_AddValue(mp_CMakeRoot, f_EnableValues());

				return &mp_CMakeRoot;
			}
			else if (PropertyName == gc_ConstKey_Builtin_MalterlibExe.m_Name)
			{
				if (_Context.m_pStorePositions)
					_Context.m_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.MalterlibExe")->f_AddValue(mp_MalterlibExe, f_EnableValues());

				return &mp_MalterlibExe;
			}
			else
			{
				bool bSuccess = false;
				auto Return = mp_GeneratorInterface->f_GetBuiltin(_Context.m_pStorePositions, PropertyName, bSuccess);
				if (!bSuccess)
					fs_ThrowError(_Context, CStr::CFormat("Unrecognized builtin '{}'") << PropertyName);
				return Return;
			}
		}
		else if (PropertyType == EPropertyType_This)
		{
			auto &ContextKey = pOriginalContext->f_GetKey();

			if (ContextKey.m_Type == EEntityType_Root)
				fs_ThrowError(_Context, "Cannot access this on root entity");

			if (PropertyName == gc_ConstString_Identity.m_String)
			{
				if (!ContextKey.m_Name.f_IsConstantString())
					fs_ThrowError(_Context, "Identity can only be gotten from entity with constant string name. Value: {} Path: {}"_f << ContextKey.m_Name << pOriginalContext->f_GetPath());

				return {pOriginalContext->f_GetKeyName()};
			}
			else if (PropertyName == gc_ConstString_EntityPath.m_String)
				return {pOriginalContext->f_GetPath()};
			else if (PropertyName == gc_ConstString_IdentityAsAbsolutePath.m_String)
			{
				if (!ContextKey.m_Name.f_IsConstantString())
					fs_ThrowError(_Context, "Identity can only be gotten from entity with constant string name. Value: {} Path: {}"_f << ContextKey.m_Name << pOriginalContext->f_GetPath());

				return {CFile::fs_GetExpandedPath(pOriginalContext->f_GetKeyName(), CFile::fs_GetPath(pOriginalContext->f_Data().m_Position.m_File))};
			}
			else if (PropertyName == gc_ConstString_IdentityPath.m_String)
				return CEJsonSorted(pOriginalContext->f_Data().m_Position.m_File);
			else if (PropertyName == gc_ConstString_Type.m_String)
				return CEJsonSorted(fg_EntityTypeToStr(ContextKey.m_Type));
			else
				fs_ThrowError(_Context, CStr::CFormat("Unrecognized entity (this) accessor '{}'") << PropertyName);
		}
		else if (pOriginalContext == &_Context.m_OriginalContext)
		{
			CPropertyKeyReference Key(CAssertAddedToStringCache(), PropertyType, PropertyName, PropertyNameHash);

			for (auto *pEvaluatedProperties = _Context.m_EvalContext.m_pEvaluatedProperties; pEvaluatedProperties; pEvaluatedProperties = pEvaluatedProperties->m_pParentProperties)
			{
				auto pValue = pEvaluatedProperties->m_Properties.f_FindEqual(Key);
				if (pValue)
				{
					if (_Context.m_pStorePositions)
						_Context.m_pStorePositions->f_AddPositions(pValue->m_pPositions);

					if (_bMoveCache)
						return CValuePotentiallyByRef(&pValue->m_Value, true);
					else
						return &pValue->m_Value;
				}
			}

			CBuildSystemPropertyInfo PropertyInfo;
			auto Return = fp_EvaluateEntityProperty
				(
					_Context.m_OriginalContext
					, _Context.m_OriginalContext
					, Key
					, _Context.m_EvalContext
					, PropertyInfo
					, _Context.m_Position
					, &_Context
					, _bMoveCache
				)
			;

			if (_Context.m_pStorePositions && PropertyInfo.m_pPositions)
				_Context.m_pStorePositions->f_AddPositions(*PropertyInfo.m_pPositions);

			return Return;
		}
		else
		{
			CPropertyKeyReference Key(CAssertAddedToStringCache(), PropertyType, PropertyName, PropertyNameHash);

			CEvaluatedProperty * pValue;
			{
				pValue = pOriginalContext->m_EvaluatedProperties.m_Properties.f_FindEqual(Key);
				if (pValue)
					return &pValue->m_Value;
			}

			pValue = pOriginalContext->m_EvaluatedProperties.m_Properties.f_FindEqual(Key);
			if (!pValue)
			{
				CChangePropertiesScope ChangeProperties(_Context.m_EvalContext, &pOriginalContext->m_EvaluatedProperties);

				CBuildSystemPropertyInfo PropertyInfo;

				auto Return = fp_EvaluateEntityProperty(*pOriginalContext, *pOriginalContext, Key, _Context.m_EvalContext, PropertyInfo, _Context.m_Position, &_Context, false);

				if (_Context.m_pStorePositions && PropertyInfo.m_pPositions)
					_Context.m_pStorePositions->f_AddPositions(*PropertyInfo.m_pPositions);

				return Return;
			}
			else
				return &pValue->m_Value;
		}
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluatePropertyValueParam(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CParam const &_Value) const
	{
		switch (_Value.m_Param.f_GetTypeID())
		{
			case 0: return &_Value.m_Param.f_Get<0>();
			case 1: return fp_EvaluatePropertyValueObject(_Context, _Value.m_Param.f_Get<1>());
			case 2: return fp_EvaluatePropertyValueArray(_Context, _Value.m_Param.f_Get<2>());
			case 3: return fp_EvaluatePropertyValueIdentifier(_Context, _Value.m_Param.f_Get<3>().f_Get(), false);
			case 4: return fp_EvaluatePropertyValueIdentifierReference(_Context, _Value.m_Param.f_Get<4>().f_Get());
			case 5: return CEJsonSorted(fp_EvaluatePropertyValueEvalString(_Context, _Value.m_Param.f_Get<5>()));
			case 6: return fp_EvaluatePropertyValueWildcardString(_Context, _Value.m_Param.f_Get<6>());
			case 7: return fp_EvaluatePropertyValueExpression(_Context, _Value.m_Param.f_Get<7>().f_Get());
			case 8: return fp_EvaluatePropertyValueExpressionAppend(_Context, _Value.m_Param.f_Get<8>().f_Get());
			case 9: return fp_EvaluatePropertyValueTernary(_Context, _Value.m_Param.f_Get<9>().f_Get());
			case 10: return fp_EvaluatePropertyValueBinaryOperator(_Context, _Value.m_Param.f_Get<10>().f_Get());
			case 11: return fp_EvaluatePropertyValuePrefixOperator(_Context, _Value.m_Param.f_Get<11>().f_Get());
			default: DNeverGetHere; break;
		}
		return CEJsonSorted();
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluatePropertyValueExpression(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CExpression const &_Value) const
	{
		if (_Value.m_Expression.f_IsOfType<CBuildSystemSyntax::CFunctionCall>())
			return fp_EvaluatePropertyValueFunctionCall(_Context, _Value.m_Expression.f_GetAsType<CBuildSystemSyntax::CFunctionCall>(), false);
		else if (_Value.m_Expression.f_IsOfType<CBuildSystemSyntax::CParam>())
			return fp_EvaluatePropertyValueParam(_Context, _Value.m_Expression.f_GetAsType<CBuildSystemSyntax::CParam>());
		else if (_Value.m_Expression.f_IsOfType<TCIndirection<CBuildSystemSyntax::CJsonAccessor>>())
			return fp_EvaluatePropertyValueJsonAccessor(_Context, _Value.m_Expression.f_GetAsType<TCIndirection<CBuildSystemSyntax::CJsonAccessor>>().f_Get());
		else
			DMibNeverGetHere;

		return CEJsonSorted();
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluatePropertyValueTernary(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CTernary const &_Value) const
	{
		auto Conditional = fp_EvaluatePropertyValueParam(_Context, _Value.m_Conditional);
		auto &ConditionalRef = Conditional.f_Get();
		if (!ConditionalRef.f_IsBoolean())
			fs_ThrowError(_Context, "Expected conditional to evalute to a boolean");

		if (ConditionalRef.f_Boolean())
			return fp_EvaluatePropertyValueParam(_Context, _Value.m_Left);
		else
			return fp_EvaluatePropertyValueParam(_Context, _Value.m_Right);
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluateRootValue
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CRootValue const &_Value
			, CPropertyKey const *_pProperty
			, bool &o_bTypeAlreadyChecked
		) const
	{
		if (!_Value.m_Accessors.f_IsEmpty())
		{
			if (!_pProperty)
				fs_ThrowError(_Context, "Accessors are only supported at root");

			CBuildSystemSyntax::CIdentifier Identifier;
			Identifier.m_PropertyType = _pProperty->f_GetType();
			Identifier.m_Name = _pProperty->f_GetStringAndHash();

			auto Return = fp_EvaluatePropertyValueIdentifier(_Context, Identifier, true);
			CEJsonSorted *pWriteDestination = Return.f_MakeMutable();
			bool bAppliedAccessors = false;

			fp_ApplyAccessors
				(
					_Context
					, _Value.m_Accessors
					, [&](CStr const &_MemberName, bool _bOptionalChain)
					{
						if (_bOptionalChain)
							fs_ThrowError(_Context, "Optional chaining operator does not make sense for a write accessor");

						if (!pWriteDestination->f_IsObject() && pWriteDestination->f_IsValid())
							fs_ThrowError(_Context, "Not an object so cannot apply member");

						auto &Member = (*pWriteDestination)[_MemberName];

						pWriteDestination = &Member;
						bAppliedAccessors = true;
					}
					, [&](int64 _Index, bool _bOptionalChain)
					{
						if (_bOptionalChain)
							fs_ThrowError(_Context, "Optional chaining operator does not make sense for a write accessor");

						if (!pWriteDestination->f_IsArray())
							fs_ThrowError(_Context, "Not an array so cannot apply subscript index");

						auto &Array = pWriteDestination->f_Array();

						if (_Index >= int64(TCLimitsInt<smint>::mc_Max) || _Index < 0 || _Index >= int64(Array.f_GetLen()))
							fs_ThrowError(_Context, "Index {} is out of range for array that has {} elements"_f << _Index << Array.f_GetLen());

						pWriteDestination = &Array[_Index];
						bAppliedAccessors = true;
					}
				)
			;

			bool bTypeAlreadyChecked = false;
			CWritePropertyContext WriteContext
				{
					.m_Property = *_pProperty
					, .m_bTypeAlreadyChecked = bTypeAlreadyChecked
					, .m_pWriteDestination = pWriteDestination
					, .m_pAccessors = &_Value.m_Accessors
				}
			;
			*pWriteDestination = fp_EvaluatePropertyValue(_Context, _Value.m_Value, &WriteContext).f_Move();

			if (!bAppliedAccessors && bTypeAlreadyChecked)
				o_bTypeAlreadyChecked = true;

			return Return;
		}

		CWritePropertyContext WriteContext{*_pProperty, o_bTypeAlreadyChecked};
		return fp_EvaluatePropertyValue(_Context, _Value.m_Value, &WriteContext);
	}

	CValuePotentiallyByRef CBuildSystem::fp_EvaluatePropertyValue(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CValue const &_Value, CWritePropertyContext *_pWriteContext) const
	{
		switch (_Value.m_Value.f_GetTypeID())
		{
		case 0: return &_Value.m_Value.f_Get<0>();
		case 1: return fp_EvaluatePropertyValueObject(_Context, _Value.m_Value.f_Get<1>());
		case 2: return fp_EvaluatePropertyValueArray(_Context, _Value.m_Value.f_Get<2>());
		case 3: return fp_EvaluatePropertyValueWildcardString(_Context, _Value.m_Value.f_Get<3>());
		case 4: return CEJsonSorted(fp_EvaluatePropertyValueEvalString(_Context, _Value.m_Value.f_Get<4>()));
		case 5: return fp_EvaluatePropertyValueExpression(_Context, _Value.m_Value.f_Get<5>());
		case 6: return fp_EvaluatePropertyValueExpressionAppend(_Context, _Value.m_Value.f_Get<6>());
		case 7: return fp_EvaluatePropertyValueOperator(_Context, _Value.m_Value.f_Get<7>(), _pWriteContext);
		case 8: return fp_EvaluatePropertyValueDefine(_Context, _Value.m_Value.f_Get<8>());
		default: DMibNeverGetHere; return CEJsonSorted();
		}
	}

	void CBuildSystem::fp_EvaluatePropertyValueFunctionCallCollectParams
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CFunctionCall const &_FunctionCall
			, CBuildSystemSyntax::CFunctionType const &_FunctionType
			, TCFunctionNoAlloc<void (CEJsonSorted &&_Param, CBuildSystemSyntax::CFunctionParameter const &_FunctionParam, bool _bEllipsis, bool _bAddDefault)> const &_fConsumeParam
			, CFilePosition const &_TypePosition
		) const
	{
		TCVector<CEJsonSorted> EvalParams;
		for (auto &Param : _FunctionCall.m_Params)
		{
			if (Param.m_Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>())
			{
				auto &Expression = Param.m_Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>().f_Get();
				auto ToAppend = fp_EvaluatePropertyValueExpression(_Context, Expression);
				auto &ToAppendRef = ToAppend.f_Get();
				if (ToAppendRef.f_IsArray())
					EvalParams.f_Insert(ToAppend.f_MoveArray());
				else if (!ToAppendRef.f_IsValid())
					; // Undefined values are ignored
				else
					fs_ThrowError(_Context, "Append expressions expects an array to expand. {} resulted in : {}"_f << Expression << ToAppendRef);
			}
			else
				EvalParams.f_Insert(fp_EvaluatePropertyValueParam(_Context, Param).f_Move());
		}

		{
			auto iParamType = _FunctionType.m_Parameters.f_GetIterator();

			mint iParam = 0;
			for (auto &EvalParam : EvalParams)
			{
				if (!iParamType)
					fs_ThrowError(_Context, "Too many parameters for function '{}' with type: {}"_f << _FunctionCall.m_PropertyKey << _FunctionType);

				fp_CheckValueConformToType
					(
						_Context
						, iParamType->m_Type.f_Get()
						, EvalParam
						, _Context.m_Position
						, _TypePosition
						, [&]() -> CStr
						{
							return "In call to function '{}' parameter {} ({})"_f << _FunctionCall.m_PropertyKey << iParamType->m_Name << iParam;
						}
						, EDoesValueConformToTypeFlag_None
					)
				;

				if (iParamType->m_ParamType == CBuildSystemSyntax::CFunctionParameter::EParamType_Ellipsis)
					_fConsumeParam(fg_Move(EvalParam), *iParamType, true, false);
				else
				{
					_fConsumeParam(fg_Move(EvalParam), *iParamType, false, false);
					++iParamType;
				}

				++iParam;
			}

			for (; iParamType; ++iParamType)
			{
				if (iParamType->m_ParamType == CBuildSystemSyntax::CFunctionParameter::EParamType_Ellipsis)
				{
					_fConsumeParam({}, *iParamType, true, true);
					break;
				}
				if (iParamType->m_ParamType == CBuildSystemSyntax::CFunctionParameter::EParamType_Optional)
				{
					CEJsonSorted Param; // Undefined

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

					_fConsumeParam(fg_Move(Param), *iParamType, false, false);
				}
				else
					fs_ThrowError(_Context, "Missing parameters for function '{}' with type: {}"_f << _FunctionCall.m_PropertyKey << _FunctionType);
				++iParam;
			}
		}
	}

	NEncoding::CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueFunctionCallBuiltin
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemSyntax::CFunctionCall const &_FunctionCall
			, CBuiltinFunction const *_pFunction
		) const
	{
		CBuildSystemUniquePositions::CPosition *pPosition = nullptr;
		if (_Context.m_pStorePositions)
			pPosition = _Context.m_pStorePositions->f_AddPosition(_pFunction->m_Position, "Call builtin '{}'"_f << _FunctionCall.m_PropertyKey.m_Name);

		TCVector<CEJsonSorted> Params;
		TCVector<CEJsonSorted> *pEllipsis = nullptr;
		fp_EvaluatePropertyValueFunctionCallCollectParams
			(
				_Context
				, _FunctionCall
				, _pFunction->m_Type
				, [&](CEJsonSorted &&_Param, CBuildSystemSyntax::CFunctionParameter const &_FunctionParam, bool _bEllipsis, bool _bAddDefault)
				{
					if (_bEllipsis)
					{
						if (!pEllipsis)
							pEllipsis = &Params.f_Insert(EJsonType_Array).f_Array();
						if (!_bAddDefault)
							pEllipsis->f_Insert(fg_Move(_Param));
					}
					else
						Params.f_Insert(fg_Move(_Param));
				}
				, _pFunction->m_Position
			)
		;

		auto Return = _pFunction->m_fFunction(*this, _Context, fg_Move(Params));

		if (pPosition)
			pPosition->f_AddValue(Return, f_EnableValues());

		return Return;
	}

	NEncoding::CEJsonSorted CBuildSystem::fp_EvaluatePropertyValueFunctionCall(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CFunctionCall const &_FunctionCall, bool _bMoveCache) const
	{
		if (_FunctionCall.m_PropertyKey.f_GetType() == EPropertyType_Property)
		{
			auto pFunction = fg_RemoveQualifiers(mp_BuiltinFunctions).f_FindEqual(_FunctionCall.m_PropertyKey.m_Name);
			if (pFunction)
				return fp_EvaluatePropertyValueFunctionCallBuiltin(_Context, _FunctionCall, pFunction);
		}

		auto KeyReference = _FunctionCall.m_PropertyKey.f_Reference();

		auto *pTypeWithPosition = fp_GetTypeForProperty(_Context, KeyReference);
		if (!pTypeWithPosition)
			fs_ThrowError(_Context, "No such function: {}"_f << KeyReference);

		auto pTypePosition = &pTypeWithPosition->m_Position;

		CBuildSystemSyntax::CType const *pType = fp_GetCanonicalType(_Context, &pTypeWithPosition->m_Type, pTypePosition);

		if (!pType->m_Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>())
			fs_ThrowError(_Context, "Type is a variable, not a fuction: {}"_f << KeyReference);

		auto &FunctionType = pType->m_Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

		CEvaluatedProperties TempProperties;
		TempProperties.m_pParentProperties = _Context.m_EvalContext.m_pEvaluatedProperties;

		TCVector<CEJsonSorted> *pEllipsis = nullptr;
		fp_EvaluatePropertyValueFunctionCallCollectParams
			(
				_Context
				, _FunctionCall
				, FunctionType
				, [&](CEJsonSorted &&_Param, CBuildSystemSyntax::CFunctionParameter const &_FunctionParam, bool _bEllipsis, bool _bAddDefault)
				{
					if (_bEllipsis)
					{
						if (!pEllipsis)
						{
							pEllipsis = &(TempProperties.m_Properties[CPropertyKey(mp_StringCache, _FunctionParam.m_Name)].m_Value = EJsonType_Array).f_Array();
							pEllipsis->f_Clear();
						}

						if (!_bAddDefault)
							pEllipsis->f_Insert(fg_Move(_Param));
					}
					else
						TempProperties.m_Properties[CPropertyKey(mp_StringCache, _FunctionParam.m_Name)].m_Value = fg_Move(_Param);
				}
				, *pTypePosition
			)
		;

		CChangePropertiesScope ChangeProperties(_Context.m_EvalContext, &TempProperties);

		auto pOldFunction = _Context.m_EvalContext.m_pCallingFunction;
		_Context.m_EvalContext.m_pCallingFunction = &FunctionType;
		auto Cleanup = g_OnScopeExit / [&]
			{
				_Context.m_EvalContext.m_pCallingFunction = pOldFunction;
			}
		;

		CBuildSystemPropertyInfo PropertyInfo;

		auto Return = fp_EvaluateEntityProperty
			(
				_Context.m_OriginalContext
				, _Context.m_OriginalContext
				, KeyReference
				, _Context.m_EvalContext
				, PropertyInfo
				, _Context.m_Position
				, &_Context
				, _bMoveCache
			)
		;

		if (_Context.m_pStorePositions && PropertyInfo.m_pPositions)
			_Context.m_pStorePositions->f_AddPositions(*PropertyInfo.m_pPositions);

		return Return.f_Move();
	}
}
