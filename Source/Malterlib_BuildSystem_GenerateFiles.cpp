// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_GenerateGlobalFiles(CBuildSystemData &_BuildSystemData, bool _bBeforeImports) const
	{
		fp_GenerateFiles(_BuildSystemData, _BuildSystemData.m_RootEntity, false, EEntityType_Root, &gc_ConstKey_GenerateFile_BeforeImports, _bBeforeImports);
	}

	void CBuildSystem::f_GenerateWorkspaceFiles(CBuildSystemData &_BuildSystemData, CEntity &_Target) const
	{
		fp_GenerateFiles(_BuildSystemData, _Target, true, EEntityType_Workspace, nullptr, false);
	}

	bool CBuildSystem::f_GenerateTargetFiles(CBuildSystemData &_BuildSystemData, CEntity &_Target) const
	{
		return fp_GenerateFiles(_BuildSystemData, _Target, true, EEntityType_Target, nullptr, false);
	}

	bool CBuildSystem::fp_GenerateFiles
		(
			CBuildSystemData &_BuildSystemData
			, CEntity &_Entity
			, bool _bRecursive
			, EEntityType _Type
			, CPropertyKeyReference const *_pConditionalProperty
			, bool _bConditional
		) const
	{
		CStr Workspace;

		bool bChanged = false;

		auto * pParent = &_Entity;
		while (pParent && pParent->m_pParent)
		{
			if (pParent->f_GetKey().m_Type == EEntityType_Workspace)
			{
				Workspace = f_EvaluateEntityPropertyString(*pParent, gc_ConstKey_Workspace_Name);
				break;
			}
			pParent = pParent->m_pParent;
		}

		auto fGenerateFile = [&](this auto &&_fThis, CEntity &_Entity) -> void
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
				{
					auto &Key = iChild->f_GetKey();
					if
						(
							_bRecursive
							&&
							(
								Key.m_Type == EEntityType_Group
								|| Key.m_Type == EEntityType_Target
								|| Key.m_Type == EEntityType_Import
							)
						)
					{
						if (_Type == EEntityType_Workspace && Key.m_Type == EEntityType_Target)
							continue;
						if (_Type == EEntityType_Target && !Key.m_Name.f_IsConstantString())
							continue;

						_fThis(*iChild);
						continue;
					}

					auto &ToGenerate = *iChild;

					if (Key.m_Type != EEntityType_GenerateFile)
						continue;

					auto &ToGenerateData = ToGenerate.f_Data();

					CEvaluatedProperties TempProperties;
					TempProperties.m_pParentProperties = &ToGenerate.m_EvaluatedProperties;

					TCVector<CEJSONSorted> Identities;
					CBuildSystemUniquePositions Positions;
					{
						CEvaluationContext EvalContext(&TempProperties);
						CEvalPropertyValueContext Context{ToGenerate, ToGenerate, ToGenerateData.m_Position, EvalContext, nullptr, f_EnablePositions(&Positions)};
						auto Value = fp_EvaluatePropertyValue(Context, ToGenerate.f_GetKey().m_Name, nullptr);
						auto ValueRef = Value.f_Get();
						if (ValueRef.f_IsArray())
							Identities = Value.f_MoveArray();
						else
							Identities.f_Insert(Value.f_Move());
					}

					if (ToGenerateData.m_Position.f_IsValid())
					{
						CStr Message = gc_ConstString_File_entity;
						Positions.f_AddPositionFirst(ToGenerateData.m_Position, Message, Message);
					}

					for (auto &IdentityObject : Identities)
					{
						CStr Identity;

						if (IdentityObject.f_IsObject())
						{
							auto *pName = IdentityObject.f_GetMember(gc_ConstString_Name, EJSONType_String);
							if (!pName)
								fs_ThrowError(Positions, "Expected a 'Name' for string type in generate file object");

							Identity = pName->f_String();
						}
						else if (IdentityObject.f_IsString())
							Identity = IdentityObject.f_String();
						else
							fs_ThrowError(Positions, "Expected generate file to be a string or an object");

						CEntityKey NewEntityKey{.m_Type = EEntityType_GenerateFile, .m_Name = {Identity}};

						auto NewEntityMap = ToGenerate.m_ChildEntitiesMap(NewEntityKey, &ToGenerate);
						auto &TempEntity = *NewEntityMap;
						TempEntity.f_DataWritable().m_Position = ToGenerateData.m_Position;

						auto Cleanup = g_OnScopeExit / [&]
							{
								if (NewEntityMap.f_WasCreated())
									ToGenerate.m_ChildEntitiesMap.f_Remove(NewEntityKey);
							}
						;

						if (IdentityObject.f_IsObject())
						{
							auto *pProperties = IdentityObject.f_GetMember("Properties", EJSONType_Object);
							if (!pProperties)
								fs_ThrowError(Positions, "Expected a 'Properties' of object type in generate file object");

							for (auto &SourceProperty : pProperties->f_Object())
							{
								CPropertyKey Key = CPropertyKey::fs_FromString(mp_StringCache, SourceProperty.f_Name(), ToGenerateData.m_Position);

								auto &Property = TempEntity.m_EvaluatedProperties.m_Properties[Key];
								Property.m_Value = SourceProperty.f_Value();
								Property.m_Type = EEvaluatedPropertyType_External;
								Property.m_pProperty = &mp_ExternalProperty[Key.f_GetType()];
							}
						}

						static constexpr CPropertyKeyReference const *c_AllGenerateFileProperties[] =
							{
								&gc_ConstKey_GenerateFile_Name
								, &gc_ConstKey_GenerateFile_Contents
								, &gc_ConstKey_GenerateFile_UnicodeBOM
								, &gc_ConstKey_GenerateFile_UTF16
								, &gc_ConstKey_GenerateFile_Executable
								, &gc_ConstKey_GenerateFile_UnixLineEnds
								, &gc_ConstKey_GenerateFile_Symlink
								, &gc_ConstKey_GenerateFile_SymlinkDirectory
								, &gc_ConstKey_GenerateFile_NoDateCheck
								, &gc_ConstKey_GenerateFile_KeepGeneratedFile
								, &gc_ConstKey_GenerateFile_SymlinkBasePath
								, &gc_ConstKey_GenerateFile_Group
							}
						;

						if (!f_EvalCondition(TempEntity, ToGenerateData.m_Condition, ToGenerateData.m_DebugFlags & EPropertyFlag_TraceCondition))
							continue;

						if (_pConditionalProperty)
						{
							if (f_EvaluateEntityPropertyBool(TempEntity, *_pConditionalProperty, false) != _bConditional)
								continue;
						}

						CStr Path = mp_GeneratorInterface->f_GetExpandedPath(f_EvaluateEntityPropertyString(TempEntity, gc_ConstKey_GenerateFile_Name, CStr()), f_GetBaseDir());

						if (Path.f_IsEmpty())
							fsp_ThrowError(iChild->f_Data().m_Position, "No path specified for generated file");

						EGeneratedFileFlag GeneratedFileFlags = EGeneratedFileFlag_None;

						CStr Contents = f_EvaluateEntityPropertyString(TempEntity, gc_ConstKey_GenerateFile_Contents, CStr());
						bool bUnicodeBOM = f_EvaluateEntityPropertyBool(TempEntity, gc_ConstKey_GenerateFile_UnicodeBOM, true);
						bool bUTF16 = f_EvaluateEntityPropertyBool(TempEntity, gc_ConstKey_GenerateFile_UTF16, false);
						bool bExecutable = f_EvaluateEntityPropertyBool(TempEntity, gc_ConstKey_GenerateFile_Executable, false);
						bool bUnixLineEnds = f_EvaluateEntityPropertyBool(TempEntity, gc_ConstKey_GenerateFile_UnixLineEnds, false);
						bool bSymlink = f_EvaluateEntityPropertyBool(TempEntity, gc_ConstKey_GenerateFile_Symlink, false);
						bool bSymlinkDirectory = f_EvaluateEntityPropertyBool(TempEntity, gc_ConstKey_GenerateFile_SymlinkDirectory, false);

						if (f_EvaluateEntityPropertyBool(TempEntity, gc_ConstKey_GenerateFile_NoDateCheck, false) || bSymlink)
							GeneratedFileFlags |= EGeneratedFileFlag_NoDateCheck;

						if (f_EvaluateEntityPropertyBool(TempEntity, gc_ConstKey_GenerateFile_KeepGeneratedFile, false))
							GeneratedFileFlags |= EGeneratedFileFlag_KeepGeneratedFile;

						if (bSymlink)
							GeneratedFileFlags |= EGeneratedFileFlag_Symlink;

						Contents = Contents.f_Replace("\r\n", "\n");
						if (!bUnixLineEnds)
							Contents = Contents.f_Replace("\n", "\r\n");

						bool bWasCreated;
						if (!f_AddGeneratedFile(Path, Contents, Workspace, bWasCreated, GeneratedFileFlags))
						{
							fsp_ThrowError(iChild->f_Data().m_Position, fg_Format("Same file generated with different data: {}", Path));
						}

						if (bWasCreated)
						{
							CFile::fs_CreateDirectory(CFile::fs_GetPath(Path));
							if (bSymlink)
							{
								CStr SymlinkBasePath = f_EvaluateEntityPropertyString(TempEntity, gc_ConstKey_GenerateFile_SymlinkBasePath, CStr());
								if (SymlinkBasePath.f_IsEmpty())
								{
									fsp_ThrowError
										(
											iChild->f_Data().m_Position
											, "If you specify SymLink you also need to specify the SymlinkBasePath."
											" This is the path under which there can be no symlinks in the parent path of the symlink being created."
											" If a symlink is found here it will be deleted before the new symlink is created."
										)
									;
								}

								if (!Path.f_StartsWith(SymlinkBasePath + "/") || Path == SymlinkBasePath)
								{
									fsp_ThrowError
										(
											iChild->f_Data().m_Position
											, "The name of the generated file '{}' must be prefixed by the SymlinkBasePath '{}'"_f << Path << SymlinkBasePath
										)
									;
								}

								CFile::fs_CreateDirectory(SymlinkBasePath);

								do
								{
									auto fCheckParentPaths = [&]() -> bool
										{
											TCVector<CStr> ParentPaths;
											CStr ParentPath = CFile::fs_GetPath(Path);
											ParentPaths.f_Insert(ParentPath);
											while (ParentPath != SymlinkBasePath)
											{
												ParentPath = CFile::fs_GetPath(ParentPath);
												ParentPaths.f_Insert(ParentPath);
											}
											for (auto &ParentPath : ParentPaths.f_Reverse())
											{
												if (CFile::fs_FileExists(ParentPath, EFileAttrib_Link))
												{
													f_OutputConsole("Deleting symlink in parent path '{}'\n"_f << ParentPath, true);
													CFile::fs_DeleteFile(ParentPath);
													return true;
												}
											}
											return false;
										}
									;
									auto fIsValidAlready = [&]
										{
											if (CFile::fs_FileExists(Path))
											{
												try
												{
													auto Resolved = CFile::fs_ResolveSymbolicLink(Path);
		#ifdef DPlatformFamily_Windows
													if (Resolved.f_CmpNoCase(Contents) == 0)
														return true;
		#else
													if (Resolved == Contents)
														return true;
		#endif
												}
												catch (CExceptionFile const &)
												{
												}
											}
											return false;
										}
									;
									if (fIsValidAlready())
										break;

									CStr FileLockName = SymlinkBasePath / ".lock";
									CLockFile LockFile(FileLockName);

									if (mp_bDebugFileLocks)
										f_OutputConsole("{} File lock: {}\n"_f << &LockFile << FileLockName, true);

									LockFile.f_Lock();

									if (mp_bDebugFileLocks)
										f_OutputConsole("{} File locked: {}\n"_f << &LockFile << FileLockName, true);

									auto CleanupLock = g_OnScopeExit / [&]
										{
											if (mp_bDebugFileLocks)
												f_OutputConsole("{} File lock released: {}\n"_f << &LockFile << FileLockName, true);
										}
									;

									if (CFile::fs_FileExists(Path))
									{
										try
										{
											auto Resolved = CFile::fs_ResolveSymbolicLink(Path);
#ifdef DPlatformFamily_Windows
											if (Resolved.f_CmpNoCase(Contents) == 0)
												break;
#else
											if (Resolved == Contents)
												break;
#endif
											f_OutputConsole("Deleting invalid symlink '{}': {} != {}\n"_f << Path << Resolved << Contents, true);
										}
										catch (CExceptionFile const &)
										{
											f_OutputConsole("Deleting path invalid symlink '{}'\n"_f << Path, true);
										}

										if (!fCheckParentPaths())
											CFile::fs_DeleteDirectoryRecursive(Path);
									}
									else
										fCheckParentPaths();

									CFile::fs_CreateDirectory(CFile::fs_GetPath(Path));
									CFile::fs_CreateSymbolicLink(Contents, Path, bSymlinkDirectory ? EFileAttrib_Directory : EFileAttrib_None, ESymbolicLinkFlag_Relative);
								}
								while (false)
									;
							}
							else
							{
								CByteVector FileDataVector;
								if (bUTF16)
								{
									NStr::CWStr UTF16 = fg_ForceStrUTF16(Contents);
									uint8 BOM[] = {0xFF, 0xFE};
									if (bUnicodeBOM)
										FileDataVector.f_Insert(BOM, sizeof(BOM));
									FileDataVector.f_Insert((const uint8 *)UTF16.f_GetStr(), UTF16.f_GetLen()*sizeof(ch16));
								}
								else
									CFile::fs_WriteStringToVector(FileDataVector, CStr(Contents), bUnicodeBOM);

								f_WriteFile(FileDataVector, Path);

								auto OldAttribs = CFile::fs_GetAttributes(CStr(Path));
								auto NewAttribs = OldAttribs;
								if (bExecutable)
									NewAttribs |= EFileAttrib_Executable;
								else
									NewAttribs &= ~EFileAttrib_Executable;
								if (NewAttribs != OldAttribs)
									CFile::fs_SetAttributes(CStr(Path), NewAttribs);
							}
						}

						if (ToGenerate.m_pParent->f_GetKey().m_Type == EEntityType_Root)
							continue;

						CEntityKey NewKey;
						NewKey.m_Type = EEntityType_File;
						auto &FullFileName = Path;
						NewKey.m_Name.m_Value = FullFileName;

						CStr Group = f_EvaluateEntityPropertyString(TempEntity, gc_ConstKey_GenerateFile_Group, CStr());
						auto pParentGroup = &_Entity;

						while (!Group.f_IsEmpty())
						{
							CStr ParentGroup = fg_GetStrSep(Group, "/");

							CEntityKey Key;
							Key.m_Name.m_Value = ParentGroup;
							Key.m_Type = EEntityType_Group;
							auto Child = pParentGroup->m_ChildEntitiesMap(Key, pParentGroup);
							(*Child).f_DataWritable().m_Position = pParentGroup->f_Data().m_Position;
							pParentGroup = &*Child;
						}

						mint ExpandedOrGeneratedFromSource = _Entity.f_ExpandedOrGeneratedFromSource();

						auto pOldEntity = pParentGroup->m_ChildEntitiesMap.f_FindEqual(NewKey);

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
								continue;
							}

							OldExpandedOrGeneratedFromSet = OldEntityData.m_ExpandedOrGeneratedFromSet;
							if (OldEntityData.m_ExpandedOrGeneratedFrom)
								OldExpandedOrGeneratedFromSet[OldEntityData.m_ExpandedOrGeneratedFrom];

							DCheck(pOldEntity != &(*iChild));
							pParentGroup->m_ChildEntitiesMap.f_Remove(pOldEntity);
						}

						bChanged = true;

						auto &NewEntity = pParentGroup->m_ChildEntitiesMap(NewKey, ToGenerate, pParentGroup, EEntityCopyFlag_CopyChildren).f_GetResult();
						auto &NewEntityData = NewEntity.f_DataWritable();

						NewEntityData.m_ExpandedOrGeneratedFromSet = fg_Move(OldExpandedOrGeneratedFromSet);
						NewEntityData.m_ExpandedOrGeneratedFrom = ExpandedOrGeneratedFromSource;

						NewEntity.m_EvaluatedProperties.m_Properties.f_Clear();

						for (auto &pProperty : c_AllGenerateFileProperties)
							NewEntityData.m_Properties.f_Remove(*pProperty);

						if (IdentityObject.f_IsObject())
						{
							auto *pProperties = IdentityObject.f_GetMember("Properties", EJSONType_Object);
							if (!pProperties)
								fs_ThrowError(Positions, "Expected a 'Properties' of object type in generate file object");

							for (auto &SourceProperty : pProperties->f_Object())
							{
								CPropertyKey Key = CPropertyKey::fs_FromString(mp_StringCache, SourceProperty.f_Name(), ToGenerateData.m_Position);

								auto &Property = NewEntity.m_EvaluatedProperties.m_Properties[Key];
								Property.m_Value = SourceProperty.f_Value();
								Property.m_Type = EEvaluatedPropertyType_External;
								Property.m_pProperty = &mp_ExternalProperty[Key.f_GetType()];
							}
						}

						NewEntityData.m_HasFullEval = 0;

						for (auto &Property : NewEntityData.m_Properties)
						{
							auto &Key = NewEntityData.m_Properties.fs_GetKey(Property);
							if (Key.m_Name == gc_ConstString_FullEval.m_String)
								NewEntityData.m_HasFullEval |= 1 << Key.f_GetType();
						}
					}
				}
			}
		;

		fGenerateFile(_Entity);

		return bChanged;
	}
}
