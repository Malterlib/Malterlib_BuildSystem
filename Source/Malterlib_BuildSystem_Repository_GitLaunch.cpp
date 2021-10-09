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
		)
		: m_BaseDir(_BaseDir)
		, m_ProgressDescription(_ProgressDescription)
		, m_AnsiFlags(_AnsiFlags)
		, m_fOutputConsole(_fOutputConsole)
	{
	}

	CGitLaunches::CGitLaunches
		(
			CStr const &_BaseDir
			, CStr const &_ProgressDescription
			, EAnsiEncodingFlag _AnsiFlags
			, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
		)
		: m_pState(fg_Construct(_BaseDir, _ProgressDescription, _AnsiFlags, _fOutputConsole))
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
	}

	void CGitLaunches::CState::f_OutputState() const
	{
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
		return g_OnScopeExitActor > [pState = m_pState]
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

	TCFuture<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::fp_Launch(CProcessLaunchActor::CSimpleLaunch &&_Launch) const
	{
		auto &State = *m_pState;
		return State.m_LaunchSequencer / [=, pState = m_pState, Launch = fg_Move(_Launch)]() mutable -> TCFuture<CProcessLaunchActor::CSimpleLaunchResult>
			{
				auto &State = *pState;
				TCActor<CProcessLaunchActor> LaunchActor = fg_Construct();
				mint LaunchID;
				{
					DLock(State.m_Lock);
					LaunchID = State.m_LaunchID++;
					State.m_Launches[LaunchID] = LaunchActor;
				}
				TCPromise<CProcessLaunchActor::CSimpleLaunchResult> Promise;
				LaunchActor(&CProcessLaunchActor::f_LaunchSimple, fg_Move(Launch)) > [=](TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult> &&_Result)
					{
						auto &State = *pState;
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
							Promise.f_SetResult(fg_Move(_Result));
							return;
						}

						LaunchActor.f_Destroy() > [=, Result = fg_Move(_Result)](auto &&) mutable
							{
								Promise.f_SetResult(fg_Move(Result));
								return;
							}
						;
					}
				;
				return Promise.f_MoveFuture();
			}
		;
	}

	TCFuture<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::f_Launch(CRepository const &_Repo, TCVector<CStr> const &_Params, TCMap<CStr, CStr> const &_Environment) const
	{
		TCVector<CStr> CommandLineParams{"-C", _Repo.m_Location};
		CommandLineParams.f_Insert(_Params);

		CProcessLaunchActor::CSimpleLaunch LaunchParams{"git", CommandLineParams};

		LaunchParams.m_Params.m_Environment += _Environment;
		return fp_Launch(fg_Move(LaunchParams));
	}

	TCFuture<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::f_OpenRepoEditor(CRepoEditor const &_Editor, CStr const &_Repo) const
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

		TCPromise<CProcessLaunchActor::CSimpleLaunchResult> Promise;
		fp_Launch(fg_Move(LaunchParams)) > [=](TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult> &&_Result)
			{
				if (!_Editor.m_Sleep)
				{
					Promise.f_SetResult(fg_Move(_Result));
					return;
				}

				fg_Timeout(_Editor.m_Sleep) > Promise / [=, Result = fg_Move(_Result)]() mutable
					{
						Promise.f_SetResult(fg_Move(Result));
					}
				;
			}
		;

		return Promise.f_MoveFuture();
	}

	uint32 CGitLaunches::fs_MaxProcesses()
	{
#ifdef DPlatformFamily_Windows
		return fg_Min(fg_Max(32000u / (NSys::fg_Thread_GetVirtualCores()*3u), 32u), 512u);
#else
		return 512u;
#endif
	}

	TCFuture<void> CGitLaunches::f_Launch
		(
		 	CRepository const &_Repo
		 	, TCVector<CStr> const &_Params
		 	, TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> &&_fHandleResult
		 	, CStr const &_Prefix
		 	, TCMap<CStr, CStr> const &_Environment
		) const
	{
		auto &State = *m_pState;

		TCVector<CStr> CommandLineParams{"-C", _Repo.m_Location};
		CommandLineParams.f_Insert(_Params);

		CProcessLaunchActor::CSimpleLaunch LaunchParams{"git", CommandLineParams};
		LaunchParams.m_Params.m_Environment += _Environment;
		TCPromise<void> Result;
		fp_Launch(fg_Move(LaunchParams)) > State.m_OutputActor / [Result, _Prefix, This = *this, _Repo, fHandleResult = fg_Move(_fHandleResult)]
			(TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult> &&_Result) mutable
			{
				bool bIsError = !_Result || _Result->m_ExitCode;

				bool bDidOutput = false;

				if (_Result)
				{
					CStr Output = fHandleResult(*_Result);

					if (!Output.f_IsEmpty())
					{
						bDidOutput = true;
						This.f_Output(bIsError ? EOutputType_Error : EOutputType_Normal, _Repo, Output, _Prefix);
					}
				}

				if (!_Result)
				{
					This.f_Output(EOutputType_Error, _Repo, "Failed: {}\n"_f << _Result.f_GetExceptionStr(), _Prefix);
					bDidOutput = true;
					Result.f_SetException(_Result);
				}
				else if (_Result->m_ExitCode)
				{
					This.f_Output(EOutputType_Error, _Repo, "Failed with exit code: {}\n"_f << _Result->m_ExitCode, _Prefix);
					Result.f_SetException(DMibErrorInstance("Error status"));
					bDidOutput = true;
				}
				else
					Result.f_SetResult();
			}
		;

		return Result.f_MoveFuture();
	}
}
