// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	namespace
	{
		// How much a stage of a dependency orders the build: Compile orders the compiles of the target after the dependency, Link its
		// link, Runtime only the validation after the build, and None nothing
		auto fg_DependencyStageRestriction(CStr const &_Stage) -> umint
		{
			if (_Stage == "Compile")
				return 3;
			if (_Stage == "Link")
				return 2;
			if (_Stage == "Runtime")
				return 1;
			return 0;
		}
	}

	void CBuildSystem::f_GenerateBuildSystem_Workspace
		(
			CConfiguration const &_Config
			, CConfiguraitonData const &_ConfigData
			, CWorkspaceInfo *_pWorkspace
			, TCSet<CStr> const &_ReservedGroups
			, CStr const &_DependencyFilesName
		)
		const
	{
		DMibFastCheck(_DependencyFilesName.f_IsEmpty() || _DependencyFilesName.f_IsConstant());
		CPropertyKeyReference DependencyFileNamePropertyKey(CAssertAddedToStringCache(), EPropertyType_Property, _DependencyFilesName, _DependencyFilesName.f_Hash());

		auto &Workspace = *_pWorkspace;
		auto &ConfigData = _ConfigData;
		auto &Config = _Config;
		auto &Targets = ConfigData.m_Targets;

		Workspace.m_pEvaluated = fg_Construct(_ConfigData.m_Evaluated);
		auto &Evaluated = *Workspace.m_pEvaluated;

		f_PopulateAllRepositories(Evaluated);

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
				auto pNextEntity = pEntity->m_ChildEntitiesMap.f_FindEqual(Key);
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
					for (auto pSmallest = _Entity.m_ChildEntitiesMap.f_FindSmallest(); pSmallest; pSmallest = pSmallest->m_ChildEntitiesMap.f_FindSmallest())
					{
						pTarget = pSmallest->m_ChildEntitiesMap.f_FindEqual(EntityKey);
						if (pTarget)
							break;
					}
				}

				if (!pTarget)
					fs_ThrowError(_Entity.f_Data().m_Position, CStr::CFormat("Dependency target '{}' not found for configuration: {}") << EntityName << Config.f_GetFullName());

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
							, CStr::CFormat("A entity with this name already exists ({} != {})") << Target.m_EntityName << TargetEntity.f_GetKey().m_Name
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

				Target.m_bObjectLibrary = f_EvaluateEntityPropertyBool(TargetEntity, gc_ConstKey_Target_ObjectLibrary);

				return pTargetInfo;
			}
		;
		auto fFindTargets = [&](CEntity &_Entity, CGroupInfo *_pParentGroup)
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
					default: break;
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
			f_CheckCancelledException();

			TCSet<CTargetInfo *> TargetsToProcess;
			for (auto &TargetInfo : Workspace.m_Targets)
				TargetsToProcess[&TargetInfo];

			TCSet<CTargetInfo *> TargetsAlreadyProcessed;

			while (!TargetsToProcess.f_IsEmpty())
			{
				TargetsAlreadyProcessed += TargetsToProcess;
				f_CheckCancelledException();

				TCSet<CTargetInfo *> NewTargetsToProcess;

				// The properties that the build system gives a declaration of a dependency, which its own properties can depend on, as
				// Dependency.Link Dependency.ObjectLibrary, so they are set before the declaration is merged
				auto fSetDeclarationProperties = [&](CEntity &_Declaration, CTargetInfo const &_DependentTarget, bool _bIndirect, bool _bIndirectOrdered)
					{
						f_AddExternalProperty(_Declaration, gc_ConstKey_Dependency_ObjectLibrary, _DependentTarget.m_bObjectLibrary);
						f_AddExternalProperty(_Declaration, gc_ConstKey_Dependency_IndirectEvaluated, _bIndirect);
						f_AddExternalProperty(_Declaration, gc_ConstKey_Dependency_IndirectOrderedEvaluated, _bIndirectOrdered);
						f_AddExternalProperty(_Declaration, gc_ConstKey_Dependency_TargetPropertyPath, _DependentTarget.m_pInnerEntity->f_GetPathForGetProperty());
					}
				;

				// The most restrictive of the stages of the declarations of a dependency, and linked or compiled after generated files where any
				// of them is, also in a build system without the settings that declare them, as those of the tests. A declaration that links the
				// dependency needs it when the target links, so it counts as the stage Link at least, whatever stage it declares. The dependency
				// passes its injections and dependencies on where any declaration does: one of the stage None that doesn't link it doesn't, also
				// where it orders the build to compile after the files that the dependency generates.
				auto fMergeDeclaration = [&](CTargetDependencyInfo &o_Dependency, CEntity &_Declaration)
					{
						CBuildSystemPropertyInfo StageInfo;
						auto Stage = f_EvaluateEntityPropertyNoDefault(_Declaration, gc_ConstKey_Dependency_Stage, StageInfo);
						CStr DeclaredStage = Stage.f_Get().f_IsString() ? Stage.f_Get().f_String() : CStr("Link");

						bool const bLink = f_EvaluateEntityPropertyBool(_Declaration, gc_ConstKey_Dependency_Link, true);
						if (bLink || DeclaredStage != "None")
							o_Dependency.m_bPassesOn = true;

						if (bLink)
						{
							o_Dependency.m_bMergedLink = true;
							if (fg_DependencyStageRestriction(DeclaredStage) < fg_DependencyStageRestriction("Link"))
								DeclaredStage = "Link";
						}

						if (o_Dependency.m_MergedStage.f_IsEmpty() || fg_DependencyStageRestriction(DeclaredStage) > fg_DependencyStageRestriction(o_Dependency.m_MergedStage))
							o_Dependency.m_MergedStage = fg_Move(DeclaredStage);

						CBuildSystemPropertyInfo CompileAfterGeneratedInfo;
						auto CompileAfterGenerated = f_EvaluateEntityPropertyNoDefault(_Declaration, gc_ConstKey_Dependency_CompileAfterGenerated, CompileAfterGeneratedInfo);
						if (CompileAfterGenerated.f_Get().f_IsBoolean() && CompileAfterGenerated.f_Get().f_Boolean())
							o_Dependency.m_bMergedCompileAfterGenerated = true;
					}
				;

				auto fGetTarget = [&](CStr const &_EntityName, CStr const &_TargetName, CFilePosition const &_Position, CEJsonSorted &&_Properties) -> CTargetInfo *
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
								fs_ThrowError(_Position, CStr::CFormat("Could not find dependency '{}' using entity name '{}' in workspace") << _TargetName << _EntityName);
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
								Group = f_EvaluateEntityPropertyUncachedString(**pSourceTarget, gc_ConstKey_Target_Group, PropertyInfo, &EvaluatedProperties, CStr());
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
								fs_ThrowError(_Position, CStr::CFormat("The target entity with name '{}' has already been added to this group") << EntityKey.m_Name);
							auto &NewTarget = pParentGroup->m_ChildEntitiesMap(EntityKey, pParentGroup).f_GetResult();
							NewTarget.f_DataWritable().m_Position = _Position;
							pNewTarget = &NewTarget;
						}

						pNewTarget->m_EvaluatedProperties.m_Properties += EvaluatedProperties;

						TCSet<CStr> AlreadyAddedGroups;
						bool bChanged = false;
						fp_ExpandEntity(*pNewTarget, *pNewTarget->m_pParent, nullptr, bChanged);
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
							f_CheckCancelledException();

							_pTarget->m_DependenciesMap.f_Clear();

							f_ExpandTargetDependencies(Workspace, Evaluated, *_pTarget->m_pOuterEntity, _pTarget->m_DependenciesBackup);

							_pTarget->m_bIsExpanded = true;

							TCMap<CTargetInfo *, zbool> ToProcess;
							// The copies that the dependency that is processed has added to the target, with what each is a copy of
							TCMap<CEntity *, CEntity const *> CreatedThisTime;
							CEntity *pDistinguisher = nullptr;
							// The dependencies of a dependency that passes them on to the target are processed for the target too
							auto fFollowDependencies = [&](CTargetInfo *_pDependentTarget, bool _bIndirect)
								{
									ToProcess[_pDependentTarget] = _bIndirect;
								}
							;
							// A declaration that a dependency passes on merges into the dependency that the target already has. As where the
							// target declares it more than once, the dependency is indirect only where each declaration that orders the build is.
							// It can make one that only ordered the build, as of the stage None that compiles after the files that the dependency
							// generates, pass its dependencies on too, and an indirect one direct. Its dependencies are followed then, again where
							// they were followed as those of an indirect dependency, as they are direct dependencies of the target where it is.
							auto fMergePassedOn = [&](CTargetDependencyInfo &o_Dependency, CEntity &_Declaration, CTargetInfo *_pDependentTarget, bool _bIndirect)
								{
									bool const bIndirectBefore = o_Dependency.m_bIndirect;
									bool const bPassedOnBefore = o_Dependency.m_bPassesOn;

									CBuildSystemPropertyInfo OrderedInfo;
									auto Ordered = f_EvaluateEntityPropertyNoDefault(_Declaration, gc_ConstKey_Dependency_Ordered, OrderedInfo);
									if (!Ordered.f_Get().f_IsValid() || Ordered.f_Get().f_Boolean())
										o_Dependency.m_bIndirect = o_Dependency.m_bIndirect && _bIndirect;

									fMergeDeclaration(o_Dependency, _Declaration);
									if (!o_Dependency.m_bPassesOn || !_pDependentTarget)
										return;

									if (bPassedOnBefore && o_Dependency.m_bIndirect == bIndirectBefore)
										return;

									if (f_EvaluateEntityPropertyBool(*_pDependentTarget->m_pInnerEntity, gc_ConstKey_Target_FollowIndirectDependencies, false))
										fFollowDependencies(_pDependentTarget, o_Dependency.m_bIndirect);
								}
							;
							TCFunction<void (CEntity &_Entity, bool _bRecursive, bool _bIndirect)> fFindDependencies = [&](CEntity &_Entity, bool _bRecursive, bool _bIndirect)
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
										default: break;
										}
									}

									while (!Dependencies.f_IsEmpty())
									{
										auto Dependency = Dependencies.f_Pop();
										auto &ChildEntity = *fg_Get<0>(Dependency);
										bool bExpandedDependency = fg_Get<1>(Dependency);

										bool bExternal = !f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Dependency_Type, CStr()).f_IsEmpty();

										CStr TargetName;
										CStr EntityName;

										if (bExternal)
											TargetName = f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Dependency_Name);
										else
										{
											TargetName = ChildEntity.f_GetKeyName();
											EntityName = f_EvaluateEntityPropertyString(ChildEntity, gc_ConstKey_Dependency_Target, TargetName);
										}

										auto pEntity = &ChildEntity;
										bool bIndirect = true;
										if (_bRecursive)
										{
											// A dependency that orders the build already only merges what the one that a dependency passes on asks for, as
											// to link it or compile after it, and keeps its declaration. One of the stage None is replaced, as the target then
											// links it.
											CTargetDependencyInfo *pMergeInto = nullptr;
											if (auto *pExisting = _pTarget->m_DependenciesMap.f_FindEqual(TargetName); pExisting && pExisting->m_bOrdered)
											{
												// External dependencies have no stages or links to merge
												if (bExternal)
													continue;

												// A declaration is merged once, as the dependency that declares it can be followed again, and once
												// more where it was merged as that of an indirect dependency and the dependency became direct
												auto MappedMerged = pExisting->m_MergedPassedOn(&ChildEntity);
												bool &bMergedIndirect = MappedMerged.f_GetResult();
												if (!MappedMerged.f_WasCreated() && (!bMergedIndirect || _bIndirect))
													continue;

												bMergedIndirect = _bIndirect;
												pMergeInto = pExisting;
											}

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

												// The copy needs another place where an entity of the target itself is in its place, or the copy of
												// another entity of the dependency that is processed: its target and what the workspace adds to it can
												// have groups and dependencies of the same names. Copies only share the copy of a group that they are
												// both in, as a group gives what is in it its settings.
												auto fCopyCollides = [&](CEntity &_Parent) -> bool
													{
														auto *pCopyParent = &_Parent;
														for (auto &pSourceParent : Parents)
														{
															auto *pCopy = pCopyParent->m_ChildEntitiesMap.f_FindEqual(pSourceParent->f_GetKey());
															if (!pCopy)
																return false;

															auto *pCopyOf = CreatedThisTime.f_FindEqual(pCopy);
															if (!pCopyOf || *pCopyOf != pSourceParent)
																return true;

															pCopyParent = pCopy;
														}

														return true;
													}
												;

												bool bCreatedDistinguisher = false;
												if (fCopyCollides(*pParent))
												{
													if (!pDistinguisher || fCopyCollides(*pDistinguisher))
													{
														CEntityKey Key;
														Key.m_Type = EEntityType_Group;
														umint iDistinguish = 0;
														Key.m_Name.m_Value = fg_Format("Dependency Distinguisher {}", iDistinguish);
														while (auto *pOldDistinguisher = pParent->m_ChildEntitiesMap.f_FindEqual(Key))
														{
															if (pOldDistinguisher->f_HasOnlyGroups())
															{
																pParent->m_ChildEntitiesMap.f_Remove(Key);
																break;
															}
															else
															{
																++iDistinguish;
																Key.m_Name.m_Value = fg_Format("Dependency Distinguisher {}", iDistinguish);
															}
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

												CEntity *pRootCreated = nullptr;
												bool bAborted = false;
												for (auto &pSourceParent : Parents)
												{
													auto &SourceParentKey = pSourceParent->f_GetKey();
													auto Mapped = pParent->m_ChildEntitiesMap(SourceParentKey, *pSourceParent, pParent, EEntityCopyFlag_None);
													if (Mapped.f_WasCreated())
													{
														auto &NewDependency = Mapped.f_GetResult();

														if (NewDependency.f_GetKey().m_Type == EEntityType_Dependency)
														{
															TCVector<CEntity *> CreatedDependencies;
															bool bChanged = false;
															if (fp_ExpandEntity(NewDependency, *pParent, &CreatedDependencies, bChanged))
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

														CreatedThisTime[&NewDependency] = pSourceParent;
														ToDelete.f_Insert(&NewDependency);
														pParent = &NewDependency;
														if (!pRootCreated)
															pRootCreated = &NewDependency;
													}
													else
													{
														pParent = &Mapped.f_GetResult();
														DCheck(pSourceParent->f_GetKey().m_Type != EEntityType_Dependency);
														DCheck(CreatedThisTime.f_FindEqual(pParent) && CreatedThisTime[pParent] == pSourceParent);
													}
												}

												if (bAborted)
												{
													if (bCreatedDistinguisher)
														pDistinguisher = nullptr;
													continue;
												}

												TCSet<CEntity *> Deleted;
												if (pRootCreated)
													fp_EvaluateDataRecursive(*pRootCreated, Deleted);
												pEntity = pParent;

												bool const bPassedOn
													= !Deleted.f_FindEqual(pEntity) && (bExternal || f_EvaluateEntityPropertyBool(*pEntity, gc_ConstKey_Dependency_Indirect, false))
												;
												// The copy in the target, where its conditions are evaluated, is only kept for a dependency that the target
												// doesn't order the build by already, as it would add a declaration in every pass
												if (bPassedOn && pMergeInto)
												{
													auto *pDependentTarget = Workspace.m_Targets.f_FindEqual(TargetName);
													if (pDependentTarget)
														fSetDeclarationProperties(*pEntity, *pDependentTarget, _bIndirect, false);
													fMergePassedOn(*pMergeInto, *pEntity, pDependentTarget, _bIndirect);
												}

												if (!bPassedOn || pMergeInto)
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

												if (!(bExternal || f_EvaluateEntityPropertyBool(*pEntity, gc_ConstKey_Dependency_Indirect, false)))
												{
													CreatedThisTime.f_Remove(pEntity);
													pEntity->m_pParent->m_ChildEntitiesMap.f_Remove(pEntity);
													continue;
												}

												if (pMergeInto)
												{
													auto *pDependentTarget = Workspace.m_Targets.f_FindEqual(TargetName);
													if (pDependentTarget)
														fSetDeclarationProperties(*pEntity, *pDependentTarget, _bIndirect, false);
													fMergePassedOn(*pMergeInto, *pEntity, pDependentTarget, _bIndirect);
													continue;
												}
											}
										}
										else
											bIndirect = f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Dependency_Indirect, false);

										if (bExternal)
										{
											auto &Dep = _pTarget->m_DependenciesMap(TargetName).f_GetResult();

											_pTarget->m_DependenciesOrdered.f_Insert(Dep);
											Dep.m_pEntity = fg_Explicit(pEntity);
											Dep.m_Position = pEntity->f_Data().m_Position;
											Dep.m_bExternal = true;
										}
										else
										{
											DMibFastCheck(bIndirect == f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Dependency_Indirect, false));

											auto Properties = f_EvaluateEntityPropertyObject(ChildEntity, gc_ConstKey_Dependency_TargetProperties, CEJsonSorted(EJsonType_Object));

											CTargetInfo *pDependentTarget = fGetTarget(EntityName, TargetName, pEntity->f_Data().m_Position, Properties.f_Move());

											if (pDependentTarget->m_TriedDependenciesMap(pDependentTarget->f_GetName()).f_WasCreated())
												bDependencyAdded = true;

											auto MappedDep = _pTarget->m_DependenciesMap(pDependentTarget->f_GetName());
											auto &Dep = MappedDep.f_GetResult();
											bool const bDeclarationIndirect = (bIndirect && !_bRecursive) || _bIndirect;
											bool const bDeclarationIndirectOrdered = !_bRecursive && f_EvaluateEntityPropertyBool(ChildEntity, gc_ConstKey_Dependency_IndirectOrdered, false);
											// Before the declaration is evaluated, as whether it orders the build can depend on them through its link
											fSetDeclarationProperties(*pEntity, *pDependentTarget, bDeclarationIndirect, bDeclarationIndirectOrdered);

											// Evaluated in the target, which the dependency that another target passes on has its conditions in. Every
											// dependency orders the build in a build system without the settings that declare it, as those of the tests.
											CBuildSystemPropertyInfo OrderedInfo;
											auto Ordered = f_EvaluateEntityPropertyNoDefault(*pEntity, gc_ConstKey_Dependency_Ordered, OrderedInfo);
											bool const bOrdered = !Ordered.f_Get().f_IsValid() || Ordered.f_Get().f_Boolean();

											fMergeDeclaration(Dep, *pEntity);

											_pTarget->m_DependenciesOrdered.f_Insert(Dep);
											// The generators read the declaration that orders the build. One of the stage None that it replaces, or
											// that comes after it, still names the target, as for its properties.
											if (!MappedDep.f_WasCreated() && Dep.m_pEntity.f_Get() && Dep.m_pEntity.f_Get() != pEntity)
											{
												if (Dep.m_bOrdered && !bOrdered)
												{
													Dep.m_ReplacedEntities.f_Insert(fg_Explicit(pEntity));
													continue;
												}
												Dep.m_ReplacedEntities.f_Insert(Dep.m_pEntity);
											}
											// A dependency that declarations order the build by is indirect only where each of them is, and ordered as
											// indirect where any is, also where the last of them, which the generators read, isn't
											bool const bMergeIndirect = !MappedDep.f_WasCreated() && Dep.m_bOrdered && bOrdered;
											Dep.m_pEntity = fg_Explicit(pEntity);
											Dep.m_Position = pEntity->f_Data().m_Position;
											Dep.m_bIndirect = bMergeIndirect ? Dep.m_bIndirect && bDeclarationIndirect : bDeclarationIndirect;
											Dep.m_bIndirectOrdered = bMergeIndirect ? Dep.m_bIndirectOrdered || bDeclarationIndirectOrdered : bDeclarationIndirectOrdered;
											Dep.m_bObjectLibrary = pDependentTarget->m_bObjectLibrary;
											// Also where another dependency, as of a library that the target links, reaches the same target
											Dep.m_bOrdered = bOrdered || (!MappedDep.f_WasCreated() && Dep.m_bOrdered);

											bool bFollowIndirectDependencies = f_EvaluateEntityPropertyBool
												(
													*pDependentTarget->m_pInnerEntity
													, gc_ConstKey_Target_FollowIndirectDependencies
													, false
												)
											;

											if (!bFollowIndirectDependencies)
												continue;

											if (!TargetsAlreadyProcessed.f_FindEqual(pDependentTarget))
												NewTargetsToProcess[pDependentTarget];

											// The dependencies of a dependency of the stage None that the target doesn't link, which only gives the target
											// its properties or orders its compiles after the files that the dependency generates, aren't dependencies of
											// the target. The declarations of the dependency merged decide it, as one that doesn't pass them on can make a
											// dependency direct whose dependencies an earlier declaration follows as those of an indirect one.
											if (Dep.m_bOrdered && Dep.m_bPassesOn)
												fFollowDependencies(pDependentTarget, Dep.m_bIndirect);
										}
									}
								}
							;

							fFindDependencies(*_pTarget->m_pOuterEntity, false, false);

							while (!ToProcess.f_IsEmpty())
							{
								auto ThisTime = fg_Move(ToProcess);
								DMibMovedFromValid(ToProcess);

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
							TCVector<CStr> DependencyTargets;
							TCVector<CStr> Dependencies;
							TCVector<CStr> ExternalDependencies;
							TCVector<CStr> DependencyInjectionGroups;

							// The entities of the dependencies are new in each evaluation, so the merged values are compared with those of the last one
							CEJsonSorted MergedDependencies = EJsonType_Object;
							for (auto &Dependency : _pTarget->m_DependenciesOrdered)
							{
								Dependencies.f_Insert(Dependency.m_pEntity->f_GetPathForGetProperty());

								if (Dependency.m_bExternal)
								{
									ExternalDependencies.f_Insert(Dependency.m_pEntity->f_GetPathForGetProperty());
									continue;
								}

								auto &TargetName = Dependency.f_GetName();

								CTargetInfo *pDependentTarget = Workspace.m_Targets.f_FindEqual(TargetName);
								if (!pDependentTarget)
									fs_ThrowError(Dependency.m_pEntity->f_Data().m_Position, "Internal error dependency not found: '{}'"_f << TargetName);

								// A dependency of the stage None that the target doesn't link injects nothing, also where it orders the build, as
								// what it injects can need its library, as the sources of Lib_Malterlib do
								if (Dependency.m_bOrdered && Dependency.m_bPassesOn)
								{
									DependencyInjectionGroups.f_Insert
										(
											f_EvaluateEntityPropertyStringArray(*pDependentTarget->m_pInnerEntity, gc_ConstKey_Target_DependencyInjectionGroups, TCVector<CStr>())
										)
									;
								}

								auto TargetPropertyPath = pDependentTarget->m_pInnerEntity->f_GetPathForGetProperty();
								for (auto &pReplacedEntity : Dependency.m_ReplacedEntities)
									f_AddExternalProperty(*pReplacedEntity, gc_ConstKey_Dependency_TargetPropertyPath, TargetPropertyPath);
								// Also for an indirect dependency that doesn't order the build, whose properties the target can read
								f_AddExternalProperty(*Dependency.m_pEntity, gc_ConstKey_Dependency_TargetPropertyPath, TargetPropertyPath);

								f_AddExternalProperty(*Dependency.m_pEntity, gc_ConstKey_Dependency_StageEvaluated, Dependency.m_MergedStage);
								f_AddExternalProperty(*Dependency.m_pEntity, gc_ConstKey_Dependency_LinkEvaluated, Dependency.m_bMergedLink);
								f_AddExternalProperty(*Dependency.m_pEntity, gc_ConstKey_Dependency_CompileAfterGeneratedEvaluated, Dependency.m_bMergedCompileAfterGenerated);
								// Whether its declarations order the build as they were merged: the declaration that is left can evaluate another
								// way with the merged values below, as one that only links or compiles after generated files while indirect
								f_AddExternalProperty(*Dependency.m_pEntity, gc_ConstKey_Dependency_OrderedEvaluated, Dependency.m_bOrdered);
								// The declaration was evaluated with its own, which the generators read merged as the other values
								f_AddExternalProperty(*Dependency.m_pEntity, gc_ConstKey_Dependency_IndirectEvaluated, Dependency.m_bIndirect);
								f_AddExternalProperty(*Dependency.m_pEntity, gc_ConstKey_Dependency_IndirectOrderedEvaluated, Dependency.m_bIndirectOrdered);

								// The merged values can depend on what an evaluation of the target changes, so they settle as its lists do
								auto &Merged = MergedDependencies[TargetName];
								Merged = EJsonType_Array;
								Merged.f_Insert(Dependency.m_MergedStage);
								Merged.f_Insert(Dependency.m_bMergedLink);
								Merged.f_Insert(Dependency.m_bMergedCompileAfterGenerated);
								Merged.f_Insert(Dependency.m_bIndirect);
								Merged.f_Insert(Dependency.m_bIndirectOrdered);
								Merged.f_Insert(Dependency.m_bOrdered);

								if (Dependency.m_bIndirect && !Dependency.m_bIndirectOrdered)
									continue;

								if (Dependency.m_bMergedLink)
								{
									Dependency.m_bLink = true;

									if (!_DependencyFilesName.f_IsEmpty())
									{
										CStr FileToAdd = f_EvaluateEntityPropertyString(*pDependentTarget->m_pInnerEntity, gc_ConstKey_Target_DependencyFile, CStr());
										if (!FileToAdd.f_IsEmpty())
											DependencyFiles.f_Insert(FileToAdd);
									}
								}
								// The targets that the target builds after, as the pre-build script of Xcode does after their dependency files
								if (Dependency.m_bOrdered)
									DependencyTargets.f_Insert(TargetPropertyPath);
							}

							auto fAddNameList = [&](CPropertyKeyReference const &_Key, TCVector<CStr> &&_Names) -> bool
								{
									CEJsonSorted NameListArray = EJsonType_Array;

									for (auto &Name : _Names)
										NameListArray.f_Insert(fg_Move(Name));

									return f_AddExternalProperty(fg_RemoveQualifiers(*_pTarget->m_pOuterEntity), _Key, fg_Move(NameListArray));
								}
							;

							bool bChanged = MergedDependencies != _pTarget->m_MergedDependencies;
							if (bChanged)
								_pTarget->m_MergedDependencies = fg_Move(MergedDependencies);

							if (!_DependencyFilesName.f_IsEmpty())
							{
								DependencyFiles.f_Sort();
								if (fAddNameList(DependencyFileNamePropertyKey, fg_Move(DependencyFiles)))
									bChanged = true;
							}

							DependencyTargets.f_Sort();
							bChanged = fAddNameList(gc_ConstKey_Target_DependencyTargets, fg_Move(DependencyTargets)) || bChanged;

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
				CEJsonSorted AllTargetsArray;
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
					{
						CExpandEntityState ExpandState;
						for (bool bDoneSomething = true; fg_Exchange(bDoneSomething, false);)
						{
							f_CheckCancelledException();

							f_PopulateTargetAllFiles(ExpandState, *_Target.m_pOuterEntity);

							if (f_ExpandTargetGroups(ExpandState, Evaluated, *_Target.m_pOuterEntity))
								bDoneSomething = true;

							f_PopulateTargetAllFiles(ExpandState, *_Target.m_pOuterEntity);

							if (f_ExpandTargetFiles(ExpandState, Evaluated, *_Target.m_pOuterEntity))
								bDoneSomething = true;

							f_PopulateTargetAllFiles(ExpandState, *_Target.m_pOuterEntity);

							if (f_GenerateTargetFiles(Evaluated, *_Target.m_pOuterEntity))
								bDoneSomething = true;

							f_PopulateTargetAllFiles(ExpandState, *_Target.m_pOuterEntity);
						}

						ExpandState.m_bEnabled = true;
					}

					auto fFindFiles = [&](this auto &&_fThis, CEntity &_Entity, CGroupInfo *_pParentGroup) -> void
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
										_fThis(ChildEntity, _pParentGroup);
									}
									break;
								case EEntityType_Group:
									{
										CEJsonSorted Hidden = f_GetExternalProperty(ChildEntity, gc_ConstKey_HiddenGroup);
										if (Hidden.f_IsBoolean() && Hidden.f_Boolean())
											_fThis(ChildEntity, _pParentGroup);
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
											_fThis(ChildEntity, &Group);
										}
									}
									break;
								case EEntityType_File:
									{
										CStr EntityName = ChildEntity.f_GetKeyName();

										CFileKey FileKey;
										FileKey.m_FileName = EntityName;
										if (_pParentGroup)
											FileKey.m_GroupPath = _pParentGroup->f_GetPath();

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
														, "Same file specified twice in project is not supported. FileName: {}. GroupPath: {}"_f << EntityName << FileKey.m_GroupPath
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
								default: break;
								}
							}
						}
					;
					fFindFiles(*_Target.m_pOuterEntity, nullptr);

					_Target.m_RootGroup.f_PruneEmpty();
					for (auto iGroup = _Target.m_Groups.f_GetIterator(); iGroup;)
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

		Workspace.m_RootGroup.f_PruneEmpty();
		for (auto iGroup = Workspace.m_Groups.f_GetIterator(); iGroup;)
		{
			if (iGroup->m_Children.f_IsEmpty())
				iGroup.f_Remove();
			else
				++iGroup;
		}
	}

	TCFuture<void> CBuildSystem::f_GenerateBuildSystem(TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> *o_pConfigurations, TCMap<CPropertyKey, CEJsonSorted> const *_pValues) const
	{
		{
			NContainer::TCMap<NStr::CStr, TCSharedPointer<CHashDigest_SHA256>> SourceFiles;
			{
				DMibLockRead(mp_SourceFilesLock);
				SourceFiles = mp_SourceFiles;
			}

			co_await fg_ParallelForEach
				(
					*o_pConfigurations
					, [&](TCUniquePointer<CConfiguraitonData> &o_pConfig) -> TCUnsafeFuture<void>
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
				, [&](TCUniquePointer<CConfiguraitonData> &o_pConfig) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureMalterlibExceptions;
					co_await f_CheckCancelled();

					auto &ConfigData = *o_pConfig;
					TCMap<CPropertyKey, CEJsonSorted> ConfigValues = *_pValues;
					for (auto iTuple = ConfigData.m_Tuples.f_GetIterator(); iTuple; ++iTuple)
					{
						if (iTuple->m_Name)
							ConfigValues[CPropertyKey(mp_StringCache, iTuple->m_Type)] = iTuple->m_Name;
						else
							ConfigValues[CPropertyKey(mp_StringCache, iTuple->m_Type)] = CEJsonSorted{};
					}

					f_EvaluateDataMain(ConfigData.m_Evaluated, ConfigValues);
					f_PopulateAllRepositories(ConfigData.m_Evaluated);
					f_GenerateGlobalFiles(ConfigData.m_Evaluated, true);
					f_ExpandDynamicImports(ConfigData.m_Evaluated);
					f_ExpandGlobalTargetsAndWorkspaces(ConfigData.m_Evaluated);
					f_EvalGlobalWorkspaces(ConfigData.m_Evaluated, ConfigData.m_Targets);
					f_ExpandGlobalEntities(ConfigData.m_Evaluated);
					f_PopulateAllRepositories(ConfigData.m_Evaluated);
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
							fs_ThrowError(Entity.f_Data().m_Position, CStr::CFormat("A entity with this name already exists ({} != {})") << EntityName << Workspace.m_EntityName);
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
