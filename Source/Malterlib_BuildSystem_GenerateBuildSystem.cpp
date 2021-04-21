// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_GenerateBuildSystem
		(
			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> &o_Configurations
			, TCMap<CPropertyKey, CEJSON> const &_Values
			, TCSet<CStr> const &_ReservedGroups
			, CStr const &_DependencyFilesName
		) const
	{
		{
			DMibLockRead(mp_SourceFilesLock);
			fg_ParallellForEach
				(
					o_Configurations
					, [&](TCUniquePointer<CConfiguraitonData> &_pConfig)
					{
						_pConfig->m_Evaluated.m_SourceFiles = mp_SourceFiles;
					}
				)
			;
		}

		fg_ParallellForEach
			(
				o_Configurations
				, [&](TCUniquePointer<CConfiguraitonData> &_pConfig)
				{
					TCMap<CPropertyKey, CEJSON> ConfigValues = _Values;
					for (auto iTuple = _pConfig->m_Tuples.f_GetIterator(); iTuple; ++iTuple)
					{
						if (iTuple->m_Name)
							ConfigValues[CPropertyKey(iTuple->m_Type)] = iTuple->m_Name;
						else
							ConfigValues[CPropertyKey(iTuple->m_Type)] = CEJSON{};
					}

					f_EvaluateDataMain(_pConfig->m_Evaluated, ConfigValues);
					f_ExpandDynamicImports(_pConfig->m_Evaluated);
					f_ExpandGlobalTargetsAndWorkspaces(_pConfig->m_Evaluated);
					f_EvalGlobalWorkspaces(_pConfig->m_Evaluated, _pConfig->m_Targets);
					f_ExpandGlobalEntities(_pConfig->m_Evaluated);
					f_GenerateGlobalFiles(_pConfig->m_Evaluated);

					auto &ConfigData = *_pConfig;

					// Find workspaces
					for (auto iEntity = ConfigData.m_Evaluated.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
					{
						auto &Entity = *iEntity;
						auto &Key = Entity.f_GetKey();

						if (Key.m_Type != EEntityType_Workspace)
							continue;

						if (!f_EvaluateEntityPropertyBool(Entity, EPropertyType_Workspace, "Enabled", true))
							continue;

						CStr Name = f_EvaluateEntityPropertyString(Entity, EPropertyType_Workspace, "Name");

						auto &Workspace = *ConfigData.m_Workspaces(Name, ConfigData.m_Evaluated);
						auto pChild = Workspace.m_Evaluated.m_RootEntity.m_ChildEntitiesMap.f_FindEqual(Key);
						DMibCheck(pChild);
						Workspace.m_pEntity = fg_Explicit(pChild);
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
				}
			)
		;

		struct CWorkspaceConfigPair
		{
			CConfiguration const &m_Config;
			CConfiguraitonData *m_pConfigData;
			CWorkspaceInfo *m_pWorkspace;
		};

		TCVector<CWorkspaceConfigPair> WorkspaceConfigPairs;

		for (auto &pConfig : o_Configurations)
		{
			for (auto &Workspace : pConfig->m_Workspaces)
				WorkspaceConfigPairs.f_Insert({o_Configurations.fs_GetKey(pConfig), pConfig.f_Get(), &Workspace});
		}

		fg_ParallellForEach
			(
				WorkspaceConfigPairs
				, [&](CWorkspaceConfigPair &_Pair)
				{
					auto &Workspace = *_Pair.m_pWorkspace;
					auto &ConfigData = *_Pair.m_pConfigData;
					auto &Config = _Pair.m_Config;
					auto &Targets = ConfigData.m_Targets;

					TCMap<CStr, CEntity *> WorkspaceTargets;

					for (auto &pTarget : Targets)
					{
						TCVector<CEntityKey> Keys;
						for (auto *pEntity = pTarget; pEntity && pEntity->m_pParent; pEntity = pEntity->m_pParent)
							Keys.f_Insert(pEntity->f_GetKey());

						Keys = Keys.f_Reverse();

						CEntity *pEntity = &Workspace.m_Evaluated.m_RootEntity;

						for (auto &Key : Keys)
							pEntity = pEntity->m_ChildEntitiesMap.f_FindEqual(Key);

						WorkspaceTargets[Targets.fs_GetKey(pTarget)] = pEntity;
					}

					f_ExpandWorkspaceTargets(Workspace.m_Evaluated, *Workspace.m_pEntity);
					f_EvaluateTargetsInWorkspace(Workspace.m_Evaluated, WorkspaceTargets, *Workspace.m_pEntity);

					TCVector<CStr> AllTargets;
					TCSet<CStr> TargetEntities;
					TCFunction<void (CEntity &_Entity, CGroupInfo *_pParentGroup)> fFindTargetsRecursive;
					auto fAddTarget
						= [&](CEntity &_Entity, CGroupInfo *_pParentGroup) -> CTargetInfo *
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

							CStr Name = f_EvaluateEntityPropertyString(TargetEntity, EPropertyType_Target, "Name");

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
										fAddTarget(ChildEntity, _pParentGroup);
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

						TCVector<CTargetInfo *> TargetsToProcess;
						for (auto &TargetInfo : Workspace.m_Targets)
							TargetsToProcess.f_Insert(&TargetInfo);

						while (!TargetsToProcess.f_IsEmpty())
						{
							TCVector<CTargetInfo *> NewTargetsToProcess;

							auto fGetTarget
								= [&](CStr const &_EntityName, CStr const &_TargetName, CFilePosition const &_Position, CEJSON const &_Properties) -> CTargetInfo *
								{
									CTargetInfo *pDependentTarget;
									bool bTargetEntity;
									{
										pDependentTarget = Workspace.m_Targets.f_FindEqual(_TargetName);
										bTargetEntity = TargetEntities.f_FindEqual(_TargetName);
									}

									if (pDependentTarget)
										return pDependentTarget;

									CEntity **pSourceTarget = WorkspaceTargets.f_FindEqual(_EntityName);
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
										CPropertyKey Key = CPropertyKey::fs_FromString(Property.f_Name(), _Position);

										auto &EvalProperty = EvaluatedProperties[Key];
										EvalProperty.m_Value = Property.f_Value();
										EvalProperty.m_Type = EEvaluatedPropertyType_External;
										EvalProperty.m_pProperty = &mp_ExternalProperty[Key.m_Type];
									}

									CEntity *pNewTarget;
									CGroupInfo *pParentGroupInfo = nullptr;
									{
										CStr Group;
										{
											CProperty const *pFromProperty = nullptr;
											Group = f_EvaluateEntityPropertyUncachedString
												(
													**pSourceTarget
													, EPropertyType_Target
													, "Group"
													, pFromProperty
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

									fp_ExpandEntity(*pNewTarget, *pNewTarget->m_pParent, nullptr);
									f_EvaluateTarget(ConfigData.m_Evaluated, WorkspaceTargets, *pNewTarget);
									CTargetInfo *pTargetInfo = fAddTarget(*pNewTarget, pParentGroupInfo);
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

									f_ExpandTargetDependenciesBackup(Workspace.m_Evaluated, *pTargetInfo->m_pOuterEntity, pTargetInfo->m_DependenciesBackup);

									NewTargetsToProcess.f_Insert(pTargetInfo);

									return pTargetInfo;
								}
							;

							fg_ForEach
							//fg_ParallellForEach. Some race condition in this code
								(
									TargetsToProcess
									, [&](CTargetInfo *_pTarget)
									{
										_pTarget->m_DependenciesMap.f_Clear();
										f_ExpandTargetDependencies(Workspace.m_Evaluated, *_pTarget->m_pOuterEntity, _pTarget->m_DependenciesBackup);

										TCSet<CTargetInfo *> ToProcess;
										TCSet<CEntity *> CreatedThisTime;
										CEntity *pDistinguisher = nullptr;
										TCFunction<void (CEntity &_Entity, bool _bRecursive)> fFindDependencies
											= [&](CEntity &_Entity, bool _bRecursive)
											{
												TCLinkedList<TCTuple<CEntity *, zbool>> Dependencies;
												for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
												{
													auto &ChildEntity = *iEntity;

													switch (ChildEntity.f_GetKey().m_Type)
													{
													case EEntityType_Group:
													case EEntityType_Target:
														{
															auto const *pToProcess = &ChildEntity;

															fFindDependencies(fg_RemoveQualifiers(*pToProcess), _bRecursive);
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

													CStr TargetName = ChildEntity.f_GetKeyName();
													CStr EntityName = f_EvaluateEntityPropertyString(ChildEntity, EPropertyType_Dependency, "Target", ChildEntity.f_GetKeyName());

													auto pEntity = &ChildEntity;
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
																|| !f_EvaluateEntityPropertyBool(*pEntity, EPropertyType_Dependency, "Indirect", false)
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

															if (!f_EvaluateEntityPropertyBool(*pEntity, EPropertyType_Dependency, "Indirect", false))
															{
																CreatedThisTime.f_Remove(pEntity);
																pEntity->m_pParent->m_ChildEntitiesMap.f_Remove(pEntity);
																continue;
															}
														}
													}
													else
													{
														bool bIndirect = f_EvaluateEntityPropertyBool(ChildEntity, EPropertyType_Dependency, "Indirect", false);

														if (bIndirect)
															continue;
													}

													CEJSON Properties = f_EvaluateEntityPropertyObject
														(
															ChildEntity
															, EPropertyType_Dependency
															, "TargetProperties"
															, CEJSON(EJSONType_Object)
														)
													;

													CTargetInfo *pDependentTarget = fGetTarget(EntityName, TargetName, pEntity->f_Data().m_Position, Properties);

													if (pDependentTarget->m_TriedDependenciesMap(pDependentTarget->f_GetName()).f_WasCreated())
														bDependencyAdded = true;

													auto &Dep = _pTarget->m_DependenciesMap(pDependentTarget->f_GetName()).f_GetResult();
													_pTarget->m_DependenciesOrdered.f_Insert(Dep);
													Dep.m_pEntity = fg_Explicit(pEntity);

													bool bFollowIndirectDependencies = f_EvaluateEntityPropertyBool
														(
															*pDependentTarget->m_pInnerEntity
															, EPropertyType_Target
															, "FollowIndirectDependencies"
															, false
														)
													;

													if (!bFollowIndirectDependencies)
														continue;

													ToProcess[pDependentTarget];
												}
											}
										;

										fFindDependencies(*_pTarget->m_pOuterEntity, false);

										while (!ToProcess.f_IsEmpty())
										{
											auto ThisTime = fg_Move(ToProcess);
											for (auto iDependency = ThisTime.f_GetIterator(); iDependency; ++iDependency)
											{
												auto &Depenency = **iDependency;
												f_ExpandTargetDependencies(Workspace.m_Evaluated, *Depenency.m_pOuterEntity, Depenency.m_DependenciesBackup);

												fFindDependencies(*Depenency.m_pOuterEntity, true);
												pDistinguisher = nullptr;
												CreatedThisTime.f_Clear();
											}
										}

										TCVector<CStr> DependencyFiles;
										TCVector<CStr> Dependencies;
										for (auto iDependency = _pTarget->m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
										{
											bool bDoLink = f_EvaluateEntityPropertyBool
												(
													*iDependency->m_pEntity
													, EPropertyType_Dependency
													, "Link"
													, true
												)
											;

											auto &TargetName = iDependency->f_GetName();

											CTargetInfo *pDependentTarget;
											{
												pDependentTarget = Workspace.m_Targets.f_FindEqual(TargetName);
											}
											if (!pDependentTarget)
												fs_ThrowError(iDependency->m_pEntity->f_Data().m_Position, "Internal error dependency not found: '{}'"_f << TargetName);

											if (bDoLink)
											{
												iDependency->m_bLink = true;

												if (!_DependencyFilesName.f_IsEmpty())
												{
													CStr FileToAdd = f_EvaluateEntityPropertyString
														(
															*pDependentTarget->m_pInnerEntity
															, EPropertyType_Target
															, "DependencyFile"
															, CStr()
														)
													;
													if (!FileToAdd.f_IsEmpty())
														DependencyFiles.f_Insert(FileToAdd);
												}

											}
											CStr Dependency = pDependentTarget->m_pInnerEntity->f_GetPathForGetProperty();
											Dependencies.f_Insert(Dependency);
										}

										if (!_DependencyFilesName.f_IsEmpty())
										{
											CPropertyKey PropertyKey;
											PropertyKey.m_Type = EPropertyType_Property;
											PropertyKey.m_Name = _DependencyFilesName;

											DependencyFiles.f_Sort();
											CEJSON DependencyFilesArray = EJSONType_Array;
											for (auto &File : DependencyFiles)
												DependencyFilesArray.f_Insert(File);

											f_AddExternalProperty
												(
													fg_RemoveQualifiers(*_pTarget->m_pOuterEntity)
													, PropertyKey
													, fg_Move(DependencyFilesArray)
												)
											;
										}

										{
											CPropertyKey PropertyKey;
											PropertyKey.m_Type = EPropertyType_Target;
											PropertyKey.m_Name = "Dependencies";

											Dependencies.f_Sort();
											CEJSON DependenciesJSON = EJSONType_Array;
											for (auto &Dependency : Dependencies)
												DependenciesJSON.f_Insert(Dependency);

											f_AddExternalProperty
												(
													fg_RemoveQualifiers(*_pTarget->m_pOuterEntity)
													, PropertyKey
													, fg_Move(DependenciesJSON)
												)
											;
										}

										fp_UpdateDependenciesNames(_pTarget->m_pOuterEntity);
									}
								)
							;
							TargetsToProcess = fg_Move(NewTargetsToProcess);
						}


						{
							CPropertyKey Key;
							Key.m_Type = EPropertyType_Workspace;
							Key.m_Name = "AllTargets";

							AllTargets.f_Sort();
							CEJSON AllTargetsArray;
							for (auto &Target : AllTargets)
								AllTargetsArray.f_Insert(Target);

							f_AddExternalProperty(*Workspace.m_pEntity, Key, fg_Move(AllTargetsArray));
						}
					}

					// Then eval target specific
					fg_ForEach
						(
							Workspace.m_Targets
							, [&](CTargetInfo &_Target)
							{
								f_ExpandTargetGroups(Workspace.m_Evaluated, *_Target.m_pOuterEntity);
								f_ExpandTargetFiles(Workspace.m_Evaluated, *_Target.m_pOuterEntity);
								f_GenerateTargetFiles(Workspace.m_Evaluated, *_Target.m_pOuterEntity);

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
												{
													fFindFilesRecursive(ChildEntity, _pParentGroup);
												}
												break;
											case EEntityType_Group:
												{
													CPropertyKey Key(EPropertyType_Property, "HiddenGroup");
													CEJSON Hidden = f_GetExternalProperty
														(
															ChildEntity
															, Key
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
															Errors.f_Insert({File.m_pEntity->f_Data().m_Position, "Previous entity"});
															Errors.f_Insert({ChildEntity.f_Data().m_Position, "Current entity"});

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

					f_ExpandWorkspaceEntities(Workspace.m_Evaluated, *Workspace.m_pEntity);
					f_GenerateWorkspaceFiles(Workspace.m_Evaluated, *Workspace.m_pEntity);

					Workspace.m_RootGroup.fr_PruneEmpty();
					for (auto iGroup = Workspace.m_Groups.f_GetIterator(); iGroup; )
					{
						if (iGroup->m_Children.f_IsEmpty())
							iGroup.f_Remove();
						else
							++iGroup;
					}
				}
			)
		;
	}
}
