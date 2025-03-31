// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"
#include <Mib/Concurrency/Actor/Timer>
#ifdef DPlatformFamily_Windows
#include <Mib/Core/PlatformSpecific/WindowsFilePath>
#endif

namespace NMib::NBuildSystem::NRepository
{
	CGitLaunches::CState::CState
		(
			CStr const &_BaseDir
			, CStr const &_ProgressDescription
			, EAnsiEncodingFlag _AnsiFlags
			, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			, NStorage::TCSharedPointer<TCAtomic<bool>> const &_pCancelled
		)
		: m_BaseDir(_BaseDir)
		, m_ProgressDescription(_ProgressDescription)
		, m_AnsiFlags(_AnsiFlags)
		, m_fOutputConsole(_fOutputConsole)
		, m_pCancelled(_pCancelled)
	{
	}

	CGitLaunches::CGitLaunches
		(
			CStr const &_BaseDir
			, CStr const &_ProgressDescription
			, EAnsiEncodingFlag _AnsiFlags
			, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			, NStorage::TCSharedPointer<TCAtomic<bool>> const &_pCancelled
		)
		: m_pState(fg_Construct(_BaseDir, _ProgressDescription, _AnsiFlags, _fOutputConsole, _pCancelled))
	{
		m_pOwner = fg_Construct(m_pState);
	}

	CGitLaunches::COwner::COwner(TCSharedPointer<CState> const &_pState)
		: m_pState(_pState)
	{
	}

	CGitLaunches::COwner::~COwner()
	{
		auto &State = *m_pState;
		if (State.m_CheckAbortTimer)
			fg_Exchange(State.m_CheckAbortTimer, nullptr)->f_Destroy().f_DiscardResult();
	}

	CGitLaunches::CGitLaunches(CGitLaunches const &_Other)
		: m_pState(_Other.m_pState)
	{
	}

	namespace
	{
		void fg_OutputRepoLine
			(
				EOutputType _OutputType
				, CStr const &_RepoName
				, mint _LongestRepoName
				, CStr const &_Line
				, EAnsiEncodingFlag _AnsiFlags
				, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			)
		{
			CStr RepoColor;

			CColors Colors(_AnsiFlags);

			switch (_OutputType)
			{
			case EOutputType_Normal: RepoColor = Colors.f_StatusNormal(); break;
			case EOutputType_Warning: RepoColor = Colors.f_StatusWarning(); break;
			case EOutputType_Error: RepoColor = Colors.f_StatusError(); break;
			default: RepoColor = "ERROR"; break;
			}

			CStr ReplacedRepo = _RepoName.f_Replace("/", "{}{}/{}"_f << Colors.f_Default() << Colors.f_Foreground256(250) << RepoColor ^ 1);
			if (_fOutputConsole)
			{
				_fOutputConsole
					(
						"{}{sl*,sf ,a-}{} {}|{}  {}\n"_f
						<< RepoColor
						<< ReplacedRepo
						<< _LongestRepoName + ReplacedRepo.f_GetLen() - _RepoName.f_GetLen()
						<< Colors.f_Default()
						<< Colors.f_Foreground256(242)
						<< Colors.f_Default()
						<< _Line
						, false
					)
				;
			}
		}

		void fg_OutputSectionLine(EAnsiEncodingFlag _AnsiFlags, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole)
		{
			CColors Colors(_AnsiFlags);

			if (_fOutputConsole)
				_fOutputConsole("{}--------------------------------------------------------------------------------{}\n"_f << Colors.f_Foreground256(242) << Colors.f_Default(), false);
		}
	}

	CGitLaunches::CState::~CState()
	{
		mint LongestRepo = 0;

		for (auto &RepoOutput : m_DeferredOutput)
		{
			auto &RepoName = m_DeferredOutput.fs_GetKey(RepoOutput);
			LongestRepo = fg_Max(LongestRepo, mint(RepoName.f_GetLen()));
		}

		bool bDidOutputSection = false;

		TCSet<CStr> AlreadyOutput;

		auto fOutputSection = [&](CStr const &_Name, TCVector<CDeferredOutput> const &_Output)
			{
				if (!AlreadyOutput(_Name).f_WasCreated())
					return;
				mint nLines = 0;
				bool bIsSection = false;
				for (auto &Output : _Output)
				{
					if (Output.m_Lines.f_GetLen() == 1 && Output.m_Lines[0] == "ForceSection")
						bIsSection = true;
					nLines += Output.m_Lines.f_GetLen();
				}

				if (nLines > 1 || bIsSection)
				{
					if (!bDidOutputSection)
					{
						fg_OutputSectionLine(m_AnsiFlags, m_fOutputConsole);
						bDidOutputSection = true;
					}
				}
				else
					bDidOutputSection = false;

				for (auto &Output : _Output)
				{
					for (auto Line : Output.m_Lines)
					{
						if (Line == "ForceSection")
							Line = "";
						fg_OutputRepoLine(Output.m_OutputType, _Name, LongestRepo, Line, m_AnsiFlags, m_fOutputConsole);
					}
				}

				if (bDidOutputSection)
					fg_OutputSectionLine(m_AnsiFlags, m_fOutputConsole);
			}
		;

		for (auto &Sections : m_OutputOrder)
		{
			for (auto &Section : Sections)
			{
				auto *pOutput = m_DeferredOutput.f_FindEqual(Section);
				if (!pOutput)
					continue;
				fOutputSection(Section, *pOutput);
			}
		}

		for (auto &RepoOutput : m_DeferredOutput)
			fOutputSection(m_DeferredOutput.fs_GetKey(RepoOutput), RepoOutput);

		fg_Move(m_LaunchSequencer).f_Destroy().f_DiscardResult();
	}

	void CGitLaunches::CState::f_OutputState() const
	{
		DMibLock(m_ConsoleOutputLock);

		CUStr ToOutput = CStr{"  {}: {}/{} repos done"_f << m_ProgressDescription << m_nDoneRepos.f_Load() << m_nRepos.f_Load()};
		if (m_nDoneRepos == m_nRepos.f_Load())
			ToOutput = CStr{"{sj*}"_f << "" << ToOutput.f_GetLen()}; // Clear previous output

		f_ConsoleOutput("{}\x1B[{}D"_f << ToOutput << ToOutput.f_GetLen());
	}

	void CGitLaunches::CState::f_ConsoleOutput(CStr const &_Output, bool _bError) const
	{
		if (m_fOutputConsole)
			m_fOutputConsole(_Output, _bError);
	}

	COnScopeExitShared CGitLaunches::f_RepoDoneScope() const
	{
		return g_OnScopeExitActor / [pState = m_pState]
			{
				auto &State = *pState;

				++State.m_nDoneRepos;
				State.f_OutputState();
			}
		;
	}

	void CGitLaunches::f_RepoDone(mint _nDone) const
	{
		auto &State = *m_pState;

		State.m_nDoneRepos += _nDone;
		State.f_OutputState();
	}

	void CGitLaunches::f_SetNumRepos(mint _nRepos, bool _bReport)
	{
		auto &State = *m_pState;

		State.m_nRepos = _nRepos;

		if (_bReport)
			f_RepoDone(0);
	}

	void CGitLaunches::f_CheckInit() const
	{
		auto &State = *m_pState;
		DMibFastCheck(State.m_bInited);
		if (!State.m_bInited)
			DMibError("Internal error: CGitLaunches have not been initialized");
	}

#if DMibConfig_RefCountDebugging
	constinit static CLowLevelLockAggregate g_TimerDestructionTrackerLock;
	constinit static NException::CCallstack g_TimerDestructionTrackerCallstack = {0};

	struct CTrackTimerDestruction
	{
		CTrackTimerDestruction() = default;
		CTrackTimerDestruction(CTrackTimerDestruction const &) = delete;

		CTrackTimerDestruction(CTrackTimerDestruction &&_Other)
			: m_bLastAlive(fg_Exchange(_Other.m_bLastAlive, false))
		{
		}

		~CTrackTimerDestruction()
		{
			if (m_bLastAlive)
			{
				DMibLock(g_TimerDestructionTrackerLock);
				g_TimerDestructionTrackerCallstack.m_CallstackLen = NSys::fg_System_GetStackTrace
					(
						g_TimerDestructionTrackerCallstack.m_Callstack
						, sizeof(g_TimerDestructionTrackerCallstack.m_Callstack) / sizeof(g_TimerDestructionTrackerCallstack.m_Callstack[0])
					)
				;
			}
		}
		bool m_bLastAlive = true;
	};
#endif

	TCUnsafeFuture<CAsyncDestroyAwaiter> CGitLaunches::f_Init()
	{
		auto pState = m_pState;
		auto &State = *pState;

		State.m_CheckAbortTimer = co_await fg_RegisterTimer
			(
				0.1
				,
				[
					pState = m_pState
#if DMibConfig_RefCountDebugging
					, DestructionTracker = CTrackTimerDestruction()
#endif
				]
				() -> TCFuture<void>
				{
					auto &State = *pState;

					if (State.m_pCancelled->f_Load())
					{
						DLock(State.m_Lock);
						for (auto &Launch : State.m_Launches)
						{
							auto &LaunchID = State.m_Launches.fs_GetKey(Launch);
							auto &bAborted = State.m_LaunchesAborted[LaunchID];
							if (!bAborted)
							{
								bAborted = true;
								Launch(&CProcessLaunchActor::f_StopProcess).f_DiscardResult();
							}
						}
					}

					co_return {};
				}
			)
		;

		State.m_bInited = true;

		co_return fg_AsyncDestroyGeneric
			(
				[this]() -> TCFuture<void>
				{
					auto This = fg_Move(*this);

					DMibFastCheck(This.m_pOwner);
					DMibFastCheck(This.m_pState);

					auto &State = *This.m_pState;
					if (State.m_CheckAbortTimer)
						co_await fg_Exchange(State.m_CheckAbortTimer, nullptr)->f_Destroy();

					This.m_pOwner.f_Clear();

#if DMibConfig_RefCountDebugging
					{
						DMibLock(This.m_pState->m_RefCount.m_Debug->m_Lock);

						if (This.m_pState.f_GetRefCount() != 0)
						{
							{
								DMibLock(g_TimerDestructionTrackerLock);
								DMibTrace2("    Destruction callstack\n");
								g_TimerDestructionTrackerCallstack.f_Trace(8);
							}

							mint iCallstack = 0;
							for (auto &Callstack : This.m_pState->m_RefCount.m_Debug->m_Callstacks)
							{
								DMibTrace2("    Reference callstack {}\n", iCallstack);
								Callstack.f_Trace(8);
								++iCallstack;
							}
							DMibFastCheck(false);
						}
					}
#else
					DMibFastCheck(This.m_pState.f_GetRefCount() == 0);
#endif
					This.m_pState.f_Clear();

					co_return {};
				}
			)
		;
	}

	void CGitLaunches::f_MeasureRepos(TCVector<TCVector<CRepository *>> const &_FilteredRepositories, bool _bReport)
	{
		auto &State = *m_pState;

		State.m_LongestRepo = 0;
		State.m_nRepos = 0;

		for (auto &Repos : _FilteredRepositories)
		{
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;
				auto &Name = State.m_RepoNames[Repo.m_Location];
				auto RelativePath = CFile::fs_MakePathRelative(Repo.m_Location, State.m_BaseDir);
				if (!RelativePath.f_IsEmpty())
					Name = "{}"_f << CFile::fs_MakePathRelative(Repo.m_Location, State.m_BaseDir);
				else
					Name = ".";
				State.m_LongestRepo = fg_Max(State.m_LongestRepo, mint(Name.f_GetLen()));
				++State.m_nRepos;
			}
		}

		if (_bReport)
			f_RepoDone(0);
	}

	void CGitLaunches::f_SetOutputOrder(TCVector<TCSet<CStr>> const &_OutputOrder) const
	{
		auto &State = *m_pState;
		{
			DLock(State.m_DeferredOutputLock);
			State.m_OutputOrder = _OutputOrder;
		}
	}

	void CGitLaunches::f_Output(EOutputType _OutputType, CStr const &_Section, CStr const &_Output) const
	{
		auto &State = *m_pState;

		CDeferredOutput Dererred;
		Dererred.m_OutputType = _OutputType;
		Dererred.m_Lines = _Output.f_SplitLine();

		{
			DLock(State.m_DeferredOutputLock);
			State.m_DeferredOutput[_Section].f_Insert(fg_Move(Dererred));
		}
		return;
	}

	CStr CGitLaunches::f_GetRepoName(CRepository const &_Repo) const
	{
		DMibFastCheck(!!m_pState);
		auto &State = *m_pState;
		return fg_Const(State.m_RepoNames)[_Repo.m_Location];
	}

	void CGitLaunches::f_Output(EOutputType _OutputType, CRepository const &_Repo, CStr const &_Output, CStr const &_Prefix) const
	{
		auto &State = *m_pState;

		auto &RepoName = fg_Const(State.m_RepoNames)[_Repo.m_Location];

		CDeferredOutput Dererred;
		Dererred.m_OutputType = _OutputType;
		Dererred.m_Lines = _Output.f_SplitLine();

		{
			DLock(State.m_DeferredOutputLock);
			State.m_DeferredOutput[_Prefix + RepoName].f_Insert(fg_Move(Dererred));
		}
	}

	TCUnsafeFuture<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::fp_Launch(CProcessLaunchActor::CSimpleLaunch _Launch) const
	{
		f_CheckInit();
		_Launch.m_Params.m_bCreateNewProcessGroup = true;

		auto pState = m_pState;
		auto &State = *pState;

		if (*State.m_pCancelled)
			co_return DMibErrorInstance("Aborted");

		auto DoneSubscription = co_await State.m_LaunchSequencer.f_Sequence();

		if (*State.m_pCancelled)
			co_return DMibErrorInstance("Aborted");

		TCActor<CProcessLaunchActor> LaunchActor = fg_Construct();
		mint LaunchID;
		{
			DLock(State.m_Lock);
			LaunchID = State.m_LaunchID++;
			State.m_Launches[LaunchID] = LaunchActor;
		}

		auto Result = co_await LaunchActor(&CProcessLaunchActor::f_LaunchSimple, fg_Move(_Launch));

		{
			TCActor<CProcessLaunchActor> LaunchActor;
			{
				DLock(State.m_Lock);
				auto pLaunchActor = State.m_Launches.f_FindEqual(LaunchID);
				if (pLaunchActor)
				{
					LaunchActor = *pLaunchActor;
					State.m_Launches.f_Remove(LaunchID);
				}
			}
			if (!LaunchActor)
			{
				pState.f_Clear();
				co_return fg_Move(Result);
			}
		}

		co_await fg_Move(LaunchActor).f_Destroy();
		pState.f_Clear();

		co_return fg_Move(Result);
	}

	TCFuture<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::f_Launch
		(
			CStr const &_Directory
			, TCVector<CStr> const &_Params
			, TCMap<CStr, CStr> const &_Environment
			, CProcessLaunchActor::ESimpleLaunchFlag _Flags
			, CStr const &_Application
		) const
	{
		TCVector<CStr> CommandLineParams;
		if (_Application == "git")
			CommandLineParams = {"-C", _Directory};
		CommandLineParams.f_Insert(_Params);

		CProcessLaunchActor::CSimpleLaunch LaunchParams{_Application, CommandLineParams};
		LaunchParams.m_SimpleFlags = _Flags;

		LaunchParams.m_Params.m_Environment += _Environment;
		if (_Application != "git")
			LaunchParams.m_Params.m_WorkingDirectory = _Directory;

		return fp_Launch(fg_Move(LaunchParams));
	}

	TCFuture<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::f_Launch
		(
			CRepository const &_Repo
			, TCVector<CStr> const &_Params
			, TCMap<CStr, CStr> const &_Environment
			, CStr const &_Application
		) const
	{
		TCVector<CStr> CommandLineParams;
		if (_Application == "git")
			CommandLineParams = {"-C", _Repo.m_Location};
		CommandLineParams.f_Insert(_Params);

		CProcessLaunchActor::CSimpleLaunch LaunchParams{_Application, CommandLineParams};

		LaunchParams.m_Params.m_Environment += _Environment;
		if (_Application != "git")
			LaunchParams.m_Params.m_WorkingDirectory = _Repo.m_Location;

		return fp_Launch(fg_Move(LaunchParams));
	}

	TCUnsafeFuture<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::f_OpenRepoEditor(CRepoEditor _Editor, CStr _Repo) const
	{
		auto Params = _Editor.m_Params;
#ifdef DPlatformFamily_Windows
		CStr RepoNative = NMib::NFile::NPlatform::fg_ConvertToWindowsPath(_Repo, false);
#else
		CStr RepoNative = _Repo;
#endif
		for (auto &Param : Params)
			Param = Param.f_Replace("{}", RepoNative);

		CProcessLaunchActor::CSimpleLaunch LaunchParams{_Editor.m_Application, Params};
		LaunchParams.m_Params.m_WorkingDirectory = _Editor.m_WorkingDir ? _Editor.m_WorkingDir : _Repo;

		auto Result = co_await fp_Launch(fg_Move(LaunchParams)).f_Wrap();

		if (_Editor.m_Sleep)
			co_await fg_Timeout(_Editor.m_Sleep);

		co_return fg_Move(Result);
	}

	uint32 CGitLaunches::fs_MaxProcesses()
	{
#ifdef DPlatformFamily_Windows
		return fg_Min(fg_Max(32000u / (NSys::fg_Thread_GetVirtualCores()*3u), 32u), 512u);
#else
		return 512u;
#endif
	}

	TCUnsafeFuture<void> CGitLaunches::f_Launch
		(
			CRepository _Repo
			, TCVector<CStr> _Params
			, TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> _fHandleResult
			, CStr _Prefix
			, TCMap<CStr, CStr> _Environment
			, CStr _Application
		) const
	{
		auto This = *this;
		auto &State = *This.m_pState;

		TCVector<CStr> CommandLineParams;
		if (_Application == "git")
			CommandLineParams = {"-C", _Repo.m_Location};
		CommandLineParams.f_Insert(_Params);

		CProcessLaunchActor::CSimpleLaunch LaunchParams{_Application, CommandLineParams};

		LaunchParams.m_Params.m_Environment += _Environment;
		if (_Application != "git")
			LaunchParams.m_Params.m_WorkingDirectory = _Repo.m_Location;

		auto Result = co_await fp_Launch(fg_Move(LaunchParams)).f_Wrap();

		co_await fg_ContinueRunningOnActor(State.m_OutputActor);

		bool bIsError = !Result || Result->m_ExitCode;

		if (Result)
		{
			CStr Output = _fHandleResult(*Result);

			if (!Output.f_IsEmpty())
				This.f_Output(bIsError ? EOutputType_Error : EOutputType_Normal, _Repo, Output, _Prefix);
		}

		if (!Result)
		{
			This.f_Output(EOutputType_Error, _Repo, "Failed: {}\n"_f << Result.f_GetExceptionStr(), _Prefix);
			co_return Result.f_GetException();
		}
		else if (Result->m_ExitCode)
		{
			This.f_Output(EOutputType_Error, _Repo, "Failed with exit code: {}\n"_f << Result->m_ExitCode, _Prefix);
			co_return DMibErrorInstance("Error status");
		}

		co_return {};
	}
}
