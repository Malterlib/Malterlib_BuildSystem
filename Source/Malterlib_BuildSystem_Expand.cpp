// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_ExpandGlobalTargetsAndWorkspaces(CBuildSystemData &_BuildSystemData) const
	{
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild;)
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if (Key.m_Type == EEntityType_Root)
					{
						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_Target && Key.m_Type != EEntityType_Workspace)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;
		fExpandEntities(_BuildSystemData.m_RootEntity);
	}

	void CBuildSystem::f_ExpandRepositoryEntities(CBuildSystemData &_BuildSystemData) const
	{
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if (Key.m_Type != EEntityType_Repository)
						continue;

					auto KeyCopy = Key;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(KeyCopy);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;
		fExpandEntities(_BuildSystemData.m_RootEntity);
	}

	void CBuildSystem::f_ExpandCreateTemplateEntities(CBuildSystemData &_BuildSystemData) const
	{
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if (Key.m_Type != EEntityType_CreateTemplate)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;
		fExpandEntities(_BuildSystemData.m_RootEntity);
	}

	void CBuildSystem::f_ExpandGlobalEntities(CBuildSystemData &_BuildSystemData) const
	{
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if (Key.m_Type == EEntityType_Target)
						continue;

					if
						(
							Key.m_Type == EEntityType_Workspace
							|| Key.m_Type == EEntityType_Group
							|| Key.m_Type == EEntityType_Root
							|| Key.m_Type == EEntityType_Import
						)
					{
						if ((Key.m_Type == EEntityType_Group || Key.m_Type == EEntityType_Import) && Child.m_pParent == &_BuildSystemData.m_RootEntity)
						{
							// Root groups shoud not be considered
							continue;
						}
						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_File && Key.m_Type != EEntityType_Dependency)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;
		fExpandEntities(_BuildSystemData.m_RootEntity);
	}

	void CBuildSystem::f_ExpandTargetDependenciesBackup(CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const
	{
		DRequire(_Target.f_GetKey().m_Type == EEntityType_Target);
		// Backup old

		TCVector<CEntityKey> Path;

		TCFunction<void (CEntity &_Entity)> fExpandDependency
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if
						(
							Key.m_Type == EEntityType_Target
							|| Key.m_Type == EEntityType_Group
							|| Key.m_Type == EEntityType_Import
						)
					{
						Path.f_Insert(Child.f_GetKey());
						fExpandDependency(Child);
						Path.f_Remove(Path.f_GetLen() - 1);
						continue;
					}

					if (Key.m_Type != EEntityType_Dependency)
						continue;

					auto &Entity = o_Backup.m_Backup(Path).f_GetResult().f_Insert(CDependenciesBackup::CEntityBackup{Child.f_GetKey(), {Child, nullptr, EEntityCopyFlag_CopyChildren}});
					Entity.m_Entity.f_CopyExternal(Child);
				}
			}
		;

		fExpandDependency(fg_RemoveQualifiers(_Target));
	}

	void CBuildSystem::f_ExpandTargetDependencies(CWorkspaceInfo &_Workspace, CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const
	{
		if (o_Backup.m_Backup.f_IsEmpty())
			f_ExpandTargetDependenciesBackup(_BuildSystemData, _Target, o_Backup);

		if (_Target.f_GetKey().m_Type == EEntityType_Target)
		{
			// Remove old
			{
				TCFunction<void (CEntity &_Entity)> fExpandDependency
					= [&](CEntity &_Entity)
					{
						for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
						{
							auto &Child = *iChild;
							++iChild;
							auto &Key = Child.f_GetKey();
							if
								(
									Key.m_Type == EEntityType_Target
									|| Key.m_Type == EEntityType_Group
		 							|| Key.m_Type == EEntityType_Import
								)
							{
								fExpandDependency(Child);
								continue;
							}

							if (Key.m_Type != EEntityType_Dependency)
								continue;

							_Entity.m_ChildEntitiesMap.f_Remove(Key);
						}
					}
				;

				fExpandDependency(fg_RemoveQualifiers(_Target));
			}
			// Restore
			{
				for (auto iBackup = o_Backup.m_Backup.f_GetIterator(); iBackup; ++iBackup)
				{
					auto &KeyPath = iBackup.f_GetKey();
					auto *pParent = &fg_RemoveQualifiers(_Target);

					for (auto &Key : KeyPath)
					{
						pParent = pParent->m_ChildEntitiesMap.f_FindEqual(Key);
						if (!pParent)
							break;
					}
					if (!pParent)
						continue;

					auto &BackedUpDependencies = *iBackup;

					for (auto &BackedUpDependency : BackedUpDependencies)
					{
						DCheck(BackedUpDependency.m_Entity.m_ChildEntitiesMap.f_IsEmpty());
						auto &NewEntity = pParent->m_ChildEntitiesMap
							(
								BackedUpDependency.m_Key
								, BackedUpDependency.m_Entity
								, pParent
								, EEntityCopyFlag_CopyChildren
							)
							.f_GetResult()
						;
						NewEntity.f_CopyExternal(BackedUpDependency.m_Entity);
					}
				}
			}
		}

		// Expand entities
		{
			TCFunction<void (CEntity &_Entity)> fExpandDependency
				= [&](CEntity &_Entity)
				{
					for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
					{
						auto &Child = *iChild;
						++iChild;
						auto &Key = Child.f_GetKey();
						if
							(
								Key.m_Type == EEntityType_Target
								|| Key.m_Type == EEntityType_Group
								|| Key.m_Type == EEntityType_Import
							)
						{
							fExpandDependency(Child);
							continue;
						}

						if (Key.m_Type != EEntityType_Dependency)
							continue;

						auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
						if (!pLastInserted)
							continue;

						_Entity.m_ChildEntitiesMap.f_Remove(Key);

						if (pLastInserted != &Child)
						{
							iChild = pLastInserted;
							++iChild;
						}
					}
				}
			;
			fExpandDependency(fg_RemoveQualifiers(_Target));
		}
	}

	void CBuildSystem::f_ExpandTargetGroups(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const
	{
		DRequire(_Target.f_GetKey().m_Type == EEntityType_Target);
		TCFunction<void (CEntity &_Entity)> fExpandGroup
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if
						(
							Key.m_Type == EEntityType_Target
							|| Key.m_Type == EEntityType_Group
 							|| Key.m_Type == EEntityType_Import
						)
					{
						fExpandGroup(Child);
					}

					if (Key.m_Type != EEntityType_Group)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;

		fExpandGroup(fg_RemoveQualifiers(_Target));
	}

	void CBuildSystem::f_ExpandTargetFiles(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const
	{
		DRequire(_Target.f_GetKey().m_Type == EEntityType_Target);
		TCFunction<void (CEntity &_Entity)> fExpandFile
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if
						(
							Key.m_Type == EEntityType_Target
							|| Key.m_Type == EEntityType_Group
 							|| Key.m_Type == EEntityType_Import
						)
					{
						fExpandFile(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_File)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;

		fExpandFile(fg_RemoveQualifiers(_Target));
	}

	void CBuildSystem::f_ExpandWorkspaceEntities(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const
	{
		DRequire(_Target.f_GetKey().m_Type == EEntityType_Workspace);
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if
						(
							Key.m_Type == EEntityType_Group
							|| Key.m_Type == EEntityType_Import
						)
					{
						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_File)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;

		fExpandEntities(fg_RemoveQualifiers(_Target));
	}

	void CBuildSystem::f_ExpandWorkspaceTargets(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const
	{
		DRequire(_Target.f_GetKey().m_Type == EEntityType_Workspace);
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();
					if
						(
							Key.m_Type == EEntityType_Group
 							|| Key.m_Type == EEntityType_Import
						)
					{
						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_Target)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr);
					if (!pLastInserted)
						continue;

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;

		fExpandEntities(fg_RemoveQualifiers(_Target));
	}

	CEntity *CBuildSystem::fp_AddEntity
		(
			CEntity const &_Entity
			, CEntity &_ParentEntity
			, CEntityKey const &_NewKey
			, CEntity *_pInsertAfter
			, TCMap<CPropertyKey, CEvaluatedProperty> const *_pExtraProperties
		) const
	{
		auto &NewEntity = _ParentEntity.m_ChildEntitiesMap(_NewKey, _Entity, &_ParentEntity, EEntityCopyFlag_CopyChildren).f_GetResult();

		if (_pInsertAfter)
		{
			DMibCheck(_ParentEntity.m_ChildEntitiesMap.f_FindEqual(_NewKey) == &NewEntity);

			NewEntity.m_Link.f_Unlink();
			_ParentEntity.m_ChildEntitiesOrdered.f_InsertAfter(NewEntity, _pInsertAfter);
		}

		NewEntity.m_EvaluatedProperties.m_Properties.f_Clear();
		if (_pExtraProperties)
			NewEntity.m_EvaluatedProperties.m_Properties += *_pExtraProperties;

		return &NewEntity;
	}

	CEntity *CBuildSystem::fp_ExpandEntity(CEntity &_Entity, CEntity &_ParentEntity, TCVector<CEntity *> *o_pCreated) const
	{
		auto &Key = _Entity.f_GetKey();
		auto &EntityData = _Entity.f_Data();

		TCVector<CEJSON> Entities;

		if (Key.m_Name.f_IsConstantString())
		{
			if (Key.m_Type != EEntityType_File)
				return nullptr;

			Entities.f_Insert(Key.m_Name.f_Constant());
		}
		else
		{
			CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
			CEvalPropertyValueContext Context{_Entity, _Entity, EntityData.m_Position, EvalContext, nullptr};
			CEJSON Data = fp_EvaluatePropertyValue(Context, Key.m_Name, nullptr);

			if (Data.f_IsArray())
				Entities = fg_Move(Data.f_Array());
			else
				Entities.f_Insert(fg_Move(Data));
		}

		bool bAllowNonExisting = false;
		if (Key.m_Type == EEntityType_File)
		{
			CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
			CPropertyKey Key;
			Key.m_Type = EPropertyType_Compile;
			Key.m_Name = "AllowNonExisting";
			CProperty const *pFromProperty = nullptr;
			auto Data = fp_EvaluateEntityProperty(_Entity, _Entity, Key, EvalContext, pFromProperty, EntityData.m_Position, nullptr);
			if (Data.f_IsValid())
			{
				if (!Data.f_IsBoolean())
					fs_ThrowError(EntityData.m_Position, "Expected a boolean value");
				bAllowNonExisting = Data.f_Boolean();
			}
		}

		CEntity *pInsertAfter = &_Entity;
		Entities.f_Sort();
		Entities.f_UniqueIfSorted();

		for (auto const &SourceEntity : Entities)
		{
			CStr EntityName;

			CEntityKey NewEntityKey{Key.m_Type, fg_RandomID()};

			auto NewEntityMap = _Entity.m_ChildEntitiesMap(NewEntityKey, &_Entity);
			auto &TempEntity = *NewEntityMap;

			auto Cleanup = g_OnScopeExit / [&]
				{
					if (NewEntityMap.f_WasCreated())
						_Entity.m_ChildEntitiesMap.f_Remove(NewEntityKey);
				}
			;

			if (SourceEntity.f_IsObject())
			{
				auto *pName = SourceEntity.f_GetMember("Name", EJSONType_String);
				if (!pName)
					fs_ThrowError(EntityData.m_Position, "Expected a 'Name' for string type in entity object");

				auto *pProperties = SourceEntity.f_GetMember("Properties", EJSONType_Object);
				if (!pProperties)
					fs_ThrowError(EntityData.m_Position, "Expected a 'Properties' of object type in entity object");

				EntityName = pName->f_String();

				for (auto &SourceProperty : pProperties->f_Object())
				{
					CPropertyKey Key = CPropertyKey::fs_FromString(SourceProperty.f_Name(), EntityData.m_Position);

					auto &Property = TempEntity.m_EvaluatedProperties.m_Properties[Key];
					Property.m_Value = SourceProperty.f_Value();
					Property.m_Type = EEvaluatedPropertyType_External;
					Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
				}
			}
			else if (SourceEntity.f_IsString())
				EntityName = SourceEntity.f_String();
			else
				fs_ThrowError(EntityData.m_Position, "Expected entity to be a string or an object, not: {}"_f << SourceEntity);

			if (Key.m_Type == EEntityType_File)
			{
				TCVector<CStr> Files;

				aint RecursiveStart = -1;
				{
					EFileAttrib Attribs = EFileAttrib_File;
					if (EntityName.f_StartsWith("~"))
					{
						EntityName = EntityName.f_Extract(1);
						Attribs = EFileAttrib_Directory;
					}

					bool bWildcardSearch = false;

					CStr SearchPath = CFile::fs_GetExpandedPath(EntityName, CFile::fs_GetPath(EntityData.m_Position.m_File));
					if (SearchPath.f_FindChars("*?") >= 0)
						bWildcardSearch = true;
					RecursiveStart = SearchPath.f_FindChar('^');
					if (RecursiveStart >= 0)
					{
						SearchPath = SearchPath.f_Replace("^", "");
						bWildcardSearch = true;
					}
					CFindOptions FindOptions(SearchPath, Attribs);
					FindOptions.m_bRecursive = RecursiveStart >= 0;

					if (bAllowNonExisting && !bWildcardSearch)
						Files.f_Insert(SearchPath);
					else
					{
						if (bWildcardSearch)
						{
							TCSet<CStr> Exclude;
 							TCVector<CStr> ExcludedFiles = f_EvaluateEntityPropertyStringArray(_Entity, EPropertyType_Property, "ExcludeFiles", TCVector<CStr>());

							for (auto &Exclude : ExcludedFiles)
							{
								if (Exclude == ">")
									FindOptions.m_bFollowLinks = false;
								else if (!Exclude.f_IsEmpty())
									FindOptions.m_Exclude[Exclude];
							}

							auto FullFiles = mp_FindCache.f_FindFiles(FindOptions, true);
							for (auto iFile = FullFiles.f_GetIterator(); iFile; ++iFile)
								Files.f_Insert(iFile->m_Path);
						}
						else
						{
							if (CFile::fs_FileExists(FindOptions.m_Path, Attribs))
							{
								mp_FindCache.f_AddSourceFile(FindOptions.m_Path);
								Files.f_Insert(FindOptions.m_Path);
							}
						}
						if (Files.f_IsEmpty() && !bAllowNonExisting)
							fsp_ThrowError(EntityData.m_Position, CStr::CFormat("No file found for pattern {}") << SearchPath);
					}
				}

				for (auto iFile = Files.f_GetIterator(); iFile; ++iFile)
				{
					auto &FullFileName = *iFile;
					CEntityKey NewKey;
					NewKey.m_Type = Key.m_Type;
					NewKey.m_Name.m_Value = FullFileName;

					auto *pParent = &_ParentEntity;

					auto pInsertAfterLocal = pInsertAfter;
					if (RecursiveStart >= 0)
					{
						CStr Path = CFile::fs_GetPath(FullFileName.f_Extract(RecursiveStart));
						while (!Path.f_IsEmpty())
						{
							CStr ThisPath = fg_GetStrSep(Path, "/");

							CEntityKey Key;
							Key.m_Type = EEntityType_Group;
							Key.m_Name.m_Value = ThisPath;
							auto Child = pParent->m_ChildEntitiesMap(Key, pParent);

							if (Child.f_WasCreated())
							{
								if (pInsertAfterLocal)
								{
									(*Child).m_Link.f_Unlink();
									pParent->m_ChildEntitiesOrdered.f_InsertAfter(*Child, pInsertAfter);
									pInsertAfter = &*Child;
								}
							}

							pParent = &*Child;
							pInsertAfterLocal = nullptr;
						}
					}

					auto pOldEntity = pParent->m_ChildEntitiesMap.f_FindEqual(NewKey);

					if (pOldEntity)
					{
						if (pOldEntity == &_Entity)
						{
							if (Files.f_GetLen() != 1 || Entities.f_GetLen() != 1)
								fsp_ThrowError(EntityData.m_Position, "Internal error: pOldEntity == &_Entity && (Files.f_GetLen() != 1 || Entities.f_GetLen() != 1)");
							return nullptr; // We don't need to do anything
						}
						if (pOldEntity == pInsertAfter)
							pInsertAfter = pParent->m_ChildEntitiesOrdered.fs_GetPrev(pInsertAfter);

						pParent->m_ChildEntitiesMap.f_Remove(pOldEntity);
						//fsp_ThrowError(_Registry, CStr::CFormat("Entity {} already declared (when adding pattern {})") << FullFileName << SearchPath);
					}

					auto pNewEntity = fp_AddEntity(_Entity, *pParent, NewKey, pInsertAfterLocal ? pInsertAfter : nullptr, &TempEntity.m_EvaluatedProperties.m_Properties);

					if (pInsertAfterLocal)
						pInsertAfter = pNewEntity;

					if (o_pCreated)
						o_pCreated->f_Insert(pNewEntity);
				}
			}
			else
			{
				CEntityKey NewKey;
				NewKey.m_Type = Key.m_Type;
				NewKey.m_Name.m_Value = EntityName;
				auto pOldEntity = _ParentEntity.m_ChildEntitiesMap.f_FindEqual(NewKey);

				if (pOldEntity)
				{
					if (pOldEntity == &_Entity)
					{
						if (Entities.f_GetLen() != 1)
							fsp_ThrowError(EntityData.m_Position, "Internal error: pOldEntity == &_Entity && (Entities.f_GetLen() != 1)");
						return nullptr; // We don't need to do anything
					}

					if (pOldEntity == pInsertAfter)
						pInsertAfter = _ParentEntity.m_ChildEntitiesOrdered.fs_GetPrev(pInsertAfter);

					_ParentEntity.m_ChildEntitiesMap.f_Remove(pOldEntity);
					//fsp_ThrowError(_Registry, CStr::CFormat("Entity {} already declared (when adding pattern {})") << EntityName << SearchPath);
				}

				auto pNewEntity = fp_AddEntity(_Entity, _ParentEntity, NewKey, pInsertAfter, &TempEntity.m_EvaluatedProperties.m_Properties);

				pInsertAfter = pNewEntity;

				if (o_pCreated)
					o_pCreated->f_Insert(pNewEntity);
			}
		}
		return pInsertAfter;
	}
}
