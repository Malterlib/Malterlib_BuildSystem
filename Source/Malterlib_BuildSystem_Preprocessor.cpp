// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Preprocessor.h"
#include "Malterlib_BuildSystem_Condition.h"

namespace NMib::NBuildSystem
{
	CBuildSystemPreprocessor::CBuildSystemPreprocessor
		(
			CBuildSystemRegistry &_ResultRegistry
			, TCSet<CStr> &_SourceFiles
			, CFindCache const &_FindCache
			, TCMap<CStr, CStr> const &_Environment
		)
		: mp_ResultRegistry(_ResultRegistry)
		, mp_SourceFiles(_SourceFiles)
		, mp_FindCache(_FindCache)
		, mp_Environment(_Environment)
	{
	}

	void CBuildSystemPreprocessor::fsp_ThrowError(CBuildSystemRegistry const &_Registry, CStr const &_Error)
	{
		DMibError(NStr::CStr::CFormat("{} {}") << _Registry.f_GetLocation() << _Error);
	}

	void CBuildSystemPreprocessor::fpr_FindFilesRecursive(CBuildSystemRegistry &_Registry, TCVector<CStr> &o_Files, CStr const &_Path, CStr const &_ToFind)
	{
		CStr PrePath = _Path;
		CStr ToFind = _ToFind;
		while (!ToFind.f_IsEmpty())
		{
			CStr Path = fg_GetStrSepNoTrim(ToFind, "/");
			CStr FullPath;
			if (PrePath.f_IsEmpty() && Path.f_IsEmpty())
				FullPath = "/";
			else
				FullPath = CFile::fs_AppendPath(PrePath, Path);
			if (Path.f_FindChars("*?") >= 0)
			{
				if (ToFind.f_IsEmpty())
				{
					// We are looking for files
					CFindOptions FindOptions(FullPath, EFileAttrib_File);
					auto Files = mp_FindCache.f_FindFiles(FindOptions, true);
					for (auto &File : Files)
						o_Files.f_Insert(File.m_Path);
				}
				else
				{
					// We are looking for directories
					CFindOptions FindOptions(FullPath, EFileAttrib_Directory);
					auto Files = mp_FindCache.f_FindFiles(FindOptions, true);
					for (auto &File : Files)
						fpr_FindFilesRecursive(_Registry, o_Files, File.m_Path, ToFind);
				}
				return;
			}
			else if (ToFind.f_IsEmpty())
			{
				if (CFile::fs_FileExists(FullPath, EFileAttrib_File))
				{
					mp_FindCache.f_AddSourceFile(FullPath);
					o_Files.f_Insert(FullPath);
				}
			}
			PrePath = FullPath;
		}
	}

	void CBuildSystemPreprocessor::fpr_HandleIncludes(CBuildSystemRegistry &_RootRegistry, CStr const &_Path, TCVector<CError> &o_Errors)
	{
		_RootRegistry.f_TransformFunc
			(
				[&](CBuildSystemRegistry &_Registry)
				{
					auto &Name = _Registry.f_GetName();
					if (!Name.f_IsUserType())
						return;

					auto *pUserType = &Name.f_UserType();

					auto *pType = pUserType->m_Value.f_GetMember("Type");

					if (!pType)
						return;

					CEJSON Param;
					if (pType->f_String() == "Expression")
					{
						auto *pParam = pUserType->m_Value.f_GetMember("Param");
						if (!pParam)
							return;

						Param = CEJSON::fs_FromJSON(*pParam);

						if (!Param.f_IsUserType())
							return;

						pUserType = &Param.f_UserType();

						pType = pUserType->m_Value.f_GetMember("Type");
						if (!pType)
							return;
					}

					if (pType->f_String() != "Identifier")
						return;

					CFilePosition Position = _Registry;
					auto Identifier = CBuildSystemSyntax::CIdentifier::fs_FromJSON(*pUserType, Position);

					if (!Identifier.f_IsNameConstantString())
						return;

					if (Identifier.m_EntityType != EEntityType_Invalid || !Identifier.m_bEmptyPropertyType)
						return;

					bool bInclude = Identifier.f_NameConstantString() == "Include";
					bool bImport = Identifier.f_NameConstantString() == "Import" && _Registry.f_GetThisValue().f_IsValid();
					if (!bInclude && !bImport)
						return;

					bool bDoInclude = true;
					if (_Registry.f_HasChildren())
					{
						CCondition Condition;
						for (auto &Child : _Registry.f_GetChildren())
							CCondition::fs_ParseCondition(Child, Condition);
						if (!Condition.f_SimpleEval(mp_Environment))
							bDoInclude = false;
					}
					auto pParent = _Registry.f_GetParent();

					if (pParent->f_GetParent() && bImport)
						fsp_ThrowError(_Registry, "You can only import at root scope");

					if (bDoInclude)
					{
						if (!_Registry.f_GetThisValue().f_IsString())
							fsp_ThrowError(_Registry, "Import should be a string value");

						CStr File = _Registry.f_GetThisValue().f_String();
						CStr FullPath = CFile::fs_GetExpandedPath(File, _Path);

						TCVector<CStr> Files;

						if (FullPath.f_FindChars("*?") >= 0)
						{
							// Wildcard search
							fpr_FindFilesRecursive(_Registry, Files, CStr(), FullPath);

							if (Files.f_IsEmpty())
							{
								if (bInclude)
									o_Errors.f_Insert(CError{&_Registry, CStr::CFormat("No files found for included pattern '{}'") << FullPath});
								else
									o_Errors.f_Insert(CError{&_Registry, CStr::CFormat("No files found for imported pattern '{}'") << FullPath});
								return;
							}
						}
						else
						{
							if (!CFile::fs_FileExists(FullPath, EFileAttrib_File))
							{
								if (bInclude)
									o_Errors.f_Insert(CError{&_Registry, CStr::CFormat("Include file '{}' does not exist") << FullPath});
								else
									o_Errors.f_Insert(CError{&_Registry, CStr::CFormat("Import file '{}' does not exist") << FullPath});
								
								return;
							}
							Files.f_Insert(FullPath);
						}

						auto fAddFile
							= [&](CStr const &_File)
							{
								auto SourceFile = mp_SourceFiles(_File);
								if (bImport && !SourceFile.f_WasCreated())
								{
									// Already imported
									return;
								}

								CStr FileData = CFile::fs_ReadStringFromFile(CStr(_File), true);
								CStr Path = CFile::fs_GetPath(_File);
								CBuildSystemRegistry IncludedRegistry;
								IncludedRegistry.f_ParseStr(FileData, _File);

								fpr_HandleIncludes(IncludedRegistry, Path, o_Errors);

								auto pPrevious = &_Registry;
								for (auto iChild = IncludedRegistry.f_GetChildIterator(); iChild; )
								{
									auto pChild = iChild.f_GetCurrent();
									++iChild;
									pParent->f_MoveChild(pChild, pPrevious);
									pPrevious = pChild;
								}
							}
						;

						for (auto &File : Files)
							fAddFile(File);
					}

					pParent->f_DeleteChild(&_Registry);
				}
			)
		;
	}

	void CBuildSystemPreprocessor::f_ReadFile(CStr const &_Path)
	{
		mp_FileLocation = CFile::fs_GetExpandedPath(_Path);
		mp_SourceFiles[mp_FileLocation];
		if (!CFile::fs_FileExists(mp_FileLocation, EFileAttrib_File))
			DError(CStr(CStr::CFormat("Input file '{}' does not exist") << mp_FileLocation));
		CStr FileData = CFile::fs_ReadStringFromFile(CStr(mp_FileLocation), true);
		CStr Path = CFile::fs_GetPath(mp_FileLocation);

		CBuildSystemRegistry TempRegistry;
		TempRegistry.f_ParseStr(FileData, mp_FileLocation);

		CBuildSystemRegistry *pPrevious = nullptr;
		for (auto iChild = mp_ResultRegistry.f_GetChildIterator(); iChild; ++iChild)
		{
			pPrevious = iChild;
		}

		for (auto iChild = TempRegistry.f_GetChildIterator(); iChild; )
		{
			auto pChild = iChild.f_GetCurrent();
			++iChild;
			mp_ResultRegistry.f_MoveChild(pChild, pPrevious);
			pPrevious = pChild;
		}

		TCVector<CError> Errors;
		fpr_HandleIncludes(mp_ResultRegistry, Path, Errors);
		if (!Errors.f_IsEmpty())
			fsp_ThrowError(*Errors.f_GetFirst().m_pRootRegistry, Errors.f_GetFirst().m_Error);
	}

	CStr const &CBuildSystemPreprocessor::f_GetFileLocation()
	{
		return mp_FileLocation;
	}
}
