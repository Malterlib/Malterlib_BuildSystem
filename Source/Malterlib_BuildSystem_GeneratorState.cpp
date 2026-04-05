// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_GeneratorState.h"

namespace NMib::NBuildSystem
{
	CGeneratorArchiveState::CProcessedFile::CProcessedFile() = default;
	CGeneratorArchiveState::CGeneratorArchiveState() = default;

	bool CGeneratorArchiveState::CProcessedFile::f_FileChanged(TCAtomic<bool> &o_bChanged, CStr const &_OutputDirectory, CBuildSystem const &_BuildSystem)
	{
		CStr FileName = CFile::fs_GetExpandedPath(f_GetFileName(), _OutputDirectory);

		if (m_Flags & EGeneratedFileFlag_NoDateCheck)
		{
			try
			{
				if (!CFile::fs_FileExists(FileName))
				{
					if (!o_bChanged.f_Exchange(true))
						_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file is missing: {}{\n}"_f << FileName);
					return true;
				}
			}
			catch (...)
			{
				if (!o_bChanged.f_Exchange(true))
					_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file is missing: {}{\n}"_f << FileName);
				return true;
			}
		}
		else if (m_Flags & EGeneratedFileFlag_ByDigest)
		{
			try
			{
				if (!m_pDigest)
				{
					if (!o_bChanged.f_Exchange(true))
						_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because digest was missing: {}{\n}"_f << FileName);
					return true;
				}
				else
				{
					auto pDigest = _BuildSystem.f_ReadFileDigest(FileName);
					if (*pDigest != *m_pDigest)
					{
						if (!o_bChanged.f_Exchange(true))
						{
							_BuildSystem.f_OutputConsole
								(
									"Dependency check: Regenerating build system because file digest changed ({} != {}): {}{\n}"_f
									<< *pDigest
									<< *m_pDigest
									<< FileName
								)
							;
						}
						return true;
					}
				}
			}
			catch (CException const &_Exception)
			{
				if (!o_bChanged.f_Exchange(true))
					_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file digest check failed: {}{\n}{}{\n}"_f << FileName << _Exception);
				return true;
			}
			catch (...)
			{
				if (!o_bChanged.f_Exchange(true))
					_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file is missing: {}{\n}"_f << FileName);
				return true;
			}
		}
		else
		{
#ifdef DPlatformFamily_Windows
			try
			{
				if (!m_WriteTime.f_IsValid())
				{
					if (CFile::fs_FileExists(FileName))
					{
						if (!o_bChanged.f_Exchange(true))
							_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file now exists: {}{\n}"_f << FileName);
						return true;
					}
				}
				else
				{
					CFile File;
					File.f_Open(FileName, EFileOpen_ReadAttribs | EFileOpen_ShareAll);
					NTime::CTime DiskTime = File.f_GetWriteTime();
					if (DiskTime != m_WriteTime)
					{
						if (!o_bChanged.f_Exchange(true))
						{
							_BuildSystem.f_OutputConsole
								(
									"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"_f
									<< NTime::fg_GetFullTimeStr(DiskTime)
									<< NTime::fg_GetFullTimeStr(m_WriteTime)
									<< FileName
								)
							;
						}
						return true;
					}
				}
			}
			catch (...)
			{
				try
				{
					CFile File;
					File.f_Open(FileName, EFileOpen_Directory | EFileOpen_ReadAttribs | EFileOpen_ShareAll);
					NTime::CTime DiskTime = File.f_GetWriteTime();
					if (DiskTime != m_WriteTime)
					{
						if (!o_bChanged.f_Exchange(true))
						{
							_BuildSystem.f_OutputConsole
								(
									"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"_f
									<< NTime::fg_GetFullTimeStr(DiskTime)
									<< NTime::fg_GetFullTimeStr(m_WriteTime)
									<< FileName
								)
							;
						}
						return true;
					}
				}
				catch (CException const &_Exception)
				{
					if (!o_bChanged.f_Exchange(true))
						_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file date check failed: {}{\n}{}{\n}"_f << FileName << _Exception);
					return true;
				}
				catch (...)
				{
					if (!o_bChanged.f_Exchange(true))
						_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file is missing: {}{\n}"_f << FileName);
					return true;
				}
			}
#else
			try
			{
				if (!m_WriteTime.f_IsValid())
				{
					if (CFile::fs_FileExists(FileName))
					{
						if (!o_bChanged.f_Exchange(true))
							_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file now exists: {}{\n}"_f << FileName);
						return true;
					}
				}
				else
				{
					NTime::CTime DiskTime = CFile::fs_GetWriteTime(FileName);
					if (DiskTime != m_WriteTime)
					{
						if (!o_bChanged.f_Exchange(true))
						{
							_BuildSystem.f_OutputConsole
								(
									"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"_f
									<< NTime::fg_GetFullTimeStr(DiskTime)
									<< NTime::fg_GetFullTimeStr(m_WriteTime)
									<< FileName
								)
							;
						}
						return true;
					}
				}
			}
			catch (CException const &_Exception)
			{
				if (!o_bChanged.f_Exchange(true))
					_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file date check failed: {}{\n}{}{\n}"_f << FileName << _Exception);
				return true;
			}
			catch (...)
			{
				if (!o_bChanged.f_Exchange(true))
					_BuildSystem.f_OutputConsole("Dependency check: Regenerating build system because file is missing: {}{\n}"_f << FileName);
				return true;
			}
#endif
		}
		return false;
	}
}
