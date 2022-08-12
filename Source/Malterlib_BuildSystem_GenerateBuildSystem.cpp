// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_GenerateBuildSystem_Workspace
		(
			CConfiguration const &_Config
			, CConfiguraitonData const &_ConfigData
			, CWorkspaceInfo *_pWorkspace
			, TCSet<CStr> const &_ReservedGroups
			, CStr const &_DependencyFilesName
		) const
	{
		DMibFastCheck(_DependencyFilesName.f_IsEmpty() || _DependencyFilesName.f_IsConstant());
		CPropertyKeyReference DependencyFileNamePropertyKey(CAssertAddedToStringCache(), EPropertyType_Property, _DependencyFilesName, _DependencyFilesName.f_Hash());

		auto &Workspace = *_pWorkspace;
		auto &ConfigData = _ConfigData;
		auto &Config = _Config;
		auto &Targets = ConfigData.m_Targets;

		Workspace.m_pEvaluated = fg_Construct(_ConfigData.m_Evaluated);
		auto &Evaluated = *Workspace.m_pEvaluated;

		CEntityKey Key;
		Key.m_Type = EEntityType_Workspace;
		Key.m_Name = CBuildSystemSyntax::CValue{_pWorkspace->m_EntityName};

		auto pChild = Evaluated.m_RootEntity.m_ChildEntitiesMap.f_FindEqual(Key);
		DMibCheck(pChild);
		Workspace.m_pEntity = fg_Explicit(pChild);

		TCMap<CStr, CEntityMutablePointer> WorkspaceTargets;

		for (auto &pTarget : Targets)
		{
			TCVector<CEntityKey> Keys;
			for (auto *pEntity = pTarget.f_Get(); pEntity && pEntity->m_pParent; pEntity = pEntity->m_pParent)
				Keys.f_Insert(pEntity->f_GetKey());

			Keys = Keys.f_Reverse();

			CEntity *pEntity = &Evaluated.m_RootEntity;

			for (auto &Key : Keys)
			{
				auto pNextEntity = pEntity->m_ChildEntitiesMap.f_FindEqual(Key);;
				DMibFastCheck(pNextEntity);
				pEntity = pNextEntity;
			}

			WorkspaceTargets[Targets.fs_GetKey(pTarget)] = fg_Explicit(pEntity);
		}

		f_ExpandWorkspaceTargets(Evaluated, *Workspace.m_pEntity);
		auto AlreadyCreatedGroupsPerEntity = f_EvaluateTargetsInWorkspace(Evaluated, WorkspaceTargets, *Workspace.m_pEntity);

		TCVector<CStr> AllTargets;
		TCSet<CStr> TargetEntities;
		TCFunction<void (CEntity &_Entity, CGroupInfo *_pParentGroup)> fFindTargetsRecursive;
		auto fAddTarget = [&](CEntity &_Entity, CGroupInfo *_pParentGroup) -> CTargetInfo *
			{
				auto &EntityName = _Entity.f_GetKeyName();
				auto &EntityKey = _Entity.f_GetKey();

				auto *pTarget = _Entity.m_ChildEntitiesMap.f_FindEqual(EntityKey);
				if (!pTarget)
				{
					for
					(
						auto pSmallest = _Entity.m_ChildEntitiesMap.f_FindSmallest()
						; pSmallest
						; pSmallest = pSmallest->m_ChildEntitiesMap.f_FindSmallest()
					)
					{
						pTarget = pSmallest->m_ChildEntitiesMap.f_FindEqual(EntityKey);
						if (pTarget)
							break;

					}
				}

				if (!pTarget)
				{
					fs_ThrowError
						(
							_Entity.f_Data().m_Position
							, CStr::CFormat("Dependency target '{}' not found for configuration: {}")
							<< EntityName
							<< Config.f_GetFullName()
						 )
					;
				}

				auto &TargetEntity = *pTarget;

				CStr Name = f_EvaluateEntityPropertyString(TargetEntity, gc_ConstKey_Target_Name);

				CTargetInfo *pTargetInfo;
				TargetEntities[Name];
				pTargetInfo = &(*Workspace.m_Targets(Name, &Workspace));
				AllTargets.f_Insert(TargetEntity.f_GetPathForGetProperty());

				auto &Target = *pTargetInfo;
				if (Target.m_EntityName.f_IsEmpty())
					Target.m_EntityName = EntityName;
				else if (Target.m_pGroup != _pParentGroup)
					fs_ThrowError(_Entity.f_Data().m_Position, "Same target specified twice with same name in workspace is not supported");
				else if (Target.m_EntityName != TargetEntity.f_GetKeyName())
				{
					fs_ThrowError
						(
							TargetEntity.f_Data().m_Position
							, CStr::CFormat("A entity with this name already exists ({} != {})")
							<< Target.m_EntityName
							<< TargetEntity.f_GetKey().m_Name
						)
					;
				}

				Target.m_pInnerEntity = fg_Explicit(&TargetEntity);
				Target.m_pOuterEntity = fg_Explicit(&_Entity);
				Target.m_pGroup = _pParentGroup;
				if (_pParentGroup)
					_pParentGroup->m_Children.f_Insert(Target);
				else
					Workspace.m_RootGroup.m_Children.f_Insert(Target);

				return pTargetInfo;
			}
		;
		auto fFindTargets
			= [&](CEntity &_Entity, CGroupInfo *_pParentGroup)
			{
				for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
				{
					auto &ChildEntity = *iEntity;
					switch (ChildEntity.f_GetKey().m_Type)
					{
					case EEntityType_Import:
						{
							fFindTargetsRecursive(ChildEntity, _pParentGroup);
						}
						break;
					case EEntityType_Group:
						{
							auto &Name = ChildEntity.f_GetKeyName();
							if (Name.f_IsEmpty())
								fs_ThrowError(ChildEntity.f_Data().m_Position, "No name specified for workspace group");
							CStr Fullpath;
							if (_pParentGroup)
								Fullpath = CFile::fs_AppendPath(_pParentGroup->f_GetPath(), Name);
							else
								Fullpath = Name;

							auto &Group = Workspace.m_Groups[Fullpath];
							Group.m_Name = Name;
							Group.m_pParent = _pParentGroup;
							if (_pParentGroup)
								_pParentGroup->m_Children.f_Insert(Group);
							else
								Workspace.m_RootGroup.m_Children.f_Insert(Group);

							fFindTargetsRecursive(ChildEntity, &Group);
						}
						break;
					case EEntityType_Target:
						{
							auto *pTargetInfo = fAddTarget(ChildEntity, _pParentGroup);
							auto pCreatedGroups = AlreadyCreatedGroupsPerEntity.f_FindEqual(&ChildEntity);

							if (!pCreatedGroups)
								fs_ThrowError(ChildEntity.f_Data().m_Position, "Internal error: No created group info");

							pTargetInfo->m_AlreadyAddedGroups = fg_Move(*pCreatedGroups);

							AlreadyCreatedGroupsPerEntity.f_Remove(pCreatedGroups);
						}
						break;
					default:
						break;
					}
				}
			}
		;

		fFindTargetsRecursive = fFindTargets;
		fFindTargets(*Workspace.m_pEntity, nullptr);

		// First add properties
		bool bDependencyAdded = true;
		while (bDependencyAdded)
		{
			bDependencyAdded = false;

			TCSet<CTargetInfo *> TargetsToProcess;
			for (auto &TargetInfo : Workspace.m_Targets)
				TargetsToProcess[&TargetInfo];

			TCSet<CTargetInfo *> TargetsAlreadyProcessed;

			while (!TargetsToProcess.f_IsEmpty())
			{
				TargetsAlreadyProcessed += TargetsToProcess;

				TCSet<CTargetInfo *> NewTargetsToProcess;

				auto fGetTarget
					= [&](CStr const &_EntityName, CStr const &_TargetName, CFilePosition const &_Position, CEJSONSorted &&_Properties) -> CTargetInfo *
					{
						CTargetInfo *pDependentTarget;
						bool bTargetEntity;
						{
							pDependentTarget = Workspace.m_Targets.f_FindEqual(_TargetName);
							bTargetEntity = TargetEntities.f_FindEqual(_TargetName);
						}

						if (pDependentTarget)
							return pDependentTarget;

						CEntityMutablePointer *pSourceTarget = WorkspaceTargets.f_FindEqual(_EntityName);
						if (!pSourceTarget || bTargetEntity)
						{
							if (!pDependentTarget)
							{
								fs_ThrowError
									(
										_Position
										, CStr::CFormat("Could not find dependency '{}' using entity name '{}' in workspace")
										<< _TargetName
										<< _EntityName
									)
								;
							}
							return pDependentTarget;
						}

						pDependentTarget = Workspace.m_Targets.f_FindEqual(_TargetName);
						if (pDependentTarget)
							return pDependentTarget;

						CEntityKey EntityKey;
						EntityKey.m_Type = EEntityType_Target;
						EntityKey.m_Name.m_Value = _EntityName;

						TCMap<CPropertyKey, CEvaluatedProperty> EvaluatedProperties;
						for (auto &Property : _Properties.f_Object())
						{
							CPropertyKey Key = CPropertyKey::fs_FromString(mp_StringCache, Property.f_Name(), _Position);

							auto &EvalProperty = EvaluatedProperties[Key];
							EvalProperty.m_Value = fg_Move(Property.f_Value());
							EvalProperty.m_Type = EEvaluatedPropertyType_External;
							EvalProperty.m_pProperty = &mp_ExternalProperty[Key.f_GetType()];
						}

						CEntity *pNewTarget;
						CGroupInfo *pParentGroupInfo = nullptr;
						{
							CStr Group;
							{
								CBuildSystemPropertyInfo PropertyInfo;
								Group = f_EvaluateEntityPropertyUncachedString
									(
										**pSourceTarget
										, gc_ConstKey_Target_Group
										, PropertyInfo
										, &EvaluatedProperties
										, CStr()
									)
								;
							}

							auto pParentGroup = Workspace.m_pEntity.f_Get();

							while (!Group.f_IsEmpty())
							{
								CStr ParentGroup = fg_GetStrSep(Group, "/");
								if (ParentGroup.f_StartsWith("{"))
									continue;

								CEntityKey Key;
								Key.m_Type = EEntityType_Group;
								Key.m_Name.m_Value = ParentGroup;
								auto Child = pParentGroup->m_ChildEntitiesMap(Key, pParentGroup);
								(*Child).f_DataWritable().m_Position = pParentGroup->f_Data().m_Position;

								pParentGroup = &*Child;

								CStr Fullpath;
								if (pParentGroupInfo)
									Fullpath = CFile::fs_AppendPath(pParentGroupInfo->f_GetPath(), ParentGroup);
								else
									Fullpath = ParentGroup;

								auto &Group = Workspace.m_Groups[Fullpath];
								Group.m_Name = ParentGroup;
								Group.m_pParent = pParentGroupInfo;
								if (pParentGroupInfo)
									pParentGroupInfo->m_Children.f_Insert(Group);
								else
									Workspace.m_RootGroup.m_Children.f_Insert(Group);

								pParentGroupInfo = &Group;

							}
							if (pParentGroup->m_ChildEntitiesMap.f_FindEqual(EntityKey))
							{
								fs_ThrowError
									(
										_Position
										, CStr::CFormat("The target entity with name '{}' has already been added to this group")
										<< EntityKey.m_Name
									)
								;
							}
							auto &NewTarget = pParentGroup->m_ChildEntitiesMap(EntityKey, pParentGroup).f_GetResult();
							NewTarget.f_DataWritable().m_Position = _Position;
							pNewTarget = &NewTarget;
						}

						pNewTarget->m_EvaluatedProperties.m_Properties += EvaluatedProperties;

						TCSet<CStr> AlreadyAddedGroups;
						fp_ExpandEntity(*pNewTarget, *pNewTarget->m_pParent, nullptr);
						f_EvaluateTarget(Evaluated, WorkspaceTargets, *pNewTarget, AlreadyAddedGroups);

						CTargetInfo *pTargetInfo = fAddTarget(*pNewTarget, pParentGroupInfo);
						pTargetInfo->m_AlreadyAddedGroups = fg_Move(AlreadyAddedGroups);

						bDependencyAdded = true;

						CStr ResultingName = pTargetInfo->f_GetName();
						if (ResultingName != _TargetName)
						{
							fs_ThrowError
								(
									_Position
									, CStr::CFormat
									(
										"When a target dependency is not already part of the workspace,"
										" the name of the dependency needs to be the same as the target entity name ('{}' != '{}'),"
										" or you need to specify the resulting dependency name: {{Name: \"TargetEntity\", "
										"Properties: {{Dependency.TargetName: \"TargetEntityName\"}}"
									)
									<< ResultingName
									<< _TargetName
								)
							;
						}

						f_ExpandTargetDependenciesBackup(Evaluated, *pTargetInfo->m_pOuterEntity, pTargetInfo->m_DependenciesBackup);

						if (!TargetsAlreadyProcessed.f_FindEqual(pTargetInfo))
							NewTargetsToProcess[pTargetInfo];

						return pTargetInfo;
					}
				;

				fg_ForEach
					(
						TargetsToProcess
						, [&](CTargetInfo *_pTarget)
						{
							_pTarget->m_DependenciesMap.f_Clear();

							f_ExpandTargetDependencies(Workspace, Evaluated, *_pTarget->m_pOuterEntity, _pTarget->m_DependenciesBackup);

							_pTarget->m_bIsExpanded = true;

							TCMap<CTargetInfo *, zbool> ToProcess;
							TCSet<CEntity *> CreatedThisTime;
							CEntity *pDistinguisher = nullptr;
							TCFunction<void (CEntity &_Entity, bool _bRecursive, bool _bIndirect)> fFindDependencies
								= [&](CEntity &_Entity, bool _bRecursive, bool _bIndirect)
								{
									TCLinkedList<TCTuple<CEntity *, zbool>> Dependencies;
									for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
									{
										auto &ChildEntity = *iEntity;

										switch (ChildEntity.f_GetKey().m_Type)
										{
										case EEntityType_Group:
										case EEntityType_Target:
										case EEntityType_Import:
											{
												auto const *pToProcess = &ChildEntity;

												fFindDependencies(fg_RemoveQualifiers(*pToProcess), _bRecursive, _bIndirect);
											}
											break;
										case EEntityType_Dependency:
											{
												DMibRequire(ChildEntity.f_GetKey().m_Name.f_IsConstantString());
												Dependencies.f_InsertLast(fg_Tuple(&ChildEntity, false));
											}
											break;
										default:
											break;
										}
									}

									while (!Dependencies.f_IsEmpty())
									{
										auto Dependency = Dependencies.f_Pop();
										auto &ChildEntity = *fg_Get<0>(Dependency);
										bool bExpandedDependency = fg_Get<1>(Dependency);

										if (!f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Dependency_Type, CStr()).f_IsEmpty())
										{
											// External dependency

											auto Name = f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Dependency_Name);
											auto &Dep = _pTarget->m_DependenciesMap(Name).f_GetResult();
											Dep.m_bExternal = true;
											_pTarget->m_DependenciesOrdered.f_Insert(Dep);
											Dep.m_pEntity = fg_Explicit(&ChildEntity);
											Dep.m_Position = ChildEntity.f_Data().m_Position;
											continue;
										}

										CStr TargetName = ChildEntity.f_GetKeyName();
										CStr EntityName = f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Dependency_Target, ChildEntity.f_GetKeyName());

										auto pEntity = &ChildEntity;
										bool bIndirect = true;
										if (_bRecursive)
										{
											if (_pTarget->m_DependenciesMap.f_FindEqual(TargetName))
												continue; // We already have dependency

											if (!bExpandedDependency)
											{
												TCVector<CEntity const *> Parents;

												{
													auto *pSourceParent = &ChildEntity;
													while (pSourceParent && pSourceParent->f_GetKey().m_Type != EEntityType_Target)
													{
														Parents.f_Insert(pSourceParent);
														pSourceParent = pSourceParent->m_pParent;
													}
													Parents = Parents.f_Reverse();
												}

												TCVector<CEntity *> ToDelete;
												CEntity *pParent = _pTarget->m_pInnerEntity.f_Get();

												bool bCreatedDistinguisher = false;
												if (auto pRoot = pParent->m_ChildEntitiesMap.f_FindEqual(Parents.f_GetFirst()->f_GetKey()))
												{
													if (!CreatedThisTime.f_FindEqual(pRoot))
													{
														if (!pDistinguisher)
														{
															CEntityKey Key;
															Key.m_Type = EEntityType_Group;
															mint iDistinguish = 0;
															Key.m_Name.m_Value = fg_Format("Dependency Distinguisher {}", iDistinguish);
															while (pParent->m_ChildEntitiesMap.f_FindEqual(Key))
															{
																++iDistinguish;
																Key.m_Name.m_Value = fg_Format("Dependency Distinguisher {}", iDistinguish);
															}
															auto Mapped = pParent->m_ChildEntitiesMap(Key, pParent);
															auto &NewDependency = Mapped.f_GetResult();
															NewDependency.f_DataWritable().m_Position = pParent->f_Data().m_Position;
															pDistinguisher = &NewDependency;
															ToDelete.f_Insert(&NewDependency);
															bCreatedDistinguisher = true;
														}
														pParent = pDistinguisher;
													}
												}

												CEntity *pRootCreated = nullptr;
												bool bAborted = false;
												for (auto &pSourceParent : Parents)
												{
													auto &SourceParentKey = pSourceParent->f_GetKey();
													auto Mapped = pParent->m_ChildEntitiesMap
														(
															SourceParentKey
															, *pSourceParent
															, pParent
															, EEntityCopyFlag_None
														)
													;
													if (Mapped.f_WasCreated())
													{
														auto &NewDependency = Mapped.f_GetResult();

														if (NewDependency.f_GetKey().m_Type == EEntityType_Dependency)
														{
															TCVector<CEntity *> CreatedDependencies;
															if (fp_ExpandEntity(NewDependency, *pParent, &CreatedDependencies))
															{
																pParent->m_ChildEntitiesMap.f_Remove(SourceParentKey);

																for (auto &pDependency : CreatedDependencies)
																{
																	DMibRequire(pDependency->f_GetKey().m_Name.f_IsConstantString());
																	Dependencies.f_Insert(fg_Tuple(pDependency, true));
																}
																bAborted = true;
																break;
															}
														}

														CreatedThisTime[&NewDependency];
														ToDelete.f_Insert(&NewDependency);
														pParent = &NewDependency;
														if (!pRootCreated)
															pRootCreated = &NewDependency;
													}
													else
													{
														pParent = &Mapped.f_GetResult();
														DCheck(pSourceParent->f_GetKey().m_Type != EEntityType_Dependency);
														DCheck(CreatedThisTime.f_FindEqual(&Mapped.f_GetResult()));
													}
												}

												if (bAborted)
												{
													if (bCreatedDistinguisher)
														pDistinguisher = nullptr;
													continue;
												}

												TCSet<CEntity *> Deleted;
												fpr_EvaluateData(*pRootCreated, Deleted);
												pEntity = pParent;

												if
												(
													Deleted.f_FindEqual(pEntity)
													|| !f_EvaluateEntityPropertyBool(*pEntity, gc_ConstKey_Dependency_Indirect, false)
												)
												{
													ToDelete = ToDelete.f_Reverse();
													for (auto &pToDelete : ToDelete)
													{
														CreatedThisTime.f_Remove(pToDelete);
														if (pDistinguisher == pToDelete)
															pDistinguisher = nullptr;
														if (!Deleted.f_FindEqual(pToDelete))
															pToDelete->m_pParent->m_ChildEntitiesMap.f_Remove(pToDelete);
													}
													continue;
												}
											}
											else
											{
												pEntity = &ChildEntity;

												if (!f_EvaluateEntityPropertyBool(*pEntity, gc_ConstKey_Dependency_Indirect, false))
												{
													CreatedThisTime.f_Remove(pEntity);
													pEntity->m_pParent->m_ChildEntitiesMap.f_Remove(pEntity);
													continue;
												}
											}
										}
										else
											bIndirect = f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Dependency_Indirect, false);

										DMibFastCheck(bIndirect == f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Dependency_Indirect, false));
										
										auto Properties = f_EvaluateEntityPropertyObject
											(
												ChildEntity
												, gc_ConstKey_Dependency_TargetProperties
												, CEJSONSorted(EJSONType_Object)
											)
										;

										CTargetInfo *pDependentTarget = fGetTarget(EntityName, TargetName, pEntity->f_Data().m_Position, Properties.f_Move());

										if (pDependentTarget->m_TriedDependenciesMap(pDependentTarget->f_GetName()).f_WasCreated())
											bDependencyAdded = true;

										auto &Dep = _pTarget->m_DependenciesMap(pDependentTarget->f_GetName()).f_GetResult();
										_pTarget->m_DependenciesOrdered.f_Insert(Dep);
										Dep.m_pEntity = fg_Explicit(pEntity);
										Dep.m_Position = pEntity->f_Data().m_Position;
										Dep.m_bIndirect = (bIndirect && !_bRecursive) || _bIndirect;

										bool bFollowIndirectDependencies = f_EvaluateEntityPropertyBool(*pDependentTarget->m_pInnerEntity, gc_ConstKey_Target_FollowIndirectDependencies, false);

										if (!bFollowIndirectDependencies)
											continue;

										if (!TargetsAlreadyProcessed.f_FindEqual(pDependentTarget))
											NewTargetsToProcess[pDependentTarget];

										ToProcess[pDependentTarget] = Dep.m_bIndirect;
									}
								}
							;

							fFindDependencies(*_pTarget->m_pOuterEntity, false, false);

							while (!ToProcess.f_IsEmpty())
							{
								auto ThisTime = fg_Move(ToProcess);
								for (auto &bIndirect : ThisTime)
								{
									auto &Depenency = *ThisTime.fs_GetKey(bIndirect);
									if (!Depenency.m_bIsExpanded)
										continue;

									fFindDependencies(*Depenency.m_pOuterEntity, true, bIndirect);
									pDistinguisher = nullptr;
									CreatedThisTime.f_Clear();
								}
							}

							TCVector<CStr> DependencyFiles;
							TCVector<CStr> Dependencies;
							TCVector<CStr> ExternalDependencies;
							TCVector<CStr> DependencyInjectionGroups;

							for (auto &Dependency : _pTarget->m_DependenciesOrdered)
							{
								if (Dependency.m_bExternal)
								{
									ExternalDependencies.f_Insert(Dependency.m_pEntity->f_GetPathForGetProperty());
									continue;
								}

								auto &TargetName = Dependency.f_GetName();

								CTargetInfo *pDependentTarget = Workspace.m_Targets.f_FindEqual(TargetName);
								if (!pDependentTarget)
									fs_ThrowError(Dependency.m_pEntity->f_Data().m_Position, "Internal error dependency not found: '{}'"_f << TargetName);

								DependencyInjectionGroups.f_Insert
									(
										f_EvaluateEntityPropertyStringArray(*pDependentTarget->m_pInnerEntity, gc_ConstKey_Target_DependencyInjectionGroups, TCVector<CStr>())
									)
								;

								if (Dependency.m_bIndirect)
									continue;

								bool bDoLink = f_EvaluateEntityPropertyBool(*Dependency.m_pEntity, gc_ConstKey_Dependency_Link, true);

								if (bDoLink)
								{
									Dependency.m_bLink = true;

									if (!_DependencyFilesName.f_IsEmpty())
									{
										CStr FileToAdd = f_EvaluateEntityPropertyString(*pDependentTarget->m_pInnerEntity, gc_ConstKey_Target_DependencyFile, CStr());
										if (!FileToAdd.f_IsEmpty())
											DependencyFiles.f_Insert(FileToAdd);
									}

								}
								Dependencies.f_Insert(pDependentTarget->m_pInnerEntity->f_GetPathForGetProperty());
							}

							auto fAddNameList = [&](CPropertyKeyReference const &_Key, TCVector<CStr> &&_Names) -> bool
								{
									CEJSONSorted NameListArray = EJSONType_Array;

									for (auto &Name : _Names)
										NameListArray.f_Insert(fg_Move(Name));
									
									return f_AddExternalProperty
										(
											fg_RemoveQualifiers(*_pTarget->m_pOuterEntity)
											, _Key
											, fg_Move(NameListArray)
										)
									;
								}
							;

							bool bChanged = false;

							if (!_DependencyFilesName.f_IsEmpty())
							{
								DependencyFiles.f_Sort();
								if (fAddNameList(DependencyFileNamePropertyKey, fg_Move(DependencyFiles)))
									bChanged = true;
							}

							Dependencies.f_Sort();
							bChanged = fAddNameList(gc_ConstKey_Target_Dependencies, fg_Move(Dependencies)) || bChanged;

							ExternalDependencies.f_Sort();
							bChanged = fAddNameList(gc_ConstKey_Target_ExternalDependencies, fg_Move(ExternalDependencies)) || bChanged;

							DependencyInjectionGroups.f_Sort();
							bChanged = fAddNameList(gc_ConstKey_Target_InjectedExtraGroups, fg_Move(DependencyInjectionGroups)) || bChanged;

							fp_UpdateDependenciesNames(_pTarget->m_pInnerEntity.f_Get());

							if (bChanged)
							{
								f_EvaluateTarget(Evaluated, WorkspaceTargets, *_pTarget->m_pOuterEntity.f_Get(), _pTarget->m_AlreadyAddedGroups);
								bDependencyAdded = true;
							}
						}
					)
				;
				TargetsToProcess = fg_Move(NewTargetsToProcess);
			}


			{
				AllTargets.f_Sort();
				CEJSONSorted AllTargetsArray;
				for (auto &Target : AllTargets)
					AllTargetsArray.f_Insert(Target);

				f_AddExternalProperty(*Workspace.m_pEntity, gc_ConstKey_Workspace_AllTargets, fg_Move(AllTargetsArray));
			}
		}

		// Then eval target specific
		fg_ForEach
			(
				Workspace.m_Targets
				, [&](CTargetInfo &_Target)
				{
					f_ExpandTargetGroups(Evaluated, *_Target.m_pOuterEntity);
					f_ExpandTargetFiles(Evaluated, *_Target.m_pOuterEntity);
					f_GenerateTargetFiles(Evaluated, *_Target.m_pOuterEntity);

					TCFunction<void (CEntity &_Entity, CGroupInfo *_pParentGroup)> fFindFilesRecursive;
					auto fFindFiles
						= [&](CEntity &_Entity, CGroupInfo *_pParentGroup)
						{
							for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
							{
								auto &ChildEntity = *iEntity;
								switch (ChildEntity.f_GetKey().m_Type)
								{
								case EEntityType_GenerateFile:
								case EEntityType_Target:
								case EEntityType_Import:
									{
										fFindFilesRecursive(ChildEntity, _pParentGroup);
									}
									break;
								case EEntityType_Group:
									{
										CEJSONSorted Hidden = f_GetExternalProperty
											(
												ChildEntity
												, gc_ConstKey_HiddenGroup
											)
										;
										if (Hidden.f_IsBoolean() && Hidden.f_Boolean())
											fFindFilesRecursive(ChildEntity, _pParentGroup);
										else
										{
											CStr Name = ChildEntity.f_GetKeyName();
											if (Name.f_IsEmpty())
												fs_ThrowError(ChildEntity.f_Data().m_Position, "No name specified for file group");
											CStr Fullpath;
											if (_pParentGroup)
												Fullpath = CFile::fs_AppendPath(_pParentGroup->f_GetPath(), Name);
											else
												Fullpath = Name;

											if (_ReservedGroups.f_FindEqual(Fullpath))
												fs_ThrowError(ChildEntity.f_Data().m_Position, "Group name '{}' conflicts with reserved group names"_f << Name);

											auto &Group = _Target.m_Groups[Fullpath];
											Group.m_Name = Name;
											Group.m_pParent = _pParentGroup;
											if (_pParentGroup)
												_pParentGroup->m_Children.f_Insert(Group);
											else
												_Target.m_RootGroup.m_Children.f_Insert(Group);
											fFindFilesRecursive(ChildEntity, &Group);
										}
									}
									break;
								case EEntityType_File:
									{
										CStr EntityName = ChildEntity.f_GetKeyName();

										CFileKey FileKey;
										FileKey.m_FileName = EntityName;
										if (_pParentGroup)
											FileKey.m_GroupPath = _pParentGroup->f_GetGroupPath();

										auto FileMap = _Target.m_Files(FileKey);

										auto &File = FileMap.f_GetResult();

										if (!FileMap.f_WasCreated())
										{
											if (File.m_pGroup != _pParentGroup)
											{
												TCVector<CBuildSystemError> Errors;
												Errors.f_Insert({CBuildSystemUniquePositions(File.m_pEntity->f_Data().m_Position, "File entity"), "Previous entity"});
												Errors.f_Insert({CBuildSystemUniquePositions(ChildEntity.f_Data().m_Position, gc_ConstString_Entity), "Current entity"});

												fs_ThrowError
													(
														ChildEntity.f_Data().m_Position
														, "Same file specified twice in project is not supported. FileName: {}. GroupPath: {}"_f
														<< EntityName
														<< FileKey.m_GroupPath
														, Errors
													)
												;
											}
										}

										File.m_pEntity = fg_Explicit(&ChildEntity);
										File.m_pGroup = _pParentGroup;
										if (_pParentGroup)
											_pParentGroup->m_Children.f_Insert(File);
										else
											_Target.m_RootGroup.m_Children.f_Insert(File);

									}
									break;
								default:
									break;
								}
							}
						}
					;
					fFindFilesRecursive = fFindFiles;

					fFindFiles(*_Target.m_pOuterEntity, nullptr);

					_Target.m_RootGroup.fr_PruneEmpty();
					for (auto iGroup = _Target.m_Groups.f_GetIterator(); iGroup; )
					{
						if (iGroup->m_Children.f_IsEmpty())
							iGroup.f_Remove();
						else
							++iGroup;
					}
				}
			)
		;

		f_ExpandWorkspaceEntities(Evaluated, *Workspace.m_pEntity);
		f_GenerateWorkspaceFiles(Evaluated, *Workspace.m_pEntity);

		Workspace.m_RootGroup.fr_PruneEmpty();
		for (auto iGroup = Workspace.m_Groups.f_GetIterator(); iGroup; )
		{
			if (iGroup->m_Children.f_IsEmpty())
				iGroup.f_Remove();
			else
				++iGroup;
		}
	}

	TCFuture<void> CBuildSystem::f_GenerateBuildSystem
		(
			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> *o_pConfigurations
			, TCMap<CPropertyKey, CEJSONSorted> const *_pValues
		) const
	{
		{
			NContainer::TCSet<NStr::CStr> SourceFiles;
			{
				DMibLockRead(mp_SourceFilesLock);
				SourceFiles = mp_SourceFiles;
			}

			co_await fg_ParallelForEach
				(
					*o_pConfigurations
					, [&](TCUniquePointer<CConfiguraitonData> &o_pConfig) -> TCFuture<void>
					{
						o_pConfig->m_Evaluated.m_MutableSourceFiles = SourceFiles;
						co_return {};
					}
					, mp_bSingleThreaded
				)
			;
		}

		co_await fg_ParallelForEach
			(
				*o_pConfigurations
				, [&](TCUniquePointer<CConfiguraitonData> &o_pConfig) -> TCFuture<void>
				{
					co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureExceptions);
					co_await f_CheckCancelled();

					auto &ConfigData = *o_pConfig;
					TCMap<CPropertyKey, CEJSONSorted> ConfigValues = *_pValues;
					for (auto iTuple = ConfigData.m_Tuples.f_GetIterator(); iTuple; ++iTuple)
					{
						if (iTuple->m_Name)
							ConfigValues[CPropertyKey(mp_StringCache, iTuple->m_Type)] = iTuple->m_Name;
						else
							ConfigValues[CPropertyKey(mp_StringCache, iTuple->m_Type)] = CEJSONSorted{};
					}

					f_EvaluateDataMain(ConfigData.m_Evaluated, ConfigValues);
					f_GenerateGlobalFiles(ConfigData.m_Evaluated, true);
					f_ExpandDynamicImports(ConfigData.m_Evaluated);
					f_ExpandGlobalTargetsAndWorkspaces(ConfigData.m_Evaluated);
					f_EvalGlobalWorkspaces(ConfigData.m_Evaluated, ConfigData.m_Targets);
					f_ExpandGlobalEntities(ConfigData.m_Evaluated);
					f_GenerateGlobalFiles(ConfigData.m_Evaluated, false);

					// Find workspaces
					for (auto iEntity = ConfigData.m_Evaluated.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
					{
						auto &Entity = *iEntity;
						auto &Key = Entity.f_GetKey();

						if (Key.m_Type != EEntityType_Workspace)
							continue;

						if (!f_EvaluateEntityPropertyBool(Entity, gc_ConstKey_Workspace_Enabled, true))
							continue;

						CStr Name = f_EvaluateEntityPropertyString(Entity, gc_ConstKey_Workspace_Name);

						auto &Workspace = **ConfigData.m_Workspaces(Name, fg_Construct());
 						auto &EntityName = Entity.f_GetKeyName();

						if (Workspace.m_EntityName.f_IsEmpty())
							Workspace.m_EntityName = EntityName;
						else if (Workspace.m_EntityName != EntityName)
						{
							fs_ThrowError
								(
									Entity.f_Data().m_Position
									, CStr::CFormat("A entity with this name already exists ({} != {})")
									<< EntityName
									<< Workspace.m_EntityName
								)
							;
						}
					}
					
					co_return {};
				}
				, mp_bSingleThreaded
			)
		;
		co_await f_CheckCancelled();

		co_return {};
	}
}
