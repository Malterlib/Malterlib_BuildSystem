// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>

namespace NMib::NBuildSystem::NRepository
{
	namespace
	{
		TCFuture<void> fg_SendVerifyHookInput(TCActor<CProcessLaunchActor> _Process, CStr _Input, TCSharedPointer<TCAtomic<bool>> _pSent)
		{
			auto SetSent = g_OnScopeExit / [_pSent]
				{
					_pSent->f_Exchange(true);
				}
			;

			co_await _Process(&CProcessLaunchActor::f_SendStdIn, _Input);
			co_await _Process(&CProcessLaunchActor::f_CloseStdIn);

			co_return {};
		}

		// Runs the hook with the batch on stdin. It gets this process's environment, and has no timeout: a hook can wait for as
		// long as it needs, such as for a review of the batch.
		TCFuture<CProcessLaunchActor::CSimpleLaunchResult> fg_LaunchVerifyHook
			(
				CStr _Command
				, CStr _WorkingDirectory
				, CStr _Input
				, TCSharedPointer<TCAtomic<bool>> _pCancelled
			)
		{
			auto Capture = co_await (g_CaptureExceptions % "Failed to run the verify hook");

			CStr Executable;
#ifdef DPlatformFamily_Windows
			auto Params = CProcessLaunchParams::fs_ParseCommandLineWindows(_Command, Executable);
#else
			auto Params = CProcessLaunchParams::fs_ParseCommandLineUnix(_Command, Executable);
#endif
			if (Executable.f_IsEmpty())
				co_return DMibErrorInstance("The verify hook command is empty");

			struct CState
			{
				CProcessLaunchActor::CSimpleLaunchResult m_Result;
				TCPromise<void> m_Launched;
				TCPromise<void> m_Exited;
				bool m_bExited = false;
			};

			TCSharedPointer<CState> pState = fg_Construct();
			CProcessLaunchActor::CSimpleLaunch Launch{Executable, Params, _WorkingDirectory, CProcessLaunchActor::ESimpleLaunchFlag_None};
			Launch.m_bWholeLineOutput = false;
			// So that cancelling finds the children a hook leaves behind, which hold its output open after it has exited
			Launch.m_Params.m_bCreateNewProcessGroup = true;
			Launch.m_Params.m_fOnOutput = [pState](EProcessLaunchOutputType _Type, CStr const &_Output)
				{
					auto &Output = pState->m_Result.m_Output.f_Insert();
					Output.m_Type = _Type;
					Output.m_Output = _Output;
				}
			;
			Launch.m_Params.m_fOnStateChange = [pState](CProcessLaunchStateChangeVariant const &_State, fp64 _TimeSinceStart)
				{
					switch (_State.f_GetTypeID())
					{
					case EProcessLaunchState_Launched:
						pState->m_Launched.f_SetResult();

						break;
					case EProcessLaunchState_LaunchFailed:
						pState->m_Launched.f_SetException(DMibErrorInstance(_State.f_Get<EProcessLaunchState_LaunchFailed>()));

						break;
					case EProcessLaunchState_Exited:
						pState->m_Result.m_ExitCode = _State.f_Get<EProcessLaunchState_Exited>();
						pState->m_bExited = true;
						pState->m_Exited.f_SetResult();

						break;
					}
				}
			;

			TCActor<CProcessLaunchActor> Process = fg_Construct();
			auto Destroy = co_await fg_AsyncDestroy(Process);
			auto Launched = pState->m_Launched.f_Future();
			auto Exited = pState->m_Exited.f_Future();
			auto Subscription = co_await Process(&CProcessLaunchActor::f_Launch, fg_Move(Launch), fg_CurrentActor());
			co_await fg_Move(Launched);

			// Sent while cancellation is watched, since a hook that doesn't read a batch larger than the pipe blocks the send. A child
			// of the hook that keeps its stdin can block it after the hook has exited.
			TCSharedPointer<TCAtomic<bool>> pSent = fg_Construct(false);
			auto SendInput = fg_SendVerifyHookInput(Process, _Input, pSent);

			// No timeout, but the command's cancellation stops the hook
			while (!pState->m_bExited || !pSent->f_Load())
			{
				if (_pCancelled && _pCancelled->f_Load())
				{
					auto Terminated = co_await Process(&CProcessLaunchActor::f_TerminateProcessTree).f_Wrap();
					co_await fg_Move(Exited);
					auto Sent = co_await fg_Move(SendInput).f_Wrap();
					if (!Terminated)
						co_return DMibErrorInstance("Cancelled, and failed to stop the verify hook: {}"_f << Terminated.f_GetExceptionStr());

					co_return DMibErrorInstance
						(
							"Cancelled while the verify hook ran{}"_f << (Sent ? CStr() : CStr(", sending its batch failed: " + Sent.f_GetExceptionStr()))
						)
					;
				}

				co_await fg_Timeout(0.2);
			}

			co_await fg_Move(Exited);

			// The exit code is the verdict: a hook can decide without reading the batch, such as /usr/bin/true, and the send then
			// fails with a broken pipe depending on timing
			auto Sent = co_await fg_Move(SendInput).f_Wrap();
			if (!Sent && pState->m_Result.m_ExitCode != 0)
			{
				auto &Output = pState->m_Result.m_Output.f_Insert();
				Output.m_Type = EProcessLaunchOutputType_StdErr;
				Output.m_Output = "\nSending the batch failed: {}"_f << Sent.f_GetExceptionStr();
			}

			co_return pState->m_Result;
		}

		// The last repository of the level to arrive at a phase runs the hook with what the phase collected, and every repository
		// that waits for the phase gets its verdict
		TCFuture<void> fg_RunVerifyHookPhase(TCSharedPointer<CVerifyHookLevel> _pLevel, umint _iPhase)
		{
			auto &Level = *_pLevel;

			TCVector<CVerifyHookEntry> Entries;
			TCVector<TCPromise<void>> Waiting;
			CExceptionPointer pRefusal;
			{
				DMibLock(Level.m_Lock);
				auto &Phase = Level.m_Phases[_iPhase];
				Entries = fg_Move(Phase.m_Entries);
				Waiting = fg_Move(Phase.m_Waiting);
				pRefusal = Level.m_pRefusal;
			}

			// A refused batch stops the level: the repositories that wait for a later phase have nothing allowed to check out
			if (!pRefusal && !Entries.f_IsEmpty())
			{
				uint32 Batch = Level.m_pBatch->f_FetchAdd(1) + 1;

				CJsonOrdered Json(EJsonType_Object);
				Json["Batch"] = Batch;
				auto &Repositories = Json["Repositories"];
				Repositories = CJsonOrdered(EJsonType_Array);
				for (auto &Entry : Entries)
					Repositories.f_Array().f_Insert(Entry.f_ToJson());

				auto Result = co_await fg_LaunchVerifyHook(Level.m_Command, Level.m_BaseDir, Json.f_ToString() + "\n", Level.m_pCancelled).f_Wrap();
				if (!Result)
					pRefusal = Result.f_GetException();
				else if (Result->m_ExitCode != 0)
				{
					CStr Error = Result->f_GetStdErr().f_Trim();
					if (Error.f_IsEmpty())
						Error = "Exit code {}"_f << Result->m_ExitCode;

					pRefusal = DMibErrorInstance("Verify hook refused batch {}:\n{}"_f << Batch << Error);
				}

				if (pRefusal)
				{
					DMibLock(Level.m_Lock);
					Level.m_pRefusal = pRefusal;
				}
			}

			for (auto &Promise : Waiting)
			{
				if (pRefusal)
					Promise.f_SetException(pRefusal);
				else
					Promise.f_SetResult();
			}

			co_return {};
		}

		// _pWaiter, when given, gets the verdict on the phase's batch
		void fg_ArriveAtVerifyHookPhase(TCSharedPointer<CVerifyHookLevel> const &_pLevel, umint _iPhase, TCVector<CVerifyHookEntry> &&_Entries, TCPromise<void> const *_pWaiter)
		{
			auto &Level = *_pLevel;

			bool bLast = false;
			{
				DMibLock(Level.m_Lock);
				auto &Phase = Level.m_Phases[_iPhase];
				Phase.m_Entries.f_Insert(fg_Move(_Entries));
				if (_pWaiter)
					Phase.m_Waiting.f_Insert(*_pWaiter);
				bLast = ++Phase.m_nArrived == Level.m_nRepositories;
			}

			// The phase reports its verdict to the repositories that wait for it, and catches every error to do so
			if (bLast)
				fg_RunVerifyHookPhase(_pLevel, _iPhase).f_DiscardResult();
		}
	}

	CJsonOrdered CVerifyHookEntry::f_ToJson() const
	{
		CJsonOrdered Json(EJsonType_Object);
		Json["Path"] = m_Path;
		Json["Url"] = m_Url;
		Json["From"] = m_From;
		Json["To"] = m_To;
		Json["New"] = m_bNew;
		Json["Submodule"] = m_bSubmodule;
		Json["Parent"] = m_Parent;
		Json["ConfigFile"] = m_ConfigFile;
		Json["ConfigKey"] = m_ConfigKey;

		return Json;
	}

	CVerifyHookLevel::CVerifyHookLevel
		(
			CStr const &_Command
			, CStr const &_BaseDir
			, TCSharedPointer<TCAtomic<uint32>> const &_pBatch
			, TCSharedPointer<TCAtomic<bool>> const &_pCancelled
			, umint _nRepositories
		)
		: m_Command(_Command)
		, m_BaseDir(_BaseDir)
		, m_pBatch(_pBatch)
		, m_pCancelled(_pCancelled)
		, m_nRepositories(_nRepositories)
	{
	}

	CVerifyHookRepository::CVerifyHookRepository(TCSharedPointer<CVerifyHookLevel> const &_pLevel)
		: m_pLevel(_pLevel)
	{
	}

	CVerifyHookRepository::~CVerifyHookRepository()
	{
		f_Leave();
	}

	// Arrives at the phases the repository hasn't reached, once it has nothing more to verify
	void CVerifyHookRepository::f_Leave()
	{
		if (!m_pLevel)
			return;

		for (; m_iNextPhase < CVerifyHookLevel::EPhase_Count; ++m_iNextPhase)
			fg_ArriveAtVerifyHookPhase(m_pLevel, m_iNextPhase, {}, nullptr);
	}

	// Arrives at the phases before _Phase without entries, and waits for the verdict on the batch of _Phase
	TCFuture<void> CVerifyHookRepository::f_Verify(CVerifyHookLevel::EPhase _Phase, TCVector<CVerifyHookEntry> &&_Entries)
	{
		DMibRequire(m_pLevel);

		TCPromise<void> Promise;
		auto Future = Promise.f_Future();

		if (m_iNextPhase > _Phase)
		{
			Promise.f_SetException(DMibErrorInstance("The verify hook batch of this phase has already run"));
			return Future;
		}

		for (; m_iNextPhase < _Phase; ++m_iNextPhase)
			fg_ArriveAtVerifyHookPhase(m_pLevel, m_iNextPhase, {}, nullptr);

		++m_iNextPhase;
		fg_ArriveAtVerifyHookPhase(m_pLevel, _Phase, fg_Move(_Entries), &Promise);

		return Future;
	}
}
