// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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

	CMalterlibDependencyTracker::CMalterlibDependencyTracker(bool _bUseHash)
		: mp_bUseHash(_bUseHash)
	{
	}

	void CMalterlibDependencyTracker::f_AddInputFile(CStr const &_File)
	{
		auto & Input = mp_Inputs.f_Insert();
		Input.m_Path = _File;
		Input.m_Time = fg_GetTime(_File);
		if (mp_bUseHash)
			Input.m_Digest = NFile::CFile::fs_GetFileChecksum(_File);
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

	namespace
	{
		void fg_EscapeGccDepPath(CStr::CAppender &_Appender, CStr const &_String)
		{
			auto *pParse = _String.f_GetStr();
			while (*pParse)
			{
				auto Char = *pParse;
				if (Char == ' ' || Char == '\t' || Char == '#' || Char == '\\')
				{
					_Appender += '\\';
					_Appender += Char;
				}
				else if (Char == '$')
				{
					_Appender += "$$";
				}
				else
				{
					_Appender += Char;
				}
				++pParse;
			}
		}
	}

	void CMalterlibDependencyTracker::f_WriteDependencyFile(CStr const &_File, CStr const &_DepFile)
	{
		bool bDepfileMode = !_DepFile.f_IsEmpty();

		// Write depfile if requested (GCC format: output: input1 input2 ...)
		if (bDepfileMode)
		{
			CStr DepContents;
			{
				CStr::CAppender Appender(DepContents);

				// When using hash mode, skip depfile (MalterlibDependency handles it)
				if (!mp_bUseHash)
				{
					if (mp_Outputs.f_IsEmpty())
						DError("Cannot write depfile: no outputs specified");

					// All outputs as targets: output1 output2: input1 input2
					for (aint i = 0; i < mp_Outputs.f_GetLen(); ++i)
					{
						if (i > 0)
							Appender += ' ';
						fg_EscapeGccDepPath(Appender, mp_Outputs[i]);
					}
					Appender += ':';

					for (auto &Input : mp_Inputs)
					{
						Appender += ' ';
						fg_EscapeGccDepPath(Appender, Input.m_Path);
					}
					Appender += '\n';
				}
			}

			NFile::CFile::fs_CreateDirectory(CFile::fs_GetPath(_DepFile));
			NFile::CFile::fs_WriteStringToFile(CStr(_DepFile), DepContents, true);
		}

		// Write MalterlibDependency file
		// Always write to avoid stale data from previous runs
		CStr Contents;
		{
			CStr::CAppender Appender(Contents);

			// In depfile mode without hash: only write Outputs + Finds (ninja handles inputs via depfile)
			// In depfile mode with hash: write Outputs + inputs with hashes + Finds
			// In legacy mode: write everything
			bool bHasFinds = !mp_Finds.f_IsEmpty();
			bool bWriteInputs = !bDepfileMode || mp_bUseHash;
			bool bWriteOutputs = bWriteInputs || bHasFinds;

			if (bWriteOutputs)
			{
				for (auto &Output : mp_Outputs)
				{
					Appender += "Output ";
					Appender += Output;
					Appender += '\n';
				}
			}

			if (bWriteInputs)
			{
				for (auto &File : mp_Inputs)
				{
					if (mp_bUseHash)
					{
						Appender.f_Commit().m_String += "FileDigest {nfh,sj16,sf0} {nfh,sj16,sf0} {}"_f
							<< File.m_Time.f_GetSeconds()
							<< File.m_Time.f_GetFractionInt()
							<< File.m_Digest.f_GetString()
						;
					}
					else
					{
						Appender.f_Commit().m_String += "File {nfh,sj16,sf0} {nfh,sj16,sf0}"_f
							<< File.m_Time.f_GetSeconds()
							<< File.m_Time.f_GetFractionInt()
						;
					}

					fg_AddStrSepEscaped(Appender, File.m_Path, ' ', " \"\\");
					Appender += '\n';
				}
			}

			// Write Finds for directory monitoring
			for (auto &Find : mp_Finds)
			{
				Appender.f_Commit().m_String += "Directory {} {nfh,sj8,sf0} {} {nfh,sj16,sf0} {nfh,sj16,sf0}"_f
					<< Find.m_bRecurse
					<< Find.m_Attributes
					<< Find.m_bFollowLinks
					<< Find.m_Time.f_GetSeconds()
					<< Find.m_Time.f_GetFractionInt()
				;

				fg_AddStrSepEscaped(Appender, Find.m_Path, ' ', " \"\\");
				fg_AddStrSepEscaped(Appender, CFile::fs_GetFile(Find.m_Pattern), ' ', " \"\\");

				Appender += '\n';

				for (CStr const &File : Find.m_Excluded)
				{
					fg_AddStrSepEscaped(Appender, File, '-', " \"\\");
					Appender += '\n';
				}

				for (CStr const &File : Find.m_Results)
				{
					fg_AddStrSepEscaped(Appender, File, '\t', " \"\\");
					Appender += '\n';
				}	
			}
		}

		NFile::CFile::fs_CreateDirectory(CFile::fs_GetPath(_File));
		NFile::CFile::fs_WriteStringToFile(CStr(_File), Contents, true);
	}
}
