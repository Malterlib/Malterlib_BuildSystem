// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Dependency.h"
#include <Mib/File/VirtualFS>

namespace NMib::NBuildSystem
{
	NTime::CTime fg_GetDirectoryWriteTime(NStr::CStr const &_File)
	{
		NFile::CFile File;
		File.f_Open(_File, EFileOpen_ShareAll | EFileOpen_ReadAttribs | EFileOpen_Directory);
		return File.f_GetWriteTime();
	}

	NTime::CTime fg_GetTime(NStr::CStr const &_File)
	{
		CFileSystemInterface_Disk DiskSystem;
		CTime Time;
		if (CFile::fs_FileExists(_File, EFileAttrib_File))
			Time = DiskSystem.f_GetWriteTime(_File);
		else if (CFile::fs_FileExists(_File, EFileAttrib_Directory))
			Time = fg_GetDirectoryWriteTime(_File);

		return Time;
	}


	void CMalterlibDependencyTracker::f_AddInputFile(CStr const &_File)
	{
		auto & Input = mp_Inputs.f_Insert();
		Input.m_Path = _File;
		Input.m_Time = fg_GetTime(_File);
	}

	void CMalterlibDependencyTracker::f_AddOutputFile(CStr const &_File)
	{
		mp_Outputs.f_Insert(_File);
	}

	void CMalterlibDependencyTracker::f_AddFind
		(
			CStr const &_SearchPattern
			, bool _bRecurse
			, bool _bFollowLinks
			, EFileAttrib _Attributes
			, TCVector<CStr> const &_Results
			, TCVector<CStr> const &_Excluded
		)
	{
		auto & Find = mp_Finds.f_Insert();

		Find.m_Pattern = CFile::fs_GetFile(_SearchPattern);
		Find.m_Path = CFile::fs_GetPath(_SearchPattern);

		Find.m_Time = fg_GetTime(Find.m_Path);
		Find.m_Results = _Results;
		Find.m_Attributes = _Attributes;
		Find.m_bRecurse = _bRecurse;
		Find.m_Excluded = _Excluded;
		Find.m_bFollowLinks = _bFollowLinks;
	}

	void CMalterlibDependencyTracker::f_WriteDependencyFile(CStr const &_File)
	{
		CStr Contents;
		for (auto & Output : mp_Outputs)
			Contents += CStr::CFormat("Output {}\n") << Output;

		for (auto & File : mp_Inputs)
		{
			Contents += CStr::CFormat("File {nfh,sj16,sf0} {nfh,sj16,sf0}") << File.m_Time.f_GetSeconds() << File.m_Time.f_GetFractionInt();
			fg_AddStrSepEscaped(Contents, File.m_Path, ' ', " \"\\");
			Contents+= "\n";

		}

		for (auto & Find : mp_Finds)
		{
			Contents += CStr::CFormat("Directory {} {nfh,sj8,sf0} {} {nfh,sj16,sf0} {nfh,sj16,sf0}") << Find.m_bRecurse << Find.m_Attributes << Find.m_bFollowLinks << Find.m_Time.f_GetSeconds() << Find.m_Time.f_GetFractionInt();
			fg_AddStrSepEscaped(Contents, Find.m_Path, ' ', " \"\\");
			fg_AddStrSepEscaped(Contents, CFile::fs_GetFile(Find.m_Pattern), ' ', " \"\\");
			Contents += "\n";

			for (CStr const& File : Find.m_Excluded)
			{
				fg_AddStrSepEscaped(Contents, File, '-', " \"\\");
				Contents += "\n";
			}

			for (CStr const& File : Find.m_Results)
			{
				fg_AddStrSepEscaped(Contents, File, '\t', " \"\\");
				Contents += "\n";
			}
		}

		NFile::CFile::fs_CreateDirectory(CFile::fs_GetPath(_File));
		NFile::CFile::fs_WriteStringToFile(CStr(_File), Contents, true);
	}
}
