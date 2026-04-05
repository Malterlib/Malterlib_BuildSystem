// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_BuildSystem_Syntax.h"

namespace NMib::NBuildSystem
{
	struct CEntityKey
	{
		inline_always auto operator <=> (CEntityKey const &_Right) const noexcept = default;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		NStr::CStr const &f_GetName(CFilePosition const &_Position) const;

		EEntityType m_Type = EEntityType_Root;
		CBuildSystemSyntax::CValue m_Name;
	};

	enum EEntityCopyFlag
	{
		EEntityCopyFlag_None = 0
		, EEntityCopyFlag_CopyChildren = DMibBit(0)
		, EEntityCopyFlag_CopyExternal = DMibBit(1)
		, EEntityCopyFlag_MergeEntities = DMibBit(2)
		, EEntityCopyFlag_ClearExistingChildren = DMibBit(3)
		, EEntityCopyFlag_NoCheckTypes = DMibBit(4)
	};

	struct CTypeWithPosition
	{
		CBuildSystemSyntax::CType m_Type;
		CFilePosition m_Position;
		NStr::CStr m_Whitespace;
	};

	#define DMibBuildSystemTypeWithPosition(...) CTypeWithPosition{.m_Type = CBuildSystemSyntax::CType{__VA_ARGS__}, .m_Position = CFilePosition(NStr::CParseLocation{.m_File = DMibPFile, .m_Line = DMibPLine})}

	struct CTypeWithConditions
	{
		CTypeWithPosition m_Type;
		NStorage::TCSharedPointer<CCondition> m_pConditions;
		EPropertyFlag m_DebugFlags = EPropertyFlag_None;
	};

	struct CEntityData
	{
		using CPropertyContainer = NContainer::TCLinkedList<CProperty>;
		//using CPropertyContainer = NContainer::TCVector<CProperty, NMemory::CAllocator_Heap, NContainer::TCVectorOptions<1, true, true>>;

		CEntityData();
		CEntityData(CEntityData const &_Other);

		NStorage::CIntrusiveRefCount m_RefCount;

		CCondition m_Condition;

		NContainer::TCMap<CPropertyKey, NContainer::TCLinkedList<CTypeWithConditions>> m_VariableDefinitions;
		NContainer::TCMap<NStr::CStr, NContainer::TCLinkedList<CTypeWithConditions>> m_UserTypes;
		NContainer::TCMap<CPropertyKey, CPropertyContainer> m_Properties;

		umint m_ExpandedOrGeneratedFrom = 0;
		NContainer::TCSet<umint> m_ExpandedOrGeneratedFromSet;

		CFilePosition m_Position;
		EPropertyFlag m_DebugFlags = EPropertyFlag_None;
		uint32 m_HasFullEval = 0;
	};

	struct CEntityChildDependantData
	{
		CEntityChildDependantData();
		CEntityChildDependantData(CEntityChildDependantData const &_Other);

		NStorage::CIntrusiveRefCount m_RefCount;

		NContainer::TCMap<CPropertyKey, NContainer::TCSet<CFilePosition>> m_ChildrenVariableDefinitions;
		NContainer::TCMap<NStr::CStr, NContainer::TCSet<CFilePosition>> m_ChildrenUserTypes;
	};

	struct CEntity
	{
		CEntity(CEntity const &_Other) = delete;
		CEntity(CEntity &&_Other);
		CEntity(CEntity *_pParent);
		CEntity(CEntity const &_Other, CEntity *_pParent, EEntityCopyFlag _CopyFlags);
#ifdef DMibBuildSystem_DebugReferences
		~CEntity();
#endif

		umint f_ExpandedOrGeneratedFromSource() const;
		void f_Assign(CEntity const &_Other);
		void f_CheckChildren() const;
		NContainer::TCVector<CEntityKey> f_GetPathKey() const;
		inline_always void f_CheckParents() const;
		CEntity const *f_GetRoot() const;
		void fr_GetPath(NStr::CStr &_Destination) const;
		void fr_GetPathForGetProperty(NStr::CStr &_Destination) const;
		NStr::CStr f_GetPath() const;
		NStr::CStr f_GetPathForGetProperty() const;
		void f_CopyTypes(CEntity const &_Other);
		void f_CheckTypes(CEntity const &_Other);
		void f_CopyProperties(CEntity const &_Other);
		void f_CopyProperties(CEntity &&_Other);
		void f_ClearReferences();
		void f_CopyEntities(CEntity const &_Other, EEntityCopyFlag _Flags);
		void f_CopyExternal(CEntity const &_Other);
		void f_CopyAll(CEntity const &_Other, bool _bCopyChildren);
		CProperty &f_AddProperty(CPropertyKeyReference const &_Key, CBuildSystemSyntax::CRootValue const &_Value, CFilePosition const &_Position);
		void f_ForEachChild(NFunction::TCFunction<void (CEntity *_pChild)> const &_fChild);
		NStr::CStr const &f_GetKeyName() const;
		CFilePosition const &f_GetFirstValidPosition() const;
		bool f_HasFullEval(EPropertyType _PropertyType)const ;
		bool f_HasParent(CEntity const *_pEntity) const;
		bool f_HasOnlyGroups() const;

		CEntityData const &f_Data() const;
		CEntityData &f_DataWritable();

		CEntityChildDependantData const &f_ChildDependentData() const;
		CEntityChildDependantData &f_ChildDependentDataWritable();

		CEntityKey const &f_GetKey() const;

#ifdef DMibBuildSystem_DebugReferences
		NStorage::CIntrusiveRefCount m_RefCount;
#endif
		NStorage::TCSharedPointer<CEntityData> m_pData;
		NStorage::TCSharedPointer<CEntityChildDependantData> m_pChildDependentData;

		DMibListLinkDS_Link(CEntity, m_Link);
		NContainer::TCMap<CEntityKey, CEntity> m_ChildEntitiesMap;
		DMibListLinkDS_List(CEntity, m_Link) m_ChildEntitiesOrdered;
		CEntity *m_pParent;

		CEvaluatedProperties m_EvaluatedProperties;

#if defined(DMibBuildSystem_DebugReferences)
		DIfRefCountDebugging(NStorage::CRefCountDebugReference m_DebugSelfRef);
#endif

	private:
		void fpr_GetPathKey(NContainer::TCVector<CEntityKey> &_Dest) const;
		void fpr_CheckParents() const;

#if defined(DMibBuildSystem_DebugReferences) && defined(DMibBuildSystem_DebugReferencesAdvanced)
		static NThread::CMutual mp_DebugSetLock;
		static NContainer::TCSet<CEntity const *> mp_DebugSet;
#endif
	};
}
