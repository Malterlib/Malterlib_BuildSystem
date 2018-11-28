// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_GeneratorState.h"

namespace NMib::NBuildSystem
{
	CGeneratorArchiveState::CProcessedFile::CProcessedFile() = default;
	CGeneratorArchiveState::CGeneratorArchiveState() = default;

	bool CGeneratorArchiveState::CProcessedFile::f_FileChanged(TCAtomic<bool> &o_bChanged, CStr const &_OutputDirectory)
	{
		CStr FileName = CFile::fs_GetExpandedPath(f_GetFileName(), _OutputDirectory);

		if (m_Flags & EGeneratedFileFlag_NoDateCheck)
		{
			try
			{
				if (!CFile::fs_FileExists(FileName))
				{
					if (!o_bChanged.f_Exchange(true))
						DConOut("Dependency check: Regenerating build system because file is missing: {}{\n}", FileName);
					return true;
				}
			}
			catch (...)
			{
				if (!o_bChanged.f_Exchange(true))
					DConOut("Dependency check: Regenerating build system because file is missing: {}{\n}", FileName);
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
							DConOut2("Dependency check: Regenerating build system because file now exists: {}{\n}", FileName);
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
							DConOut
								(
									"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"
									, NTime::fg_GetFullTimeStr(DiskTime) << NTime::fg_GetFullTimeStr(m_WriteTime) << FileName
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
							DConOut
								(
									"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"
									, NTime::fg_GetFullTimeStr(DiskTime) << NTime::fg_GetFullTimeStr(m_WriteTime) << FileName
								)
							;
						}
						return true;
					}
				}
				catch (CException const &_Exception)
				{
					if (!o_bChanged.f_Exchange(true))
						DConOut2("Dependency check: Regenerating build system because file date check failed: {}{\n}{}{\n}", FileName, _Exception);
					return true;
				}
				catch (...)
				{
					if (!o_bChanged.f_Exchange(true))
						DConOut("Dependency check: Regenerating build system because file is missing: {}{\n}", FileName);
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
							DConOut2("Dependency check: Regenerating build system because file now exists: {}{\n}", FileName);
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
							DConOut
								(
									"Dependency check: Regenerating build system because file changed ({} != {}): {}{\n}"
									, NTime::fg_GetFullTimeStr(DiskTime) << NTime::fg_GetFullTimeStr(m_WriteTime) << FileName
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
					DConOut2("Dependency check: Regenerating build system because file date check failed: {}{\n}{}{\n}", FileName, _Exception);
				return true;
			}
			catch (...)
			{
				if (!o_bChanged.f_Exchange(true))
					DConOut("Dependency check: Regenerating build system because file is missing: {}{\n}", FileName);
				return true;
			}
#endif
		}
		return false;
	}
}
