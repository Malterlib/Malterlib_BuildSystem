// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

#include "Malterlib_BuildSystem_GeneratorState.h"

namespace NMib::NBuildSystem
{
	void fg_Malterlib_BuildSystem_MakeActive_VisualStudio();
	void fg_Malterlib_BuildSystem_MakeActive_Xcode();
	
	CBuildSystem::CBuildSystem()
		: mp_NowUTC(NTime::CTime::fs_NowUTC())
	{
		mp_Now = mp_NowUTC.f_ToLocal();
		fg_Malterlib_BuildSystem_MakeActive_VisualStudio();
		fg_Malterlib_BuildSystem_MakeActive_Xcode();
	}
	
	TCSet<CStr> CBuildSystem::f_GetSourceFiles() const
	{
		DMibLockRead(mp_SourceFilesLock);
		return mp_SourceFiles;
	}
	
	void CBuildSystem::f_AddSourceFile(CStr const &_File) const
	{
		DMibLock(mp_SourceFilesLock);
		mp_SourceFiles[_File];
	}

	bool CBuildSystem::f_AddGeneratedFile(CStr const &_File, CStr const &_Data, CStr const &_Workspace, bool &_bWasCreated, bool _bNoDateCheck) const
	{
		DMibLock(mp_GeneratedFilesLock);
		auto &File = mp_GeneratedFiles[_File];

		if (!File.m_bAdded)
		{
			File.m_Contents = _Data;
			File.m_bAdded = true;
			_bWasCreated = true;
		}
		else if (File.m_Contents != _Data)
		{
			DMibConOut("Original contents:" DNewLine DNewLine "{}" DNewLine DNewLine, File.m_Contents);
			DMibConOut("Incompatible contents:" DNewLine DNewLine "{}" DNewLine DNewLine, _Data);
			return false;
		}
		else
			_bWasCreated = false;

		File.m_Workspaces[_Workspace];
		if (_bNoDateCheck)
			File.m_bNoDateCheck = true;

		return true;
	}

	CStr CBuildSystem::f_GetBaseDir() const
	{
		return mp_BaseDir;
	}

	void CBuildSystem::f_SetGeneratorInterface(CGeneratorInterface *_pInterface) const
	{
		mp_GeneratorInterface = _pInterface;
	}

	bool CBuildSystem::f_WriteFile(TCVector<uint8> const& _FileData, CStr const& _File, EFileAttrib _AddAttribs) const
	{
#			if 0
			CStr FileExtension = CFile::fs_GetExtension(_File);
			CStr FileName = CFile::fs_GetFile(_File);
		
			if (FileName != "VersionInfo.cpp" && FileExtension != "plist" && CFile::fs_FileExists(_File))
			{
				TCVector<uint8> OldFileContents = CFile::fs_ReadFile(_File);

				CStr Hash = NMib::NDataProcessing::fg_GetHashedUuidString(_File, NMib::NDataProcessing::CUniversallyUniqueIdentifier("{72048B5E-1F9C-4385-AF16-997FDC21F215}"));
				CStr UniqueName = fg_Format("{}-{}", CFile::fs_GetFile(_File), Hash);
				if (OldFileContents != _FileData)
					NSys::fg_Debug_DiffStrings(CFile::fs_ReadStringFromVector(OldFileContents), CFile::fs_ReadStringFromVector(_FileData), UniqueName, UniqueName);
			}
#			endif
		if (CFile::fs_CopyFileDiff(_FileData, _File, NTime::CTime::fs_NowUTC(), _AddAttribs))
		{
			f_SetFileChanged(_File);
			return true;
		}
		return false;
	}
	
	void CBuildSystem::f_SetFileChanged(CStr const& _File) const
	{
		//DConOut("Changed: {}\n", _File);
		mp_FileChanged = true;
	}

	TCMap<CPropertyKey, CStr> CBuildSystem::f_GetExternalValues(CEntity const &_Entity) const
	{
		TCMap<CPropertyKey, CStr> Ret;
		
		DLockReadLocked(_Entity.m_Lock);

		for (auto iEval = _Entity.m_EvaluatedProperties.f_GetIterator(); iEval; ++iEval)
		{
			if (iEval->m_Type == EEvaluatedPropertyType_External)
				Ret[iEval.f_GetKey()] = iEval->m_Value;
		}
		return Ret;
	}

	CStr CBuildSystem::f_GetExternalProperty(CEntity &_Entity, CPropertyKey const &_Key) const
	{
		DMibLockRead(_Entity.m_Lock);
		auto pEvaluated = _Entity.m_EvaluatedProperties.f_FindEqual(_Key);
		if (pEvaluated)
			return pEvaluated->m_Value;
		return CStr();
	}
	
	void CBuildSystem::f_AddExternalProperty(CEntity &_Entity, CPropertyKey const &_Key, CStr const &_Value) const
	{
		DMibLock(_Entity.m_Lock);
		auto &Evaluated = _Entity.m_EvaluatedProperties[_Key];
		Evaluated.m_Value = _Value;
		Evaluated.m_Type = EEvaluatedPropertyType_External;
		Evaluated.m_pProperty = &mp_ExternalProperty;
	}

	void CBuildSystem::f_InitEntityForEvaluationNoEnv(CEntity &_Entity, TCMap<CPropertyKey, CStr> const &_InitialValues) const
	{
		DMibLock(_Entity.m_Lock);
		for (auto iValue = _InitialValues.f_GetIterator(); iValue; ++iValue)
		{
			auto &Evaluated = _Entity.m_EvaluatedProperties[iValue.f_GetKey()];
			Evaluated.m_Value = *iValue;
			Evaluated.m_Type = EEvaluatedPropertyType_External;
			Evaluated.m_pProperty = &mp_ExternalProperty;
		}
	}

	void CBuildSystem::f_InitEntityForEvaluation(CEntity &_Entity, TCMap<CPropertyKey, CStr> const &_InitialValues) const
	{
		TCMap<CPropertyKey, CStr> Values = _InitialValues;

		TCMap<CStr, CStr> Environment = NSys::fg_Process_GetEnvironmentVariables();

		for (auto iEnv = Environment.f_GetIterator(); iEnv; ++iEnv)
		{
			CPropertyKey Key;
			Key.m_Name = iEnv.f_GetKey();
			auto Mapped = Values(Key);
			if (Mapped.f_WasCreated())
				Mapped.f_GetResult() = *iEnv;
		}

		f_InitEntityForEvaluationNoEnv(_Entity, Values);
	}

	TCVector<TCVector<CConfigurationTuple>> CBuildSystem::f_EvaluateConfigurationTuples(TCMap<CPropertyKey, CStr> const &_InitialValues) const
	{
		return fp_EvaluateConfigurationTuples(_InitialValues);
	}
	
}
