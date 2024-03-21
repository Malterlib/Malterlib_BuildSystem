// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Concurrency/ActorSequencerActor>
#include <Mib/CommandLine/TableRenderer>
#include <Mib/CommandLine/AnsiEncodingParse>

namespace NMib::NBuildSystem
{
	using namespace NRepository;

	namespace
	{
		struct CRepoStatusState
		{
			bool m_bIsChange = false;
			CStr m_ToPush;
			CStr m_ToPull;

			TCVector<CLocalFileChange> m_LocalChanges;
			TCVector<CStr> m_Remotes;
			CGitBranches m_LocalBranches;
			CGitBranches m_RemoteBranches;

			bool f_HasRemoteBranch(CStr const &_FullBranch) const
			{
				return m_RemoteBranches.m_Branches.f_FindEqual(_FullBranch);
			}
		};

		struct CMessage
		{
			CStr m_Branch;
			CStr m_Description;
			CStr m_Verbose;
		};

		struct CCompareMessage
		{
			EOutputType m_OutputType = EOutputType_Normal;
			CStr m_Branch;
			TCVector<CMessage> m_LocalMessages;
			TCVector<CMessage> m_OriginMessages;
			TCVector<CMessage> m_OtherMessages;
			bool m_bIsCurrentBranch = false;
		};

		struct CRepoResult
		{
			CStr m_RepoName;
			CRepository m_Repo;
			TCVector<CCompareMessage> m_CompareMessages;
			mint m_iSequence = 0;
			bool m_bActionNeeded = false;
			bool m_bOpenEditor = false;
		};

		struct CBranchResult
		{
			CCompareMessage m_Message;
			bool m_bActionNeeded = false;
			bool m_bShouldShow = false;
		};

		struct COutStatusTableParams
		{
			TCSharedPointer<CCommandLineControl> m_pCommandLine;
			TCVector<CRepoResult> m_Results;
			bool m_bIsVerbose;
		};

		TCFuture<void> fg_OutputStatusTable(COutStatusTableParams _Params)
		{
			CColors Colors(_Params.m_pCommandLine->m_AnsiFlags);

			auto TableRenderer = _Params.m_pCommandLine->f_TableRenderer();
			TableRenderer.f_SetOptions(CTableRenderHelper::EOption_AvoidRowSeparators | CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_NoExtraLines);

			CTableRenderHelper::CColumnHelper Columns(0);
			Columns.f_AddHeading("Repo", 0);
			Columns.f_AddHeading("Branch", 0);

			if (_Params.m_bIsVerbose)
				Columns.f_AddHeading("Local and branch on origin", 0);
			else
				Columns.f_AddHeading("Local", 0);

			Columns.f_AddHeading("Branch on origin", 0);
			Columns.f_AddHeading("Others", 0);

			TableRenderer.f_AddHeadings(&Columns);

			TCVector<CRepoResult const *> SortedResults;
			for (auto &Result : _Params.m_Results)
				SortedResults.f_Insert(&Result);

			SortedResults.f_Sort
				(
					[&](CRepoResult const *_pLeft, CRepoResult const *_pRight)
					{
						return _pLeft->m_RepoName <=> _pRight->m_RepoName;
					}
				)
			;

			bool bHasLocalMessages = false;
			bool bHasOriginMessages = false;
			bool bHasOtherMessages = false;

			for (auto &pResult : SortedResults)
			{
				auto &Result = *pResult;

				bool bHasSeveralRows = Result.m_CompareMessages.f_GetLen() > 1u;
				if (bHasSeveralRows)
					TableRenderer.f_ForceRowSeparator();

				mint MaxBranchNameLenLocal = 0;
				mint MaxBranchNameLenOrigin = 0;
				mint MaxBranchNameLenOther = 0;

				for (auto &Message : Result.m_CompareMessages)
				{
					auto LocalMessages = Message.m_LocalMessages;
					auto OriginMessages = Message.m_OriginMessages;

					if (_Params.m_bIsVerbose)
					{
						LocalMessages.f_Insert(fg_Move(OriginMessages));
						OriginMessages.f_Clear();
					}

					auto fMeasureMessages = [&](TCVector<CMessage> const &_Messages, mint &o_MaxLen)
						{
							for (auto &Message : _Messages)
							{
								if (!Message.m_Branch)
									continue;
								o_MaxLen = fg_Max(o_MaxLen, CAnsiEncodingParse::fs_RenderedStrLen(Message.m_Branch) + 1);
							}
						}
					;
					fMeasureMessages(LocalMessages, MaxBranchNameLenLocal);
					fMeasureMessages(OriginMessages, MaxBranchNameLenOrigin);
					fMeasureMessages(Message.m_OtherMessages, MaxBranchNameLenOther);
				}

				for (auto &Message : Result.m_CompareMessages)
				{
					CStr RepoColor;
					switch (Message.m_OutputType)
					{
					case EOutputType_Normal: RepoColor = Colors.f_StatusNormal(); break;
					case EOutputType_Warning: RepoColor = Colors.f_StatusWarning(); break;
					case EOutputType_Error: RepoColor = Colors.f_StatusError(); break;
					}

					CStr ReplacedRepo = Result.m_RepoName.f_Replace("/", "{}{}/{}"_f << Colors.f_Default() << Colors.f_Foreground256(250) << RepoColor ^ 1);

					auto LocalMessages = Message.m_LocalMessages;
					auto OriginMessages = Message.m_OriginMessages;

					if (_Params.m_bIsVerbose)
					{
						LocalMessages.f_Insert(fg_Move(OriginMessages));
						OriginMessages.f_Clear();
					}

					if (!LocalMessages.f_IsEmpty())
						bHasLocalMessages = true;

					if (!OriginMessages.f_IsEmpty())
						bHasOriginMessages = true;

					if (!Message.m_OtherMessages.f_IsEmpty())
						bHasOtherMessages = true;

					auto fFormatMessages = [&](TCVector<CMessage> const &_Messages, mint _MaxLen) -> CStr
						{
							TCVector<CStr> Return;

							for (auto &Message : _Messages)
							{
								Return.f_Insert
									(
										"{}{sf ,sj*}{}"_f << Message.m_Branch << "" << (_MaxLen - CAnsiEncodingParse::fs_RenderedStrLen(Message.m_Branch)) << Message.m_Description
									)
								;
								if (Message.m_Verbose)
									Return.f_Insert(Message.m_Verbose.f_Indent("   ").f_SplitLine());
							}

							return CStr::fs_Join(Return, "\n");
						}
					;

					TableRenderer.f_AddRow
						(
							"{}{}{}"_f << RepoColor << ReplacedRepo << Colors.f_Default()
							, "{}{}{}{}"_f << Colors.f_BranchName() << (Message.m_bIsCurrentBranch ? "*" : " ") << Message.m_Branch << Colors.f_Default()
							, fFormatMessages(LocalMessages, MaxBranchNameLenLocal)
							, fFormatMessages(OriginMessages, MaxBranchNameLenOrigin)
							, fFormatMessages(Message.m_OtherMessages, MaxBranchNameLenOther)
						)
					;
				}
				if (bHasSeveralRows)
					TableRenderer.f_ForceRowSeparator();
			}

			if (!bHasLocalMessages)
			{
				if (_Params.m_bIsVerbose)
					Columns.f_SetVerbose("Local and branch on origin");
				else
					Columns.f_SetVerbose("Local");
			}

			if (!bHasOriginMessages)
				Columns.f_SetVerbose("Branch on origin");

			if (!bHasOtherMessages)
				Columns.f_SetVerbose("Others");

			TableRenderer.f_Output(CTableRenderHelper::EOutputType_HumanReadable);
			co_return {};
		}
	}

	TCFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_Status
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, ERepoStatusFlag _Flags
			, TCVector<CStr> const &_HideBranches
			, TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		co_await (ECoroutineFlag_AllowReferences | ECoroutineFlag_CaptureMalterlibExceptions);

		TCSharedPointer<CGenerateEphemeralState> pGenerateState = fg_Construct();

		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, *pGenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		CRepoFilter Filter = _Filter;

		EFilterRepoFlag FilterFlags = EFilterRepoFlag_IncludePull;

		if (_Flags & CBuildSystem::ERepoStatusFlag_OnlyTracked)
			FilterFlags |= EFilterRepoFlag_OnlyTracked;

		CFilteredRepos FilteredRepositories = co_await fg_GetFilteredRepos(Filter, *this, mp_Data, FilterFlags);

		if (_Flags & ERepoStatusFlag_UpdateRemotes)
			co_await fg_UpdateRemotes(*this, FilteredRepositories);

		CGitLaunches Launches{f_GetBaseDir(), "Getting repo status", mp_AnsiFlags, mp_fOutputConsole, f_GetCancelledPointer()};
		auto DestroyLaunchs = co_await co_await Launches.f_Init();

		CColors Colors(mp_AnsiFlags);

		Launches.f_MeasureRepos(FilteredRepositories.m_FilteredRepositories);

		bool bOpenEditor = _Flags & ERepoStatusFlag_OpenEditor;
		bool bUseDefaultUpstream = _Flags & ERepoStatusFlag_UseDefaultUpstreamBranch;
		bool bIsVerbose = _Flags & ERepoStatusFlag_Verbose;

		TCActorResultVector<CRepoResult> RepoResults;

		auto AllRepos = FilteredRepositories.f_GetAllRepos();

		for (auto &[Repo, iSequence] : AllRepos)
		{
			g_Dispatch / [=, Repo = Repo, iSequence = iSequence]() -> TCFuture<CRepoResult>
				{
					auto [LocalChanges, Remotes, LocalBranches, RemoteBranches] = co_await 
						(
							fg_GetLocalFileChanges(Launches, Repo, !(_Flags & ERepoStatusFlag_OnlyTracked))
							+ fg_GetRemotes(Launches, Repo)
							+ fg_GetBranches(Launches, Repo, false)
							+ fg_GetBranches(Launches, Repo, true)
						)
					;

					TCSharedPointer<CRepoStatusState> pState = fg_Construct();

					auto &State = *pState;

					State.m_LocalChanges = fg_Move(LocalChanges);
					State.m_Remotes = fg_Move(Remotes);
					State.m_LocalBranches = fg_Move(LocalBranches);
					State.m_RemoteBranches = fg_Move(RemoteBranches);

					TCSet<CStr> Branches;

					if (_Flags & ERepoStatusFlag_AllBranches)
						Branches = State.m_LocalBranches.m_Branches;
					else if (!State.m_LocalBranches.m_Current.f_IsEmpty())
						Branches = {State.m_LocalBranches.m_Current};

					TCActorResultVector<CBranchResult> BranchResults;
					for (auto &Branch : Branches)
					{
						g_Dispatch / [=]() -> TCFuture<CBranchResult>
							{
								CBranchResult BranchResult;
								TCActorResultMap<CStr, TCVector<CLogEntry>> ToPush;
								TCActorResultMap<CStr, TCVector<CLogEntry>> ToPull;
								TCSet<CStr> ToPushMissing;
								TCSet<CStr> ToPullAdded;

								for (auto &Remote : State.m_Remotes)
								{
									CStr RemoteBranch = "{}/{}"_f << Remote << Branch;
									CStr PullRemoteBranch = RemoteBranch;
									CStr PullRemoteBranchName = Remote;
									CStr MissingRemoteBranch = RemoteBranch;

									CStr DefaultUpstreamBranch = Repo.m_DefaultUpstreamBranch;

									auto *pRemote = Repo.m_Remotes.f_FindEqual(Remote);
									if (pRemote && pRemote->m_DefaultBranch)
										DefaultUpstreamBranch = pRemote->m_DefaultBranch;

									if (bUseDefaultUpstream && Branch == Repo.m_DefaultBranch && !DefaultUpstreamBranch.f_IsEmpty() && !State.f_HasRemoteBranch(RemoteBranch))
									{
										PullRemoteBranch = "{}/{}"_f << Remote << DefaultUpstreamBranch;
										PullRemoteBranchName = PullRemoteBranch;
									}

									if (Branch == Repo.m_DefaultBranch && !DefaultUpstreamBranch.f_IsEmpty() && !State.f_HasRemoteBranch(RemoteBranch))
										MissingRemoteBranch = "{}/{}"_f << Remote << DefaultUpstreamBranch;

									if (!(_Flags & ERepoStatusFlag_NonDefaultToAll) && Remote != "origin" && Branch != Repo.m_DefaultBranch)
										;
									else
									{
										if (State.f_HasRemoteBranch(RemoteBranch))
											fg_GetLogEntries(Launches, Repo, RemoteBranch, Branch) > ToPush.f_AddResult(Remote);
										else if ((Remote == "origin" && Branch == Repo.m_DefaultBranch) || !State.f_HasRemoteBranch(MissingRemoteBranch))
											ToPushMissing[Remote];

										if (State.f_HasRemoteBranch(PullRemoteBranch))
										{
											if (ToPullAdded(PullRemoteBranchName).f_WasCreated())
												fg_GetLogEntries(Launches, Repo, Branch, PullRemoteBranch) > ToPull.f_AddResult(PullRemoteBranchName);
										}
									}

									CStr DefaultBranch = Repo.m_DefaultBranch;
									if (pRemote && pRemote->m_DefaultBranch)
										DefaultBranch = pRemote->m_DefaultBranch;

									if (Branch != DefaultBranch && !DefaultBranch.f_IsEmpty())
									{
										CStr AgainstDefaultName = "{}/{}"_f << Remote << DefaultBranch;
										CStr ExtraRemoteBranch = "{}/{}"_f << Remote << DefaultBranch;
										if (State.f_HasRemoteBranch(ExtraRemoteBranch))
										{
											fg_GetLogEntries(Launches, Repo, ExtraRemoteBranch, Branch) > ToPush.f_AddResult(AgainstDefaultName);
											if (ToPullAdded(AgainstDefaultName).f_WasCreated())
												fg_GetLogEntries(Launches, Repo, Branch, ExtraRemoteBranch) > ToPull.f_AddResult(AgainstDefaultName);
										}
									}
								}

								auto [ToPushResults, ToPullResults] = co_await
									(
										ToPush.f_GetResults()
										+ ToPull.f_GetResults()
									)
								;

								TCMap<CStr, TCVector<CLogEntry>> ToRemotePush;
								TCMap<CStr, TCVector<CLogEntry>> ToLocalPull;

								TCMap<TCVector<CStr>, TCTuple<TCVector<CStr>, TCVector<CLogEntry>>> ToRemotePushByHashes;
								TCMap<TCVector<CStr>, TCTuple<TCVector<CStr>, TCVector<CLogEntry>>> ToLocalPullByHashes;

								auto fGetHashes = [](TCVector<CLogEntry> const &_LogEntries)
									{
										TCVector<CStr> Hashes;
										for (auto &Entry : _LogEntries)
											Hashes.f_Insert(Entry.m_Hash);

										return Hashes;
									}
								;

								TCPromise<void> CombineResultsPromise;
								if
									(
										!fg_CombineResults
										(
											CombineResultsPromise
											, fg_Move(ToPushResults)
											, [&](CStr const &_Remote, TCVector<CLogEntry> &&_Log)
											{
												auto &[Remotes, LogEntries] = ToRemotePushByHashes[fGetHashes(_Log)];
												Remotes.f_Insert(_Remote);
												LogEntries = fg_Move(_Log);
											}
										)
									)
								{
									auto Result = CombineResultsPromise.f_MoveResult();
									CStr Error = Result.f_GetExceptionStr();
									Launches.f_Output(EOutputType_Error, Repo, "Error getting to push for branch '{}': {}"_f << Branch << Error);
									co_return Result.f_GetException();
								}

								if
									(
										!fg_CombineResults
										(
											CombineResultsPromise
											, fg_Move(ToPullResults)
											, [&](CStr const &_Remote, TCVector<CLogEntry> &&_Log)
											{
												auto &[Remotes, LogEntries] = ToLocalPullByHashes[fGetHashes(_Log)];
												Remotes.f_Insert(_Remote);
												LogEntries = fg_Move(_Log);
											}
										)
									)
								{
									auto Result = CombineResultsPromise.f_MoveResult();
									CStr Error = Result.f_GetExceptionStr();
									Launches.f_Output(EOutputType_Error, Repo, "Error getting to pull for branch '{}': {}"_f << Branch << Error);
									co_return Result.f_GetException();
								}

								auto fJoinNames = [](TCSet<CStr> const &_Names) -> CStr
									{
										if (_Names.f_GetLen() == 1)
											return *_Names.f_FindSmallest();
										return "<{}>"_f << CStr::fs_Join(_Names, ", ");
									}
								;
								auto fPriority = [](mint _Priority) -> CStr
									{
										return "{sj*}"_f << "" << _Priority;
									}
								;

								auto fHandleRemotes = [&](auto &o_Entries, auto &_SourceEntries, bool _bOtherBranches)
									{
										for (auto &[Remotes, LogEntries] : _SourceEntries)
										{
											TCSet<CStr> RemotesForName;
											TCSet<CStr> Branches;
											for (auto &RemoteBranch : Remotes)
											{
												if (_bOtherBranches != RemoteBranch.f_FindChar('/') >= 0)
													continue;

												if (_bOtherBranches)
												{
													CStr Branch = RemoteBranch;
													CStr Remote = fg_GetStrSep(Branch, "/");
													RemotesForName[Remote];
													Branches[Branch];
												}
												else
													RemotesForName[RemoteBranch];
											}

											if (Branches.f_IsEmpty() && RemotesForName.f_IsEmpty())
												continue;

											CStr RemoteName = fJoinNames(RemotesForName);

											if (!Branches.f_IsEmpty())
											{
												RemoteName += "/";
												RemoteName += fJoinNames(Branches);
											}

											if (!_bOtherBranches)
												RemoteName = fPriority(12) + RemoteName;
											else
												RemoteName = fPriority(2) + RemoteName;

											o_Entries[RemoteName] = LogEntries;
										}
									}
								;

								fHandleRemotes(ToRemotePush, ToRemotePushByHashes, false);
								fHandleRemotes(ToRemotePush, ToRemotePushByHashes, true);
								fHandleRemotes(ToLocalPull, ToLocalPullByHashes, false);
								fHandleRemotes(ToLocalPull, ToLocalPullByHashes, true);

								bool bHasRemotes = !ToRemotePush.f_IsEmpty() || !ToLocalPull.f_IsEmpty();
								bool bIsChanged = !ToPushMissing.f_IsEmpty();

								if ((_Flags & ERepoStatusFlag_NeedActionOnPush) && !ToPushMissing.f_IsEmpty())
									BranchResult.m_bActionNeeded = true;

								bool bIsCurrentBranch = Branch == State.m_LocalBranches.m_Current;

								enum EMessageType
								{
									EMessageType_Local
									, EMessageType_Origin
									, EMessageType_Other
								};

								auto fRemoteNameToMessageType = [](CStr const &_Str)
									{
										if (_Str.f_FindChar('/') >= 0)
											return EMessageType_Other;
										else
											return EMessageType_Origin;
									}
								;

								auto fBranchHidden = [&](CStr const &_Name) -> bool
									{
										return NStr::fg_StrMatchesAnyWildcardInContainer(_Name, _HideBranches);
									}
								;

								auto fAddMessage = [&](CMessage &&_Message, EMessageType _Type, CStr const &_BranchName)
									{
										if (_Type == EMessageType_Other && fBranchHidden(_BranchName))
											return;

										if (bIsVerbose)
											return;

										switch (_Type)
										{
										case EMessageType_Local: BranchResult.m_Message.m_LocalMessages.f_Insert(fg_Move(_Message)); break;
										case EMessageType_Origin: BranchResult.m_Message.m_OriginMessages.f_Insert(fg_Move(_Message)); break;
										case EMessageType_Other: BranchResult.m_Message.m_OtherMessages.f_Insert(fg_Move(_Message)); break;
										}
									}
								;

								if (bIsCurrentBranch && !State.m_LocalChanges.f_IsEmpty())
								{
									bIsChanged = true;
									if (_Flags & ERepoStatusFlag_NeedActionOnLocalChanges)
										BranchResult.m_bActionNeeded = true;

									fAddMessage
										(
											CMessage
											{
												.m_Branch = ""
												, .m_Description = "{}Commit {}{}"_f
												<< Colors.f_ToCommit()
												<< Colors.f_Default()
												<< State.m_LocalChanges.f_GetLen()
											}
											, EMessageType_Local
											, ""
										)
									;
								}

								if (!ToPushMissing.f_IsEmpty())
								{
									CStr NewMissing = fPriority(10) + fJoinNames(ToPushMissing);
									ToPushMissing = {NewMissing};
								}

								TCSet<CStr> RemotesWithAction = ToPushMissing;

								TCSet<CStr> NoPull;
								TCSet<CStr> NoPush;

								for (auto &ToPush : ToRemotePush)
								{
									if (!ToPush.f_IsEmpty())
									{
										bIsChanged = true;
										if (_Flags & ERepoStatusFlag_NeedActionOnPush)
											BranchResult.m_bActionNeeded = true;
										RemotesWithAction[ToRemotePush.fs_GetKey(ToPush)];
									}
									else
										NoPush[ToRemotePush.fs_GetKey(ToPush)];
								}

								for (auto &ToPull : ToLocalPull)
								{
									if (!ToPull.f_IsEmpty())
									{
										bIsChanged = true;
										if (_Flags & ERepoStatusFlag_NeedActionOnPull)
											BranchResult.m_bActionNeeded = true;
										RemotesWithAction[ToLocalPull.fs_GetKey(ToPull)];
									}
									else
										NoPull[ToRemotePush.fs_GetKey(ToPull)];
								}

								TCSet<CStr> NoPullNoPush;
								for (auto &Remote : NoPull)
								{
									if (NoPush.f_FindEqual(Remote))
									{
										if (Remote.f_FindChar('/') >= 0)
											NoPullNoPush[fPriority(1) + Remote];
										else
											NoPullNoPush[fPriority(11) + Remote];
									}
								}

								RemotesWithAction += NoPullNoPush;

								for (auto &RemoteName : RemotesWithAction)
								{
									auto TrimmedRemoteName = RemoteName.f_Trim();

									TCVector<CStr> RemoteMessages;
									auto pToPush = ToRemotePush.f_FindEqual(RemoteName);
									auto pToPull = ToLocalPull.f_FindEqual(RemoteName);
									bool bMissing = ToPushMissing.f_FindEqual(RemoteName);
									bool bNoPullNoPush = NoPullNoPush.f_FindEqual(RemoteName);

									if (pToPush && !pToPush->f_IsEmpty())
									{
										RemoteMessages.f_Insert
											(
												"{}Push {}{}"_f
												<< Colors.f_ToPush()
												<< Colors.f_Default()
												<< pToPush->f_GetLen()
											)
										;
									}

									if (bMissing)
									{
										RemoteMessages.f_Insert
											(
												"{}Push {}?"_f
												<< Colors.f_ToPush()
												<< Colors.f_Default()
											)
										;
									}

									if (pToPull && !pToPull->f_IsEmpty())
									{
										RemoteMessages.f_Insert
											(
												"{}Pull {}{}"_f
												<< Colors.f_ToPull()
												<< Colors.f_Default()
												<< pToPull->f_GetLen()
											)
										;
									}

									CStr Description;

									if (bNoPullNoPush)
									{
										Description = "{}Same{}"_f
											<< Colors.f_Foreground256(248)
											<< Colors.f_Default()
										;
									}
									else
										Description = CStr::fs_Join(RemoteMessages, " ");

									if (TrimmedRemoteName == "origin")
										fAddMessage(CMessage{.m_Branch = "", .m_Description = Description}, fRemoteNameToMessageType(TrimmedRemoteName), TrimmedRemoteName);
									else
									{
										fAddMessage
											(
												CMessage
												{
													.m_Branch = "{}{}{}"_f
													<< Colors.f_ChangedBranchName(TrimmedRemoteName)
													<< TrimmedRemoteName
													<< Colors.f_Default()
													, .m_Description = Description
												}
												, fRemoteNameToMessageType(TrimmedRemoteName)
												, TrimmedRemoteName
											)
										;
									}
								}

								if (!bIsChanged && !(_Flags & ERepoStatusFlag_ShowUnchanged))
									co_return fg_Move(BranchResult);

								BranchResult.m_bShouldShow = true;

								EOutputType OutputType = EOutputType_Normal;

								if (bIsChanged)
									OutputType = EOutputType_Error;
								else if (!bHasRemotes)
									OutputType = EOutputType_Warning;

								BranchResult.m_Message.m_OutputType = OutputType;
								BranchResult.m_Message.m_Branch = Branch;
								BranchResult.m_Message.m_bIsCurrentBranch = bIsCurrentBranch;

								if (!bIsVerbose)
									co_return fg_Move(BranchResult);

								auto fAddVerboseMessage = [&](CMessage &&_Message, EMessageType _Type, CStr const &_BranchName)
									{
										if (_Type == EMessageType_Other && fBranchHidden(_BranchName))
											return;

										switch (_Type)
										{
										case EMessageType_Local: BranchResult.m_Message.m_LocalMessages.f_Insert(fg_Move(_Message)); break;
										case EMessageType_Origin: BranchResult.m_Message.m_OriginMessages.f_Insert(fg_Move(_Message)); break;
										case EMessageType_Other: BranchResult.m_Message.m_OtherMessages.f_Insert(fg_Move(_Message)); break;
										}
									}
								;

								if (bIsCurrentBranch && !State.m_LocalChanges.f_IsEmpty())
								{
									CMessage Message;
									Message.m_Branch = "{}Local{}"_f
										<< Colors.f_ChangedBranchName("Local")
										<< Colors.f_Default()
									;

									Message.m_Description = "{}To Commit{}"_f
										<< Colors.f_ToCommit()
										<< Colors.f_Default()
									;

									for (auto &Change : State.m_LocalChanges)
									{
										fg_AddStrSep
											(
												Message.m_Verbose
												, "{}{}{} {}"_f
												<< Colors.f_ToCommit()
												<< Change.m_ChangeType
												<< Colors.f_Default()
												<< Change.m_File
												, "\n"
											)
										;
									}

									fAddVerboseMessage(fg_Move(Message), EMessageType_Local, "");
								}

								for (auto &ToPush : ToRemotePush)
								{
									if (ToPush.f_IsEmpty())
										continue;

									auto &RemoteName = ToRemotePush.fs_GetKey(ToPush);
									auto TrimmedRemoteName = RemoteName.f_Trim();

									CMessage Message;
									Message.m_Branch = "{}{}{}"_f
										<< Colors.f_ChangedBranchName(TrimmedRemoteName)
										<< TrimmedRemoteName
										<< Colors.f_Default()
									;

									Message.m_Description = "{}To Push{}"_f
										<< Colors.f_ToPush()
										<< Colors.f_Default()
									;

									for (auto &Commit : ToPush)
									{
										fg_AddStrSep
											(
												Message.m_Verbose
												, "{}{}{} {}"_f
												<< Colors.f_ToPush()
												<< Commit.m_Hash
												<< Colors.f_Default()
												<< Commit.m_Description
												, "\n"
											)
										;
									}

									fAddVerboseMessage(fg_Move(Message), fRemoteNameToMessageType(TrimmedRemoteName), TrimmedRemoteName);
								}

								for (auto &RemoteName : ToPushMissing)
								{
									auto TrimmedRemoteName = RemoteName.f_Trim();

									CMessage Message;
									Message.m_Branch = "{}{}{}"_f
										<< Colors.f_ChangedBranchName(TrimmedRemoteName)
										<< TrimmedRemoteName
										<< Colors.f_Default()
									;

									Message.m_Description = "{}To Push{}"_f
										<< Colors.f_ToPush()
										<< Colors.f_Default()
									;

									Message.m_Verbose = "{}???????{} Branch missing on remote"_f
										<< Colors.f_ToPush()
										<< Colors.f_Default()
									;

									fAddVerboseMessage(fg_Move(Message), fRemoteNameToMessageType(TrimmedRemoteName), TrimmedRemoteName);
								}

								for (auto &ToPull : ToLocalPull)
								{
									if (ToPull.f_IsEmpty())
										continue;

									auto &RemoteName = ToLocalPull.fs_GetKey(ToPull);
									auto TrimmedRemoteName = RemoteName.f_Trim();

									CMessage Message;

									Message.m_Branch = "{}{}{}"_f
										<< Colors.f_ChangedBranchName(TrimmedRemoteName)
										<< TrimmedRemoteName
										<< Colors.f_Default()
									;

									Message.m_Description = "{}To Pull{}"_f
										<< Colors.f_ToPull()
										<< Colors.f_Default()
									;

									for (auto &Commit : ToPull)
									{
										fg_AddStrSep
											(
												Message.m_Verbose
												, "{}{}{} {}"_f
												<< Colors.f_ToPull()
												<< Commit.m_Hash
												<< Colors.f_Default()
												<< Commit.m_Description
												, "\n"
											)
										;
									}

									fAddVerboseMessage(fg_Move(Message), fRemoteNameToMessageType(TrimmedRemoteName), TrimmedRemoteName);
								}

								co_return fg_Move(BranchResult);
							}
							> BranchResults.f_AddResult()
						;
					}

					auto Results = co_await BranchResults.f_GetUnwrappedResults();

					CRepoResult ReturnResult{.m_RepoName = Launches.f_GetRepoName(Repo), .m_Repo = Repo, .m_iSequence = iSequence};

					for (auto &BranchResult : Results)
					{
						ReturnResult.m_bActionNeeded = ReturnResult.m_bActionNeeded || BranchResult.m_bActionNeeded;

						if (BranchResult.m_bShouldShow)
							ReturnResult.m_CompareMessages.f_Insert(fg_Move(BranchResult.m_Message));
					}

					if (ReturnResult.m_bActionNeeded && bOpenEditor)
						ReturnResult.m_bOpenEditor = true;

					Launches.f_RepoDone();
					co_return fg_Move(ReturnResult);
				}
				> RepoResults.f_AddResult()
			;
		}

		auto Results = co_await (co_await RepoResults.f_GetResults() | g_Unwrap);

		co_await fg_OutputStatusTable({.m_pCommandLine = _pCommandLine, .m_Results = Results, .m_bIsVerbose = bIsVerbose});

		TCMap<mint, TCVector<CRepository>> EditorsToLaunch;
		bool bActionNeeded = false;
		for (auto &Result : Results)
		{
			if (Result.m_bOpenEditor)
				EditorsToLaunch[Result.m_iSequence].f_Insert(Result.m_Repo);

			bActionNeeded = bActionNeeded || Result.m_bActionNeeded;
		}

		if (!EditorsToLaunch.f_IsEmpty())
		{
			CRepoEditor RepoEditor = fg_GetRepoEditor(*this, mp_Data);

			{
				CSequencer EditorLaunchSequencer("BuildSystem Action Repository Status EditorLaunchSequencer", RepoEditor.m_bOpenSequential ? 1 : 16);

				auto AsyncDestroy = co_await fg_AsyncDestroyLogError(EditorLaunchSequencer);

				for (auto &EditorLaunches : EditorsToLaunch)
				{
					TCActorResultVector<void> EditorLaunchResults;
					for (auto &Repo : EditorLaunches)
					{
						EditorLaunchSequencer.f_RunSequenced
							(
								g_ActorFunctorWeak / [=](CActorSubscription &&_Subscription) -> TCFuture<void>
								{
									auto Result = co_await Launches.f_OpenRepoEditor(RepoEditor, Repo.m_Location);
									if (Result.m_ExitCode)
									{
										Launches.f_Output
											(
												EOutputType_Error
												, Repo
												, "Failed to launch repository editor: {}"_f
												<< Result.f_GetCombinedOut().f_Trim()
											)
										;
									}

									(void)_Subscription;

									co_return {};
								}
							)
							> EditorLaunchResults.f_AddResult();
						;

					}
					co_await (co_await EditorLaunchResults.f_GetResults() | g_Unwrap);
				}
			}

			co_await g_AsyncDestroy;
		}

		co_await fg_Move(DestroyLaunchs);

		if (bActionNeeded)
			f_OutputConsole("\a", true);

		co_return ERetry_None;
	}
}
