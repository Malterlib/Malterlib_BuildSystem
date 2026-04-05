// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Storage/Indirection>
#include <Mib/Storage/Optional>
#include <Mib/Encoding/EJson>
#include <Mib/Encoding/EJsonParse>
#include <Mib/Encoding/EJsonGenerate>
#include <Mib/Encoding/EJsonImpl>

#include "Malterlib_BuildSystem_StringCache.h"
#include "Malterlib_BuildSystem_ConstantStrings.h"

namespace NMib::NBuildSystem
{
	constexpr NEncoding::EJsonDialectFlag gc_BuildSystemJsonParseFlags = NEncoding::EJsonDialectFlag_AllowUndefined | NEncoding::EJsonDialectFlag_AllowInvalidFloat;

	struct CFilePosition;
	struct CPropertyKeyReference;

	enum EPropertyType : uint32
	{
		EPropertyType_Invalid
		, EPropertyType_Property
		, EPropertyType_Compile
		, EPropertyType_Target
		, EPropertyType_Workspace
		, EPropertyType_Dependency
		, EPropertyType_Import
		, EPropertyType_Repository
		, EPropertyType_CreateTemplate
		, EPropertyType_Group
		, EPropertyType_This
		, EPropertyType_Builtin
		, EPropertyType_Type
		, EPropertyType_GenerateFile
		, EPropertyType_GeneratorSetting

		, EPropertyType_Max
	};

	enum ESwitchType
	{
		ESwitchType_Bare
		, ESwitchType_Default
		, ESwitchType_Error
	};

	struct CPropertyKeyTypeAndHash
	{
		constexpr CPropertyKeyTypeAndHash()
			: CPropertyKeyTypeAndHash(EPropertyType_Property, 0)
		{
		}

		constexpr CPropertyKeyTypeAndHash(EPropertyType _Type, uint32 _NameHash)
			: m_Data(uint64(_Type) << 32 | uint64(_NameHash))
		{
		}

		CPropertyKeyTypeAndHash(CPropertyKeyTypeAndHash const &) = default;
		CPropertyKeyTypeAndHash(CPropertyKeyTypeAndHash &&) = default;

		CPropertyKeyTypeAndHash &operator = (CPropertyKeyTypeAndHash const &) = default;
		CPropertyKeyTypeAndHash &operator = (CPropertyKeyTypeAndHash &&) = default;

		auto operator <=> (CPropertyKeyTypeAndHash const &) const noexcept = default;

		constexpr inline_always EPropertyType f_GetType() const
		{
			return (EPropertyType)(m_Data >> 32);
		}

		constexpr inline_always uint32 f_GetNameHash() const
		{
			return (m_Data & DMibBitRangeTyped(0, 31, uint64));
		}

		uint64 m_Data;
	};

	struct CPropertyKey
	{
		CPropertyKey();
		CPropertyKey(CStringCache &o_StringCache, NStr::CStr const &_Name);
		CPropertyKey(CStringCache &o_StringCache, EPropertyType _Type, NStr::CStr const &_Name);
		CPropertyKey(CStringCache &o_StringCache, EPropertyType _Type, NStr::CStr const &_Name, uint32 _Hash);
		CPropertyKey(CAssertAddedToStringCache _Dummy, EPropertyType _Type, NStr::CStr const &_Name, uint32 _Hash);

		CPropertyKey(CPropertyKey const &_Other);
		CPropertyKey(CPropertyKey &&_Other);
		explicit CPropertyKey(CPropertyKeyReference const &_Other);
		CPropertyKey &operator = (CPropertyKey const &_Other);
		CPropertyKey &operator = (CPropertyKey &&_Other);

		CPropertyKeyReference f_Reference() const;

		inline_always bool operator == (CPropertyKey const &_Right) const noexcept;
		inline_always bool operator == (CPropertyKeyReference const &_Right) const noexcept;
		inline_always COrdering_Strong operator <=> (CPropertyKey const &_Right) const noexcept;
		inline_always COrdering_Strong operator <=> (CPropertyKeyReference const &_Right) const noexcept;

		struct CCompareByString
		{
			inline_always COrdering_Strong operator() (CPropertyKey const &_Left, CPropertyKey const &_Right) const;
		};

		template <typename tf_CContext>
		static CPropertyKey fs_FromString(CStringCache &o_StringCache, NStr::CStr const &_String, tf_CContext &&_Context);

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		constexpr inline_always EPropertyType f_GetType() const
		{
			return m_TypeAndHash.f_GetType();
		}

		constexpr inline_always uint32 f_GetNameHash() const
		{
			return m_TypeAndHash.f_GetNameHash();
		}

		constexpr CStringAndHash f_GetStringAndHash() const
		{
			return CStringAndHash(CAssertAddedToStringCache(), m_Name, m_TypeAndHash.f_GetNameHash());
		}

		CPropertyKeyTypeAndHash m_TypeAndHash;
		NStr::CStr const m_Name;
	};

	enum EEntityType : uint32
	{
		EEntityType_Invalid
		, EEntityType_Root
		, EEntityType_Target
		, EEntityType_Group
		, EEntityType_Workspace
		, EEntityType_File
		, EEntityType_Dependency
		, EEntityType_GeneratorSetting
		, EEntityType_GenerateFile
		, EEntityType_Import
		, EEntityType_Repository
		, EEntityType_CreateTemplate
	};

	EEntityType fg_EntityTypeFromStr(NStr::CStr const &_String);
	NStr::CStr const &fg_EntityTypeToStr(EEntityType _Type);

	EPropertyType fg_PropertyTypeFromStr(NStr::CStr const &_String);
	NStr::CStr fg_PropertyTypeToStr(EPropertyType _Type);

	struct CBuildSystemSyntax
	{
		struct CRootKey;
		struct CRootValue;
		struct CIdentifier;
		struct CExpression;
		struct CExpressionAppend;
		struct CValue;

		struct CEvalStringToken
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CEvalStringToken fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson(bool _bRawString) const;
			COrdering_Partial operator <=> (CEvalStringToken const &_Right) const noexcept;
			bool operator == (CEvalStringToken const &_Right) const noexcept;

			bool f_IsExpression() const;
			CExpression const &f_Expression() const;

			bool f_IsString() const;
			NStr::CStr const &f_String() const;

			NStorage::TCVariant<NStr::CStr, NStorage::TCIndirection<CExpression>> m_Token;
		};

		struct CEvalString
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str, bool _bAddQuotes = true) const;
			static CEvalString fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			NEncoding::CEJsonSorted f_ToJsonArray(bool _bRawString) const;
			COrdering_Partial operator <=> (CEvalString const &_Right) const noexcept;
			bool operator == (CEvalString const &_Right) const noexcept;

			NContainer::TCVector<CEvalStringToken> m_Tokens;
		};

		struct CWildcardString
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CWildcardString fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CWildcardString const &_Right) const noexcept;
			bool operator == (CWildcardString const &_Right) const noexcept;

			NStorage::TCVariant<NStr::CStr, CEvalString> m_String;
		};

		struct CArray
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static auto fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed)
				-> NStorage::TCVariant<NEncoding::CEJsonSorted, CArray>
			;
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CArray const &_Right) const noexcept;
			bool operator == (CArray const &_Right) const noexcept;

			NContainer::TCVector<NStorage::TCIndirection<CValue>> m_Array;
		};

		struct CAppendObject
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			COrdering_Partial operator <=> (CAppendObject const &_Right) const noexcept;
			bool operator == (CAppendObject const &_Right) const noexcept;
		};

		struct CAppendObjectWithoutUndefined
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			COrdering_Partial operator <=> (CAppendObjectWithoutUndefined const &_Right) const noexcept;
			bool operator == (CAppendObjectWithoutUndefined const &_Right) const noexcept;
		};

		struct CObjectKey
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			COrdering_Partial operator <=> (CObjectKey const &_Right) const noexcept;
			bool operator == (CObjectKey const &_Right) const noexcept;

			NStorage::TCVariant<NStr::CStr, CEvalString, CAppendObject, CAppendObjectWithoutUndefined> m_Key;
		};

		struct CObject
		{
			struct CObjectValue
			{
				COrdering_Partial operator <=> (CObjectValue const &_Right) const noexcept;
				bool operator == (CObjectValue const &_Right) const noexcept;

				DMibListLinkDS_Link(CObjectValue, m_Link);
				NStorage::TCIndirection<CValue> m_Value;
			};

			CObject();
			CObject(CObject const &_Other);
			CObject(CObject &&_Other);

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static auto fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed)
				-> NStorage::TCVariant<NEncoding::CEJsonSorted, CObject>
			;
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CObject const &_Right) const noexcept;
			bool operator == (CObject const &_Right) const noexcept;

			NContainer::TCMap<CObjectKey, CObjectValue> m_Object;
			DMibListLinkDS_List(CObjectValue, m_Link) m_ObjectSorted;
		};

		struct CTernary;
		struct CPrefixOperator;
		struct CBinaryOperator;
		struct CIdentifierReference;

		struct CParam
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CParam fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, NStr::CStr const &_Type, bool _bAppendAllowed);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CParam const &_Right) const noexcept;
			bool operator == (CParam const &_Right) const noexcept;
			bool f_IsBinaryOperator() const;
			CBinaryOperator &f_BinaryOperator();
			CBinaryOperator const &f_BinaryOperator() const;

			bool f_IsJson() const;
			NEncoding::CEJsonSorted const &f_Json() const;

			bool f_IsObject() const;
			CObject const &f_Object() const;

			bool f_IsArray() const;
			CArray const &f_Array() const;

			bool f_IsIdentifier() const;
			CIdentifier const &f_Identifier() const;

			bool f_IsEvalString() const;
			CEvalString const &f_EvalString() const;

			bool f_IsWildcardString() const;
			CWildcardString const &f_WildcardString() const;

			bool f_IsExpression() const;
			CExpression const &f_Expression() const;

			bool f_IsExpressionAppend() const;
			CExpressionAppend const &f_ExpressionAppend() const;

			bool f_IsTernary() const;
			CTernary const &f_Ternary() const;

			bool f_IsPrefixOperator() const;
			CPrefixOperator const &f_PrefixOperator() const;

			NStorage::TCVariant
				<
					NEncoding::CEJsonSorted
					, CObject
					, CArray
					, NStorage::TCIndirection<CIdentifier>
					, NStorage::TCIndirection<CIdentifierReference>
					, CEvalString
					, CWildcardString
					, NStorage::TCIndirection<CExpression>
					, NStorage::TCIndirection<CExpressionAppend>
					, NStorage::TCIndirection<CTernary>
					, NStorage::TCIndirection<CBinaryOperator>
					, NStorage::TCIndirection<CPrefixOperator>
				> m_Param
			;
		};

		struct CFunctionCall
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CFunctionCall fs_FromJson(CStringCache &o_StringCache, NEncoding::CJsonSorted const &_Json, CFilePosition const &_Position, NStr::CStr const &_Type);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CFunctionCall const &_Right) const noexcept;
			bool operator == (CFunctionCall const &_Right) const noexcept;

			NContainer::TCVector<CParam> m_Params;
			CPropertyKey m_PropertyKey;
			bool m_bEmptyPropertyType = false;
			bool m_bPostFunction = false;
		};

		struct CJsonAccessor;

		struct CTernary
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CTernary fs_FromJson(CStringCache &o_StringCache, NEncoding::CJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CTernary const &_Right) const noexcept;
			bool operator == (CTernary const &_Right) const noexcept;

			CParam m_Conditional;
			CParam m_Left;
			CParam m_Right;
		};

		struct CBinaryOperator
		{
			enum EOperator
			{
				EOperator_LessThan
				, EOperator_LessThanEqual
				, EOperator_GreaterThan
				, EOperator_GreaterThanEqual
				, EOperator_Equal
				, EOperator_NotEqual
				, EOperator_MatchEqual
				, EOperator_MatchNotEqual
				, EOperator_Add
				, EOperator_Subtract
				, EOperator_Divide
				, EOperator_Multiply
				, EOperator_Modulus

				, EOperator_BitwiseLeftShift
				, EOperator_BitwiseRightShift
				, EOperator_BitwiseAnd
				, EOperator_BitwiseXor
				, EOperator_BitwiseOr

				, EOperator_And
				, EOperator_Or

				, EOperator_NullishCoalescing
			};

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CBinaryOperator fs_FromJson(CStringCache &o_StringCache, NEncoding::CJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CBinaryOperator const &_Right) const noexcept;
			bool operator == (CBinaryOperator const &_Right) const noexcept;

			EOperator m_Operator = EOperator_LessThan;
			CParam m_Left;
			CParam m_Right;
		};

		struct CPrefixOperator
		{
			enum EOperator
			{
				EOperator_LogicalNot
				, EOperator_BitwiseNot
				, EOperator_UnaryPlus
				, EOperator_UnaryMinus
			};

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CPrefixOperator fs_FromJson(CStringCache &o_StringCache, NEncoding::CJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CPrefixOperator const &_Right) const noexcept;
			bool operator == (CPrefixOperator const &_Right) const noexcept;

			EOperator m_Operator = EOperator_LogicalNot;
			CParam m_Right;
		};

		struct CExpression
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CExpression fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bParen);
			NEncoding::CEJsonSorted f_ToJson(bool _bAppendExpression = false) const;
			NEncoding::CEJsonSorted f_ToJsonRaw() const;
			COrdering_Partial operator <=> (CExpression const &_Right) const noexcept;
			bool operator == (CExpression const &_Right) const noexcept;

			bool f_IsParam() const;
			CParam const &f_Param() const;

			bool f_IsFunctionCall() const;
			CFunctionCall const &f_FunctionCall() const;

			bool f_IsJsonAccessor() const;
			CJsonAccessor const &f_JsonAccessor() const;

			NStorage::TCVariant<CParam, CFunctionCall, NStorage::TCIndirection<CJsonAccessor>> m_Expression;
			bool m_bParen = false;
		};

		struct CExpressionAppend : public CExpression
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CExpressionAppend fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bParen);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CExpressionAppend const &_Right) const noexcept;
			bool operator == (CExpressionAppend const &_Right) const noexcept;
		};

		struct CJsonSubscript
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			COrdering_Partial operator <=> (CJsonSubscript const &_Right) const noexcept;
			bool operator == (CJsonSubscript const &_Right) const noexcept;

			NStorage::TCVariant<uint32, CExpression> m_Index;
		};

		struct CJsonAccessorEntry
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CJsonAccessorEntry fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			static NStorage::TCVariant<NStr::CStr, CExpression, CJsonSubscript> fs_AccessorFromJson
				(
					CStringCache &o_StringCache
					, NEncoding::CEJsonSorted const &_Json
					, CFilePosition const &_Position
				)
			;
			NEncoding::CEJsonSorted f_AccessorToJson() const;
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CJsonAccessorEntry const &_Right) const noexcept;
			bool operator == (CJsonAccessorEntry const &_Right) const noexcept;

			NStorage::TCVariant<NStr::CStr, CExpression, CJsonSubscript> m_Accessor;
			bool m_bOptional = false;
		};

		struct CJsonAccessor
		{
			template <typename tf_CStr>
			static void fs_FormatAccessors(tf_CStr &o_Str, NContainer::TCVector<CJsonAccessorEntry> const &_Accessors);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CJsonAccessor fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CJsonAccessor const &_Right) const noexcept;
			bool operator == (CJsonAccessor const &_Right) const noexcept;

			CParam m_Param;
			NContainer::TCVector<CJsonAccessorEntry> m_Accessors;
		};

		struct CIdentifier
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CIdentifier fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CIdentifier const &_Right) const noexcept;
			bool operator == (CIdentifier const &_Right) const noexcept;

			bool f_IsNameConstantString() const;
			NStr::CStr const &f_NameConstantString() const;
			uint32 f_NameConstantStringHash() const;
			CRootValue f_RootValue() &&;
			CRootKey f_RootKey() &&;
			CPropertyKey f_PropertyKeyConstant() const;
			CPropertyKeyReference f_PropertyKeyReferenceConstant() const;
			CPropertyKeyReference f_PropertyKeyReferenceConstant(EPropertyType _PropertyType) const;
			EPropertyType f_PropertyTypeConstant() const;
			bool f_IsPropertyTypeConstant() const;

			NStorage::TCVariant<CStringAndHash, CEvalString> m_Name;
			EEntityType m_EntityType = EEntityType_Invalid;
			NStorage::TCVariant<EPropertyType, CEvalString> m_PropertyType = EPropertyType_Invalid;
			bool m_bEmptyPropertyType = false;
		};

		struct CIdentifierReference
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			COrdering_Partial operator <=> (CIdentifierReference const &_Right) const noexcept;
			bool operator == (CIdentifierReference const &_Right) const noexcept;
			NEncoding::CEJsonSorted f_ToJson() const;

			CIdentifier m_Identifier;
		};

		struct CType;

		struct CDefaultType
		{
			enum EType
			{
				EType_Any
				, EType_Void
				, EType_String
				, EType_Integer
				, EType_FloatingPoint
				, EType_Boolean
				, EType_Date
				, EType_Binary
				, EType_Identifier
			};

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CDefaultType fs_FromJson(NEncoding::CJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CDefaultType const &_Right) const noexcept;
			bool operator == (CDefaultType const &_Right) const noexcept;

			static CType const ms_Any;
			static CType const ms_Void;
			static CType const ms_String;
			static CType const ms_Integer;
			static CType const ms_FloatingPoint;
			static CType const ms_Boolean;
			static CType const ms_Date;
			static CType const ms_Binary;
			static CType const ms_IdentifierReference;

			EType m_Type = EType_Any;
		};

		struct CUserType
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CUserType fs_FromJson(NEncoding::CJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CUserType const &_Right) const noexcept;
			bool operator == (CUserType const &_Right) const noexcept;

			NStr::CStr m_Name;
		};

		struct CClassType
		{
			struct CMember
			{
				COrdering_Partial operator <=> (CMember const &_Right) const noexcept;
				bool operator == (CMember const &_Right) const noexcept;

				CMember();
				CMember(CMember const &_Other);
				CMember(CMember &&_Other);
				CMember(CType const &_Type, bool _bOptional = false);

				NStorage::TCIndirection<CType> m_Type;
				bool m_bOptional = false;
				DMibListLinkDS_Link(CMember, m_Link);
			};

			CClassType();
			CClassType(CClassType const &_Other);
			CClassType(CClassType &&_Other);
			CClassType(NContainer::TCVector<NStorage::TCTuple<NStr::CStr, CMember>> const &_Members, NStorage::TCOptional<CType> const &_OtherKeysType);

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CClassType fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CClassType const &_Right) const noexcept;
			bool operator == (CClassType const &_Right) const noexcept;

			NContainer::TCMap<NStr::CStr, CMember> m_Members;
			NStorage::TCOptional<NStorage::TCIndirection<CType>> m_OtherKeysType;
			DMibListLinkDS_List(CMember, m_Link) m_MembersSorted;
		};

		struct CArrayType
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CArrayType fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CArrayType const &_Right) const noexcept;
			bool operator == (CArrayType const &_Right) const noexcept;

			NStorage::TCIndirection<CType> m_Type;
		};

		struct COneOf
		{
			using CVariant = NStorage::TCVariant<NEncoding::CEJsonSorted, NStorage::TCIndirection<CType>>;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static COneOf fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (COneOf const &_Right) const noexcept;
			bool operator == (COneOf const &_Right) const noexcept;

			NContainer::TCVector<CVariant> m_OneOf;
		};

		struct CFunctionParameter
		{
			enum EParamType
			{
				EParamType_None
				, EParamType_Optional
				, EParamType_Ellipsis
			};

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CFunctionParameter fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CFunctionParameter const &_Right) const noexcept;
			bool operator == (CFunctionParameter const &_Right) const noexcept;

			NStorage::TCIndirection<CType> m_Type;
			NStr::CStr m_Name;
			EParamType m_ParamType = EParamType_None;
		};

		struct CFunctionType
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CFunctionType fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CFunctionType const &_Right) const noexcept;
			bool operator == (CFunctionType const &_Right) const noexcept;

			NStorage::TCIndirection<CType> m_Return;
			NContainer::TCVector<CFunctionParameter> m_Parameters;
		};

		struct CTypeDefaulted
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CTypeDefaulted fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CTypeDefaulted const &_Right) const noexcept;
			bool operator == (CTypeDefaulted const &_Right) const noexcept;

			NStorage::TCIndirection<CType> m_Type;
			CParam m_DefaultValue;
		};

		struct CType
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CType fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CType const &_Right) const noexcept;
			bool operator == (CType const &_Right) const noexcept;
			bool f_IsAny(NFunction::TCFunctionNoAlloc<CType const *(CType const *_pType)> const &_fGetCanonical = {}) const;
			bool f_IsOptional() const;
			bool f_IsDefaulted() const;
			bool f_IsFunction() const;

			using CVariant = NStorage::TCVariant<CDefaultType, CUserType, CClassType, CArrayType, COneOf, CFunctionType, CTypeDefaulted>;

			CVariant m_Type;
			bool m_bOptional = false;
		};

		struct CDefine
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CDefine fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CDefine const &_Right) const noexcept;
			bool operator == (CDefine const &_Right) const noexcept;

			CType m_Type;
			bool m_bLegacy = false;
		};

		struct COperator
		{
			enum EOperator
			{
				EOperator_LessThan
				, EOperator_LessThanEqual
				, EOperator_GreaterThan
				, EOperator_GreaterThanEqual
				, EOperator_Equal
				, EOperator_NotEqual
				, EOperator_MatchEqual
				, EOperator_MatchNotEqual
				, EOperator_Append
				, EOperator_Prepend
			};

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static COperator fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (COperator const &_Right) const noexcept;
			bool operator == (COperator const &_Right) const noexcept;

			EOperator m_Operator = EOperator_LessThan;
			NStorage::TCIndirection<CValue> m_Right;
		};

		struct CValue
		{
			using CVariant = NStorage::TCVariant<NEncoding::CEJsonSorted, CObject, CArray, CWildcardString, CEvalString, CExpression, CExpressionAppend, COperator, CDefine>;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CValue fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed);
			NEncoding::CEJsonSorted f_ToJson() const;
			static CVariant fs_FromJsonToken
				(
					CStringCache &o_StringCache
					, NEncoding::CEJsonSorted const &_Token
					, NStr::CStr const &_TokenType
					, CFilePosition const &_Position
					, bool _bAppendAllowed
				)
			;
			COrdering_Partial operator <=> (CValue const &_Right) const noexcept;
			bool operator == (CValue const &_Right) const noexcept;

			bool f_IsConstantString() const;
			NStr::CStr const &f_ConstantString() const;

			bool f_IsIdentifier() const;
			CBuildSystemSyntax::CIdentifier const &f_Identifier() const;

			bool f_IsConstant() const;
			NEncoding::CEJsonSorted const &f_Constant() const;
			bool f_IsValid() const;

			bool f_IsJson() const;
			NEncoding::CEJsonSorted const &f_Json() const;

			bool f_IsArray() const;
			CArray const &f_Array() const;

			bool f_IsEvalString() const;
			CEvalString const &f_EvalString() const;

			bool f_IsExpression() const;
			CExpression const &f_Expression() const;

			static CValue fs_Identifier(CStringCache &o_StringCache, NStr::CStr const &_Identifier, EPropertyType _PropertyType = EPropertyType_Property);
			static CValue fs_Identifier(CPropertyKeyReference const &_KeyReference);

			CVariant m_Value;
		};

		struct CRootValue
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CRootValue fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position, bool _bAppendAllowed);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CRootValue const &_Right) const noexcept;
			bool operator == (CRootValue const &_Right) const noexcept;

			CValue m_Value;
			NContainer::TCVector<CJsonAccessorEntry> m_Accessors;
		};

		struct CKeyPrefixOperator
		{
			enum EOperator
			{
				EOperator_Entity
				, EOperator_ConfigurationTuple
				, EOperator_Equal
				, EOperator_NotEqual
				, EOperator_Pragma
			};

			static EOperator fs_TypeFromJson(NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			static CKeyPrefixOperator fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			static CKeyPrefixOperator fs_FromJson(CStringCache &o_StringCache, EOperator _Operator, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			static CKeyPrefixOperator fs_Entity(CStringCache &o_StringCache, NStr::CStr const &_Name);

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			COrdering_Partial operator <=> (CKeyPrefixOperator const &_Right) const noexcept;
			bool operator == (CKeyPrefixOperator const &_Right) const noexcept;

			EOperator m_Operator = EOperator_Equal;
			CValue m_Right;
		};

		struct CNamespace
		{
			static CNamespace fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			COrdering_Partial operator <=> (CNamespace const &_Right) const noexcept;
			bool operator == (CNamespace const &_Right) const noexcept;
		};

		struct CKeyLogicalOperator
		{
			enum EOperator
			{
				EOperator_And
				, EOperator_Or
				, EOperator_Not
			};

			static CKeyLogicalOperator fs_FromJson(NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			COrdering_Partial operator <=> (CKeyLogicalOperator const &_Right) const noexcept;
			bool operator == (CKeyLogicalOperator const &_Right) const noexcept;

			EOperator m_Operator = EOperator_And;
		};

		struct CRootKey
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			static CRootKey fs_FromJson(CStringCache &o_StringCache, NEncoding::CEJsonSorted const &_Json, CFilePosition const &_Position);
			NEncoding::CEJsonSorted f_ToJson() const;
			COrdering_Partial operator <=> (CRootKey const &_Right) const noexcept;
			bool operator == (CRootKey const &_Right) const noexcept;
			bool f_IsValue() const;
			bool f_IsKeyPrefixOperator() const;
			bool f_IsKeyLogicalOperator() const;
			bool f_IsNamespace() const;
			CValue const &f_Value() const;
			CKeyPrefixOperator const &f_KeyPrefixOperator() const;
			CKeyLogicalOperator const &f_KeyLogicalOperator() const;
			bool f_IsValid() const;

			aint f_Cmp(CRootKey const &_Right) const;

			using CVariant = NStorage::TCVariant<CValue, CKeyPrefixOperator, CKeyLogicalOperator, CNamespace>;

			CVariant m_Value;
		};

		static void fs_FormatString(NStr::CStr &o_String, NStr::CStr const &_SourceString);
		static void fs_FormatString(NStr::CStrNonTracked &o_String, NStr::CStr const &_SourceString);
		static void fs_FormatKeyString(NStr::CStr &o_String, NStr::CStr const &_SourceString);
		static void fs_FormatKeyString(NStr::CStrNonTracked &o_String, NStr::CStr const &_SourceString);
	};
}

