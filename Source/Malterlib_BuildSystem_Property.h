// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem_Condition.h"

namespace NMib::NBuildSystem
{
	enum EPropertyFlag : uint32
	{
		EPropertyFlag_None = 0
		, EPropertyFlag_TraceEval = DMibBit(0)
		, EPropertyFlag_TraceEvalSuccess = DMibBit(1)
		, EPropertyFlag_TraceCondition = DMibBit(2)
		, EPropertyFlag_NeedPerFile = DMibBit(3)
	};

	struct CPropertyKey;
	struct CAssertAddedToStringCache;
	struct CStringAndHash;

	struct CPropertyKeyReference
	{
		CPropertyKeyReference(CAssertAddedToStringCache _Dummy, EPropertyType _Type, NStr::CStr const &_Name, uint32 _NameHash);

		template <mint t_nChars>
		constexpr CPropertyKeyReference(EPropertyType _Type, NStr::TCStrConstDataAndStr<t_nChars> const &_Name);
		constexpr CPropertyKeyReference(EPropertyType _Type, CStringAndHash const &_Name);
		
		inline_always COrdering_Strong operator <=> (CPropertyKey const &_Right) const;
		inline_always COrdering_Strong operator <=> (CPropertyKeyReference const &_Right) const;

		inline_always bool operator == (CPropertyKey const &_Right) const;
		inline_always bool operator == (CPropertyKeyReference const &_Right) const;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		constexpr inline_always EPropertyType f_GetType() const;
		constexpr inline_always uint32 f_GetNameHash() const;

		CPropertyKeyTypeAndHash m_TypeAndHash;
		NStr::CStr const &m_Name;

	private:
		friend struct CPropertyKey;
		CPropertyKeyReference(CPropertyKeyTypeAndHash const &_TypeAndHash, NStr::CStr const &_Name);
	};

	struct CProperty
	{
		CProperty();
		~CProperty();

		CProperty(CProperty const &_Other);
		CProperty &operator = (CProperty const &_Other);

		constexpr inline_always bool f_NeedPerFile() const;

		NStorage::TCSharedPointer<CCondition> m_pCondition;
		CBuildSystemSyntax::CRootValue m_Value;
		CFilePosition m_Position;
		CBuildSystemRegistry const *m_pRegistry = nullptr;
		EPropertyFlag m_Flags = EPropertyFlag_None;
	};

	enum EEvaluatedPropertyType
	{
		EEvaluatedPropertyType_Implicit
		, EEvaluatedPropertyType_Explicit
		, EEvaluatedPropertyType_External
		, EEvaluatedPropertyType_ExternalEnvironment
	};

	struct CEvaluatedProperty
	{
		inline_always CEvaluatedProperty();
		bool f_IsExternal() const;
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("{}") << m_Value;
		}

		NEncoding::CEJSONSorted m_Value;
		EEvaluatedPropertyType m_Type;
		NStorage::TCPointer<CProperty const> m_pProperty;
		NStorage::TCSharedPointer<CBuildSystemUniquePositions> m_pPositions;
	};

	struct CEvaluatedProperties
	{
		NContainer::TCMap<CPropertyKey, CEvaluatedProperty> m_Properties;
		CEvaluatedProperties *m_pParentProperties = nullptr;
	};
}

#include "Malterlib_BuildSystem_ConstantKeys.h"
