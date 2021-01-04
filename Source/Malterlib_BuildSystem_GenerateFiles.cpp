// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_GenerateGlobalFiles(CBuildSystemData &_BuildSystemData) const
	{
		fp_GenerateFiles(_BuildSystemData, _BuildSystemData.m_RootEntity, false, EEntityType_Root);
	}
	
	void CBuildSystem::f_GenerateWorkspaceFiles(CBuildSystemData &_BuildSystemData, CEntity & _Target) const
	{
		fp_GenerateFiles(_BuildSystemData, _Target, true, EEntityType_Workspace);
	}
	
	void CBuildSystem::f_GenerateTargetFiles(CBuildSystemData &_BuildSystemData, CEntity & _Target) const
	{
		fp_GenerateFiles(_BuildSystemData, _Target, true, EEntityType_Target);
	}
	
	void CBuildSystem::fp_GenerateFiles(CBuildSystemData &_BuildSystemData, CEntity & _Entity, bool _bRecursive, EEntityType _Type) const
	{
		CStr Workspace;
		
		auto * pParent = &_Entity;
		while (pParent && pParent->m_pParent)
		{
			if (pParent->f_GetMapKey().m_Type == EEntityType_Workspace)
			{
				Workspace = f_EvaluateEntityProperty(*pParent, EPropertyType_Workspace, "Name");
				break;
			}
			pParent = pParent->m_pParent;
		}
		
		TCFunction<void (CEntity &_Entity)> fGenerateFile
			= [&](CEntity &_Entity)
			{
				for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
				{
					auto &Key = iChild->f_GetMapKey();
					if 
						(
							_bRecursive
							&& 
							(
								Key.m_Type == EEntityType_Group
								|| Key.m_Type == EEntityType_Target
							)
						)
					{
						if (_Type == EEntityType_Workspace && Key.m_Type == EEntityType_Target)
							continue;
						fGenerateFile(*iChild);
						continue;
					}

					auto &ToGenerate = *iChild;

					if (Key.m_Type != EEntityType_GenerateFile)
						continue;

					DMibLock(ToGenerate.m_Lock);
					CStr Identities;
					{
						CEvaluationContext EvalContext(&ToGenerate.m_EvaluatedProperties);
						Identities = fp_EvaluatePropertyValue(ToGenerate, ToGenerate, ToGenerate.m_Key.m_Name, ToGenerate.m_Position, EvalContext);
					}

					while (!Identities.f_IsEmpty())
					{
						CStr IdentityFull = fg_GetStrSep(Identities, ";");
						if (IdentityFull.f_IsEmpty())
							continue;
						
						CStr Identity = fg_GetStrSep(IdentityFull, "|");
						
						CEntity TempEntity(&ToGenerate);

						TempEntity.m_Key.m_Type = EEntityType_File;
						TempEntity.m_Key.m_Name = Identity;

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
						
						CStr Path = mp_GeneratorInterface->f_GetExpandedPath(f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "Name"), f_GetBaseDir());

						if (Path.f_IsEmpty())
						{
							fsp_ThrowError(iChild->m_Position, "No path specified for generated file");
						}

						EGeneratedFileFlag GeneratedFileFlags = EGeneratedFileFlag_None;

						CStr Contents = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "Contents");
						bool bUnicodeBOM = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "UnicodeBOM") != "false";
						bool bUTF16 = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "UTF16") == "true";
						bool bExecutable = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "Executable") == "true";
						bool bUnixLineEnds = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "UnixLineEnds") == "true";
						bool bSymlink = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "Symlink") == "true";
						bool bSymlinkDirectory = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "SymlinkDirectory") == "true";
						if (f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "NoDateCheck") == "true" || bSymlink)
							GeneratedFileFlags |= EGeneratedFileFlag_NoDateCheck;
						if (f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "KeepGeneratedFile") == "true")
							GeneratedFileFlags |= EGeneratedFileFlag_KeepGeneratedFile;
						if (bSymlink)
							GeneratedFileFlags |= EGeneratedFileFlag_Symlink;

						Contents = Contents.f_Replace("\r\n", "\n");
						if (!bUnixLineEnds)
							Contents = Contents.f_Replace("\n", "\r\n");

						bool bWasCreated;
						if (!f_AddGeneratedFile(Path, Contents, Workspace, bWasCreated, GeneratedFileFlags))
						{
							fsp_ThrowError(iChild->m_Position, fg_Format("Same file generated with different data: {}", Path));
						}

						if (bWasCreated)
						{
							CFile::fs_CreateDirectory(CFile::fs_GetPath(Path));
							if (bSymlink)
							{
								CStr SymlinkBasePath = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "SymlinkBasePath");
								if (SymlinkBasePath.f_IsEmpty())
									fsp_ThrowError(iChild->m_Position, "If you specify SymLink you also need to specify the SymlinkBasePath. This is the path under which there can be no symlinks in the parent path of the symlink being created. If a symlink is found here it will be deleted before the new symlink is created.");

								if (!Path.f_StartsWith(SymlinkBasePath + "/") || Path == SymlinkBasePath)
									fsp_ThrowError(iChild->m_Position, "The name of the generated file '{}' must be prefixed by the SymlinkBasePath '{}'"_f << Path << SymlinkBasePath);

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
													DConErrOut2("Deleting symlink in parent path '{}'\n", ParentPath);
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
										DMibConErrOut2("{} File lock: {}\n", &LockFile, FileLockName);

									LockFile.f_Lock();

									if (mp_bDebugFileLocks)
										DMibConErrOut2("{} File locked: {}\n", &LockFile, FileLockName);

									auto CleanupLock = g_OnScopeExit > [&]
										{
											if (mp_bDebugFileLocks)
												DMibConErrOut2("{} File lock released: {}\n", &LockFile, FileLockName);
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
											DConErrOut2("Deleting invalid symlink '{}': {} != {}\n", Path, Resolved, Contents);
										}
										catch (CExceptionFile const &)
										{
											DConErrOut2("Deleting path invalid symlink '{}'\n", Path);
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

						if (ToGenerate.m_pParent->m_Key.m_Type != EEntityType_Root)
						{
							CEntityKey NewKey;
							NewKey.m_Type = EEntityType_File;
							auto &FullFileName = Path;
							NewKey.m_Name = FullFileName;
							
							CStr Group = f_EvaluateEntityProperty(TempEntity, EPropertyType_Property, "Group");
							auto pParentGroup = &_Entity;
							
							while (!Group.f_IsEmpty())
							{
								CStr ParentGroup = fg_GetStrSep(Group, "/");
								
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
							}
							
							auto pOldEntity = pParentGroup->m_ChildEntitiesMap.f_FindEqual(NewKey);

							if (pOldEntity)
							{
								DCheck(pOldEntity != &(*iChild));
								pParentGroup->m_ChildEntitiesMap.f_Remove(pOldEntity);
							}

							auto &NewEntity = pParentGroup->m_ChildEntitiesMap(NewKey, pParentGroup).f_GetResult();
							pParentGroup->m_ChildEntitiesOrdered.f_Insert(NewEntity);
							NewEntity = ToGenerate;
							NewEntity.m_Key = NewKey;
							NewEntity.m_EvaluatedProperties.f_Clear();
							NewEntity.m_PotentialExplicitProperties.f_Clear();
							NewEntity.m_PerFilePotentialExplicitProperties.f_Clear();

							CPropertyKey PropertyKey;
							PropertyKey.m_Type = EPropertyType_Property;
							PropertyKey.m_Name = "Contents";
							NewEntity.m_Properties.f_Remove(PropertyKey);
							PropertyKey.m_Name = "Name";
							NewEntity.m_Properties.f_Remove(PropertyKey);
							PropertyKey.m_Name = "Unicode";
							NewEntity.m_Properties.f_Remove(PropertyKey);

							f_ReEvaluateData(NewEntity);
						}
					}
				}
			}
		;

		fGenerateFile(_Entity);
	}
}
