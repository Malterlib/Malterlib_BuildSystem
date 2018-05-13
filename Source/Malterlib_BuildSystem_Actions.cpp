// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_HandleAction(CStr const &_Action, TCVector<CStr> const &_Params)
	{
		CDisableExceptionTraceScope DisableTrace;

		TCVector<CStr> Params = _Params;

		CRepoFilter RepoFilter;

		bool bTypeSpecified = false;

		auto fParseFilter = [&]
			{
				TCVector<CStr> NewParams;

				for (auto &Param : Params)
				{
					CStr Value = Param;
					CStr Key = fg_GetStrSep(Value, "=");

					if (Param.f_StartsWith("Name="))
					{
						if (!bTypeSpecified)
							RepoFilter.m_Type = "";
						RepoFilter.m_NameWildcard = Value;
					}
					else if (Param.f_StartsWith("Type="))
					{
						RepoFilter.m_Type = Value;
						bTypeSpecified = true;
					}
					else if (Param.f_StartsWith("Tags="))
					{
						if (!bTypeSpecified)
							RepoFilter.m_Type = "";
						RepoFilter.m_Tags.f_AddContainer(Value.f_Split(";"));
					}
					else if (Param.f_StartsWith("OnlyChanged="))
						RepoFilter.m_bOnlyChanged = Value == "true";
					else
						NewParams.f_Insert(Param);
				}

				Params = fg_Move(NewParams);
			}
		;

		if (_Action == "update_repos")
		{
			if (!Params.f_IsEmpty())
				DMibError("Extra params found: {vs}"_f << Params);
			return;
		}
		else if (_Action == "git" || _Action == "sgit" || _Action == "cgit" || _Action == "scgit")
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

			ERepoStatusFlag Flags = ERepoStatusFlag_Quiet;

			for (; !Params.f_IsEmpty(); Params.f_Remove(0))
			{
				auto &Param = Params.f_GetFirst();
				if (Param == "-u" || Param == "--show-unchanged")
					Flags &= ~ERepoStatusFlag_Quiet;
				else if (Param == "-v" || Param == "--verbose")
					Flags |= ERepoStatusFlag_Verbose;
				else if (Param == "-r" || Param == "--update-remotes")
					Flags |= ERepoStatusFlag_UpdateRemotes;
				else if (Param == "-t" || Param == "--only-tracked")
					Flags |= ERepoStatusFlag_ShowOnlyTracked;
				else if (Param == "-a" || Param == "--all-braches")
					Flags |= ERepoStatusFlag_AllBranches;
				else if (Param == "-b" || Param == "--use-default-upstream-branch")
					Flags |= ERepoStatusFlag_UseDefaultUpstreamBranch;
				else if (Param == "-e" || Param == "--open-editor")
					Flags |= ERepoStatusFlag_OpenEditor;
				else if (Param == "-p" || Param == "--need-action-on-push")
					Flags |= ERepoStatusFlag_NeedActionOnPush;
				else if (Param == "-h" || Param == "--help")
				{
					DMibConOut2
						(
							"-e --open-editor\n"
						 	"-r --update-remotes\n"
							"-u --show-unchanged\n"
							"-v --verbose\n"
							"-t --only-tracked\n"
							"-a --all-braches\n"
							"-b --use-default-upstream-branch\n"
							"-p --need-action-on-push\n"
							"-h --help\n"
						)
					;
					return;
				}
				else
					DMibError("Unknown option: {}"_f << Param);
			}

			if (!Params.f_IsEmpty())
				DMibError("Extra params found: {vs}"_f << Params);

			fp_Repository_Status(RepoFilter, Flags);
		}
		else if (_Action == "push")
		{
			fParseFilter();

			bool bPretend = false;
			bool bTags = true;
			bool bNonDefaultToAll = false;

			{
				TCVector<CStr> NewParams;

				for (auto &Param : Params)
				{
					if (Param == "--pretend")
					 	bPretend = true;
					else if (Param == "--no-tags")
					 	bTags = false;
					else if (Param == "--non-default-to-all")
					 	bNonDefaultToAll = true;
					else
						NewParams.f_Insert(Param);
				}

				Params = fg_Move(NewParams);
			}

			fp_Repository_Push(RepoFilter, Params, bPretend, bTags, bNonDefaultToAll);
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
			uint32 MaxCommits = 50;
			uint32 MaxCommitsMainRepo = 500;

			for (; !Params.f_IsEmpty(); Params.f_Remove(0))
			{
				auto &Param = Params.f_GetFirst();
				if (Param == "-l" || Param == "--local")
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
				else if (Param.f_StartsWith("--max-commits="))
					MaxCommits = Param.f_Extract(14).f_ToInt(uint32(50));
				else if (Param.f_StartsWith("--max-commits-main="))
					MaxCommitsMainRepo = Param.f_Extract(19).f_ToInt(uint32(500));
				else if (FromRef.f_IsEmpty())
					FromRef = Param;
				else if (ToRef.f_IsEmpty())
					ToRef = Param;
				else
					DMibError("Extra params found: {vs}"_f << Params);
			}

			if (FromRef.f_IsEmpty())
				FromRef = "origin/master";

			if (ToRef.f_IsEmpty())
				ToRef = "HEAD";

			fp_Repository_ListCommits(RepoFilter, FromRef, ToRef, Flags, WildcardColumns, Prefix, MaxCommitsMainRepo, MaxCommits);
		}
		else
			DMibError("Uknown action: {}"_f << _Action);
	}
}
