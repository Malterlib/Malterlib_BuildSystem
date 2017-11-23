// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_HandleAction(CStr const &_Action, TCVector<CStr> const &_Params)
	{
		TCVector<CStr> Params = _Params;

		CRepoFilter RepoFilter;

		bool bTypeSpecified = false;

		auto fParseFilter = [&]
			{
				while (!Params.f_IsEmpty() && Params.f_GetFirst().f_Find("=") >= 0)
				{
					CStr Value = Params.f_GetFirst();
					CStr Key = fg_GetStrSep(Value, "=");

					if (Key == "Name")
					{
						if (!bTypeSpecified)
							RepoFilter.m_Type = "";
						RepoFilter.m_NameWildcard = Value;
					}
					else if (Key == "Type")
					{
						RepoFilter.m_Type = Value;
						bTypeSpecified = true;
					}
					else if (Key == "Tags")
					{
						if (!bTypeSpecified)
							RepoFilter.m_Type = "";
						RepoFilter.m_Tags.f_AddContainer(Value.f_Split(";"));
					}
					else if (Key == "OnlyChanged")
						RepoFilter.m_bOnlyChanged = Value == "true";
					else
						DMibError("Uknown option: {}"_f << Key);

					Params.f_Remove(0);
				}
			}
		;

		if (_Action == "git" || _Action == "sgit" || _Action == "cgit" || _Action == "scgit")
		{
			RepoFilter.m_Type = "Malterlib";
			RepoFilter.m_bOnlyChanged = _Action == "cgit" || _Action == "scgit";
			fParseFilter();
			fp_Repository_ForEachRepo(RepoFilter, _Action == "git" || _Action == "cgit", Params);
		}
		else if (_Action == "branch")
		{
			RepoFilter.m_Type = "Malterlib";
			RepoFilter.m_bOnlyChanged = true;
			fParseFilter();
			if (Params.f_IsEmpty())
				DMibError("You need to specify branch");

			if (!RepoFilter.m_NameWildcard.f_IsEmpty())
				DMibError("branch does not support name wildcards");

			if (!RepoFilter.m_Tags.f_IsEmpty())
				DMibError("branch does not support tags");

			CStr Branch = Params.f_GetFirst();
			Params.f_Remove(0);

			if (Branch.f_IsEmpty())
				DMibError("Branch cannot be empty, use unbranch to checkout default branch");

			if (!Params.f_IsEmpty())
				DMibError("Extra params found: {vs}"_f << Params);

			fp_Repository_Branch(RepoFilter, Branch);
		}
		else if (_Action == "unbranch")
		{
			RepoFilter.m_Type = "Malterlib";
			RepoFilter.m_bOnlyChanged = true;
			fParseFilter();

			if (!Params.f_IsEmpty())
				DMibError("Extra params found: {vs}"_f << Params);

			if (!RepoFilter.m_NameWildcard.f_IsEmpty())
				DMibError("branch does not support name wildcards");

			if (!RepoFilter.m_Tags.f_IsEmpty())
				DMibError("branch does not support tags");

			fp_Repository_Unbranch(RepoFilter);
		}
		else if (_Action == "cleanup-branches")
		{
			RepoFilter.m_Type = "Malterlib";
			RepoFilter.m_bOnlyChanged = false;
			fParseFilter();

			ERepoCleanupBranchesFlag Flags = ERepoCleanupBranchesFlag_None;

			for (; !Params.f_IsEmpty(); Params.f_Remove(0))
			{
				auto &Param = Params.f_GetFirst();
				if (Param == "-p" || Param == "--pretend")
					Flags |= ERepoCleanupBranchesFlag_Pretend;
				else if (Param == "-r" || Param == "--remote")
					Flags |= ERepoCleanupBranchesFlag_Remote;
				else
					DMibError("Unknown option: {}"_f << Param);
			}

			fp_Repository_CleanupBranches(RepoFilter, Flags);
		}
		else if (_Action == "status")
		{
			RepoFilter.m_Type = "";
			RepoFilter.m_bOnlyChanged = false;
			fParseFilter();

			ERepoStatusFlag Flags = ERepoStatusFlag_None;

			for (; !Params.f_IsEmpty(); Params.f_Remove(0))
			{
				auto &Param = Params.f_GetFirst();
				if (Param == "-v" || Param == "--verbose")
					Flags |= ERepoStatusFlag_Verbose;
				else if (Param == "-r" || Param == "--remote")
					Flags |= ERepoStatusFlag_UpdateRemotes;
				else if (Param == "-u" || Param == "--untracked")
					Flags |= ERepoStatusFlag_ShowUntracked;
				else if (Param == "-q" || Param == "--quiet")
					Flags |= ERepoStatusFlag_Quiet;
				else if (Param == "-a" || Param == "--all-braches")
					Flags |= ERepoStatusFlag_AllBranches;
				else if (Param == "-b" || Param == "--use-default-upstream-branch")
					Flags |= ERepoStatusFlag_UseDefaultUpstreamBranch;
				else if (Param == "-s" || Param == "--open-source-tree")
					Flags |= ERepoStatusFlag_OpenSourceTree;
				else
					DMibError("Unknown option: {}"_f << Param);
			}

			if (!Params.f_IsEmpty())
				DMibError("Extra params found: {vs}"_f << Params);

			fp_Repository_Status(RepoFilter, Flags);
		}
		else if (_Action == "push")
		{
			RepoFilter.m_Type = "Malterlib";
			RepoFilter.m_bOnlyChanged = false;
			fParseFilter();

			fp_Repository_Push(RepoFilter, Params);
		}
		else if (_Action == "list-commits")
		{
			RepoFilter.m_Type = "";
			RepoFilter.m_bOnlyChanged = false;
			fParseFilter();

			CStr FromRef;
			CStr ToRef;

			ERepoListCommitsFlag Flags = ERepoListCommitsFlag_UpdateRemotes | ERepoListCommitsFlag_Color;
			TCVector<CWildcardColumn> WildcardColumns;
			CStr Prefix;

			for (; !Params.f_IsEmpty(); Params.f_Remove(0))
			{
				auto &Param = Params.f_GetFirst();
				if (Param == "-l")
					Flags &= ~ERepoListCommitsFlag_UpdateRemotes;
				else if (Param == "--no-color")
					Flags &= ~ERepoListCommitsFlag_Color;
				else if (Param == "--compact")
					Flags |= ERepoListCommitsFlag_Compact;
				else if (Param.f_StartsWith("--columns="))
				{
					for (auto &Column : Param.f_Extract(10).f_Split(";"))
					{
						CStr Wildcard = Column;
						CStr Name = fg_GetStrSep(Wildcard, ":");
						WildcardColumns.f_Insert({Name, Wildcard});
					}
				}
				else if (Param.f_StartsWith("--prefix="))
					Prefix = Param.f_Extract(9);
				else if (FromRef.f_IsEmpty())
					FromRef = Param;
				else if (ToRef.f_IsEmpty())
					ToRef = Param;
				else
					DMibError("Extra params found: {vs}"_f << Params);
			}

			if (FromRef.f_IsEmpty())
				DMibError("Missing FromRef");

			if (ToRef.f_IsEmpty())
				DMibError("Missing ToRef");

			fp_Repository_ListCommits(RepoFilter, FromRef, ToRef, Flags, WildcardColumns, Prefix);
		}
		else
			DMibError("Uknown action: {}"_f << _Action);
	}
}
