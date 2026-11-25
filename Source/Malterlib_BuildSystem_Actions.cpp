// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem.h"
#include <Mib/Encoding/EJson>

namespace NMib::NBuildSystem
{
	TCUnsafeFuture<CBuildSystem::ERetry> DMibWorkaroundUBSanSectionErrors CBuildSystem::fs_RunBuildSystem
		(
			NFunction::TCFunctionMovable<TCFuture<CBuildSystem::ERetry> (CBuildSystem *_pBuildSystem)> _fCommand
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			, CGenerateOptions const &_GenerateOptions
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		TCSharedPointer<TCAtomic<bool>> pCancelled = fg_Construct();

		auto CancellationSubscription = _pCommandLine->f_RegisterForCancellation
			(
				g_ActorFunctor
				(
					g_ActorSubscription / []()
					{
					}
				)
				/ [pCancelled, _pCommandLine, nAborts = 0]() mutable -> TCFuture<bool>
				{
					if (nAborts > 5)
						co_await _pCommandLine->f_StdErr("\33[2K\rAborting again won't make it go faster\n");
					else if (nAborts > 0)
						co_await _pCommandLine->f_StdErr("\33[2K\rAlready aborting\n");
					else
						co_await _pCommandLine->f_StdErr("\33[2K\rAborting\n");

					++nAborts;
					pCancelled->f_Exchange(true);
					co_return false;
				}
			)
		;

		co_await fg_ContinueRunningOnActor(fg_ConcurrentActorLowPrio());

		CBuildSystem::ERetry Retry = CBuildSystem::ERetry_Again;
		while (Retry != CBuildSystem::ERetry_None)
		{
			auto AnsiFlags = _pCommandLine->f_AnsiEncoding().f_Flags();
			CBuildSystem BuildSystem(AnsiFlags, _pCommandLine->m_CommandLineWidth, _fOutputConsole, pCancelled, _GenerateOptions.m_bShowProgress);

			if (_GenerateOptions.m_DetailedPositions == EDetailedPositions_Enable)
				BuildSystem.f_SetEnablePositions();

			if (_GenerateOptions.m_bDetailedValues)
				BuildSystem.f_SetEnableValues();

			if (Retry == CBuildSystem::ERetry_Again_NoReconcileOptions)
				BuildSystem.f_NoReconcileOptions();
			else if (Retry == CBuildSystem::ERetry_Again_ForceUpdate)
				BuildSystem.f_ForceUpdate();
			else if (Retry == CBuildSystem::ERetry_Again_EnablePositions)
				BuildSystem.f_SetEnablePositions();

			auto RetryResult = co_await _fCommand(&BuildSystem).f_Wrap();
			if (!RetryResult)
			{
				if
				(
					!BuildSystem.f_GetDisableOnDemandPositions()
					&& RetryResult.f_HasExceptionType<NStr::CExceptionParse>()
					&& !BuildSystem.f_EnablePositions()
					&& _GenerateOptions.m_DetailedPositions == EDetailedPositions_OnDemand
				)
				{
					CAnsiEncoding AnsiEncoding(AnsiFlags);

					_fOutputConsole
						(
							"\n"
							"{}Provisional errors:{}\n"
							"{}\n"_f
							<< AnsiEncoding.f_StatusError()
							<< AnsiEncoding.f_Default()
							<< RetryResult.f_GetExceptionStr()
							, true
						)
					;
					_fOutputConsole
						(
							"\n"
							"{}Re-running with detailed positions.{} See: --detailed-positions. {}\n"
							"\n"_f
							<< AnsiEncoding.f_StatusWarning()
							<< AnsiEncoding.f_Default()
							<< (BuildSystem.f_EnableValues() ? "" : "Detailed values is not enabled. See --detailed-values")
							, true
						)
					;

					Retry = CBuildSystem::ERetry_Again_EnablePositions;
				}
				else
					co_return RetryResult.f_GetException();
			}
			else
				Retry = *RetryResult;

			if (Retry == CBuildSystem::ERetry_Relaunch || Retry == CBuildSystem::ERetry_Relaunch_NoReconcileOptions)
				co_return Retry;
		}
		co_return CBuildSystem::ERetry_None;
	}

	bool CBuildSystem::CRepoFilter::f_IsEmpty() const
	{
		return f_MatchesAllNames() && m_Type.f_IsEmpty() && m_Branch.f_IsEmpty() && m_Tags.f_IsEmpty() && !m_bOnlyChanged && !m_bFormat && !m_bTidy;
	}

	bool CBuildSystem::CRepoFilter::f_MatchesAllNames() const
	{
		return m_NameWildcards.f_IsEmpty() || m_NameWildcards.f_Contains("*") >= 0;
	}

	// _Name is the repository's path relative to the base directory, and "." for the base directory itself.
	bool CBuildSystem::CRepoFilter::f_MatchesName(NStr::CStr const &_Name) const
	{
		if (f_MatchesAllNames())
			return true;

		return NStr::fg_StrMatchesAnyWildcardInContainer<NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted>(_Name, m_NameWildcards);
	}

	CBuildSystem::CRepoFilter CBuildSystem::CRepoFilter::fs_ParseParams(NEncoding::CEJsonSorted const &_Params)
	{
		CBuildSystem::CRepoFilter Filter;

		// An omitted name filter is the option's default, "*". An empty one is what a script passes when it selected nothing, so it is an
		// error rather than no filter, which would let a command such as 'branch -f' act on every repository.
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoName))
		{
			// A wildcard can start with a '[', so the value is JSON only where it parses as an array, whose entries must then be strings.
			auto fJsonArray = [](CStr const &_Value) -> NStorage::TCOptional<TCVector<CStr>>
				{
					if (!_Value.f_Trim().f_StartsWith("["))
						return {};

					CEJsonSorted Json;
					try
					{
						Json = CEJsonSorted::fs_FromString(_Value);
					}
					catch (NException::CException const &)
					{
						return {};
					}

					if (!Json.f_IsArray())
						return {};

					try
					{
						return Json.f_StringArray();
					}
					catch (NException::CException const &_Exception)
					{
						DMibError("The repository name filter (-n, --repo-name) is a JSON array with an entry that is no string: {}: {}"_f << Json << _Exception);
					}
				}
			;

			TCVector<CStr> Wildcards;
			if (pValue->f_IsArray())
				Wildcards = pValue->f_StringArray();
			else if (auto Strings = fJsonArray(pValue->f_String()))
				Wildcards = fg_Move(*Strings);
			else if (!pValue->f_String().f_IsEmpty())
			{
				// Only '\,' is an escape, a comma of a wildcard: another backslash stays, so '\*' remains no '*' that matches
				// every repository.
				CStr const &Text = pValue->f_String();
				CStr Wildcard;
				for (umint iChar = 0; iChar < Text.f_GetLen(); ++iChar)
				{
					ch8 Char = Text[iChar];
					if (Char == '\\' && iChar + 1 < Text.f_GetLen() && Text[iChar + 1] == ',')
					{
						Wildcard.f_AddChar(',');
						++iChar;
					}
					else if (Char == ',')
					{
						Wildcards.f_Insert(Wildcard);
						Wildcard.f_Clear();
					}
					else
						Wildcard.f_AddChar(Char);
				}
				Wildcards.f_Insert(fg_Move(Wildcard));
			}

			for (auto &Wildcard : Wildcards)
			{
				if (Wildcard.f_IsEmpty())
					DMibError("An empty wildcard in the repository name filter (-n, --repo-name) would match no repository");

				Filter.m_NameWildcards.f_Insert(Wildcard);
			}

			if (Filter.m_NameWildcards.f_IsEmpty())
				DMibError("The repository name filter (-n, --repo-name) is empty, so it would match no repository. Leave it out, or use '*', to match all");
		}
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoType))
			Filter.m_Type = pValue->f_String();
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoBranch))
			Filter.m_Branch = pValue->f_String();
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoTags))
			Filter.m_Tags.f_AddContainer(pValue->f_String().f_Split<true>(";"));
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoOnlyChanged))
			Filter.m_bOnlyChanged = pValue->f_Boolean();
		if (auto pValue = _Params.f_GetMember(gc_ConstString_RepoIncludePull))
			Filter.m_bIncludePull = pValue->f_Boolean();

		return Filter;
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_Update(CGenerateOptions const &_GenerateOptions)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;
		co_return ERetry_None;
	}
}
