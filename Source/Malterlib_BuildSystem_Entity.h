// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	enum EEntityType
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
	};
	
	struct CEntityKey
	{
		inline_always CEntityKey();
		
		inline_always bool operator < (CEntityKey const &_Right) const;
		inline_always bool operator == (CEntityKey const &_Right) const;

		EEntityType m_Type;
		CStr m_Name;
	};
	
	struct CEntity 
#ifdef DMibBuildSystem_DebugReferences
		: public TCSharedPointerIntrusiveBase<>
#endif
	{
		CEntity(CEntity &&_Other);
		CEntity &operator = (CEntity const &_Other);
		CEntity(CEntity *_pParent);
		CEntity(CEntity const &_Other);
#ifdef DMibBuildSystem_DebugReferences
		~CEntity();
#endif
		CEntity &operator = (CEntity &&_Other);
		
#ifdef DMibBuildSystem_DebugReferences
		aint f_RefCountIncrease() const;
#endif
		void f_CheckChildren() const;
		inline_always CEntityKey const &f_GetMapKey() const;
		TCVector<CEntityKey> f_GetPathKey() const;
		inline_always void f_CheckParents() const;
		CEntity const *f_GetRoot() const;
		void fr_GetPath(CStr &_Destination) const;
		void fr_GetPathForGetProperty(CStr &_Destination) const;
		CStr f_GetPath() const;
		CStr f_GetPathForGetProperty() const;
		void f_CopyProperties(CEntity const &_Other);
		void f_ClearReferences();
		void f_CopyEntities(CEntity const &_Other, bool _bDirectCopy = false);
		void f_MergeEntities(CEntity const &_Other);
		void f_SetProperties(CEntity const &_Other);
		void f_SetEntities(CEntity const &_Other);
		void f_CopyFrom(CEntity const &_Other, bool _bCopyChildren, CEntityKey const* _pKey = nullptr, bool _bDirectCopy = false);
		void f_CopyExternal(CEntity const &_Other);
		void f_CopyFromWithCopyFrom(CEntity const &_Other, bool _bCopyChildren);
		CProperty &f_AddProperty(CPropertyKey const &_Key, CStr const &_Value, CFilePosition const &_Position);

	private:
		void fpr_GetPathKey(TCVector<CEntityKey> &_Dest) const;
		void fpr_CheckParents() const;
		
	public:
		DLinkDS_Link(CEntity, m_Link);
		TCMap<CEntityKey, CEntity> m_ChildEntitiesMap;
		DLinkDS_List(CEntity, m_Link) m_ChildEntitiesOrdered;
		CEntity *m_pParent;
		CEntityKey m_Key;
		CCondition m_Condition;
		
		mutable CMutualManyReadSpin m_Lock;
		mutable TCMap<CPropertyKey, CEvaluatedProperty> m_EvaluatedProperties;
		mutable TCMap<CPropertyKey, TCVector<CProperty *>> m_PotentialExplicitProperties;
		mutable TCMap<CPropertyKey, TCVector<CProperty *>> m_PerFilePotentialExplicitProperties;
		TCMap<CPropertyKey, TCLinkedList<CProperty>> m_Properties;
		DMibListLinkDS_List(CProperty, m_LinkEvalOrder) m_PropertiesEvalOrder;
		CFilePosition m_Position;
		CEntityPointer m_pCopiedFrom;
		CEntityPointer m_pCopiedFromEvaluated;
		
		zbool m_bEvaluated;
#if defined(DMibBuildSystem_DebugReferences) && defined(DMibBuildSystem_DebugReferencesAdvanced)
		static CMutual mp_DebugSetLock;
		static TCMap<CEntity const *, TCLinkedList<CCallstack>> mp_DebugSet;
#endif
	};
	
	EEntityType fg_EntityTypeFromStr(CStr const &_String);
	CStr fg_EntityTypeToStr(EEntityType _Type);
}
