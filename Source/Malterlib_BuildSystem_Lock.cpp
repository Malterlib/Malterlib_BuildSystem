// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"

#include <Mib/Process/Platform>
#include <Mib/Process/ProcessLaunch>

namespace NMib::NBuildSystem
{
	CBuildSystemLock::CBuildSystemLock(CStr const &_LockFile)
		: m_LockFile(_LockFile)
		, m_Lock(_LockFile)
	{
	}

	// The owner file only informs the processes that find the lock taken: the lock itself decides who runs, and an owner
	// that dies leaves its file behind.
	CStr CBuildSystemLock::fs_OwnerFile(CStr const &_LockFile)
	{
		return _LockFile + ".owner";
	}

	CStr CBuildSystemLock::fs_ReadOwner(CStr const &_LockFile)
	{
		CStr Owner;
		try
		{
			Owner = CFile::fs_ReadStringFromFile(fs_OwnerFile(_LockFile), true).f_Trim();
		}
		catch (CExceptionFile const &)
		{
		}

		if (Owner.f_IsEmpty())
			return "owner unknown";

		return Owner;
	}

	void CBuildSystemLock::f_WriteOwner(CStr const &_Description) const
	{
		CStr Owner = "process {}, since {}, {}"_f
			<< NProcess::NPlatform::fg_Process_GetCurrentUID()
			<< CTime::fs_NowLocal()
			<< _Description
		;

		CByteVector FileData;
		CFile::fs_WriteStringToVector(FileData, Owner, false);
		CFile::fs_CopyFileDiff(FileData, fs_OwnerFile(m_LockFile), CTime::fs_NowUTC());
	}

	// Takes the lock of _LockFile for _What, such as "generation of the build system in '...'". With
	// EBuildSystemLockWait::mc_Refuse a lock that another process holds throws, naming its owner. Otherwise it waits,
	// reporting what it waits for, until the lock is free, the timeout has passed, or the command is cancelled.
	TCUnsafeFuture<TCUniquePointer<CBuildSystemLock>> CBuildSystem::f_Lock(CStr _LockFile, CStr _What, CBuildSystemLockSettings _Settings) const
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		constexpr fp64 c_PollInterval = 0.2;
		constexpr fp64 c_ReportInterval = 60.0;

		struct CAttempt
		{
			CLockFile::ELockResult m_Result = CLockFile::ELockResult_TimedOut;
			CStr m_Error;
			CStr m_Owner;
		};

		TCVector<CStr> Arguments{CFile::fs_GetProgramPath()};
		for (aint iParameter = 0; iParameter < fg_GetSys()->f_NumCommandLineParameters(); ++iParameter)
			Arguments.f_Insert(fg_GetSys()->f_CommandLineParameter(iParameter));

		CStr CommandLine = NProcess::CProcessLaunchParams::fs_GetParams(Arguments);

		TCUniquePointer<CBuildSystemLock> pLock = fg_Construct(_LockFile);

		CStopwatch Stopwatch(true);
		fp64 NextReport = 0.0;

		while (true)
		{
			CAttempt Attempt;
			{
				// A checkout for each attempt, so that waiting holds no thread
				auto BlockingActorCheckout = fg_BlockingActor();
				Attempt = co_await
					(
						g_Dispatch(BlockingActorCheckout) / [pLock = pLock.f_Get(), CommandLine]() -> CAttempt
						{
							CFile::fs_CreateDirectory(CFile::fs_GetPath(pLock->m_LockFile));

							CAttempt Attempt;
							Attempt.m_Result = pLock->m_Lock.f_Lock(0.0, &Attempt.m_Error);
							if (Attempt.m_Result == CLockFile::ELockResult_Locked)
								pLock->f_WriteOwner(CommandLine);
							else if (Attempt.m_Result == CLockFile::ELockResult_TimedOut)
								Attempt.m_Owner = CBuildSystemLock::fs_ReadOwner(pLock->m_LockFile);

							return Attempt;
						}
					)
				;
			}

			if (Attempt.m_Result == CLockFile::ELockResult_Locked)
				co_return fg_Move(pLock);

			// CLockFile reports a lock file that fails to open for other reasons, as on a full disk, as held too, so those wait
			// like contention
			if (Attempt.m_Result != CLockFile::ELockResult_TimedOut)
				co_return DMibErrorInstance("Failed to take the lock '{}' for {}: {}"_f << _LockFile << _What << Attempt.m_Error);

			if (_Settings.m_Wait == EBuildSystemLockWait::mc_Refuse)
			{
				co_return DMibErrorInstance
					(
						"Another {} is already running ({}). To wait for it to finish instead, pass --wait-for-lock"_f
						<< _What
						<< Attempt.m_Owner
					)
				;
			}

			fp64 Waited = Stopwatch.f_GetTime();

			if (_Settings.m_Timeout >= 0.0 && Waited >= _Settings.m_Timeout)
			{
				co_return DMibErrorInstance
					(
						"Gave up after waiting {fe0} seconds for another {} ({}) to finish"_f
						<< Waited
						<< _What
						<< Attempt.m_Owner
					)
				;
			}

			if (Waited >= NextReport)
			{
				if (NextReport == 0.0)
					f_OutputConsole("Waiting for another {} ({}) to finish{\n}"_f << _What << Attempt.m_Owner);
				else
					f_OutputConsole("Still waiting for another {} ({}) to finish, after {fe0} seconds{\n}"_f << _What << Attempt.m_Owner << Waited);

				NextReport = Waited + c_ReportInterval;
			}

			co_await f_CheckCancelled();
			co_await fg_Timeout(c_PollInterval);
		}
	}
}
