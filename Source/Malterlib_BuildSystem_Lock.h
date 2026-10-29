// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/File/File>

namespace NMib::NBuildSystem
{
	enum class EBuildSystemLockWait
	{
		mc_Refuse
		, mc_Wait
	};

	struct CBuildSystemLockSettings
	{
		EBuildSystemLockWait m_Wait = EBuildSystemLockWait::mc_Refuse;
		fp64 m_Timeout = -1.0; // Negative waits without a limit
	};

	struct CBuildSystemLock
	{
		CBuildSystemLock(NStr::CStr const &_LockFile);

		static NStr::CStr fs_OwnerFile(NStr::CStr const &_LockFile);
		static NStr::CStr fs_ReadOwner(NStr::CStr const &_LockFile);
		void f_WriteOwner(NStr::CStr const &_Description) const;

		NStr::CStr m_LockFile;
		NFile::CLockFile m_Lock;
	};
}
