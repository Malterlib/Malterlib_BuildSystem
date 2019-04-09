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
					auto &Key = Child.f_GetMapKey();
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
		TCFunction<void (CEntity &_Entity, bool _bInWorkspace)> fExpandEntities
			= [&](CEntity &_Entity, bool _bInWorkspace)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
					if (Key.m_Type != EEntityType_Repository)
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
		fExpandEntities(_BuildSystemData.m_RootEntity, false);
	}

	void CBuildSystem::f_ExpandCreateTemplateEntities(CBuildSystemData &_BuildSystemData) const
	{
		TCFunction<void (CEntity &_Entity, bool _bInWorkspace)> fExpandEntities
			= [&](CEntity &_Entity, bool _bInWorkspace)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
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
		fExpandEntities(_BuildSystemData.m_RootEntity, false);
	}

	void CBuildSystem::f_ExpandGlobalEntities(CBuildSystemData &_BuildSystemData) const
	{
		TCFunction<void (CEntity &_Entity, bool _bInWorkspace)> fExpandEntities
			= [&](CEntity &_Entity, bool _bInWorkspace)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
					if (Key.m_Type == EEntityType_Target)
						continue;
					
					if 
						(
							Key.m_Type == EEntityType_Workspace
							|| Key.m_Type == EEntityType_Group 
							|| Key.m_Type == EEntityType_Root
						)
					{
						if (Key.m_Type == EEntityType_Group && Child.m_pParent == &_BuildSystemData.m_RootEntity)
						{
							// Root groups shoud not be considered
							continue;
						}
						fExpandEntities(Child, _bInWorkspace || Key.m_Type == EEntityType_Workspace);
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
		fExpandEntities(_BuildSystemData.m_RootEntity, false);
	}
	
	void CBuildSystem::f_ExpandTargetDependencies(CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const
	{
		DRequire(_Target.m_Key.m_Type == EEntityType_Target);
		
		if (o_Backup.m_Backup.f_IsEmpty())
		{
			// Backup old
			
			TCVector<CEntityKey> Path;
			
			TCFunction<void (CEntity &_Entity)> fExpandDependency
				= [&](CEntity &_Entity)
				{
					for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
					{
						auto &Child = *iChild;
						++iChild;
						auto &Key = Child.f_GetMapKey();
						if 
							(
								(Key.m_Type == EEntityType_Target)
								|| Key.m_Type == EEntityType_Group 
							)
						{
							Path.f_Insert(Child.m_Key);
							fExpandDependency(Child);
							Path.f_Remove(Path.f_GetLen() - 1);
							continue;
						}

						if (Key.m_Type != EEntityType_Dependency)
							continue;
						
						auto &Entity = o_Backup.m_Backup(Path).f_GetResult().f_Insert(fg_Construct(nullptr));
						Entity.f_CopyFrom(Child, true, nullptr, true);
						Entity.f_CopyExternal(Child);
					}
				}
			;

			fExpandDependency(fg_RemoveQualifiers(_Target));
		}
		else
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
							auto &Key = Child.f_GetMapKey();
							if 
								(
									(Key.m_Type == EEntityType_Target)
									|| Key.m_Type == EEntityType_Group 
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
						auto &NewEntity = pParent->m_ChildEntitiesMap(BackedUpDependency.m_Key, pParent).f_GetResult();
						pParent->m_ChildEntitiesOrdered.f_Insert(NewEntity);
						DCheck(BackedUpDependency.m_ChildEntitiesMap.f_IsEmpty());
						NewEntity.f_CopyFrom(BackedUpDependency, true, nullptr, true);
						NewEntity.f_CopyExternal(BackedUpDependency);
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
						auto &Key = Child.f_GetMapKey();
						if 
							(
								(Key.m_Type == EEntityType_Target)
								|| Key.m_Type == EEntityType_Group 
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
		DRequire(_Target.m_Key.m_Type == EEntityType_Target);
		TCFunction<void (CEntity &_Entity)> fExpandGroup
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
					if
						(
							(Key.m_Type == EEntityType_Target)
							|| Key.m_Type == EEntityType_Group 
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
		DRequire(_Target.m_Key.m_Type == EEntityType_Target);
		TCFunction<void (CEntity &_Entity)> fExpandFile
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
					if
						(
							(Key.m_Type == EEntityType_Target)
							|| Key.m_Type == EEntityType_Group 
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
		DRequire(_Target.m_Key.m_Type == EEntityType_Workspace);
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
					if
						(
							Key.m_Type == EEntityType_Group 
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
		DRequire(_Target.m_Key.m_Type == EEntityType_Workspace);
		TCFunction<void (CEntity &_Entity)> fExpandEntities
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetMapKey();
					if
						(
							Key.m_Type == EEntityType_Group 
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
			CEntity &_Entity
			, CEntity &_ParentEntity
			, CEntityKey const &_NewKey
			, CEntity *_pInsertAfter
			, TCMap<CPropertyKey, CEvaluatedProperty> const *_pExtraProperties
		) const
	{
		auto &NewEntity = _ParentEntity.m_ChildEntitiesMap(_NewKey, &_ParentEntity).f_GetResult();
		
		if (_pInsertAfter)
			_ParentEntity.m_ChildEntitiesOrdered.f_InsertAfter(NewEntity, _pInsertAfter);
		else
			_ParentEntity.m_ChildEntitiesOrdered.f_Insert(NewEntity);
		NewEntity.f_CopyFrom(_Entity, true, &_NewKey, true);
		NewEntity.m_pCopiedFrom = nullptr;
		NewEntity.m_EvaluatedProperties.f_Clear();
		NewEntity.m_PotentialExplicitProperties.f_Clear();
		NewEntity.m_PerFilePotentialExplicitProperties.f_Clear();
		if (_pExtraProperties)
			NewEntity.m_EvaluatedProperties += *_pExtraProperties;
		
		if (_NewKey.m_Type != EEntityType_Target && _NewKey.m_Type != EEntityType_Workspace)
			f_ReEvaluateData(NewEntity);
		
		return &NewEntity;
	}
	
	CEntity *CBuildSystem::fp_ExpandEntity(CEntity &_Entity, CEntity &_ParentEntity, TCVector<CEntity *> *o_pCreated) const
	{
		DMibLock(_Entity.m_Lock);
		
		auto &Key = _Entity.m_Key;
		
		CStr Data;
		if (Key.m_Name.f_Find("@(") >= 0)
		{
			CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
			Data = fp_EvaluatePropertyValue(_Entity, _Entity, Key.m_Name, _Entity.m_Position, EvalContext);
		}
		else
		{
			if (Key.m_Type != EEntityType_File)
				return nullptr;
			Data = Key.m_Name;
		}

		bool bAllowNonExisting = false;
		if (Key.m_Type == EEntityType_File)
		{
			{
				CEvaluationContext EvalContext(&_Entity.m_EvaluatedProperties);
				CPropertyKey Key;
				Key.m_Type = EPropertyType_Compile;
				Key.m_Name = "AllowNonExisting";
				CProperty const *pFromProperty = nullptr;
				CStr Data = fp_EvaluateEntityProperty(_Entity, _Entity, Key, EvalContext, pFromProperty);
				if (Data == "true")
					bAllowNonExisting = true;
			}
		}

		TCSet<CStr> Entities;
		while (!Data.f_IsEmpty())
		{
			CStr Entity = fg_GetStrSep(Data, ";");
			if (!Entity.f_IsEmpty())
				Entities[Entity];
		}

		CEntity *pInsertAfter = &_Entity;

		for (auto iEntity = Entities.f_GetIterator(); iEntity; ++iEntity)
		{
			CStr EntityName;

			CEntity TempEntity(&_Entity);
			
			{
				CStr IdentityFull = *iEntity;
				if (IdentityFull.f_IsEmpty())
					continue;
				
				EntityName = fg_GetStrSep(IdentityFull, "|");
				
				while (!IdentityFull.f_IsEmpty())
				{
					CStr Var = fg_GetStrSep(IdentityFull, "|");
					CPropertyKey Key;
					Key.m_Name = fg_GetStrSep(Var, "=");
					Key.m_Type = EPropertyType_Property;

					auto &Property = TempEntity.m_EvaluatedProperties[Key];
					Property.m_Value = Var;
					Property.m_Type = EEvaluatedPropertyType_External;
					Property.m_pProperty = &mp_ExternalProperty[Key.m_Type];
				}
			}
			
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

					CStr SearchPath = CFile::fs_GetExpandedPath(EntityName, CFile::fs_GetPath(_Entity.m_Position.m_FileName));
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
							CStr ExcludedFiles = f_EvaluateEntityProperty(_Entity, EPropertyType_Property, "ExcludeFiles");
							while (!ExcludedFiles.f_IsEmpty())
							{
								CStr Value = fg_GetStrSep(ExcludedFiles, ";");
								if (Value == ">")
									FindOptions.m_bFollowLinks = false;
								else if (!Value.f_IsEmpty())
									FindOptions.m_Exclude[Value];
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
							fsp_ThrowError(_Entity.m_Position, CStr::CFormat("No file found for pattern {}") << SearchPath);
					}
				}
				
				for (auto iFile = Files.f_GetIterator(); iFile; ++iFile)
				{
					auto &FullFileName = *iFile;
					CEntityKey NewKey;
					NewKey.m_Type = Key.m_Type;
					NewKey.m_Name = FullFileName;
					
					auto *pParent = &_ParentEntity;
					
					auto pInsertAfterLocal = pInsertAfter;
					if (RecursiveStart >= 0)
					{
						CStr Path = CFile::fs_GetPath(FullFileName.f_Extract(RecursiveStart));
						while (!Path.f_IsEmpty())
						{
							CStr ThisPath = fg_GetStrSep(Path, "/");
							
							CEntityKey Key;
							Key.m_Name = ThisPath;
							Key.m_Type = EEntityType_Group;
							auto Child = pParent->m_ChildEntitiesMap(Key, pParent);
							
							if (Child.f_WasCreated())
							{
								(*Child).m_Key = Key;
								if (pInsertAfterLocal)
								{
									pParent->m_ChildEntitiesOrdered.f_InsertAfter(*Child, pInsertAfter);
									pInsertAfter = &*Child;
								}
								else
									pParent->m_ChildEntitiesOrdered.f_Insert(*Child);
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
								fsp_ThrowError(_Entity.m_Position, "Internal error: pOldEntity == &_Entity && (Files.f_GetLen() != 1 || Entities.f_GetLen() != 1)");
							return nullptr; // We don't need to do anything
						}
						pParent->m_ChildEntitiesMap.f_Remove(pOldEntity);
						//fsp_ThrowError(_Registry, CStr::CFormat("Entity {} already declared (when adding pattern {})") << FullFileName << SearchPath);
					}

					auto pNewEntity = fp_AddEntity(_Entity, *pParent, NewKey, pInsertAfterLocal ? pInsertAfter : nullptr, &TempEntity.m_EvaluatedProperties);
					
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
				NewKey.m_Name = EntityName;
				auto pOldEntity = _ParentEntity.m_ChildEntitiesMap.f_FindEqual(NewKey);
				
				if (pOldEntity)
				{
					if (pOldEntity == &_Entity)
					{
						if (Entities.f_GetLen() != 1)
							fsp_ThrowError(_Entity.m_Position, "Internal error: pOldEntity == &_Entity && (Entities.f_GetLen() != 1)");
						return nullptr; // We don't need to do anything
					}
					_ParentEntity.m_ChildEntitiesMap.f_Remove(pOldEntity);
					//fsp_ThrowError(_Registry, CStr::CFormat("Entity {} already declared (when adding pattern {})") << EntityName << SearchPath);
				}
				
				auto pNewEntity = fp_AddEntity(_Entity, _ParentEntity, NewKey, pInsertAfter, &TempEntity.m_EvaluatedProperties);
				pInsertAfter = pNewEntity;
				
				if (o_pCreated)
					o_pCreated->f_Insert(pNewEntity);
			}
		}
		return pInsertAfter;
	}
}
