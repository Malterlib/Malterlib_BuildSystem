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

					if (Key.m_Type != EEntityType_Target && Key.m_Type != EEntityType_Workspace && Key.m_Type != EEntityType_Group)
						continue;

					bool bChanged = false;
					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
					if (!pLastInserted)
					{
						if (Key.m_Type == EEntityType_Group)
						{
							DMibCheck(Child.f_GetKey().m_Name.f_IsConstantString());
							fExpandEntities(Child);
						}

						continue;
					}

					_Entity.m_ChildEntitiesMap.f_Remove(Key);

					if (pLastInserted != &Child)
						iChild = pLastInserted;
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

					bool bChanged = false;
					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
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

					bool bChanged = false;
					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
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

						if (!Child.f_GetKey().m_Name.f_IsConstantString())
						{
							CBuildSystemUniquePositions Positions;
							Positions.f_AddPosition(Child.f_GetFirstValidPosition(), "Entity");
							fs_ThrowError(Positions, "Expanding not supported");
							continue;
						}

						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_File && Key.m_Type != EEntityType_Dependency)
						continue;

					bool bChanged = false;
					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
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

		auto fStoreBackup = [&]
			(
#ifndef DCompiler_Workaround_Apple_clang
				this
#endif
				auto &&_fThis
				, CEntity &_Entity
			) -> void
			{
#ifdef DCompiler_Workaround_Apple_clang
#define _fThis(...) _fThis(_fThis, __VA_ARGS__)
#endif
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
						if (!Child.f_GetKey().m_Name.f_IsConstantString())
							continue;

						Path.f_Insert(Child.f_GetKey());
						_fThis(Child);
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

#ifdef DCompiler_Workaround_Apple_clang
#define fStoreBackup(...) fStoreBackup(fStoreBackup, __VA_ARGS__)
#endif
		fStoreBackup(fg_RemoveQualifiers(_Target));
	}

	void CBuildSystem::f_ExpandTargetDependencies(CWorkspaceInfo &_Workspace, CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const
	{
		if (o_Backup.m_Backup.f_IsEmpty())
			f_ExpandTargetDependenciesBackup(_BuildSystemData, _Target, o_Backup);

		if (_Target.f_GetKey().m_Type == EEntityType_Target)
		{
			// Remove old
			{
				auto fRemoveOld = [&]
					(
#ifndef DCompiler_Workaround_Apple_clang
						this
#endif
						auto &&_fThis
						, CEntity &_Entity
					) -> void
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
								if (!Child.f_GetKey().m_Name.f_IsConstantString())
									continue;

								_fThis(Child);
								continue;
							}

							if (Key.m_Type != EEntityType_Dependency)
								continue;

							_Entity.m_ChildEntitiesMap.f_Remove(Key);
						}
					}
				;

#ifdef DCompiler_Workaround_Apple_clang
#define fRemoveOld(...) fRemoveOld(fRemoveOld, __VA_ARGS__)
#endif
				fRemoveOld(fg_RemoveQualifiers(_Target));
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
			auto fExpandDependency = [&]
				(
#ifndef DCompiler_Workaround_Apple_clang
				 this
#endif
					auto &&_fThis
					, CEntity &_Entity
					, bool _bUnexpandedGroup
				) -> void
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
							_fThis(Child, _bUnexpandedGroup || !Child.f_GetKey().m_Name.f_IsConstantString());
							continue;
						}

						if (Key.m_Type != EEntityType_Dependency)
							continue;

						if (_bUnexpandedGroup)
						{
							CBuildSystemUniquePositions Positions;
							Positions.f_AddPosition(Child.f_GetFirstValidPosition(), "Entity");
							fs_ThrowError(Positions, "Dependency is inside an unexpanded group, this is not supported");
						}

						bool bChanged = false;
						auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
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
#ifdef DCompiler_Workaround_Apple_clang
#define fExpandDependency(...) fExpandDependency(fExpandDependency, __VA_ARGS__)
#endif
			fExpandDependency(fg_RemoveQualifiers(_Target), false);
		}
	}

	CBuildSystem::CExpandEntityState::CExpandEntityState() = default;

	CBuildSystem::CExpandEntityState::~CExpandEntityState()
	{
		if (!m_bEnabled)
			return;

		m_OldEntitiesToRemove.f_ExtractAll
			(
				[&](auto &&_Handle)
				{
					auto *pPtr = _Handle->f_Get();
					_Handle->f_Clear();
					pPtr->m_pParent->m_ChildEntitiesMap.f_Remove(pPtr);
				}
			)
		;
	}

	void CBuildSystem::f_PopulateTargetAllFiles(CEntity &o_Target) const
	{
		TCMap<CStr, TCVector<CStr>> AllFiles;
		auto fFindFiles = [&]
			(
#ifndef DCompiler_Workaround_Apple_clang
				this
#endif
				auto &&_fThis
				, CEntity &_Entity
			) -> void
			{
				for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
				{
					auto &ChildEntity = *iEntity;
					switch (ChildEntity.f_GetKey().m_Type)
					{
					case EEntityType_GenerateFile:
					case EEntityType_Target:
					case EEntityType_Import:
					case EEntityType_Group:
						{
							if (!ChildEntity.f_GetKey().m_Name.f_IsConstantString())
								continue;

							_fThis( ChildEntity);
						}
						break;
					case EEntityType_File:
						{
							if (!ChildEntity.f_GetKey().m_Name.f_IsConstantString())
								continue;

							CStr CompileType = f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Compile_Type, CStr());

							AllFiles[CompileType].f_Insert(ChildEntity.f_GetPathForGetProperty());
						}
						break;
					default:
						break;
					}
				}
			}
		;
#ifdef DCompiler_Workaround_Apple_clang
#define fFindFiles(...) fFindFiles(fFindFiles, __VA_ARGS__)
#endif
		fFindFiles(o_Target);

		for (auto &Files : AllFiles)
		{
			auto &Type = AllFiles.fs_GetKey(Files);

			Files.f_Sort();
			CEJsonSorted AllFilesArray;
			for (auto &File : Files)
				AllFilesArray.f_Insert(fg_Move(File));

			CPropertyKey PropertyKey(mp_StringCache, EPropertyType_Target, "AllFiles_{}"_f << Type);

			f_AddExternalProperty(o_Target, PropertyKey.f_Reference(), fg_Move(AllFilesArray));
		}
	}

	bool CBuildSystem::f_ExpandTargetGroups(CExpandEntityState &_ExpandState, CBuildSystemData &_BuildSystemData, CEntity const &_Target) const
	{
		DRequire(_Target.f_GetKey().m_Type == EEntityType_Target);
		bool bChanged = false;
		auto fExpandGroup = [&]
			(
#ifndef DCompiler_Workaround_Apple_clang
				this
#endif
				auto &&_fThis
				, CEntity &_Entity
			) -> void
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; )
				{
					auto &Child = *iChild;
					++iChild;
					auto &Key = Child.f_GetKey();

					if (Key.m_Type == EEntityType_Group)
					{
						auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
						if (pLastInserted)
						{
							_ExpandState.m_OldEntitiesToRemove[&Child];

							if (pLastInserted != &Child)
							{
								iChild = pLastInserted;
								continue;
							}
						}
					}

					if
						(
							Key.m_Type == EEntityType_Target
							|| Key.m_Type == EEntityType_Group
							|| Key.m_Type == EEntityType_Import
						)
					{
						if (!Child.f_GetKey().m_Name.f_IsConstantString())
							continue;
						_fThis(Child);
					}
				}
			}
		;

#ifdef DCompiler_Workaround_Apple_clang
#define fExpandGroup(...) fExpandGroup(fExpandGroup, __VA_ARGS__)
#endif
		fExpandGroup(fg_RemoveQualifiers(_Target));

		return bChanged;
	}

	bool CBuildSystem::f_ExpandTargetFiles(CExpandEntityState &_ExpandState, CBuildSystemData &_BuildSystemData, CEntity const &_Target) const
	{
		DRequire(_Target.f_GetKey().m_Type == EEntityType_Target);
		bool bChanged = false;
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
						if (!Child.f_GetKey().m_Name.f_IsConstantString())
							continue;

						fExpandFile(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_File)
						continue;

					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
					if (!pLastInserted)
						continue;

					_ExpandState.m_OldEntitiesToRemove[&Child];

					if (pLastInserted != &Child)
					{
						iChild = pLastInserted;
						++iChild;
					}
				}
			}
		;

		fExpandFile(fg_RemoveQualifiers(_Target));

		return bChanged;
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
						if (!Child.f_GetKey().m_Name.f_IsConstantString())
						{
							CBuildSystemUniquePositions Positions;
							Positions.f_AddPosition(Child.f_GetFirstValidPosition(), "Entity");
							fs_ThrowError(Positions, "Expanding not supported");
							continue;
						}

						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_File)
						continue;

					bool bChanged = false;
					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
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
						if (!Child.f_GetKey().m_Name.f_IsConstantString())
						{
							CBuildSystemUniquePositions Positions;
							Positions.f_AddPosition(Child.f_GetFirstValidPosition(), "Entity");
							fs_ThrowError(Positions, "Expanding not supported");
							continue;
						}

						fExpandEntities(Child);
						continue;
					}

					if (Key.m_Type != EEntityType_Target)
						continue;

					bool bChanged = false;
					auto pLastInserted = fp_ExpandEntity(Child, _Entity, nullptr, bChanged);
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

	CEntity *CBuildSystem::fp_ExpandEntity(CEntity &_Entity, CEntity &_ParentEntity, TCVector<CEntity *> *o_pCreated, bool &o_bChanged) const
	{
		auto &Key = _Entity.f_GetKey();
		auto &EntityData = _Entity.f_Data();

		if (EntityData.m_ExpandedOrGeneratedFrom)
			return nullptr;

		TCVector<CEJsonSorted> Entities;

		CEvaluatedProperties TempProperties;
		TempProperties.m_pParentProperties = &_Entity.m_EvaluatedProperties;
		mint ExpandedOrGeneratedFromSource = _Entity.f_ExpandedOrGeneratedFromSource();

		if (Key.m_Name.f_IsConstantString())
		{
			if (Key.m_Type != EEntityType_File)
				return nullptr;
			else
				_Entity.f_DataWritable().m_ExpandedOrGeneratedFrom = ExpandedOrGeneratedFromSource;

			Entities.f_Insert(Key.m_Name.f_Constant());
		}
		else
		{
			CEvaluationContext EvalContext(&TempProperties);
			CBuildSystemUniquePositions StorePositions;
			CEvalPropertyValueContext Context{_Entity, _Entity, EntityData.m_Position, EvalContext, nullptr, f_EnablePositions(&StorePositions)};
			auto Data = fp_EvaluatePropertyValue(Context, Key.m_Name, nullptr);

			if (Data.f_Get().f_IsArray())
				Entities = Data.f_MoveArray();
			else
				Entities.f_Insert(Data.f_Move());
		}

		bool bAllowNonExisting = false;
		if (Key.m_Type == EEntityType_File)
		{
			CEvaluationContext EvalContext(&TempProperties);

			CBuildSystemPropertyInfo PropertyInfo;
			auto Data = fp_EvaluateEntityProperty(_Entity, _Entity, gc_ConstKey_Compile_AllowNonExisting, EvalContext, PropertyInfo, EntityData.m_Position, nullptr, false);
			auto &DataRef = Data.f_Get();
			if (DataRef.f_IsValid())
			{
				if (!DataRef.f_IsBoolean())
					fs_ThrowError(PropertyInfo, EntityData.m_Position, "Expected a boolean value");
				bAllowNonExisting = DataRef.f_Boolean();
			}
		}

		CEntity *pInsertAfter = &_Entity;
		Entities.f_Sort();
		Entities.f_UniqueIfSorted();

		for (auto const &SourceEntity : Entities)
		{
			CStr EntityName;

			NContainer::TCMap<CPropertyKey, CEvaluatedProperty> TempProperties;

			if (SourceEntity.f_IsObject())
			{
				auto *pName = SourceEntity.f_GetMember(gc_ConstString_Name, EJsonType_String);
				if (!pName)
					fs_ThrowError(EntityData.m_Position, "Expected a 'Name' for string type in entity object");

				auto *pProperties = SourceEntity.f_GetMember("Properties", EJsonType_Object);
				if (!pProperties)
					fs_ThrowError(EntityData.m_Position, "Expected a 'Properties' of object type in entity object");

				EntityName = pName->f_String();

				for (auto &SourceProperty : pProperties->f_Object())
				{
					CPropertyKey Key = CPropertyKey::fs_FromString(mp_StringCache, SourceProperty.f_Name(), EntityData.m_Position);

					auto &Property = TempProperties[Key];
					Property.m_Value = SourceProperty.f_Value();
					Property.m_Type = EEvaluatedPropertyType_External;
					Property.m_pProperty = &mp_ExternalProperty[Key.f_GetType()];
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
							TCVector<CStr> ExcludedFiles = f_EvaluateEntityPropertyStringArray(_Entity, gc_ConstKey_ExcludeFiles, TCVector<CStr>());

							for (auto &Exclude : ExcludedFiles)
							{
								if (Exclude == gc_ConstString_Symbol_OperatorGreaterThan.m_String)
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
								mp_FindCache.f_AddSourceFile(FindOptions.m_Path, nullptr);
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
							(*Child).f_DataWritable().m_Position = EntityData.m_Position;

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

					TCSet<mint> OldExpandedOrGeneratedFromSet;
					if (pOldEntity)
					{
						auto &OldEntityData = pOldEntity->f_Data();

						if
							(
								(OldEntityData.m_ExpandedOrGeneratedFrom && OldEntityData.m_ExpandedOrGeneratedFrom == ExpandedOrGeneratedFromSource)
								|| OldEntityData.m_ExpandedOrGeneratedFromSet.f_FindEqual(ExpandedOrGeneratedFromSource)
							)
						{
							if (o_pCreated)
								o_pCreated->f_Insert(pOldEntity);
							continue;
						}

						if (pOldEntity == &_Entity)
						{
							if (Files.f_GetLen() != 1 || Entities.f_GetLen() != 1)
								fsp_ThrowError(EntityData.m_Position, "Internal error: pOldEntity == &_Entity && (Files.f_GetLen() != 1 || Entities.f_GetLen() != 1)");

							return nullptr; // We don't need to do anything
						}

						OldExpandedOrGeneratedFromSet = OldEntityData.m_ExpandedOrGeneratedFromSet;
						if (OldEntityData.m_ExpandedOrGeneratedFrom)
							OldExpandedOrGeneratedFromSet[OldEntityData.m_ExpandedOrGeneratedFrom];

						if (pOldEntity == pInsertAfter)
							pInsertAfter = pParent->m_ChildEntitiesOrdered.fs_GetPrev(pInsertAfter);

						pParent->m_ChildEntitiesMap.f_Remove(pOldEntity);
					}

					o_bChanged = true;

					auto pNewEntity = fp_AddEntity(_Entity, *pParent, NewKey, pInsertAfterLocal ? pInsertAfter : nullptr, &TempProperties);

					auto &NewEntityData = pNewEntity->f_DataWritable();
					NewEntityData.m_ExpandedOrGeneratedFromSet = fg_Move(OldExpandedOrGeneratedFromSet);
					NewEntityData.m_ExpandedOrGeneratedFrom = ExpandedOrGeneratedFromSource;

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

				TCSet<mint> OldExpandedOrGeneratedFromSet;
				if (pOldEntity)
				{
					auto &OldEntityData = pOldEntity->f_Data();

					if
						(
							(OldEntityData.m_ExpandedOrGeneratedFrom && OldEntityData.m_ExpandedOrGeneratedFrom == ExpandedOrGeneratedFromSource)
							|| OldEntityData.m_ExpandedOrGeneratedFromSet.f_FindEqual(ExpandedOrGeneratedFromSource)
						)
					{
						if (o_pCreated)
							o_pCreated->f_Insert(pOldEntity);
						continue;
					}

					if (pOldEntity == &_Entity)
					{
						if (Entities.f_GetLen() != 1)
							fsp_ThrowError(EntityData.m_Position, "Internal error: pOldEntity == &_Entity && (Entities.f_GetLen() != 1)");
						return nullptr; // We don't need to do anything
					}

					OldExpandedOrGeneratedFromSet = OldEntityData.m_ExpandedOrGeneratedFromSet;
					if (OldEntityData.m_ExpandedOrGeneratedFrom)
						OldExpandedOrGeneratedFromSet[OldEntityData.m_ExpandedOrGeneratedFrom];

					if (pOldEntity == pInsertAfter)
						pInsertAfter = _ParentEntity.m_ChildEntitiesOrdered.fs_GetPrev(pInsertAfter);

					_ParentEntity.m_ChildEntitiesMap.f_Remove(pOldEntity);
				}

				o_bChanged = true;

				auto pNewEntity = fp_AddEntity(_Entity, _ParentEntity, NewKey, pInsertAfter, &TempProperties);

				auto &NewEntityData = pNewEntity->f_DataWritable();
				NewEntityData.m_ExpandedOrGeneratedFromSet = fg_Move(OldExpandedOrGeneratedFromSet);
				NewEntityData.m_ExpandedOrGeneratedFrom = ExpandedOrGeneratedFromSource;

				pInsertAfter = pNewEntity;

				if (o_pCreated)
					o_pCreated->f_Insert(pNewEntity);
			}
		}

		return pInsertAfter;
	}
}
