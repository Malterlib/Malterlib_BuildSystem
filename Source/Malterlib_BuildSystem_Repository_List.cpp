// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/CommandLine/TableRenderer>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace NRepository
	{
		CStr fg_RelativeToBase(CStr const &_Path, CStr const &_BaseDir)
		{
			CStr Relative = CFile::fs_MakePathRelative(_Path, _BaseDir);
			if (Relative.f_IsEmpty())
				return ".";

			return Relative;
		}

		CRepositoryPinLocation fg_GetRepositoryPinLocation
			(
				CRepository const &_Repo
				, TCMap<CStr, CRepository const *> const &_RepositoryByLocation
				, CStateHandler &_StateHandler
				, CStr const &_BaseDir
			)
		{
			CRepositoryPinLocation PinLocation;
			if (_Repo.m_ConfigFile.f_IsEmpty())
				return PinLocation;

			// A root that no repository definition names owns the config files that no other repository has
			CStr OwnerLocation;
			if (CBuildSystem::fs_FindContainingPath(_RepositoryByLocation, CFile::fs_GetPath(_Repo.m_ConfigFile), OwnerLocation))
				PinLocation.m_Parent = fg_RelativeToBase(OwnerLocation, _BaseDir);
			else if (_Repo.m_ConfigFile.f_StartsWith(_BaseDir + "/"))
				PinLocation.m_Parent = ".";

			PinLocation.m_ConfigFile = fg_RelativeToBase(_Repo.m_ConfigFile, _BaseDir);

			auto Config = _StateHandler.f_GetConfig(_Repo.m_ConfigFile, _Repo.m_Location, _Repo.m_Identity, false);
			PinLocation.m_ConfigKey = Config.m_Key;
			PinLocation.m_Pin = Config.m_Hash;

			return PinLocation;
		}
	}

	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_List
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, bool _bJson
			, TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_None, EFilterRepoFlag_IncludeNotCloned);

		TCMap<CStr, CRepository const *> RepositoryByLocation;
		for (auto &Repos : FilteredRepositories.m_ReposOrdered)
		{
			for (auto &RepoLocation : Repos)
			{
				for (auto &Repo : RepoLocation.m_Repositories)
					RepositoryByLocation[Repo.m_Location] = &Repo;
			}
		}

		CGitLaunches Launches{f_GetGitLaunchOptions("list-repos"), "Listing repositories"};
		auto DestroyLaunches = co_await co_await Launches.f_Init();

		CStateHandler StateHandler{f_GetBaseDir(), mp_OutputDir, mp_AnsiFlags, mp_fOutputConsole};

		CJsonOrdered Repositories(EJsonType_Array);

		auto fRootHead = [&]() -> TCUnsafeFuture<CStr>
			{
				co_await ECoroutineFlag_CaptureMalterlibExceptions;

				auto Result = co_await Launches.f_Launch(f_GetBaseDir(), {"rev-parse", "HEAD"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
				if (Result.m_ExitCode)
					co_return DMibErrorInstance("Failed to read HEAD of the root repository: {}"_f << Result.f_GetCombinedOut().f_Trim());

				co_return Result.f_GetStdOut().f_Trim();
			}
		;

		// A checkout's root is only in the repository definitions when a build file defines it with Type "Root"
		bool bHasRoot = false;
		for (auto &Repos : FilteredRepositories.m_ReposOrdered)
		{
			for (auto &RepoLocation : Repos)
			{
				for (auto &Repo : RepoLocation.m_Repositories)
				{
					if (fg_AreGitPathsSame(Repo.m_Location, f_GetBaseDir()))
						bHasRoot = true;
				}
			}
		}

		// Only a name filter can match a root without a definition, which has no type, tags or settings to match
		auto fRootMatchesFilter = [&]() -> bool
			{
				CRepoFilter WithoutName = _Filter;
				WithoutName.m_NameWildcard.f_Clear();
				if (!WithoutName.f_IsEmpty())
					return false;

				return _Filter.m_NameWildcard.f_IsEmpty()
					|| NStr::fg_StrMatchWildcard(".", _Filter.m_NameWildcard.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted
				;
			}
		;

		if (!bHasRoot && fRootMatchesFilter() && CFile::fs_FileExists(f_GetBaseDir() / ".git", EFileAttrib_Directory | EFileAttrib_File))
		{
			auto UrlResult = co_await Launches.f_Launch(f_GetBaseDir(), {"config", "--get", "remote.origin.url"}, {}, CProcessLaunchActor::ESimpleLaunchFlag_None);
			auto MainBranchInfo = fg_GetMainRepoBranchInfo(f_GetBaseDir(), FilteredRepositories.m_ReposOrdered);

			auto &Entry = Repositories.f_Array().f_Insert(CJsonOrdered(EJsonType_Object));
			Entry["Path"] = ".";
			Entry["Url"] = UrlResult.m_ExitCode == 0 ? UrlResult.f_GetStdOut().f_Trim() : CStr();
			Entry["DefaultBranch"] = MainBranchInfo.m_DefaultBranch;
			Entry["Binary"] = false;
			Entry["UpdateSubmodules"] = false;
			Entry["Parent"] = "";
			Entry["ConfigFile"] = "";
			Entry["ConfigKey"] = "";
			Entry["Pin"] = co_await fRootHead();
		}

		// The filtered repositories are in the order of the levels they are pinned at, so parents come before their children
		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCMap<CStr, CRepository const *> SortedRepos;
			for (auto *pRepo : Repos)
				SortedRepos[pRepo->m_Location] = pRepo;

			for (auto *pRepo : SortedRepos)
			{
				auto &Repo = *pRepo;
				bool bIsRoot = fg_AreGitPathsSame(Repo.m_Location, f_GetBaseDir());

				auto &Entry = Repositories.f_Array().f_Insert(CJsonOrdered(EJsonType_Object));
				Entry["Path"] = fg_RelativeToBase(Repo.m_Location, f_GetBaseDir());
				Entry["Url"] = Repo.m_OriginProperties.m_URL;
				Entry["DefaultBranch"] = Repo.m_OriginProperties.m_DefaultBranch;
				// Deliberately the LFS release store, whose files a review can't read, and not whether the repository holds binaries,
				// which nothing in the repository definitions says
				Entry["Binary"] = Repo.m_OriginProperties.m_bLfsReleaseStore;

				if (auto *pUpstream = Repo.m_Remotes.m_Remotes.f_FindEqual("upstream"))
				{
					Entry["UpstreamUrl"] = pUpstream->m_Properties.m_URL;
					Entry["DefaultUpstreamBranch"] = pUpstream->m_Properties.m_DefaultBranch.f_IsEmpty()
						? Repo.m_DefaultUpstreamBranch
						: pUpstream->m_Properties.m_DefaultBranch
					;
				}

				Entry["UpdateSubmodules"] = Repo.m_bUpdateSubmodules;

				if (bIsRoot)
				{
					Entry["Parent"] = "";
					Entry["ConfigFile"] = "";
					Entry["ConfigKey"] = "";
					Entry["Pin"] = co_await fRootHead();
					continue;
				}

				auto PinLocation = fg_GetRepositoryPinLocation(Repo, RepositoryByLocation, StateHandler, f_GetBaseDir());
				Entry["Parent"] = PinLocation.m_Parent;
				Entry["ConfigFile"] = PinLocation.m_ConfigFile;
				Entry["ConfigKey"] = PinLocation.m_ConfigKey;
				Entry["Pin"] = PinLocation.m_Pin;
			}
		}

		if (_bJson)
		{
			*_pCommandLine += Repositories.f_ToString() + "\n";
			co_return ERetry_None;
		}

		auto TableRenderer = _pCommandLine->f_TableRenderer();
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_AvoidRowSeparators | CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_NoExtraLines);

		CTableRenderHelper::CColumnHelper Columns(0);
		Columns.f_AddHeading("Repository", 0);
		Columns.f_AddHeading("Pin", 0);
		Columns.f_AddHeading("Pinned in", 0);
		Columns.f_AddHeading("Default branch", 0);
		Columns.f_AddHeading("URL", 0);
		TableRenderer.f_AddHeadings(&Columns);

		for (auto &Entry : Repositories.f_Array())
		{
			CStr PinnedIn;
			if (!Entry["ConfigFile"].f_String().f_IsEmpty())
				PinnedIn = "{} [{}]"_f << Entry["ConfigFile"].f_String() << Entry["ConfigKey"].f_String();

			TableRenderer.f_AddRow
				(
					Entry["Path"].f_String()
					, Entry["Pin"].f_String()
					, PinnedIn
					, Entry["DefaultBranch"].f_String()
					, Entry["Url"].f_String()
				)
			;
		}

		TableRenderer.f_Output(CTableRenderHelper::EOutputType_HumanReadable);

		co_return ERetry_None;
	}
}
