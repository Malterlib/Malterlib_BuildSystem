// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Data.h"

namespace NMib::NBuildSystem
{
	CStr const &CEntityKey::f_GetName(CFilePosition const &_Position) const
	{
		if (!m_Name.f_IsConstantString())
			CBuildSystem::fs_ThrowError(_Position, "Entity should be a string");

		return m_Name.f_ConstantString();
	}

	CEntity::CEntity(CEntity &&_Other)
		: m_pData(fg_Move(_Other.m_pData))
		, m_pChildDependentData(fg_Move(_Other.m_pChildDependentData))
		, m_Link(fg_Move(_Other.m_Link))
		, m_ChildEntitiesMap(fg_Move(_Other.m_ChildEntitiesMap))
		, m_ChildEntitiesOrdered(fg_Move(_Other.m_ChildEntitiesOrdered))
		, m_pParent(fg_Exchange(_Other.m_pParent, nullptr))
		, m_EvaluatedProperties(fg_Move(_Other.m_EvaluatedProperties))
	{
#ifdef DMibBuildSystem_DebugReferences
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		{
			DLock(mp_DebugSetLock);
			mp_DebugSet[this];
		}
#endif
		this->m_RefCount.f_Increase(DMibRefCountDebuggingOnly(m_DebugSelfRef));
#endif
	}

	CEntity::CEntity(CEntity *_pParent)
		: m_pParent(_pParent)
		, m_pData(fg_Construct())
		, m_pChildDependentData(fg_Construct())
	{
		if (_pParent)
			_pParent->m_ChildEntitiesOrdered.f_Insert(*this);

#ifdef DMibBuildSystem_DebugReferences
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		{
			DLock(mp_DebugSetLock);
			mp_DebugSet[this];
		}
#endif
		this->m_RefCount.f_Increase(DMibRefCountDebuggingOnly(m_DebugSelfRef));
#endif
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
				if (Child.m_RefCount.f_Get() == 1)
				{
					m_ChildEntitiesMap.f_Remove(&Child);
					bDoneSomething = true;
				}
			}
		}
		m_ChildEntitiesMap.f_Clear();
		mint RefCount = this->m_RefCount.f_Decrease(DMibRefCountDebuggingOnly(m_DebugSelfRef));
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		if (RefCount > 1)
		{
			DLock(mp_DebugSetLock);
			CStr ThisPath = f_GetPath();
			DTrace("{}\n\n", ThisPath);
			auto pThis = mp_DebugSet.f_FindEqual(this);
			(void)pThis;
			DMibPDebugBreak;
		}
		{
			DLock(mp_DebugSetLock);
			mp_DebugSet.f_Remove(this);
		}
#endif
#if DMibConfig_RefCountDebugging
		if (RefCount != 1)
		{
			DMibLock(this->m_Debug->m_Lock);
			mint iCallstack = 0;
			for (auto &Callstack : this->m_Debug->m_Callstacks)
			{
				DMibTrace2("        Reference callstack {}\n", iCallstack);
				Callstack.f_Trace(12);
				++iCallstack;
			}
		}
#endif
		DCheck(RefCount == 1)(RefCount)(f_GetPath());
		if (RefCount != 1)
			DMibPDebugBreak;
	}
#endif

	CEntity::CEntity(CEntity const &_Other, CEntity *_pParent, EEntityCopyFlag _CopyFlags)
		: m_pData(_Other.m_pData)
		, m_pChildDependentData(_Other.m_pChildDependentData)
		, m_pParent(_pParent)
	{
		DMibCheck(!m_pParent || this->f_GetKey().m_Name.f_IsValid());

		if (_pParent)
			_pParent->m_ChildEntitiesOrdered.f_Insert(*this);

#ifdef DMibBuildSystem_DebugReferences
#ifdef DMibBuildSystem_DebugReferencesAdvanced
		{
			DLock(mp_DebugSetLock);
			mp_DebugSet[this];
		}
#endif
		this->m_RefCount.f_Increase(DMibRefCountDebuggingOnly(m_DebugSelfRef));
#endif

		if (!(_CopyFlags & EEntityCopyFlag_NoCheckTypes))
			f_CheckTypes(_Other);

		if (_CopyFlags & EEntityCopyFlag_CopyExternal)
			f_CopyExternal(_Other);

		if (_CopyFlags & EEntityCopyFlag_CopyChildren)
			f_CopyEntities(_Other, _CopyFlags);
	}

	namespace
	{
		CEntityKey g_RootKey;
	}

	CEntityKey const &CEntity::f_GetKey() const
	{
		if (m_pParent)
			return NContainer::TCMap<CEntityKey, CEntity>::fs_GetKey(*this);

		return g_RootKey;
	}

	void CEntity::f_CopyAll(CEntity const &_Other, bool _bCopyChildren)
	{
		m_pData = _Other.m_pData;
		m_pChildDependentData = _Other.m_pChildDependentData;

		f_CheckTypes(_Other);
		f_CopyExternal(_Other);

		if (_bCopyChildren)
			f_CopyEntities(_Other, EEntityCopyFlag_ClearExistingChildren | EEntityCopyFlag_CopyChildren | EEntityCopyFlag_CopyExternal);
	}

	void CEntity::f_Assign(CEntity const &_Other)
	{
		DMibRequire(m_pParent == nullptr); // Only roots

		m_pData = _Other.m_pData;
		m_pChildDependentData = _Other.m_pChildDependentData;

		f_CopyEntities(_Other, EEntityCopyFlag_CopyChildren | EEntityCopyFlag_NoCheckTypes);
	}

	void CEntity::f_CopyEntities(CEntity const &_Other, EEntityCopyFlag _Flags)
	{
		if (_Flags & EEntityCopyFlag_ClearExistingChildren)
			m_ChildEntitiesMap.f_Clear();

		if (_Other.m_ChildEntitiesOrdered.f_IsEmpty())
			return;

		auto iChild = _Other.m_ChildEntitiesOrdered.f_GetIterator();
		m_ChildEntitiesMap.f_BatchMapIfNotMapped
			(
				[&](TCMap<CEntityKey, CEntity>::CConditionalMapper &_Mapper) -> bool
				{
					auto Mapped = _Mapper
						(
							iChild->f_GetKey()
							, *iChild
							, this
							, _Flags | EEntityCopyFlag_CopyChildren
						)
					;
					if ((_Flags & EEntityCopyFlag_MergeEntities) && !Mapped.f_WasCreated())
					{
						auto &NewChild = *Mapped;
						auto &NewData = NewChild.f_DataWritable();
						NewData.m_Condition.m_Children.f_Insert(_Other.f_Data().m_Condition.m_Children);
						NewChild.f_CopyProperties(*iChild);
						NewChild.f_CopyEntities(*iChild, _Flags);
					}
					++iChild;
					return iChild;
				}
			)
		;
	}

	void CEntity::f_CheckTypes(CEntity const &_Other)
	{
		auto &ThisData = f_Data();

		for (auto &Definition : ThisData.m_VariableDefinitions)
		{
			auto &Key = ThisData.m_VariableDefinitions.fs_GetKey(Definition);
			for (auto *pParent = m_pParent; pParent; pParent = pParent->m_pParent)
			{
				if (auto pExistingType = pParent->f_Data().m_VariableDefinitions.f_FindEqual(Key))
					CBuildSystem::fs_ThrowError(Definition.m_Type.m_Position, "User type name collision with parent entity", TCVector<CBuildSystemError>{{pExistingType->m_Type.m_Position, "Defined here"}});

				if (auto *pDefinition = pParent->f_ChildDependentData().m_ChildrenVariableDefinitions.f_FindEqual(Key); pDefinition && pDefinition->f_FindEqual(Definition.m_Type.m_Position))
					continue;

				auto &ChildDependentData = pParent->f_ChildDependentDataWritable();
				ChildDependentData.m_ChildrenVariableDefinitions[Key].f_Insert(Definition.m_Type.m_Position);
			}
		}

		for (auto &UserType : ThisData.m_UserTypes)
		{
			auto &Key = ThisData.m_UserTypes.fs_GetKey(UserType);

			for (auto *pParent = m_pParent; pParent; pParent = pParent->m_pParent)
			{
				if (auto pExistingType = pParent->f_Data().m_UserTypes.f_FindEqual(Key))
					CBuildSystem::fs_ThrowError(UserType.m_Position, "User type name collision with parent entity", TCVector<CBuildSystemError>{{pExistingType->m_Position, "Defined here"}});

				if (auto *pUserType = pParent->f_ChildDependentData().m_ChildrenUserTypes.f_FindEqual(Key); pUserType && pUserType->f_FindEqual(UserType.m_Position))
					continue;

				auto &ChildDependentData = pParent->f_ChildDependentDataWritable();
				ChildDependentData.m_ChildrenUserTypes[Key][UserType.m_Position];
			}
		}
	}

	void CEntity::f_CopyTypes(CEntity const &_Other)
	{
		auto &OtherData = _Other.f_Data();
		auto &ThisData = f_Data();

		for (auto &Definition : OtherData.m_VariableDefinitions)
		{
			auto &Key = OtherData.m_VariableDefinitions.fs_GetKey(Definition);
			if (auto pOld = ThisData.m_VariableDefinitions.f_FindEqual(Key))
			{
				if (pOld->m_Type.m_Type == Definition.m_Type.m_Type)
					continue;
			}
			CBuildSystem::fs_AddEntityVariableDefinition
				(
					nullptr
					, *this
					, Key
					, Definition.m_Type.m_Type
					, Definition.m_Type.m_Position
					, Definition.m_Type.m_Whitespace
					, Definition.m_pConditions
				)
			;
		}

		for (auto &UserType : OtherData.m_UserTypes)
		{
			auto &Key = OtherData.m_UserTypes.fs_GetKey(UserType);
			if (auto pOld = ThisData.m_UserTypes.f_FindEqual(Key))
			{
				if (pOld->m_Type == UserType.m_Type)
					continue;
			}
			CBuildSystem::fs_AddEntityUserType(*this, Key, UserType.m_Type, UserType.m_Position);
		}
	}

	void CEntity::fpr_GetPathKey(TCVector<CEntityKey> &_Dest) const
	{
		if (m_pParent && m_pParent->f_GetKey().m_Type != EEntityType_Root)
			m_pParent->fpr_GetPathKey(_Dest);
		_Dest.f_Insert(f_GetKey());
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
			if (pRet->f_GetKey().m_Type == EEntityType_Root)
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

		auto &Key = f_GetKey();

		if (!Key.m_Name.f_IsValid())
			_Destination += "{}"_f << fg_EntityTypeToStr(Key.m_Type);
		else if (Key.m_Name.f_IsConstantString())
			_Destination += "{}:{}"_f << fg_EntityTypeToStr(Key.m_Type) << Key.m_Name.f_ConstantString();
		else
			_Destination += "{}:{}"_f << fg_EntityTypeToStr(Key.m_Type) << Key.m_Name;
	}

	void CEntity::fr_GetPathForGetProperty(CStr &_Destination) const
	{
		auto &Key = f_GetKey();

		if (Key.m_Type == EEntityType_Root)
			return;

		if (m_pParent)
			m_pParent->fr_GetPathForGetProperty(_Destination);

		if (!_Destination.f_IsEmpty())
			_Destination += ".";
		auto &Name = f_GetKeyName();
		if (Name.f_FindChars(".\"") >= 0)
		{
			CStr ToEspace = fg_EntityTypeToStr(Key.m_Type);
			ToEspace += ":";
			ToEspace += Name;
			_Destination += ToEspace.f_EscapeStr();
		}
		else
		{
			_Destination += fg_EntityTypeToStr(Key.m_Type);
			_Destination += ":";
			_Destination += Name;
		}
	}

	CFilePosition const &CEntity::f_GetFirstValidPosition() const
	{
		for (auto *pParent = this; pParent; pParent = pParent->m_pParent)
		{
			auto &Data = pParent->f_Data();
			if (Data.m_Position.f_IsValid())
				return Data.m_Position;
		}

		return f_Data().m_Position;
	}

	NStr::CStr const &CEntity::f_GetKeyName() const
	{
		return f_GetKey().f_GetName(f_Data().m_Position);
	}

	bool CEntity::f_HasFullEval(EPropertyType _PropertyType) const
	{
		return (f_Data().m_HasFullEval & (1 << _PropertyType)) != 0;
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
		auto &ThisData = f_DataWritable();

		f_CopyTypes(_Other);

		auto &OtherData = _Other.f_Data();

		if (OtherData.m_Properties.f_IsEmpty())
			return;

		auto iProp = OtherData.m_Properties.f_GetIterator();
		ThisData.m_Properties.f_BatchMapIfNotMapped
			(
				[&](TCMap<CPropertyKey, CEntityData::CPropertyContainer>::CConditionalMapper &_Mapper) -> bool
				{
					auto Mapped = _Mapper(iProp.f_GetKey());
					(*Mapped).f_Insert(*iProp);

					++iProp;
					return iProp;
				}
			)
		;

		ThisData.m_HasFullEval |= OtherData.m_HasFullEval;
	}

	void CEntity::f_CopyProperties(CEntity &&_Other)
	{
		if (_Other.m_pData->m_RefCount.f_Get() != 0)
			return f_CopyProperties(fg_Const(_Other));

		auto &ThisData = f_DataWritable();

		f_CopyTypes(_Other);

		auto &OtherData = _Other.f_DataWritable();

		if (OtherData.m_Properties.f_IsEmpty())
			return;

		if (ThisData.m_Properties.f_IsEmpty())
		{
			ThisData.m_Properties = fg_Move(OtherData.m_Properties);
			ThisData.m_HasFullEval = OtherData.m_HasFullEval;
			return;
		}

		auto iProp = OtherData.m_Properties.f_GetIterator();
		ThisData.m_Properties.f_BatchMapIfNotMapped
			(
				[&](TCMap<CPropertyKey, CEntityData::CPropertyContainer>::CConditionalMapper &_Mapper) -> bool
				{
					auto Mapped = _Mapper(iProp.f_GetKey());
					(*Mapped).f_Insert(fg_Move(*iProp));

					++iProp;
					return iProp;
				}
			)
		;

		ThisData.m_HasFullEval |= OtherData.m_HasFullEval;
	}

	void CEntity::f_ClearReferences()
	{
		for (auto &Child : m_ChildEntitiesOrdered)
			Child.f_ClearReferences();
	}

	void CEntity::f_ForEachChild(TCFunction<void (CEntity *_pChild)> const &_fChild)
	{
		for (auto &Child : m_ChildEntitiesMap)
		{
			_fChild(&Child);
			Child.f_ForEachChild(_fChild);
		}
	}

	void CEntity::f_CopyExternal(CEntity const &_Other)
	{
		for (auto iEvaluated = _Other.m_EvaluatedProperties.m_Properties.f_GetIterator(); iEvaluated; ++iEvaluated)
		{
			if (iEvaluated->f_IsExternal())
				m_EvaluatedProperties.m_Properties(iEvaluated.f_GetKey(), *iEvaluated);
		}
	}

	CProperty &CEntity::f_AddProperty(CPropertyKey const &_Key, CBuildSystemSyntax::CRootValue const &_Value, CFilePosition const &_Position)
	{
		auto &ThisData = f_DataWritable();
		auto &NewProperty = ThisData.m_Properties[_Key].f_Insert();
		NewProperty.m_Key = _Key;
		NewProperty.m_Value = _Value;
		NewProperty.m_Position = _Position;
		DMibCheck(NewProperty.m_Key.m_Type != EPropertyType_Type);

		if (_Key.m_Name == "FullEval")
			ThisData.m_HasFullEval |= 1 << _Key.m_Type;

		return NewProperty;
	}

#ifdef DMibBuildSystem_DebugReferences
#ifdef DMibBuildSystem_DebugReferencesAdvanced
	CMutual CEntity::mp_DebugSetLock;
	TCSet<CEntity const *> CEntity::mp_DebugSet;
#endif
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
				if (!m_ChildEntitiesMap.f_FindEqual(pChild->f_GetKey()))
				{
					DConOut("!m_ChildEntitiesMap.f_FindEqual(pChild->f_GetKey())" DNewLine, 0);
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

	CEntityData::CEntityData() = default;

	CEntityData::CEntityData(CEntityData const &_Other)
		: m_Condition(_Other.m_Condition)
		, m_Properties(_Other.m_Properties)
		, m_VariableDefinitions(_Other.m_VariableDefinitions)
		, m_UserTypes(_Other.m_UserTypes)
		, m_Position(_Other.m_Position)
		, m_Debug(_Other.m_Debug)
		, m_HasFullEval(_Other.m_HasFullEval)
	{
	}

	CEntityData const &CEntity::f_Data() const
	{
		return *m_pData;
	}

	CEntityData &CEntity::f_DataWritable()
	{
		if (m_pData->m_RefCount.f_Get() == 0)
			return *m_pData;

		TCSharedPointer<CEntityData> pCopied = fg_Construct(*m_pData);
		m_pData = fg_Move(pCopied);

		return *m_pData;
	}

	CEntityChildDependantData::CEntityChildDependantData() = default;
	CEntityChildDependantData::CEntityChildDependantData(CEntityChildDependantData const &_Other)
		: m_ChildrenVariableDefinitions(_Other.m_ChildrenVariableDefinitions)
		, m_ChildrenUserTypes(_Other.m_ChildrenUserTypes)
	{
	}

	CEntityChildDependantData const &CEntity::f_ChildDependentData() const
	{
		return *m_pChildDependentData;
	}

	CEntityChildDependantData &CEntity::f_ChildDependentDataWritable()
	{
		if (m_pChildDependentData->m_RefCount.f_Get() == 0)
			return *m_pChildDependentData;

		TCSharedPointer<CEntityChildDependantData> pCopied = fg_Construct(*m_pChildDependentData);
		m_pChildDependentData = fg_Move(pCopied);

		return *m_pChildDependentData;
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
		else if (_String == "CreateTemplate") return EEntityType_CreateTemplate;
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
		case EEntityType_CreateTemplate: return "CreateTemplate";
		default: DMibNeverGetHere; return CStr();
		}
	}
}
