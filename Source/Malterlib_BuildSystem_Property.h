// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	struct CPropertyKey
	{
		inline_always CPropertyKey();
		inline_always CPropertyKey(NStr::CStr const &_Name);
		inline_always CPropertyKey(EPropertyType _Type, NStr::CStr const &_Name);
		inline_always bool operator < (CPropertyKey const &_Right) const;
		static CPropertyKey fs_FromString(NStr::CStr const &_String, CFilePosition const &_Position);

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		EPropertyType m_Type;
		NStr::CStr m_Name;
	};

	struct CProperty
	{
		CProperty();
		~CProperty();

		inline_always EPropertyType f_GetType() const;
		inline_always NStr::CStr const &f_GetName() const;

		CProperty(CProperty const &_Other);
		CProperty &operator = (CProperty const &_Other);

		CPropertyKey m_Key;
		CCondition m_Condition;
		CBuildSystemSyntax::CRootValue m_Value;
		CFilePosition m_Position;
		CBuildSystemRegistry const *m_pRegistry = nullptr;
		NStr::CStr m_Debug;
		bool m_bNeedPerFile = false;
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

		NEncoding::CEJSON m_Value;
		EEvaluatedPropertyType m_Type;
		NStorage::TCPointer<CProperty const> m_pProperty;
	};

	struct CEvaluatedProperties
	{
		NContainer::TCMap<CPropertyKey, CEvaluatedProperty> m_Properties;
		CEvaluatedProperties *m_pParentProperties = nullptr;
	};
}

#include "Malterlib_BuildSystem_Property.hpp"
