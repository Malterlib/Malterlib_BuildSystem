// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem::NRepository
{
	CGitLaunches::CState::CState(CStr const &_BaseDir, CStr const &_ProgressDescription)
		: m_BaseDir(_BaseDir)
		, m_ProgressDescription(_ProgressDescription)
	{
	}

	CGitLaunches::CGitLaunches(CStr const &_BaseDir, CStr const &_ProgressDescription)
		: m_pState(fg_Construct(_BaseDir, _ProgressDescription))
	{
	}

	namespace
	{
		void fg_OutputRepoLine(EOutputType _OutputType, CStr const &_RepoName, mint _LongestRepoName, CStr const &_Line)
		{
			ch8 const *pRepoColor = "ERROR";

			switch (_OutputType)
			{
			case EOutputType_Normal: pRepoColor = CColors::mc_StatusNormal; break;
			case EOutputType_Warning: pRepoColor = CColors::mc_StatusWarning; break;
			case EOutputType_Error: pRepoColor = CColors::mc_StatusError; break;
			}

			CStr ReplacedRepo = _RepoName.f_Replace("/", "{}{}/{}"_f << CColors::mc_Default << DColor_256(250) << pRepoColor ^ 1);
			DMibConOut2
				(
					"{}{sl*,sf ,a-}{} {}|{}  {}\n"
					, pRepoColor
					, ReplacedRepo
					, _LongestRepoName + ReplacedRepo.f_GetLen() - _RepoName.f_GetLen()
					, CColors::mc_Default
					, DColor_256(242)
					, CColors::mc_Default
					, _Line
				)
			;
		}
		void fg_OutputSectionLine()
		{
			DMibConOut2("{}--------------------------------------------------------------------------------{}\n", DColor_256(242), CColors::mc_Default);
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

		for (auto &RepoOutput : m_DeferredOutput)
		{
			auto &RepoName = m_DeferredOutput.fs_GetKey(RepoOutput);

			mint nLines = 0;
			for (auto &Output : RepoOutput)
				nLines += Output.m_Lines.f_GetLen();

			if (nLines > 1)
			{
				if (!bDidOutputSection)
				{
					fg_OutputSectionLine();
					bDidOutputSection = true;
				}
			}
			else
				bDidOutputSection = false;

			for (auto &Output : RepoOutput)
			{
				for (auto &Line : Output.m_Lines)
					fg_OutputRepoLine(Output.m_OutputType, RepoName, LongestRepo, Line);
			}

			if (bDidOutputSection)
				fg_OutputSectionLine();
		}
	}

	void CGitLaunches::f_RepoDone(mint _nDone) const
	{
		auto &State = *m_pState;

		State.m_nDoneRepos += _nDone;

		CUStr ToOutput = CStr{"  {}: {}/{} repos done"_f << State.m_ProgressDescription << State.m_nDoneRepos.f_Load() << State.m_nRepos};
		if (State.m_nDoneRepos == State.m_nRepos)
			ToOutput = CStr{"{sj*}"_f << "" << ToOutput.f_GetLen()}; // Clear previous output

		DMibConOut2("{}\x1B[{}D", ToOutput, ToOutput.f_GetLen());
	}

	void CGitLaunches::f_MeasureRepos(TCVector<TCVector<CRepository *>> const &_FilteredRepositories)
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

		f_RepoDone(0);
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

	TCContinuation<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::f_Launch(CRepository const &_Repo, TCVector<CStr> const &_Params) const
	{
		auto &State = *m_pState;
		return State.m_LaunchSequencer > [=, pState = m_pState]() -> TCContinuation<CProcessLaunchActor::CSimpleLaunchResult>
			{
				auto &State = *pState;
				TCActor<CProcessLaunchActor> LaunchActor;
				{
					DLock(State.m_Lock);
					LaunchActor = State.m_Launches.f_Insert() = fg_Construct();
				}
				CProcessLaunchActor::CSimpleLaunch LaunchParams{"git", _Params, _Repo.m_Location};
				return LaunchActor(&CProcessLaunchActor::f_LaunchSimple, fg_Move(LaunchParams));
			}
		;
	}

	TCContinuation<CProcessLaunchActor::CSimpleLaunchResult> CGitLaunches::f_OpenDocument(CStr const &_Application, CStr const &_Document) const
	{
		auto &State = *m_pState;

		return State.m_LaunchSequencer > [=, pState = m_pState]() -> TCContinuation<CProcessLaunchActor::CSimpleLaunchResult>
			{
				auto &State = *pState;

				TCActor<CProcessLaunchActor> LaunchActor;
				{
					DLock(State.m_Lock);
					LaunchActor = State.m_Launches.f_Insert() = fg_Construct();
				}
				CProcessLaunchActor::CSimpleLaunch LaunchParams{"open", {"-a", _Application, _Document}};
				return LaunchActor(&CProcessLaunchActor::f_LaunchSimple, fg_Move(LaunchParams));
			}
		;
	}

	uint32 CGitLaunches::fs_MaxProcesses()
	{
#ifdef DPlatformFamily_Windows
		return fg_Min(fg_Max(32000u / (NSys::fg_Thread_GetVirtualCores()*3u), 32u), 512u);
#else
		return 512u;
#endif
	}

	TCContinuation<void> CGitLaunches::f_Launch
		(
		 	CRepository const &_Repo
		 	, TCVector<CStr> const &_Params
		 	, TCFunctionMovable<CStr (CProcessLaunchActor::CSimpleLaunchResult const &_Result)> &&_fHandleResult
		 	, CStr const &_Prefix
		) const
	{
		auto &State = *m_pState;

		TCContinuation<void> Result;

		State.m_LaunchSequencer > [=, pState = m_pState]() -> TCContinuation<CProcessLaunchActor::CSimpleLaunchResult>
			{
				auto &State = *pState;

				TCActor<CProcessLaunchActor> LaunchActor;
				{
					DLock(State.m_Lock);
					LaunchActor = State.m_Launches.f_Insert() = fg_Construct();
				}
				CProcessLaunchActor::CSimpleLaunch LaunchParams{"git", _Params, _Repo.m_Location};
				return LaunchActor(&CProcessLaunchActor::f_LaunchSimple, fg_Move(LaunchParams));
			}
			> State.m_OutputActor / [Result, _Prefix, This = *this, _Repo, fHandleResult = fg_Move(_fHandleResult)]
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

		return Result;
	}
}
