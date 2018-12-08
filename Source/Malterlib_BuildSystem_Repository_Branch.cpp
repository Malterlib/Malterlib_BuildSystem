// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

namespace NMib::NBuildSystem::NRepository
{
	CBranchSettings::CBranchSettings(CStr const &_OutputDir)
		: m_OutputDir(_OutputDir)
	{
	}

	void CBranchSettings::f_WriteSettings()
	{
		if (!m_bDirty)
			return;

		CStr BranchSettingsFile = "{}/BranchSettings.json"_f << m_OutputDir;

		CEJSON SettingsJson = EJSONType_Object;

		for (auto &Branch : m_Branches)
		{
			auto &OutBranch = SettingsJson[Branch.f_GetType()];
			OutBranch["BranchName"] = Branch.m_Name;
			OutBranch["OnlyChanged"] = Branch.m_bOnlyChanged;
		}

		CByteVector FileData;
		CFile::fs_WriteStringToVector(FileData, SettingsJson.f_ToString());

		CFile::fs_CreateDirectory(CFile::fs_GetPath(BranchSettingsFile));
		CFile::fs_CopyFileDiff(FileData, BranchSettingsFile, CTime::fs_NowUTC());
	}

	void CBranchSettings::f_ReadSettings()
	{
		CStr BranchSettingsFile = "{}/BranchSettings.json"_f << m_OutputDir;

		if (!CFile::fs_FileExists(BranchSettingsFile))
			return;

		CEJSON SettingsJson = CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(BranchSettingsFile), BranchSettingsFile);
		for (auto &Branch : fg_Const(SettingsJson).f_Object())
		{
			auto &OutBranch = m_Branches[Branch.f_Name()];
			OutBranch.m_Name = Branch.f_Value()["BranchName"].f_String();
			OutBranch.m_bOnlyChanged = Branch.f_Value()["OnlyChanged"].f_Boolean();
		}
	}

	void CBranchSettings::f_RemoveBranch(CStr const &_Type)
	{
		if (m_Branches.f_Remove(_Type))
			m_bDirty = true;
	}

	void CBranchSettings::f_SetBranch(CStr const &_Type, CStr const &_Branch, bool _bOnlyChanged)
	{
		if (m_Branches(_Type).f_WasCreated())
			m_bDirty = true;

		auto &Branch = m_Branches[_Type];
		if (Branch.m_Name != _Branch)
		{
			Branch.m_Name = _Branch;
			m_bDirty = true;
		}

		if (Branch.m_bOnlyChanged != _bOnlyChanged)
		{
			Branch.m_bOnlyChanged = _bOnlyChanged;
			m_bDirty = true;
		}
	}
}

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_Branch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, CStr const &_Branch, ERepoBranchFlag _Flags)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;

		CBranchSettings BranchSettings{mp_OutputDir};

		BranchSettings.f_ReadSettings();

		bool bHasEmpty = BranchSettings.m_Branches.f_FindEqual("");
		if (_Branch.f_IsEmpty())
		{
			if (bHasEmpty && !BranchSettings.m_Branches.f_IsEmpty())
				DMibError("You cannot mix branch settings for empty repo type with non-empty type");
		}
		else
		{
			if (bHasEmpty)
				DMibError("You cannot mix branch settings for empty repo type with non-empty type");
		}

		BranchSettings.f_SetBranch(_Filter.m_Type, _Branch, _Filter.m_bOnlyChanged);
		BranchSettings.f_WriteSettings();

		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{mp_BaseDir, "Branching repos"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCActorResultVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				if (fg_GetBranch(Repo) == _Branch)
					continue;

				TCVector<CStr> ParamsCheckout;

				if (!fg_BranchExists(Repo, _Branch))
					ParamsCheckout = {"checkout", "-b", _Branch};
				else
					ParamsCheckout = {"checkout", _Branch};

				if (_Flags & ERepoBranchFlag_Pretend)
					Launches.f_Output(EOutputType_Normal, Repo, "git {}"_f << CProcessLaunchParams::fs_GetParams(ParamsCheckout));
				else
				{
					TCContinuation<void> LaunchResult;
					Launches.f_Launch(Repo, ParamsCheckout, fg_LogAllFunctor()) > [=](TCAsyncResult<void> &&_Result)
						{
							Launches.f_RepoDone();
							LaunchResult.f_SetResult(_Result);
						}
					;
					LaunchResult > Results.f_AddResult();
				}
			}

			LaunchResults.f_Insert(Results.f_GetResults().f_CallSync());
		}

		for (auto &Result : LaunchResults)
			Result.f_Access();

		return ERetry_None;
	}

	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_Unbranch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoBranchFlag _Flags)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;

		CBranchSettings BranchSettings{mp_OutputDir};

		BranchSettings.f_ReadSettings();

		auto *pOldBranch = BranchSettings.m_Branches.f_FindEqual(_Filter.m_Type);

		if (!pOldBranch)
			DMibError("Repo type '{}' has not been branched"_f << _Filter.m_Type);

		CStr OldBranch = pOldBranch->m_Name;

		BranchSettings.f_RemoveBranch(_Filter.m_Type);
		BranchSettings.f_WriteSettings();

		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		CGitLaunches Launches{mp_BaseDir, "Unbranching repos"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCVector<TCAsyncResult<void>> LaunchResults;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCActorResultVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				CStr CurrentBranch = fg_GetBranch(Repo);
				if (CurrentBranch == Repo.m_DefaultBranch)
					continue;

				if (_Flags & ERepoBranchFlag_Pretend)
					Launches.f_Output(EOutputType_Normal, Repo, "git checkout {}"_f << Repo.m_DefaultBranch);
				else
				{
					TCContinuation<void> LaunchResult;
					Launches.f_Launch(Repo, {"checkout", Repo.m_DefaultBranch}, fg_LogAllFunctor()) > [Launches, LaunchResult](TCAsyncResult<void> &&_Result)
						{
							Launches.f_RepoDone();
							LaunchResult.f_SetResult(_Result);
						}
					;
					LaunchResult > Results.f_AddResult();
				}

			}

			LaunchResults.f_Insert(Results.f_GetResults().f_CallSync());

		}

		for (auto &Result : LaunchResults)
			Result.f_Access();

		return ERetry_None;
	}

	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_CleanupBranches
		(
		 	CGenerateOptions const &_GenerateOptions
		 	, CRepoFilter const &_Filter
		 	, ERepoCleanupBranchesFlag _Flags
		 	, TCVector<CStr> const &_Branches
		)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;

		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		if (_Flags & ERepoCleanupBranchesFlag_UpdateRemotes)
			fg_UpdateRemotes(*this, FilteredRepositories);

		CGitLaunches Launches{mp_BaseDir, "Cleaning up branches"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCActorResultVector<void> LaunchResults;

		TCMap<CRepository *, TCSharedPointer<TCActorSequencer<void>>> DeleteLaunchSequencers;

		for (auto &[RepoBound, iSequence] : FilteredRepositories.f_GetAllRepos())
		{
			auto &Repo = RepoBound;
			auto *pRepo = &Repo;

			auto CurrentBranch = fg_GetBranch(Repo);

			TCContinuation<void> Continuation;

			TCVector<CStr> Params = {"branch", "--format", "%(refname:short)"};

			TCContinuation<TCVector<CStr>> RemotesContinuation;

			if (_Flags & ERepoCleanupBranchesFlag_AllRemotes)
			{
				Params.f_Insert("-a");
				RemotesContinuation = fg_GetRemotes(Launches, Repo);
			}
			else
				RemotesContinuation.f_SetResult(TCVector<CStr>{});

			auto pDeleteLaunchSequencer = (DeleteLaunchSequencers[pRepo] = fg_Construct());

			auto RepoDoneScope = Launches.f_RepoDoneScope();

			Launches.f_Launch(Repo, Params) + RemotesContinuation
				> Continuation / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result, TCVector<CStr> &&_Remotes) mutable
				{
					TCActorResultVector<void> Results;

					TCSet<CStr> Remotes;
					Remotes.f_AddContainer(_Remotes);

					auto FullBranches = _Result.f_GetStdOut().f_SplitLine();

					TCSet<CStr> LoggedRemote;

					for (auto &FullBranch : _Result.f_GetStdOut().f_SplitLine())
					{
						if (FullBranch.f_IsEmpty())
							continue;

						auto Components = FullBranch.f_Split("/");

						CStr Remote;
						CStr Branch;
						if ((_Flags & ERepoCleanupBranchesFlag_AllRemotes) && Remotes.f_FindEqual(Components[0]))
						{
							Remote = Components[0];
							Components.f_Remove(0);
							Branch = CStr::fs_Join(Components, "/");
						}
						else
							Branch = FullBranch;

						if (!_Branches.f_IsEmpty())
						{
							bool bMatchedBranch = false;
							for (auto &BranchWildcard : _Branches)
							{
								if (fg_StrMatchWildcard(Branch.f_GetStr(), BranchWildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
								{
									bMatchedBranch = true;
									break;
								}
							}

							if (!bMatchedBranch)
								continue;
						}

						if (Branch == "HEAD")
							continue;

						CStr CompareRemote = Remote ? Remote : "origin";

						if (Remote && (_Flags & ERepoCleanupBranchesFlag_AllRemotes))
						{
							if (Remote != "origin")
							{
								auto pRemote = Repo.m_Remotes.f_FindEqual(Remote);
								if (!pRemote)
								{
									if (_Flags & ERepoCleanupBranchesFlag_Verbose)
									{
										if (LoggedRemote(Remote).f_WasCreated())
											Launches.f_Output(EOutputType_Normal, Repo, "{}/* - remote not in build system"_f << Remote);
									}
									continue;
								}
								if (!pRemote->m_bCanPush)
								{
									if (_Flags & ERepoCleanupBranchesFlag_Verbose)
									{
										if (LoggedRemote(Remote).f_WasCreated())
											Launches.f_Output(EOutputType_Normal, Repo, "{}/* - no write access to remote"_f << Remote);
									}
									continue;
								}
							}
						}

						if (Remote && Branch == fg_GetRemoteHead(Repo, Remote))
						{
							if (_Flags & ERepoCleanupBranchesFlag_Verbose)
								Launches.f_Output(EOutputType_Normal, Repo, "{} - default remote branch protected"_f << FullBranch);
							continue;
						}

						if (Branch == Repo.m_DefaultBranch)
						{
							if (_Flags & ERepoCleanupBranchesFlag_Verbose)
								Launches.f_Output(EOutputType_Normal, Repo, "{} - default branch protected"_f << FullBranch);
							continue;
						}

						if (Branch == CurrentBranch)
						{
							if (_Flags & ERepoCleanupBranchesFlag_Verbose)
								Launches.f_Output(EOutputType_Normal, Repo, "{} - current branch protected"_f << FullBranch);
							continue;
						}

						bool bIsProtected = false;
						for (auto &BranchWildcard : Repo.m_ProtectedBranches)
						{
							if (fg_StrMatchWildcard(Branch.f_GetStr(), BranchWildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
							{
								bIsProtected = true;
								break;
							}
						}

						if (bIsProtected)
						{
							if (_Flags & ERepoCleanupBranchesFlag_Verbose)
								Launches.f_Output(EOutputType_Normal, Repo, "{} - branch protected by Repository.ProtectedBranches"_f << FullBranch);
							continue;
						}

						TCContinuation<void> Continuation;

						Launches.f_Launch(Repo, {"merge-base", "--is-ancestor", FullBranch, "{}/{}"_f << CompareRemote << Repo.m_DefaultBranch})
							+ Launches.f_Launch(Repo, {"log", "--oneline", "--cherry", "{}/{}...{}"_f << CompareRemote << Repo.m_DefaultBranch << FullBranch})
							> Continuation / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result, CProcessLaunchActor::CSimpleLaunchResult &&_ResultRebase) mutable
							{
								bool bIsInDefault = _Result.m_ExitCode == 0;
								CStr DeleteReason = "[ancestor]";
								if (!bIsInDefault)
								{
									bool bAllEquals = true;
									for (auto &Line : _ResultRebase.f_GetStdOut().f_Trim().f_SplitLine())
									{
										if (!Line.f_StartsWith("= "))
											bAllEquals = false;
									}
									if (bAllEquals)
									{
										DeleteReason = "[rebased]";
										bIsInDefault = true;
									}
								}
								if (!bIsInDefault && !(_Flags & ERepoCleanupBranchesFlag_Force))
								{
									Launches.f_Output(EOutputType_Error, Repo, "{} - not fully merged"_f << FullBranch);
									Continuation.f_SetResult();
									return;
								}

								if (!bIsInDefault)
									DeleteReason = "({}WARNING{} - forced)"_f << CColors::ms_StatusError << CColors::ms_Default;

								if (_Flags & ERepoCleanupBranchesFlag_Pretend)
								{
									Launches.f_Output(EOutputType_Warning, Repo, "{} - would have deleted {}"_f << FullBranch << DeleteReason);
									Continuation.f_SetResult();
									return;
								}

								*pDeleteLaunchSequencer > [=]() -> TCContinuation<void>
									{
										Launches.f_Output(EOutputType_Warning, Repo, "{} - deleting {}"_f << FullBranch << DeleteReason);

										TCContinuation<void> Continuation;

										if (Remote)
											Continuation = Launches.f_Launch(Repo, {"push", Remote, "--delete", "refs/heads/{}"_f << Branch}, fg_LogAllFunctor());
										else
											Continuation = Launches.f_Launch(Repo, {"branch", "-D", FullBranch}, fg_LogAllFunctor());

										return Continuation;
									}
									> Continuation;
								;
							}
						;

						Continuation > Results.f_AddResult();
					}

					Results.f_GetResults() > Continuation / [=](TCVector<TCAsyncResult<void>> &&_Results)
						{
							if (!fg_CombineResults(Continuation, fg_Move(_Results)))
								return;

							(void)RepoDoneScope;
							Continuation.f_SetResult();
						}
					;
				}
			;

			Continuation.f_Dispatch() > LaunchResults.f_AddResult();
		}

		for (auto &Result : LaunchResults.f_GetResults().f_CallSync())
			Result.f_Access();

		return ERetry_None;
	}

	CBuildSystem::ERetry CBuildSystem::f_Action_Repository_CleanupTags
		(
		 	CGenerateOptions const &_GenerateOptions
		 	, CRepoFilter const &_Filter
		 	, ERepoCleanupTagsFlag _Flags
		 	, TCVector<CStr> const &_Tags
		)
	{
		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			return Retry;

		CFilteredRepos FilteredRepositories = fg_GetFilteredRepos(_Filter, *this, mp_Data);

		if (_Flags & ERepoCleanupTagsFlag_UpdateRemotes)
			fg_UpdateRemotes(*this, FilteredRepositories);

		CGitLaunches Launches{mp_BaseDir, "Cleaning up tags"};

		CCurrentActorScope CurrentActorScope{Launches.m_pState->m_OutputActor};

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		TCActorResultVector<void> LaunchResults;

		TCMap<CRepository *, TCSharedPointer<TCActorSequencer<void>>> DeleteLaunchSequencers;

		for (auto &[RepoBound, iSequence] : FilteredRepositories.f_GetAllRepos())
		{
			auto &Repo = RepoBound;
			auto *pRepo = &Repo;

			TCContinuation<void> Continuation;

			TCContinuation<TCVector<CStr>> RemotesContinuation;

			if (_Flags & ERepoCleanupTagsFlag_AllRemotes)
				RemotesContinuation = fg_GetRemotes(Launches, Repo);
			else
				RemotesContinuation.f_SetResult(TCVector<CStr>{});

			auto pDeleteLaunchSequencer = (DeleteLaunchSequencers[pRepo] = fg_Construct());

			auto RepoDoneScope = Launches.f_RepoDoneScope();

			Launches.f_Launch(Repo, {"tag"}) + RemotesContinuation
				> Continuation / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result, TCVector<CStr> &&_Remotes) mutable
				{
					TCSet<CStr> Remotes;
					Remotes.f_AddContainer(_Remotes);

					auto FullTags = _Result.f_GetStdOut().f_SplitLine();

					TCActorResultMap<CStr, CProcessLaunchActor::CSimpleLaunchResult> RemoteTagsResults;
					for (auto &Remote : Remotes)
						Launches.f_Launch(Repo, {"ls-remote", "--tags", Remote}) > RemoteTagsResults.f_AddResult(Remote);

					RemoteTagsResults.f_GetResults() > Continuation / [=](TCMap<CStr, TCAsyncResult<CProcessLaunchActor::CSimpleLaunchResult>> &&_RemoteTags)
						{
							TCActorResultVector<void> Results;

							TCSet<CStr> LoggedRemote;

							struct CTag
							{
								CStr f_GetName() const
								{
									if (!m_Remote)
										return m_Tag;
									return "{}/{}"_f << m_Remote << m_Tag;
								}

								CStr f_GetRef() const
								{
									if (m_Hash)
										return m_Hash;
									return "refs/tags/{}"_f << m_Tag;
								}

								bool operator < (CTag const &_Right) const
								{
									return fg_TupleReferences(m_Remote, m_Tag) < fg_TupleReferences(_Right.m_Remote, _Right.m_Tag);
								}

								CStr m_Remote;
								CStr m_Tag;
								CStr m_Hash;
							};

							TCSet<CTag> AllTags;

							TCSet<CStr> ReferencedTags;

							for (auto &LaunchResult : _RemoteTags)
							{
								auto &Remote = _RemoteTags.fs_GetKey(LaunchResult);
								if (!LaunchResult)
								{
									Launches.f_Output(EOutputType_Error, Repo, "{} - Failed to query tags: {}"_f << Remote << LaunchResult.f_GetExceptionStr());
									continue;
								}

								bool bWritable = true;
								if (Remote && Remote != "origin")
								{
									auto pRemote = Repo.m_Remotes.f_FindEqual(Remote);
									if (!pRemote)
									{
										if (_Flags & ERepoCleanupTagsFlag_Verbose)
											Launches.f_Output(EOutputType_Normal, Repo, "{}/* - remote not in build system"_f << Remote);
										bWritable = false;
									}
									else if (!pRemote->m_bCanPush)
									{
										if (_Flags & ERepoCleanupTagsFlag_Verbose)
											Launches.f_Output(EOutputType_Normal, Repo, "{}/* - no write access to remote"_f << Remote);
										bWritable = false;
									}
								}

								for (auto &Line : (*LaunchResult).f_GetStdOut().f_SplitLine())
								{
									CStr Hash;
									CStr Tag;
									aint nParsed = 0;
									(CStr::CParse("{}	refs/tags/{}") >> Hash >> Tag).f_Parse(Line, nParsed);
									if (nParsed == 2)
									{
										if (Tag.f_EndsWith("^{}"))
											Tag = Tag.f_Left(Tag.f_GetLen() - 3);

										if (bWritable)
											AllTags[CTag{Remote, Tag, Hash}].m_Hash = Hash;
										else
											ReferencedTags[Tag];
									}
								}
							}

							for (auto &Tag : _Result.f_GetStdOut().f_SplitLine())
							{
								if (Tag.f_IsEmpty())
									continue;
								if (ReferencedTags.f_FindEqual(Tag))
								{
									if (_Flags & ERepoCleanupTagsFlag_Verbose)
										Launches.f_Output(EOutputType_Normal, Repo, "{} - referenced by remote"_f << Tag);
									continue;
								}
								AllTags[CTag{"", Tag}];
							}

							for (auto &Tag : AllTags)
							{
								CStr CompareRemote = Tag.m_Remote ? Tag.m_Remote : "origin";

								if (!_Tags.f_IsEmpty())
								{
									bool bMatchedTag = false;
									for (auto &TagWildcard : _Tags)
									{
										if (fg_StrMatchWildcard(Tag.m_Tag.f_GetStr(), TagWildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
										{
											bMatchedTag = true;
											break;
										}
									}

									if (!bMatchedTag)
										continue;
								}

								bool bIsProtected = false;
								for (auto &TagWildcard : Repo.m_ProtectedTags)
								{
									if (fg_StrMatchWildcard(Tag.m_Tag.f_GetStr(), TagWildcard.f_GetStr()) == EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
									{
										bIsProtected = true;
										break;
									}
								}

								if (bIsProtected)
								{
									if (_Flags & ERepoCleanupTagsFlag_Verbose)
										Launches.f_Output(EOutputType_Normal, Repo, "{} - tag protected by Repository.ProtectedTags"_f << Tag.f_GetName());
									continue;
								}

								TCContinuation<void> Continuation;

								Launches.f_Launch(Repo, {"merge-base", "--is-ancestor", Tag.f_GetRef(), "{}/{}"_f << CompareRemote << Repo.m_DefaultBranch})
									+ Launches.f_Launch(Repo, {"log", "--oneline", "--cherry", "{}/{}...{}"_f << CompareRemote << Repo.m_DefaultBranch << Tag.f_GetRef()})
									> Continuation / [=](CProcessLaunchActor::CSimpleLaunchResult &&_Result, CProcessLaunchActor::CSimpleLaunchResult &&_ResultRebase) mutable
									{
										bool bIsInDefault = _Result.m_ExitCode == 0;
										CStr DeleteReason = "[ancestor]";
										if (!bIsInDefault)
										{
											bool bAllEquals = true;
											for (auto &Line : _ResultRebase.f_GetStdOut().f_Trim().f_SplitLine())
											{
												if (!Line.f_StartsWith("= "))
													bAllEquals = false;
											}
											if (bAllEquals)
											{
												DeleteReason = "[rebased]";
												bIsInDefault = true;
											}
										}
										if (!bIsInDefault && !(_Flags & ERepoCleanupBranchesFlag_Force))
										{
											Launches.f_Output(EOutputType_Error, Repo, "{} - not fully merged"_f << Tag.f_GetName());
											Continuation.f_SetResult();
											return;
										}

										if (!bIsInDefault)
											DeleteReason = "({}WARNING{} - forced)"_f << CColors::ms_StatusError << CColors::ms_Default;

										if (_Flags & ERepoCleanupTagsFlag_Pretend)
										{
											Launches.f_Output(EOutputType_Warning, Repo, "{} - would have deleted {}"_f << Tag.f_GetName() << DeleteReason);
											Continuation.f_SetResult();
											return;
										}

										*pDeleteLaunchSequencer > [=]() -> TCContinuation<void>
											{
												Launches.f_Output(EOutputType_Warning, Repo, "{} - deleting {}"_f << Tag.f_GetName() << DeleteReason);

												TCContinuation<void> Continuation;

												if (Tag.m_Remote)
													Continuation = Launches.f_Launch(Repo, {"push", Tag.m_Remote, "--delete", "refs/tags/{}"_f << Tag.m_Tag}, fg_LogAllFunctor());
												else
													Continuation = Launches.f_Launch(Repo, {"tag", "-d", Tag.m_Tag}, fg_LogAllFunctor());

												return Continuation;
											}
											> Continuation;
										;
									}
								;

								Continuation > Results.f_AddResult();
							}

							Results.f_GetResults() > Continuation / [=](TCVector<TCAsyncResult<void>> &&_Results)
								{
									if (!fg_CombineResults(Continuation, fg_Move(_Results)))
										return;

									(void)RepoDoneScope;
									Continuation.f_SetResult();
								}
							;
						}
					;
				}
			;

			Continuation.f_Dispatch() > LaunchResults.f_AddResult();
		}

		for (auto &Result : LaunchResults.f_GetResults().f_CallSync())
			Result.f_Access();

		return ERetry_None;
	}
}
