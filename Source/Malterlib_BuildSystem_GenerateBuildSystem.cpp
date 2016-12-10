// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_GenerateBuildSystem
		(
			TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> &o_Configurations
			, TCMap<CPropertyKey, CStr> const &_Values
			, TCSet<CStr> const &_ReservedGroups
			, CStr const &_DependencyFilesName
		) const
	{
		fg_ParallellForEach
			(
				o_Configurations
				, [&](TCUniquePointer<CConfiguraitonData> &_pConfig)
				{
					TCMap<CPropertyKey, CStr> ConfigValues = _Values;
					for (auto iTuple = _pConfig->m_Tuples.f_GetIterator(); iTuple; ++iTuple)
						ConfigValues[CPropertyKey(iTuple->m_Type)] = iTuple->m_Name;
					f_EvaluateDataMain(_pConfig->m_Evaluated, ConfigValues);
					f_ExpandGlobalTargetsAndWorkspaces(_pConfig->m_Evaluated);
					TCMap<CStr, CEntity *> Targets;
					f_EvalGlobalWorkspaces(_pConfig->m_Evaluated, Targets);
					f_ExpandGlobalEntities(_pConfig->m_Evaluated);
					f_GenerateGlobalFiles(_pConfig->m_Evaluated);

					auto &Config = o_Configurations.fs_GetKey(_pConfig);
					auto &ConfigData = *_pConfig;
	
					// Find workspaces
					for (auto iEntity = ConfigData.m_Evaluated.m_RootEntity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
					{
						auto &Entity = *iEntity;
						switch (Entity.m_Key.m_Type)
						{
						case EEntityType_Workspace:
							{
								CStr Enabled = f_EvaluateEntityProperty(Entity, EPropertyType_Workspace, "Enabled");
								if (Enabled == "false")
									continue;
								
								CStr Name = f_EvaluateEntityProperty(Entity, EPropertyType_Workspace, "Name");

								if (Name.f_IsEmpty())
									fs_ThrowError(Entity.m_Position, "No name specified for workspace");

								auto &Workspace = ConfigData.m_Workspaces[Name];
								Workspace.m_pEntity = fg_Explicit(&Entity);
								CStr EntityName = Entity.m_Key.m_Name;
								
								if (Workspace.m_EntityName.f_IsEmpty())
									Workspace.m_EntityName = EntityName;
								else if (Workspace.m_EntityName != EntityName)
								{
									fs_ThrowError
										(
											Entity.m_Position
											, CStr::CFormat("A entity with this name already exists ({} != {})") 
											<< EntityName
											<< Workspace.m_EntityName
										)
									;
								}
							}
						}
					}
					
					fg_ParallellForEach
						(
							ConfigData.m_Workspaces
							, [&](CWorkspaceInfo &_Workspace)
							{
								f_ExpandWorkspaceTargets(ConfigData.m_Evaluated, *_Workspace.m_pEntity);
								f_EvaluateTargetsInWorkspace(ConfigData.m_Evaluated, Targets, *_Workspace.m_pEntity);
								
								TCVector<CStr> AllTargets;
								CMutual WorkspaceLock;
								TCSet<CStr> TargetEntities;
								TCFunction<void (CEntity &_Entity, CGroupInfo *_pParentGroup)> flr_FindTargets;
								auto fl_AddTarget
									= [&](CEntity &_Entity, CGroupInfo *_pParentGroup) -> CTargetInfo *
									{
										CStr EntityName = _Entity.m_Key.m_Name;
										
										auto *pTarget = _Entity.m_ChildEntitiesMap.f_FindEqual(_Entity.m_Key);
										if (!pTarget)
										{
											for 
											(
												auto pSmallest = _Entity.m_ChildEntitiesMap.f_FindSmallest()
												; pSmallest
												; pSmallest = pSmallest->m_ChildEntitiesMap.f_FindSmallest()
											)
											{
												pTarget = pSmallest->m_ChildEntitiesMap.f_FindEqual(_Entity.m_Key);
												if (pTarget)
													break;

											}
										}

										if (!pTarget)
										{
											fs_ThrowError
												(
													_Entity.m_Position
													, CStr::CFormat("Dependency target '{}' not found for configuration: {}") 
													<< EntityName
													<< Config.f_GetFullName()
												 )
											;
										}

										auto &TargetEntity = *pTarget;

										CStr Name = f_EvaluateEntityProperty(TargetEntity, EPropertyType_Target, "Name");

										if (Name.f_IsEmpty())
											fs_ThrowError(TargetEntity.m_Position, "No name specified for target");
										
										CTargetInfo *pTargetInfo;
										TargetEntities[Name];
										pTargetInfo = &(*_Workspace.m_Targets(Name, &_Workspace));
										AllTargets.f_Insert(TargetEntity.f_GetPathForGetProperty());
										
										auto &Target = *pTargetInfo;
										if (Target.m_EntityName.f_IsEmpty())
											Target.m_EntityName = EntityName;
										else if (Target.m_pGroup != _pParentGroup)
											fs_ThrowError(_Entity.m_Position, "Same target specified twice with same name in workspace is not supported");
										else if (Target.m_EntityName != TargetEntity.m_Key.m_Name)
										{
											fs_ThrowError
												(
													TargetEntity.m_Position
													, CStr::CFormat("A entity with this name already exists ({} != {})") 
													<< Target.m_EntityName 
													<< TargetEntity.m_Key.m_Name
												)
											;
										}
										
										Target.m_pInnerEntity = fg_Explicit(&TargetEntity);
										Target.m_pOuterEntity = fg_Explicit(&_Entity);
										Target.m_pGroup = _pParentGroup;
										if (_pParentGroup)
											_pParentGroup->m_Children.f_Insert(Target);
										else
											_Workspace.m_RootGroup.m_Children.f_Insert(Target);
										
										return pTargetInfo;
									}
								;
								auto fl_FindTargets
									= [&](CEntity &_Entity, CGroupInfo *_pParentGroup)
									{
										for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
										{
											auto &ChildEntity = *iEntity;
											switch (ChildEntity.m_Key.m_Type)
											{
											case EEntityType_Group:
												{
													CStr Name = ChildEntity.m_Key.m_Name;
													if (Name.f_IsEmpty())
														fs_ThrowError(ChildEntity.m_Position, "No name specified for workspace group");
													CStr Fullpath;
													if (_pParentGroup)
														Fullpath = CFile::fs_AppendPath(_pParentGroup->f_GetPath(), Name);
													else
														Fullpath = Name;

													auto &Group = _Workspace.m_Groups[Fullpath];
													Group.m_Name = Name;
													Group.m_pParent = _pParentGroup;
													if (_pParentGroup)
														_pParentGroup->m_Children.f_Insert(Group);
													else
														_Workspace.m_RootGroup.m_Children.f_Insert(Group);

													flr_FindTargets(ChildEntity, &Group);
												}
												break;
											case EEntityType_Target:
												{
													fl_AddTarget(ChildEntity, _pParentGroup);
												}
												break;
											}
										}
									}
								;

								flr_FindTargets = fl_FindTargets;
								fl_FindTargets(*_Workspace.m_pEntity, nullptr);

								
								// First add properties
								bool bDependencyAdded = true;
								while (bDependencyAdded)
								{
									bDependencyAdded = false;
									
									TCVector<CTargetInfo *> TargetsToProcess;
									for (auto &TargetInfo : _Workspace.m_Targets)
										TargetsToProcess.f_Insert(&TargetInfo);
									
									while (!TargetsToProcess.f_IsEmpty())
									{
										TCVector<CTargetInfo *> NewTargetsToProcess;
										
										auto fGetTarget
											= [&](CStr const &_EntityName, CStr const &_DependencyName, CFilePosition const &_Position, CStr const &_Properties) -> CTargetInfo *
											{
												CTargetInfo *pDependentTarget;
												bool bTargetEntity;
												{
													DLock(WorkspaceLock);
													pDependentTarget = _Workspace.m_Targets.f_FindEqual(_DependencyName);
													bTargetEntity = TargetEntities.f_FindEqual(_DependencyName);
												}
												
												if (!pDependentTarget)
												{
													if (_EntityName.f_IsEmpty())
													{
														fs_ThrowError
															(
																_Position
																, CStr::CFormat("Internal error dependency not found: '{}'")
																<< _EntityName
															)
														;
													}
													CEntity **pSourceTarget = Targets.f_FindEqual(_EntityName);
													if (pSourceTarget && !bTargetEntity)
													{
	//																DLock(_pConfig->m_Evaluated.m_RootEntity.m_Lock);
														{
															DLock(WorkspaceLock);
															
															pDependentTarget = _Workspace.m_Targets.f_FindEqual(_DependencyName);
															if (!pDependentTarget)
															{
																CEntityKey EntityKey;
																EntityKey.m_Type = EEntityType_Target;
																EntityKey.m_Name = _EntityName;

																TCMap<CPropertyKey, CEvaluatedProperty> EvaluatedProperties;
																CStr Properties = _Properties;
																while (!Properties.f_IsEmpty())
																{
																	CStr Property = fg_GetStrSep(Properties, "*");
																	
																	CPropertyKey Key;
																	Key.m_Name = fg_GetStrSep(Property, "=");
																	Key.m_Type = EPropertyType_Property;

																	auto &EvalProperty = EvaluatedProperties[Key];
																	EvalProperty.m_Value = Property;
																	EvalProperty.m_Type = EEvaluatedPropertyType_External;
																	EvalProperty.m_pProperty = &mp_ExternalProperty;
																}
																
																
																CEntity *pNewTarget;
																CGroupInfo *pParentGroupInfo = nullptr;
																{
																	CProperty const *pFromProperty = nullptr;
																	CStr Group = f_EvaluateEntityPropertyUncached
																		(
																			*pSourceTarget
																			, EPropertyType_Target
																			, "Group"
																			, pFromProperty
																			, &EvaluatedProperties
																		)
																	;
																	auto pParentGroup = _Workspace.m_pEntity.f_Get();
																	

																	while (!Group.f_IsEmpty())
																	{
																		CStr ParentGroup = fg_GetStrSep(Group, "/");
																		if (ParentGroup.f_StartsWith("{"))
																			continue;
																		
																		CEntityKey Key;
																		Key.m_Name = ParentGroup;
																		Key.m_Type = EEntityType_Group;
																		auto Child = pParentGroup->m_ChildEntitiesMap(Key, pParentGroup);
																		
																		if (Child.f_WasCreated())
																		{
																			(*Child).m_Key = Key;
																			pParentGroup->m_ChildEntitiesOrdered.f_Insert(*Child);
																		}
																		pParentGroup = &*Child;
																		
																		CStr Fullpath;
																		if (pParentGroupInfo)
																			Fullpath = CFile::fs_AppendPath(pParentGroupInfo->f_GetPath(), ParentGroup);
																		else
																			Fullpath = ParentGroup;

																		auto &Group = _Workspace.m_Groups[Fullpath];
																		Group.m_Name = ParentGroup;
																		Group.m_pParent = pParentGroupInfo;
																		if (pParentGroupInfo)
																			pParentGroupInfo->m_Children.f_Insert(Group);
																		else
																			_Workspace.m_RootGroup.m_Children.f_Insert(Group);
																		
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
																	pParentGroup->m_ChildEntitiesOrdered.f_Insert(NewTarget);
																	NewTarget.m_Key = EntityKey;
																	NewTarget.m_Position = _Position;
																	pNewTarget = &NewTarget;
																}
																
																pNewTarget->m_EvaluatedProperties += EvaluatedProperties;
																
																fp_ExpandEntity(*pNewTarget, *pNewTarget->m_pParent, nullptr);
																f_EvaluateTarget(_pConfig->m_Evaluated, Targets, *pNewTarget);
																CTargetInfo *pTargetInfo = fl_AddTarget(*pNewTarget, pParentGroupInfo);
																CStr ResultingName = pTargetInfo->f_GetName();
																if (ResultingName != _DependencyName)
																{
																	fs_ThrowError
																		(
																			_Position
																			, CStr::CFormat("When a target dependency is not already part of the workspace, the name of the dependency needs to be the same as the target entity name ('{}' != '{}'), or you need to specify the resulting dependency name: EntityName=DependencyName") 
																			<< ResultingName
																			<< _DependencyName
																		)
																	;
																}
																
																f_ExpandTargetDependencies(ConfigData.m_Evaluated, *pTargetInfo->m_pOuterEntity, pTargetInfo->m_DependenciesBackup);
																
																NewTargetsToProcess.f_Insert(pTargetInfo);

																pDependentTarget = pTargetInfo;
															}
														}
													}

													if (!pDependentTarget)
													{
														fs_ThrowError
															(
																_Position
																, CStr::CFormat("Could not find dependency '{}' using enty name '{}' in workspace") 
																<< _DependencyName
																<< _EntityName
															)
														;
													}
												}
												return pDependentTarget;
											}
										;
										
										fg_ForEach
										//fg_ParallellForEach. Some race condition in this code
											(
												TargetsToProcess
												, [&](CTargetInfo *_pTarget)
												{
													_pTarget->m_DependenciesMap.f_Clear();
													f_ExpandTargetDependencies(ConfigData.m_Evaluated, *_pTarget->m_pOuterEntity, _pTarget->m_DependenciesBackup);
													
													TCSet<CEntity *> ToProcess;
													TCSet<CEntity *> CreatedThisTime;
													CEntity *pDistinguisher = nullptr;
													TCFunction<void (CEntity &_Entity, bool _bRecursive)> fFindDependencies
														= [&](CEntity &_Entity, bool _bRecursive)
														{
															TCLinkedList<TCTuple<CEntity *, zbool>> Dependencies;
															for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
															{
																auto &ChildEntity = *iEntity;
																
																switch (ChildEntity.m_Key.m_Type)
																{
																case EEntityType_Group:
																case EEntityType_Target:
																	{
																		auto const *pToProcess = &ChildEntity;
																		if (_bRecursive)
																		{
																			while (pToProcess->m_pCopiedFrom)
																				pToProcess = pToProcess->m_pCopiedFrom.f_Get();
																		}

																		fFindDependencies(fg_RemoveQualifiers(*pToProcess), _bRecursive);
																	}
																	break;
																case EEntityType_Dependency:
																	{
																		Dependencies.f_InsertLast(fg_Tuple(&ChildEntity, false));
																	}
																	break;
																}
															}
															while (!Dependencies.f_IsEmpty())
															{
																auto Dependency = Dependencies.f_Pop();
																auto &ChildEntity = *fg_Get<0>(Dependency);
																bool bExpandedDependency = fg_Get<1>(Dependency);
																
																CStr EntityNameFull = ChildEntity.m_Key.m_Name;
																
																CStr EntityName = fg_GetStrSep(EntityNameFull, "*");
																CStr DependencyName = fg_GetStrSep(EntityName, "=");
																
																if (EntityName.f_IsEmpty())
																	EntityName = DependencyName;
																else
																	fg_Swap(DependencyName, EntityName);
																
																auto pEntity = &ChildEntity;
																if (_bRecursive)
																{
																	if (_pTarget->m_DependenciesMap.f_FindEqual(DependencyName))
																		continue; // We already have dependency
																	
																	if (!bExpandedDependency)
																	{
																		TCVector<CEntity const *> Parents;

																		//DCheck(!ChildEntity.m_pCopiedFrom);
																		
																		{
																			auto *pSourceParent = &ChildEntity;
																			while (pSourceParent && pSourceParent->m_Key.m_Type != EEntityType_Target)
																			{
																				//DCheck(!pSourceParent->m_pCopiedFrom);
																				Parents.f_Insert(pSourceParent);
																				pSourceParent = pSourceParent->m_pParent;
																			}
																			Parents = Parents.f_Reverse();
																		}
																		
																		TCVector<CEntity *> ToDelete;
																		CEntity *pParent = _pTarget->m_pInnerEntity.f_Get();

																		bool bCreatedDistinguisher = false;
																		if (auto pRoot = pParent->m_ChildEntitiesMap.f_FindEqual(Parents.f_GetFirst()->m_Key))
																		{
																			if (!CreatedThisTime.f_FindEqual(pRoot))
																			{
																				if (!pDistinguisher)
																				{
																					CEntityKey Key;
																					Key.m_Type = EEntityType_Group;
																					mint iDistinguish = 0;
																					Key.m_Name = fg_Format("Dependency Distinguisher {}", iDistinguish);
																					while (pParent->m_ChildEntitiesMap.f_FindEqual(Key))
																					{
																						++iDistinguish;
																						Key.m_Name = fg_Format("Dependency Distinguisher {}", iDistinguish);
																					}
																					auto Mapped = pParent->m_ChildEntitiesMap(Key, pParent);
																					auto &NewDependency = Mapped.f_GetResult();
																					pParent->m_ChildEntitiesOrdered.f_Insert(NewDependency);
																					NewDependency.m_Position = pParent->m_Position;
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
																			auto Mapped = pParent->m_ChildEntitiesMap(pSourceParent->m_Key, pParent);
																			if (Mapped.f_WasCreated())
																			{
																				auto &NewDependency = Mapped.f_GetResult();
																				pParent->m_ChildEntitiesOrdered.f_Insert(NewDependency);
																				NewDependency.f_CopyFrom(*pSourceParent, false, &pSourceParent->m_Key, true);
																				NewDependency.m_Position = pSourceParent->m_Position;
																				
																				if (NewDependency.m_Key.m_Type == EEntityType_Dependency)
																				{
																					TCVector<CEntity *> CreatedDependencies;
																					if (fp_ExpandEntity(NewDependency, *pParent, &CreatedDependencies))
																					{
																						pParent->m_ChildEntitiesMap.f_Remove(pSourceParent->m_Key);
																						
																						for (auto &pDependency : CreatedDependencies)
																							Dependencies.f_Insert(fg_Tuple(pDependency, true));
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
																				DCheck(pSourceParent->m_Key.m_Type != EEntityType_Dependency);
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
																			||
																			(
																				f_EvaluateEntityProperty
																				(
																					*pEntity
																					, EPropertyType_Dependency
																					, "Indirect"
																				)
																				!= "true"
																			)
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

																		if 
																		(
																			f_EvaluateEntityProperty
																			(
																				*pEntity
																				, EPropertyType_Dependency
																				, "Indirect"
																			)
																			!= "true"
																		)
																		{
																			CreatedThisTime.f_Remove(pEntity);
																			pEntity->m_pParent->m_ChildEntitiesMap.f_Remove(pEntity);
																			continue;
																		}
																	}
																}
																else
																{
																	CStr Indirect = f_EvaluateEntityProperty
																		(
																			ChildEntity
																			, EPropertyType_Dependency
																			, "Indirect"
																		)
																	;

																	if (Indirect == "true")
																		continue;
																}

																CTargetInfo *pDependentTarget = fGetTarget(EntityName, DependencyName, pEntity->m_Position, EntityNameFull);

																if (pDependentTarget->m_TriedDependenciesMap(pDependentTarget->f_GetName()).f_WasCreated())
																	bDependencyAdded = true;

																auto &Dep = _pTarget->m_DependenciesMap(pDependentTarget->f_GetName()).f_GetResult();
																_pTarget->m_DependenciesOrdered.f_Insert(Dep);
																Dep.m_pEntity = fg_Explicit(pEntity);

																auto const *pToProcess = &*pDependentTarget->m_pOuterEntity;
																while (pToProcess->m_pCopiedFrom)
																	pToProcess = pToProcess->m_pCopiedFrom.f_Get();
																ToProcess[&fg_RemoveQualifiers(*pToProcess)];
															}
														}
													;
													
													fFindDependencies(*_pTarget->m_pOuterEntity, false);
													
													while (!ToProcess.f_IsEmpty())
													{
														auto ThisTime = fg_Move(ToProcess);
														for (auto iDependency = ThisTime.f_GetIterator(); iDependency; ++iDependency)
														{
															fFindDependencies(**iDependency, true);
															pDistinguisher = nullptr;
															CreatedThisTime.f_Clear();
														}
													}
													
													TCVector<CStr> DependencyFiles;
													TCVector<CStr> Dependencies;
													for (auto iDependency = _pTarget->m_DependenciesOrdered.f_GetIterator(); iDependency; ++iDependency)
													{
														CStr DoLink = f_EvaluateEntityProperty
															(
																*iDependency->m_pEntity
																, EPropertyType_Dependency
																, "Link"
															)
														;

														auto &DependencyName = iDependency->f_GetName();
														
														CTargetInfo *pDependentTarget = fGetTarget(CStr(), DependencyName, iDependency->m_pEntity->m_Position, CStr());
														
														if (DoLink == "true")
														{
															iDependency->m_bLink = true;

															if (!_DependencyFilesName.f_IsEmpty())
															{
																CStr FileToAdd = f_EvaluateEntityProperty
																	(
																		*pDependentTarget->m_pInnerEntity
																		, EPropertyType_Target
																		, "DependencyFile"
																	)
																;
																if (!FileToAdd.f_IsEmpty())
																	DependencyFiles.f_Insert(FileToAdd);
															}
															
															CStr Dependency = pDependentTarget->m_pInnerEntity->f_GetPathForGetProperty();
															Dependencies.f_Insert(Dependency);
														}
													}															

													if (!_DependencyFilesName.f_IsEmpty())
													{
														CPropertyKey PropertyKey;
														PropertyKey.m_Type = EPropertyType_Property;
														PropertyKey.m_Name = _DependencyFilesName;

														DependencyFiles.f_Sort();
														CStr DependencyFilesStr;
														for (auto &File : DependencyFiles)
															fg_AddStrSep(DependencyFilesStr, File, ";");

														f_AddExternalProperty
															(
																fg_RemoveQualifiers(*_pTarget->m_pOuterEntity)
																, PropertyKey
																, DependencyFilesStr
															)
														;
													}

													{
														CPropertyKey PropertyKey;
														PropertyKey.m_Type = EPropertyType_Target;
														PropertyKey.m_Name = "Dependencies";

														Dependencies.f_Sort();
														CStr DependenciesStr;
														for (auto &File : Dependencies)
															fg_AddStrSep(DependenciesStr, File, ";");

														f_AddExternalProperty
															(
																fg_RemoveQualifiers(*_pTarget->m_pOuterEntity)
																, PropertyKey
																, DependenciesStr
															)
														;
													}
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
										CStr AllTargetsStr;
										for (auto &Target : AllTargets)
											fg_AddStrSep(AllTargetsStr, Target, ";");

										f_AddExternalProperty(*_Workspace.m_pEntity, Key, AllTargetsStr);
									}
								}
								
								// Then eval target specific
								fg_ParallellForEach
									(
										_Workspace.m_Targets
										, [&](CTargetInfo &_Target)
										{
											f_ExpandTargetFiles(ConfigData.m_Evaluated, *_Target.m_pOuterEntity);
											f_GenerateTargetFiles(ConfigData.m_Evaluated, *_Target.m_pOuterEntity);
											
											TCFunction<void (CEntity &_Entity, CGroupInfo *_pParentGroup)> flr_FindFiles;
											auto fl_FindFiles
												= [&](CEntity &_Entity, CGroupInfo *_pParentGroup)
												{
													for (auto iEntity = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iEntity; ++iEntity)
													{
														auto &ChildEntity = *iEntity;
														switch (ChildEntity.m_Key.m_Type)
														{
														case EEntityType_GenerateFile:
														case EEntityType_Target:
															{
																flr_FindFiles(ChildEntity, _pParentGroup);
															}
															break;
														case EEntityType_Group:
															{
																CPropertyKey Key(EPropertyType_Property, "HiddenGroup");
																CStr Hidden = f_GetExternalProperty
																	(
																		ChildEntity
																		, Key
																	)
																;
																if (Hidden == "true")
																	flr_FindFiles(ChildEntity, _pParentGroup);
																else
																{
																	CStr Name = ChildEntity.m_Key.m_Name;
																	if (Name.f_IsEmpty())
																		fs_ThrowError(ChildEntity.m_Position, "No name specified for file group");
																	else if (_ReservedGroups.f_FindEqual(Name))
																		fs_ThrowError(ChildEntity.m_Position, "Group name conflicts with reserved group names");
																	CStr Fullpath;
																	if (_pParentGroup)
																		Fullpath = CFile::fs_AppendPath(_pParentGroup->f_GetPath(), Name);
																	else
																		Fullpath = Name;

																	auto &Group = _Target.m_Groups[Fullpath];
																	Group.m_Name = Name;
																	Group.m_pParent = _pParentGroup;
																	if (_pParentGroup)
																		_pParentGroup->m_Children.f_Insert(Group);
																	else
																		_Target.m_RootGroup.m_Children.f_Insert(Group);
																	flr_FindFiles(ChildEntity, &Group);
																}
															}
															break;
														case EEntityType_File:
															{
																CStr EntityName = ChildEntity.m_Key.m_Name;

																auto FileMap = _Target.m_Files(EntityName);

																auto &File = FileMap.f_GetResult();

																if (!FileMap.f_WasCreated())
																{
																	if (File.m_pGroup != _pParentGroup)
																		fs_ThrowError(ChildEntity.m_Position, "Same file specified twice in project is not supported");
																}

																File.m_pEntity = fg_Explicit(&ChildEntity);
																File.m_pGroup = _pParentGroup;
																if (_pParentGroup)
																	_pParentGroup->m_Children.f_Insert(File);
																else
																	_Target.m_RootGroup.m_Children.f_Insert(File);
																
															}
															break;
														}
													}
												}
											;
											flr_FindFiles = fl_FindFiles;				
											
											fl_FindFiles(*_Target.m_pOuterEntity, nullptr);
											
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
								
								f_ExpandWorkspaceEntities(ConfigData.m_Evaluated, *_Workspace.m_pEntity);
								f_GenerateWorkspaceFiles(ConfigData.m_Evaluated, *_Workspace.m_pEntity);
								
								_Workspace.m_RootGroup.fr_PruneEmpty();
								for (auto iGroup = _Workspace.m_Groups.f_GetIterator(); iGroup; )
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
			)
		;
	}
}
