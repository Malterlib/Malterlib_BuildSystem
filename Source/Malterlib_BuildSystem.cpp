// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

#include "Malterlib_BuildSystem_GeneratorState.h"

#include <Mib/Perforce/Wrapper>

namespace NMib::NBuildSystem
{
	void fg_Malterlib_BuildSystem_MakeActive_VisualStudio();
	void fg_Malterlib_BuildSystem_MakeActive_Xcode();

	CBuildSystem::CBuildSystem
		(
			NCommandLine::EAnsiEncodingFlag _AnsiFlags
			, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			, NStorage::TCSharedPointer<TCAtomic<bool>> const &_pCancelled
		)
		: mp_NowUTC(NTime::CTime::fs_NowUTC())
		, mp_AnsiFlags(_AnsiFlags)
		, mp_pCancelled(_pCancelled)
		, mp_fOutputConsole(_fOutputConsole)
	{
		fg_CacheConstantStrings(mp_StringCache);
		fg_CacheConstantKeys(mp_StringCache);

		fp_RegisterBuiltinFunctions();
		fp_RegisterBuiltinVariables();

		mp_Now = mp_NowUTC.f_ToLocal();
		fg_Malterlib_BuildSystem_MakeActive_VisualStudio();
		fg_Malterlib_BuildSystem_MakeActive_Xcode();
	}

	CFilePosition const &CBuildSystemPropertyInfo::f_FallbackPosition() const
	{
		if (m_pFallbackPosition)
			return *m_pFallbackPosition;

		static constinit CFilePosition s_DummyPosition;

		return s_DummyPosition;
	}

	CBuildSystemUniquePositions CBuildSystemPropertyInfo::f_GetPositions() const
	{
		CBuildSystemUniquePositions Positions;

		if (m_pPositions && !m_pPositions->f_IsEmpty())
			Positions = *m_pPositions;
		else if (m_pProperty)
			Positions.f_AddPosition(m_pProperty->m_Position, "Property");

		return Positions;
	}

	CProperty const &CBuildSystem::f_ExternalProperty(EPropertyType _Type) const
	{
		return mp_ExternalProperty[_Type];
	}

	void CBuildSystem::f_OutputConsole(CStr const &_Output, bool _bError) const
	{
		if (mp_fOutputConsole)
			mp_fOutputConsole(_Output, _bError);
	}

	NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &CBuildSystem::f_OutputConsoleFunctor() const
	{
		return mp_fOutputConsole;
	}

	EAnsiEncodingFlag CBuildSystem::f_AnsiFlags() const
	{
		return mp_AnsiFlags;
	}

	TCMap<CStr, TCSharedPointer<CHashDigest_SHA256>> CBuildSystem::f_GetSourceFiles() const
	{
		DMibLockRead(mp_SourceFilesLock);
		return mp_SourceFiles;
	}

	TCSharedPointer<CHashDigest_SHA256> CBuildSystem::f_ReadFileDigest(NStr::CStr const &_File) const
	{
		CCachedFile *pCachedFile;
		{
			DMibLock(mp_CachedFilesLock);
			pCachedFile = &mp_CachedFiles[_File];
		}

		DMibLock(pCachedFile->m_Lock);

		if (!pCachedFile->m_pDigest)
		{
			TCBinaryStreamFile<> Stream;
			Stream.f_Open(_File, EFileOpen_Read | EFileOpen_ShareAll);
			pCachedFile->m_pDigest = fg_Construct(CHash_SHA256::fs_DigestFromStream(Stream));
		}

		return pCachedFile->m_pDigest;
	}

	auto CBuildSystem::f_ReadFileWithDigest(NStr::CStr const &_File) const -> CFileWithDigest
	{
		CCachedFile *pCachedFile;
		{
			DMibLock(mp_CachedFilesLock);
			pCachedFile = &mp_CachedFiles[_File];
		}

		DMibLock(pCachedFile->m_Lock);
		if (!pCachedFile->m_bRead)
		{
			auto FileContents = CFile::fs_ReadFile(_File);
			pCachedFile->m_Contents = CFile::fs_ReadStringFromVector(FileContents, true);

			if (!pCachedFile->m_pDigest)
				pCachedFile->m_pDigest = fg_Construct(CHash_SHA256::fs_DigestFromData(FileContents));

			pCachedFile->m_bRead = true;
		}
		else if (!pCachedFile->m_pDigest)
		{
			TCBinaryStreamFile<> Stream;
			Stream.f_Open(_File, EFileOpen_Read | EFileOpen_ShareAll);
			pCachedFile->m_pDigest = fg_Construct(CHash_SHA256::fs_DigestFromStream(Stream));
		}

		return CFileWithDigest
			{
				.m_Contents = pCachedFile->m_Contents
				, .m_pDigest = pCachedFile->m_pDigest
			}
		;
	}

	NStr::CStr CBuildSystem::f_ReadFile(NStr::CStr const &_File) const
	{
		CCachedFile *pCachedFile;
		{
			DMibLock(mp_CachedFilesLock);
			pCachedFile = &mp_CachedFiles[_File];
		}

		DMibLock(pCachedFile->m_Lock);
		if (!pCachedFile->m_bRead)
		{
			pCachedFile->m_Contents = CFile::fs_ReadStringFromFile(_File, true);
			pCachedFile->m_bRead = true;
		}

		return pCachedFile->m_Contents;
	}

	void CBuildSystem::f_AddSourceFile(CStr const &_File, TCSharedPointer<CHashDigest_SHA256> &&_pDigest) const
	{
		DMibLock(mp_SourceFilesLock);
		mp_SourceFiles[_File] = fg_Move(_pDigest);
	}

	bool CBuildSystem::f_AddGeneratedFile(CStr const &_File, CStr const &_Data, CStr const &_Workspace, bool &_bWasCreated, EGeneratedFileFlag _Flags) const
	{
		DMibLock(mp_GeneratedFilesLock);
		auto &File = mp_GeneratedFiles[_File];

		if (!File.m_bAdded)
		{
			File.m_Contents = _Data;
			File.m_bAdded = true;
			_bWasCreated = true;
		}
		else if (File.m_Contents != _Data || (File.m_Flags & EGeneratedFileFlag_Symlink) != (_Flags & EGeneratedFileFlag_Symlink))
		{
			f_OutputConsole("Original contents:{\n}{\n}{}{\n}{\n}"_f << File.m_Contents);
			f_OutputConsole("Incompatible contents:{\n}{\n}{}{\n}{\n}"_f << _Data);
			return false;
		}
		else
			_bWasCreated = false;

		File.m_Workspaces[_Workspace];
		File.m_Flags |= _Flags;

		return true;
	}

	TCFuture<void> CBuildSystem::f_CheckCancelled() const
	{
		if (!mp_pCancelled || !mp_pCancelled->f_Load())
			co_return {};

		co_return DMibErrorInstance("Aborted");
	}

	void CBuildSystem::f_CheckCancelledException() const
	{
		if (mp_pCancelled && mp_pCancelled->f_Load())
			DMibError("Aborted");
	}

	TCSharedPointer<TCAtomic<bool>> CBuildSystem::f_GetCancelledPointer() const
	{
		return mp_pCancelled;
	}

	CStr const &CBuildSystem::f_GetBaseDir() const
	{
		return mp_BaseDir.f_String();
	}

	void CBuildSystem::f_SetGeneratorInterface(ICGeneratorInterface *_pInterface) const
	{
		mp_GeneratorInterface = _pInterface;
	}

	bool CBuildSystem::f_WriteFile(CByteVector const &_FileData, CStr const &_File, EFileAttrib _AddAttribs) const
	{
#if 0
		CStr FileExtension = CFile::fs_GetExtension(_File);
		CStr FileName = CFile::fs_GetFile(_File);

		if (FileName != "VersionInfo.cpp" && FileExtension != "plist" && CFile::fs_FileExists(_File))
		{
			CByteVector OldFileContents = CFile::fs_ReadFile(_File);

			CStr Hash = NMib::NCryptography::fg_GetHashedUuidString(_File, NMib::NCryptography::CUniversallyUniqueIdentifier("{72048B5E-1F9C-4385-AF16-997FDC21F215}"));
			CStr UniqueName = fg_Format("{}-{}", CFile::fs_GetFile(_File), Hash);
			if (OldFileContents != _FileData)
				NSys::fg_Debug_DiffStrings(CFile::fs_ReadStringFromVector(OldFileContents), CFile::fs_ReadStringFromVector(_FileData), UniqueName, UniqueName);
		}
#endif
		if
		(
			CFile::fs_CopyFileDiff
			(
				_FileData
				, _File
				, NTime::CTime::fs_NowUTC()
				, _AddAttribs
				, [&](CFile::EDiffCopyChange _Change, CStr const &_Source, CStr const &_Destination, CStr const &_Link)
				{
					if (_Change == CFile::EDiffCopyChange_FileChanged)
					{
						EFileAttrib Attributes = CFile::fs_GetAttributes(_Destination);

						if ((Attributes & EFileAttrib_ReadOnly) || (!(Attributes & EFileAttrib_UserWrite) && (mp_SupportedAttributes & EFileAttrib_UserWrite)))
						{
							bool bSuccess = false;
							// Perforce checkout
							try
							{
								CPerforceClientThrow Client;
								if (CPerforceClientThrow::fs_GetFromP4Config(_Destination, Client))
								{
									Client.f_OpenForEdit(_Destination);
									f_OutputConsole("Opened file for edit in Perforce: {}{\n}"_f << _Destination);
									bSuccess = true;
								}
							}
							catch (NException::CException const &_Error)
							{
								CStr Error = _Error.f_GetErrorStr();
								f_OutputConsole("Failed to checkout via Perforce:{\n}{}{\n}"_f << Error, true);
							}

							if (!bSuccess)
							{
								f_OutputConsole("Removed file write protection: {}{\n}"_f << _Destination);
								CFile::fs_SetAttributes(_Destination, (Attributes & ~EFileAttrib_ReadOnly)  | (mp_SupportedAttributes & EFileAttrib_UserWrite) | mp_ValidAttributes);
							}
						}
					}
					return CFile::EDiffCopyChangeAction_Perform;
				}
			)
		)
		{
			f_SetFileChanged(_File);
			return true;
		}
		return false;
	}

	void CBuildSystem::f_SetFileChanged(CStr const &_File) const
	{
		//m_BuildSystem.f_OutputConsole("Changed: {}\n"_f << _File);
		mp_FileChanged = true;
	}

	TCMap<CPropertyKey, NEncoding::CEJsonSorted> CBuildSystem::f_GetExternalValues(CEntity const &_Entity) const
	{
		TCMap<CPropertyKey, NEncoding::CEJsonSorted> Ret;

		for (auto &Property : _Entity.m_EvaluatedProperties.m_Properties)
		{
			if (Property.f_IsExternal())
				Ret[_Entity.m_EvaluatedProperties.m_Properties.fs_GetKey(Property)] = Property.m_Value;
		}
		return Ret;
	}

	NEncoding::CEJsonSorted CBuildSystem::f_GetExternalProperty(CEntity &_Entity, CPropertyKeyReference const &_Key) const
	{
		auto pEvaluated = _Entity.m_EvaluatedProperties.m_Properties.f_FindEqual(_Key);
		if (pEvaluated)
			return pEvaluated->m_Value;
		return {};
	}

	bool CBuildSystem::f_AddExternalProperty(CEntity &_Entity, CPropertyKeyReference const &_Key, NEncoding::CEJsonSorted &&_Value) const
	{
		auto Mapped = _Entity.m_EvaluatedProperties.m_Properties(_Key);
		auto &Evaluated = *Mapped;

		bool bChanged = Mapped.f_WasCreated() || Evaluated.m_Value != _Value;

		if (bChanged)
			Evaluated.m_Value = fg_Move(_Value);
		Evaluated.m_Type = EEvaluatedPropertyType_External;
		Evaluated.m_pProperty = &mp_ExternalProperty[_Key.f_GetType()];

		return bChanged;
	}

	void CBuildSystem::f_InitEntityForEvaluationNoEnv(CEntity &_Entity, TCMap<CPropertyKey, CEJsonSorted> const &_InitialValues, EEvaluatedPropertyType _Type) const
	{
		for (auto iValue = _InitialValues.f_GetIterator(); iValue; ++iValue)
		{
			if
				(
					_Type == EEvaluatedPropertyType_ExternalEnvironment
					&& iValue->f_IsString()
					&& iValue->f_String() == gc_ConstString_undefined.m_String
				)
			{
				continue;
			}

			auto &Evaluated = _Entity.m_EvaluatedProperties.m_Properties[iValue.f_GetKey()];
			Evaluated.m_Value = *iValue;
			Evaluated.m_Type = _Type;
			Evaluated.m_pProperty = &mp_ExternalProperty[iValue.f_GetKey().f_GetType()];

			if
				(
					_Type == EEvaluatedPropertyType_ExternalEnvironment
					&& iValue->f_IsString()
					&& iValue->f_String() == gc_ConstString_3FFADB4E_9D9D_4CD7_8FA8_539C6ABF79BA.m_String
				)
			{
				Evaluated.m_Value = gc_ConstString_undefined;
			}

			if (f_EnablePositions())
			{
				if (!Evaluated.m_pPositions)
					Evaluated.m_pPositions = fg_Construct();

				if (_Type == EEvaluatedPropertyType_ExternalEnvironment)
					Evaluated.m_pPositions->f_AddPosition(DMibBuildSystemFilePosition, "Environment variable: {}"_f << iValue.f_GetKey())->f_AddValue(Evaluated.m_Value, f_EnableValues());
				else
					Evaluated.m_pPositions->f_AddPosition(DMibBuildSystemFilePosition, "Initial value: {}"_f << iValue.f_GetKey())->f_AddValue(Evaluated.m_Value, f_EnableValues());
			}
		}
	}

	void CBuildSystem::f_InitEntityForEvaluation(CEntity &_Entity, TCMap<CPropertyKey, CEJsonSorted> const &_InitialValues) const
	{
		TCMap<CPropertyKey, CEJsonSorted> EnvironmentValues;

		for (auto iEnv = mp_Environment.f_GetIterator(); iEnv; ++iEnv)
		{
			CPropertyKey Key(mp_StringCache, iEnv.f_GetKey());

			if (!_InitialValues.f_FindEqual(Key))
				EnvironmentValues(Key, *iEnv);
		}

		f_InitEntityForEvaluationNoEnv(_Entity, _InitialValues, EEvaluatedPropertyType_External);
		f_InitEntityForEvaluationNoEnv(_Entity, EnvironmentValues, EEvaluatedPropertyType_ExternalEnvironment);
	}

	TCVector<TCVector<CConfigurationTuple>> CBuildSystem::f_EvaluateConfigurationTuples(TCMap<CPropertyKey, CEJsonSorted> const &_InitialValues) const
	{
		return fp_EvaluateConfigurationTuples(_InitialValues);
	}

	NStr::CStr CBuildSystem::f_GetEnvironmentVariable(NStr::CStr const &_Name, NStr::CStr const &_Default, bool *o_pExists) const
	{
		auto *pVariable = mp_Environment.f_FindEqual(_Name);
		if (pVariable)
		{
			if (o_pExists)
				*o_pExists = true;
			return *pVariable;
		}
		if (o_pExists)
			*o_pExists = false;
		return _Default;
	}
}
