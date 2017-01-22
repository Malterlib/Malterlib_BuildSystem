// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Data.h"

namespace NMib::NBuildSystem
{
	CEntity::CEntity(CEntity &&_Other)
		: m_ChildEntitiesMap(fg_Move(_Other.m_ChildEntitiesMap))
		, m_ChildEntitiesOrdered(fg_Move(_Other.m_ChildEntitiesOrdered))
		, m_Key(fg_Move(_Other.m_Key))
		, m_Condition(fg_Move(_Other.m_Condition))
		, m_Properties(fg_Move(_Other.m_Properties))
		, m_EvaluatedProperties(fg_Move(_Other.m_EvaluatedProperties))
		, m_PotentialExplicitProperties(fg_Move(_Other.m_PotentialExplicitProperties))
		, m_PerFilePotentialExplicitProperties(fg_Move(_Other.m_PerFilePotentialExplicitProperties))
		, m_PropertiesEvalOrder(fg_Move(_Other.m_PropertiesEvalOrder))
		, m_pParent(nullptr)
		, m_Position(fg_Move(_Other.m_Position))
	{
#ifdef DMibBuildSystem_DebugReferences
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		{
			DLock(mp_DebugSetLock);
			mp_DebugSet[this];
		}
#endif
		this->f_RefCountIncrease();
#endif
		for (auto iChild = m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
		{
			iChild->m_pParent = this;
		}
	}

	CEntity::CEntity(CEntity const &_Other)
		: m_Key(_Other.m_Key)
		, m_Condition(_Other.m_Condition)
		, m_Position(_Other.m_Position)
	{
#ifdef DMibBuildSystem_DebugReferences
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		{
			DLock(mp_DebugSetLock);
			mp_DebugSet[this];
		}
#endif
		this->f_RefCountIncrease();
#endif
		f_SetProperties(_Other);
		f_SetEntities(_Other);
		if (_Other.m_pCopiedFrom)
			m_pCopiedFrom = _Other.m_pCopiedFrom;
		else
			m_pCopiedFrom = fg_Explicit(&_Other);
		m_pCopiedFromEvaluated = fg_Explicit(&_Other);
	}
	
#ifdef DMibBuildSystem_DebugReferences
	CEntity::~CEntity()
	{
		bool bDoneSomething = true;
		while (bDoneSomething)
		{
			bDoneSomething = false;
			for (auto iChild = m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
			{
				auto &Child = *iChild;
				++iChild;
				if (Child.f_RefCountGet() == 1)
				{
					m_ChildEntitiesMap.f_Remove(&Child);
					bDoneSomething = true;
				}
			}
		}
		m_ChildEntitiesMap.f_Clear();
		mint RefCount = this->f_RefCountDecrease();
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		if (RefCount > 1)
		{
			DLock(mp_DebugSetLock);
			CStr ThisPath = f_GetPath();
			DTrace("{}\n\n", ThisPath);
			for (auto iEntity = mp_DebugSet.f_GetIterator(); iEntity; ++iEntity)
			{
				auto pEntity = iEntity.f_GetKey();
				auto pCallStack = *iEntity;
				if (pEntity->m_pCopiedFromEvaluated == this || pEntity->m_pCopiedFrom == this)
				{
					CStr Path = pEntity->f_GetPath();
					DTrace("{}\n", Path);
				}
			}
			auto pThis = mp_DebugSet.f_FindEqual(this);
			(void)pThis;
			DMibPDebugBreak;
		}
		{
			DLock(mp_DebugSetLock);
			mp_DebugSet.f_Remove(this);
		}
#endif
		DCheck(RefCount == 1)(RefCount)(f_GetPath());
		if (RefCount != 1)
			DMibPDebugBreak;
	}
#endif

	CEntity &CEntity::operator = (CEntity &&_Other)
	{
		m_ChildEntitiesMap = fg_Move(_Other.m_ChildEntitiesMap);
		m_ChildEntitiesOrdered = fg_Move(_Other.m_ChildEntitiesOrdered);
		m_Key = fg_Move(_Other.m_Key);
		m_Condition = fg_Move(_Other.m_Condition);
		m_Properties = fg_Move(_Other.m_Properties);
		m_EvaluatedProperties = fg_Move(_Other.m_EvaluatedProperties);
		m_PotentialExplicitProperties = fg_Move(_Other.m_PotentialExplicitProperties);
		m_PerFilePotentialExplicitProperties = fg_Move(_Other.m_PerFilePotentialExplicitProperties);
		m_PropertiesEvalOrder = fg_Move(_Other.m_PropertiesEvalOrder);
		m_Position = _Other.m_Position;

		for (auto iChild = m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
			iChild->m_pParent = this;

		return *this;
	}
	
	void CEntity::fpr_GetPathKey(TCVector<CEntityKey> &_Dest) const
	{
		if (m_pParent && m_pParent->m_Key.m_Type != EEntityType_Root)
			m_pParent->fpr_GetPathKey(_Dest);
		_Dest.f_Insert(m_Key);
	}

	TCVector<CEntityKey> CEntity::f_GetPathKey() const
	{
		TCVector<CEntityKey> Ret;
		fpr_GetPathKey(Ret);
		return Ret;
	}

	CEntity const *CEntity::f_GetRoot() const
	{
		CEntity const *pRet = this;
		while (pRet)
		{
			if (pRet->m_Key.m_Type == EEntityType_Root)
				break;
			pRet = pRet->m_pParent;
		}
		return pRet;
	}

	void CEntity::fr_GetPath(CStr &_Destination) const
	{
		if (m_pParent)
			m_pParent->fr_GetPath(_Destination);

		if (!_Destination.f_IsEmpty())
			_Destination += "->";
		_Destination += fg_EntityTypeToStr(m_Key.m_Type);
		_Destination += ":";
		_Destination += m_Key.m_Name;
	}

	void CEntity::fr_GetPathForGetProperty(CStr &_Destination) const
	{
		if (m_Key.m_Type == EEntityType_Root)
			return;
		
		if (m_pParent)
			m_pParent->fr_GetPathForGetProperty(_Destination);

		if (!_Destination.f_IsEmpty())
			_Destination += ".";
		if (m_Key.m_Name.f_FindChars(".\"") >= 0)
		{
			CStr ToEspace = fg_EntityTypeToStr(m_Key.m_Type);
			ToEspace += ":";
			ToEspace += m_Key.m_Name;
			_Destination += ToEspace.f_EscapeStr();
		}
		else
		{
			_Destination += fg_EntityTypeToStr(m_Key.m_Type);
			_Destination += ":";
			_Destination += m_Key.m_Name;
		}
	}
	
	CStr CEntity::f_GetPath() const
	{
		CStr Ret;
		fr_GetPath(Ret);

		return Ret;
	}

	CStr CEntity::f_GetPathForGetProperty() const
	{
		CStr Ret;
		fr_GetPathForGetProperty(Ret);

		return Ret;
	}
	
	void CEntity::f_CopyProperties(CEntity const &_Other)
	{
		if (_Other.m_PropertiesEvalOrder.f_IsEmpty())
			return;
		auto iProp = _Other.m_PropertiesEvalOrder.f_GetIterator();
		m_Properties.f_BatchMapIfNotMapped
			(
				[&](TCMap<CPropertyKey, TCLinkedList<CProperty>>::CConditionalMapper & _Mapper) -> bool
				{
					auto Mapped = _Mapper(iProp->m_Key);
					auto & NewProp = (*Mapped).f_Insert();
					NewProp = *iProp;
					m_PropertiesEvalOrder.f_Insert(NewProp);
					++iProp;
					return iProp;
				}
			)
		;
	}
	
	void CEntity::f_CopyPropertiesAndEval(CEntity const &_Other)
	{
		if (_Other.m_PropertiesEvalOrder.f_IsEmpty())
			return;
		auto iProp = _Other.m_PropertiesEvalOrder.f_GetIterator();
		m_Properties.f_BatchMapIfNotMapped
			(
				[&](TCMap<CPropertyKey, TCLinkedList<CProperty>>::CConditionalMapper & _Mapper) -> bool
				{
					auto Mapped = _Mapper(iProp->m_Key);
					auto & NewProp = (*Mapped).f_Insert();
					NewProp = *iProp;
					m_PropertiesEvalOrder.f_Insert(NewProp);
					m_PotentialExplicitProperties[iProp->m_Key].f_Insert(&NewProp);
					
					if (NewProp.m_Condition.f_NeedPerFile())
						m_PerFilePotentialExplicitProperties[iProp->m_Key].f_Insert(&NewProp);
					
					++iProp;
					return iProp;
				}
			)
		;
	}

	void CEntity::f_ClearReferences()
	{
		m_pCopiedFrom = nullptr;
		m_pCopiedFromEvaluated = nullptr;
		for (auto &Child : m_ChildEntitiesOrdered)
		{
			Child.f_ClearReferences();
		}
	}
	
	void CEntity::f_MergeEntities(CEntity const &_Other)
	{
		if (_Other.m_ChildEntitiesOrdered.f_IsEmpty())
			return;						
		auto iChild = _Other.m_ChildEntitiesOrdered.f_GetIterator();
		m_ChildEntitiesMap.f_BatchMapIfNotMapped
			(
				[&](TCMap<CEntityKey, CEntity>::CConditionalMapper & _Mapper) -> bool
				{
					auto Mapped = _Mapper(iChild->f_GetMapKey(), this);
					auto &NewChild = *Mapped;
					if (Mapped.f_WasCreated())
					{
						m_ChildEntitiesOrdered.f_Insert(NewChild);
						NewChild.f_CopyFrom(*iChild, true, nullptr, false);						
					}
					else
					{
						NewChild.m_Condition.m_Children.f_Insert(_Other.m_Condition.m_Children);
						NewChild.f_CopyProperties(*iChild);
						NewChild.f_MergeEntities(*iChild);
					}
					++iChild;
					return iChild;
				}
			)
		;
	}

	void CEntity::f_CopyEntities(CEntity const &_Other, bool _bDirectCopy)
	{
		if (_Other.m_ChildEntitiesOrdered.f_IsEmpty())
			return;						
		auto iChild = _Other.m_ChildEntitiesOrdered.f_GetIterator();
		m_ChildEntitiesMap.f_BatchMapIfNotMapped
			(
				[&](TCMap<CEntityKey, CEntity>::CConditionalMapper & _Mapper) -> bool
				{
					auto Mapped = _Mapper(iChild->f_GetMapKey(), this);
					auto &NewChild = *Mapped;
					m_ChildEntitiesOrdered.f_Insert(NewChild);
					NewChild.f_CopyFrom(*iChild, true, nullptr, _bDirectCopy);
					++iChild;
					return iChild;
				}
			)
		;
	}

	void CEntity::f_SetProperties(CEntity const &_Other)
	{
		DRequire(m_Properties.f_IsEmpty());
		if (_Other.m_PropertiesEvalOrder.f_IsEmpty())
			return;
		auto iProp = _Other.m_PropertiesEvalOrder.f_GetIterator();
		m_Properties.f_BatchMapIfNotMapped
			(
				[&](TCMap<CPropertyKey, TCLinkedList<CProperty>>::CConditionalMapper & _Mapper) -> bool
				{
					auto Mapped = _Mapper(iProp->m_Key);
					auto & NewProp = (*Mapped).f_Insert();
					NewProp = *iProp;
					m_PropertiesEvalOrder.f_Insert(NewProp);
					++iProp;
					return iProp;
				}
			)
		;
	}
	
	void CEntity::f_SetEntities(CEntity const &_Other)
	{
		DRequire(m_ChildEntitiesMap.f_IsEmpty());
		if (_Other.m_ChildEntitiesOrdered.f_IsEmpty())
			return;						
		auto iChild = _Other.m_ChildEntitiesOrdered.f_GetIterator();
		m_ChildEntitiesMap.f_BatchMap
			(
				[&](TCMap<CEntityKey, CEntity>::CMapper & _Mapper) -> bool
				{
					auto &NewChild = _Mapper(iChild->f_GetMapKey(), this);
					m_ChildEntitiesOrdered.f_Insert(NewChild);
					NewChild = *iChild;
					++iChild;
					return iChild;
				}
			)
		;
	}

	void CEntity::f_CopyFrom(CEntity const &_Other, bool _bCopyChildren, CEntityKey const* _pKey, bool _bDirectCopy)
	{
		if (_pKey)
			m_Key = *_pKey;
		else
			m_Key = _Other.m_Key;
		m_Condition = _Other.m_Condition;
		m_Position = _Other.m_Position;
		if (_bDirectCopy)
		{
			m_pCopiedFrom = _Other.m_pCopiedFrom;
			m_pCopiedFromEvaluated = _Other.m_pCopiedFromEvaluated;
		}
		else
		{
			if (_Other.m_pCopiedFrom)
				m_pCopiedFrom = _Other.m_pCopiedFrom;
			else
				m_pCopiedFrom = fg_Explicit(&_Other);
			m_pCopiedFromEvaluated = fg_Explicit(&_Other);
		}

		f_CopyProperties(_Other);
		
		if (_bCopyChildren)
			f_CopyEntities(_Other, _bDirectCopy);
	}
	
	void CEntity::f_CopyExternal(CEntity const &_Other)
	{
		for (auto iEvaluated = _Other.m_EvaluatedProperties.f_GetIterator(); iEvaluated; ++iEvaluated)
		{
			if (iEvaluated->m_Type == EEvaluatedPropertyType_External)
			{
				m_EvaluatedProperties(iEvaluated.f_GetKey(), *iEvaluated);
			}
		}
	}

	void CEntity::f_CopyFromWithCopyFrom(CEntity const &_Other, bool _bCopyChildren)
	{
		if (_Other.m_pCopiedFrom)
		{
			auto &Source = *_Other.m_pCopiedFrom;
			m_Key = Source.m_Key;
			m_Condition = Source.m_Condition;
			m_Position = Source.m_Position;
			if (Source.m_pCopiedFrom)
				m_pCopiedFrom = _Other.m_pCopiedFrom;
			else
				m_pCopiedFrom = fg_Explicit(&Source);
			f_CopyProperties(_Other);
		}
		else
			f_CopyFrom(_Other, false);

		f_CopyExternal(_Other);
		m_pCopiedFromEvaluated = fg_Explicit(&_Other);

		if (_bCopyChildren)
		{
			m_ChildEntitiesMap.f_Clear();
			
			if (!_Other.m_ChildEntitiesOrdered.f_IsEmpty())
			{
				auto iChild = _Other.m_ChildEntitiesOrdered.f_GetIterator();
				m_ChildEntitiesMap.f_BatchMap
					(
						[&](TCMap<CEntityKey, CEntity>::CMapper & _Mapper) -> bool
						{
							auto &NewChild = _Mapper(iChild->f_GetMapKey(), this);
							m_ChildEntitiesOrdered.f_Insert(NewChild);
							NewChild.f_CopyFromWithCopyFrom(*iChild, _bCopyChildren);
							++iChild;
							return iChild;
						}
					)
				;
			}
		}
	}

	CEntity &CEntity::operator = (CEntity const &_Other)
	{
		f_CopyFrom(_Other, true);
		return *this;
	}

	CEntity::CEntity(CEntity *_pParent)
		: m_pParent(_pParent)
	{
#ifdef DMibBuildSystem_DebugReferences
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		{
			DLock(mp_DebugSetLock);
			mp_DebugSet[this];
		}
#endif
		this->f_RefCountIncrease();
#endif
	}

	CProperty &CEntity::f_AddProperty(CPropertyKey const &_Key, CStr const &_Value, CFilePosition const &_Position)
	{
		CProperty Property;
		Property.m_Key = _Key;
		Property.m_Value = _Value;
		Property.m_Position = _Position;

		auto &NewProperty = m_Properties[_Key].f_Insert(Property);
		NewProperty.m_Key = _Key;
		NewProperty.m_Value = _Value;
		NewProperty.m_Position = _Position;
		m_PropertiesEvalOrder.f_Insert(NewProperty);

		return NewProperty;
	}
	
#ifdef DMibBuildSystem_DebugReferences
#ifdef DMibBuildSystem_DebugReferencesAdvanced
	CMutual CEntity::mp_DebugSetLock;
	TCMap<CEntity const *, TCLinkedList<CCallstack>> CEntity::mp_DebugSet;
#endif

	aint CEntity::f_RefCountIncrease() const
	{
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		{
			DLock(mp_DebugSetLock);
			auto &DebugSet = mp_DebugSet[this].f_Insert();
			DebugSet.m_CallstackLen = NSys::fg_System_GetStackTrace(DebugSet.m_Callstack, 128);
		}
#endif
		return TCSharedPointerIntrusiveBase<>::f_RefCountIncrease();
	}
#endif
	
	void CEntity::f_CheckChildren() const
	{
		{
			TCSet<CEntity const *> OrderedEntities;
			for (auto iChild = m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
			{
				auto pChild = &*iChild;
				OrderedEntities[pChild];
				if (pChild == this)
				{
					DConOut("pChild == this" DNewLine, 0);
					DMibPDebugBreak;
				}
				if (!m_ChildEntitiesMap.f_FindEqual(pChild->f_GetMapKey()))
				{
					DConOut("!m_ChildEntitiesMap.f_FindEqual(pChild->f_GetMapKey())" DNewLine, 0);
					DMibPDebugBreak;
				}
			}
			TCSet<CEntity const *> MappedEntities;
			for (auto iChild = m_ChildEntitiesMap.f_GetIterator(); iChild; ++iChild)
			{
				MappedEntities[&*iChild];
				if (!OrderedEntities.f_FindEqual(&*iChild))
				{
					DConOut("!OrderedEntities.f_FindEqual(&*iChild)" DNewLine, 0);
					DMibPDebugBreak;
				}
			}
			if (MappedEntities != OrderedEntities)
			{
				DConOut("MappedEntities != OrderedEntities" DNewLine, 0);
				DMibPDebugBreak;
			}
		}
		
		f_CheckParents();
		
		for (auto iChild = m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
			iChild->f_CheckChildren();
	}
	
	void CEntity::fpr_CheckParents() const
	{
		for (auto iChild = m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
		{
			auto pChild = iChild.f_GetCurrent();
			if (pChild->m_pParent != this)
			{
				DConOut("pChild->m_pParent != this" DNewLine, 0);
				DMibPDebugBreak;
			}
			pChild->fpr_CheckParents();
		}
	}
	
	EEntityType fg_EntityTypeFromStr(CStr const &_String)
	{
		if (_String == "Root") return EEntityType_Root;
		else if (_String == "Target") return EEntityType_Target;
		else if (_String == "Group") return EEntityType_Group;
		else if (_String == "Workspace") return EEntityType_Workspace;
		else if (_String == "File") return EEntityType_File;
		else if (_String == "Dependency") return EEntityType_Dependency;
		else if (_String == "GeneratorSetting") return EEntityType_GeneratorSetting;
		else if (_String == "GenerateFile") return EEntityType_GenerateFile;
		else if (_String == "Import") return EEntityType_Import;
		else if (_String == "Repository") return EEntityType_Repository;
		else return EEntityType_Invalid;
	}

	CStr fg_EntityTypeToStr(EEntityType _Type)
	{
		switch (_Type)
		{
		case EEntityType_Root: return "Root";
		case EEntityType_Target: return "Target";
		case EEntityType_Group: return "Group";
		case EEntityType_Workspace: return "Workspace";
		case EEntityType_File: return "File";
		case EEntityType_Dependency: return "Dependency";
		case EEntityType_GeneratorSetting: return "GeneratorSetting";
		case EEntityType_GenerateFile: return "GenerateFile";
		case EEntityType_Import: return "Import";
		case EEntityType_Repository: return "Repository";
		default: DMibNeverGetHere; return CStr();
		}
	}
}
