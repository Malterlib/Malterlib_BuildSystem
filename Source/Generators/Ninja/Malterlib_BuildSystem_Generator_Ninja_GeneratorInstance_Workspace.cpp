// Copyright © 2026 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Ninja.h"
#include <Mib/Process/Platform>
#include <Mib/Process/ProcessLaunch>
#include <Mib/Encoding/Json>
#include <Mib/String/MultiReplace>

namespace NMib::NBuildSystem::NNinja
{
	namespace
	{
		bool fg_IsCompileType(CStr const &_Type)
		{
			return _Type == gc_Str<"C++">.m_Str || _Type == gc_Str<"C">.m_Str || _Type == gc_Str<"ObjC">.m_Str || _Type == gc_Str<"ObjC++">.m_Str;
		}

		// Escape an environment variable value for ninja's environment block format.
		// In this format: \ becomes \\, and newline becomes \n (backslash-n literal)
		void fg_EscapeEnvironmentValue(CStr::CAppender &_Appender, CStr const &_Value)
		{
			auto *pParse = _Value.f_GetStr();
			while (*pParse)
			{
				auto Char = *pParse;
				if (Char == '\\')
					_Appender += "\\\\";
				else if (Char == '\n')
					_Appender += "\\n";
				else
					_Appender += Char;

				++pParse;
			}
		}

		// Parse ninja raw command format (command_raw = true):
		// - Arguments are split by whitespace
		// - Strings can be quoted with " or '
		// - Backslash escapes: \" \' \\ within quoted strings
		TCVector<CStr> fg_ParseNinjaRawCommand(CStr const &_Command)
		{
			TCVector<CStr> Args;
			ch8 const *pParse = _Command.f_GetStr();

			while (*pParse)
			{
				// Skip whitespace
				while (*pParse && (*pParse == ' ' || *pParse == '\t'))
					++pParse;

				if (!*pParse)
					break;

				CStr Arg;

				if (*pParse == '"')
				{
					// Double-quoted string
					++pParse;
					while (*pParse && *pParse != '"')
					{
						if (*pParse == '\\' && (pParse[1] == '"' || pParse[1] == '\\'))
						{
							++pParse;
							Arg.f_AddChar(*pParse);
						}
						else
							Arg.f_AddChar(*pParse);

						++pParse;
					}
					if (*pParse == '"')
						++pParse;
				}
				else if (*pParse == '\'')
				{
					// Single-quoted string
					++pParse;
					while (*pParse && *pParse != '\'')
					{
						if (*pParse == '\\' && (pParse[1] == '\'' || pParse[1] == '\\'))
						{
							++pParse;
							Arg.f_AddChar(*pParse);
						}
						else
							Arg.f_AddChar(*pParse);

						++pParse;
					}
					if (*pParse == '\'')
						++pParse;
				}
				else
				{
					// Unquoted argument - read until whitespace
					while (*pParse && *pParse != ' ' && *pParse != '\t')
					{
						Arg.f_AddChar(*pParse);
						++pParse;
					}
				}

				Args.f_Insert(fg_Move(Arg));
			}

			return Args;
		}
	}

	void fg_EscapeNinjaString(CStr::CAppender &_Appender, CStr const &_String, bool _bEscapeSpace)
	{
		{
			auto *pParse = _String.f_GetStr();
			while (*pParse)
			{
				auto Char = *pParse;
				if (Char == '\n')
					_Appender += "$^$\n";
				else if ((Char == ' ' && _bEscapeSpace) || Char == ':' || Char == '$')
				{
					_Appender += '$';
					_Appender += Char;
				}
				else
					_Appender += Char;

				++pParse;
			}
		}
	}

	void fg_AddNinjaArray(CStr::CAppender &_Appender, TCVector<CStr> const &_Array, CStr const &_Divider = {})
	{
		if (_Array.f_IsEmpty())
			return;

		if (_Divider)
		{
			_Appender += _Divider;
			_Appender += " ";
		}

		for (auto &Output : _Array)
		{
			fg_EscapeNinjaString(_Appender, Output, true);
			_Appender += ' ';
		}
	}

	void fg_AddNinjaArrayFiltered(CStr::CAppender &_Appender, TCVector<CStr> const &_Array, TCSet<CStr> const &_Filter, CStr const &_Divider = {})
	{
		bool bFirst = true;
		for (auto &Item : _Array)
		{
			if (_Filter.f_FindEqual(Item))
				continue;

			if (bFirst)
			{
				if (_Divider)
				{
					_Appender += _Divider;
					_Appender += " ";
				}
				bFirst = false;
			}

			fg_EscapeNinjaString(_Appender, Item, true);
			_Appender += ' ';
		}
	}

	TCUnsafeFuture<void> CGeneratorInstance::f_GenerateWorkspaceFile(CWorkspace &_Workspace, CStr const &_OutputDir) const
	{
		co_await ECoroutineFlag_CaptureExceptions;

		CGeneratorSettings WorkspaceSettings;

		co_await g_Yield;
		co_await m_BuildSystem.f_CheckCancelled();

		co_await WorkspaceSettings.f_PopulateSettings(gc_ConstKey_GeneratorSetting_Workspace, EPropertyType_Workspace, m_BuildSystem, _Workspace.m_EnabledConfigs);

		co_await g_Yield;

		CStr WorkspacedOutputDir = _OutputDir / _Workspace.m_Name;

		for (auto &Project : _Workspace.m_Projects)
		{
			co_await f_GenerateProjectFile(Project);

			co_await g_Yield;
			co_await m_BuildSystem.f_CheckCancelled();
		}

		co_await fg_ParallelForEach
			(
				_Workspace.m_WorkspaceInfos
				, [&](TCUniquePointer<CWorkspaceInfo> &_pWorkspaceInfo) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureExceptions;

					auto &Config = _Workspace.m_WorkspaceInfos.fs_GetKey(_pWorkspaceInfo);
					auto *pEntity = _Workspace.m_EnabledConfigs.f_FindEqual(Config);
					if (!pEntity)
						co_return {};

					NTime::CCyclesStopwatch YieldStopwatch(true);

					auto OnResume = co_await fg_OnResume
						(
							[&]() -> CExceptionPointer
							{
								YieldStopwatch.f_Start();
								return {};
							}
						)
					;

					CStr RequiredVersion = WorkspaceSettings.f_GetSettingWithoutPositions<CStr>(Config, gc_Str<"NinjaRequiredVersion">.m_Str);
					CStr BuildDir = WorkspaceSettings.f_GetSettingWithoutPositions<CStr>(Config, gc_Str<"BuildDir">.m_Str);
					auto PoolDepths = WorkspaceSettings.f_GetSettingWithoutPositions<CEJsonSorted>(Config, gc_Str<"PoolDepths">.m_Str);
					auto Environment = WorkspaceSettings.f_GetSettingWithoutPositions<CEJsonSorted>(Config, gc_Str<"Environment">.m_Str);
					auto DefaultTarget = WorkspaceSettings.f_GetSettingWithoutPositions<TCOptional<CStr>>(Config, gc_Str<"DefaultTarget">.m_Str);

					TCVector<CRuleAndBuild> OtherWorkspaceRules;
					auto OtherRulesJson = WorkspaceSettings.f_GetSettingWithoutPositions<CEJsonSorted>(Config, gc_Str<"OtherRules">.m_Str);
					for (auto &Rule : OtherRulesJson.f_Array())
						CRuleAndBuild::fs_RuleFromJson(OtherWorkspaceRules.f_Insert(), m_BuildSystem, fg_Move(Rule));

					CStr ConfigDirectory = WorkspacedOutputDir / Config.f_GetFullName();

					CFile::fs_CreateDirectory(ConfigDirectory);

					CStr NinjaFile = ConfigDirectory / gc_Str<"build.ninja">.m_Str;

					// Collect compile commands for compile_commands.json (grouped by output path)
					struct CCompileCommandEntry
					{
						CStr m_File;
						NEncoding::CJsonSorted m_Arguments{NEncoding::EJsonType_Array};
						CStr m_Output;
					};
					TCMap<CStr, TCVector<CCompileCommandEntry>> CompileCommandsByPath;
					CStr CompileCommandsDirectory = m_BuildSystem.f_GetBaseDir();

					CStr NinjaFileConents;
					{
						CStr::CAppender Appender(NinjaFileConents);

						Appender += "builddir = ";
						fg_EscapeNinjaString(Appender, BuildDir, false);
						Appender += "\n\n";

						Appender += "ninja_required_version = ";
						fg_EscapeNinjaString(Appender, RequiredVersion, false);
						Appender += "\n\n";

						// Write global environment settings
						// See ninja manual "Environment blocks" section for format
						if (!Environment.f_Object().f_IsEmpty())
						{
							bool bOverride = false;
							if (auto const *pOverride = Environment.f_GetMember(gc_Str<"Override">.m_Str))
								bOverride = pOverride->f_AsBoolean();

							// Build the environment block string
							CStr EnvironmentStr;
							{
								CStr::CAppender EnvAppender(EnvironmentStr);
								bool bFirst = true;

								auto const *pCopy = Environment.f_GetMember(gc_Str<"Copy">.m_Str);

								// Build set of Copy variable names for deduplication (Copy takes precedence over Set)
								TCSet<CStr> CopyVars;
								if (pCopy)
								{
									for (auto const &Var : pCopy->f_Array())
										CopyVars.f_Insert(Var.f_String());
								}

								if (bOverride)
								{
									// Copy entries: write bare variable name (no '=')
									// Ninja will copy from its runtime environment
									for (auto const &VarName : CopyVars)
									{
										if (!bFirst)
											EnvAppender += "\n";
										else
											bFirst = false;

										EnvAppender += VarName;
									}
								}

								// Set entries are written as "VAR=value" (skip if also in Copy)
								if (auto const *pSet = Environment.f_GetMember(gc_Str<"Set">.m_Str))
								{
									for (auto const &Entry : pSet->f_Object())
									{
										// Skip if this variable is also in Copy
										if (CopyVars.f_FindEqual(Entry.f_Name()))
											continue;

										if (!bFirst)
											EnvAppender += "\n";
										else
											bFirst = false;

										EnvAppender += Entry.f_Name();
										EnvAppender += '=';
										fg_EscapeEnvironmentValue(EnvAppender, Entry.f_Value().f_String());
									}
								}
							}

							if (!EnvironmentStr.f_IsEmpty())
							{
								// Use override_environment if Override is true, otherwise use environment
								if (bOverride)
									Appender += "override_environment = ";
								else
									Appender += "environment = ";
								fg_EscapeNinjaString(Appender, EnvironmentStr, false);
								Appender += "\n\n";
							}
						}

						for (auto &PoolDepth : PoolDepths.f_Object())
						{
							Appender.f_Commit().m_String +=
								"pool {}\n"
								"  depth = {}\n"_f
								<< PoolDepth.f_Name()
								<< PoolDepth.f_Value().f_Integer()
							;
						}
						Appender += "\n";

						struct CRuleInfo
						{
							CStr m_Name;
							bool m_bPhony = false;
						};

						TCMap<CRule, CRuleInfo> AllRules;

						struct CBuildInfo
						{
							CBuild m_Build;
							TCVector<TCVariant<CStr, TCVector<CStr>>> m_Flags;
							CRuleInfo const *m_pRule;
						};

						TCVector<CBuildInfo> AllBuilds;

						TCSet<CStr> RuleNames;

						auto fAddRuleEntry = [&](CBuildAndRuleEntry &&_Entry)
							{
								auto &RuleInfo = AllRules[_Entry.m_Rule];
								if (!RuleInfo.m_Name)
								{
									RuleInfo.m_bPhony = _Entry.m_Rule.f_IsPhony();
									if (RuleInfo.m_bPhony)
										RuleInfo.m_Name = "phony";
									else
									{
										if (!_Entry.m_Rule.m_Command)
											return; // Nothing to do

										TCBinaryStreamHash<CHash_SHA256> Stream;
										Stream << _Entry.m_Rule;

										auto Digest = Stream.f_GetDigest();

										umint nChars = 4;

										auto Format = "Rule_{sj*}"_f;
										Format << Digest << nChars;

										RuleInfo.m_Name = Format;

										while (!RuleNames(RuleInfo.m_Name).f_WasCreated())
										{
											nChars += 4;
											RuleInfo.m_Name = Format;
										}
									}
								}

								auto &BuildInfo = AllBuilds.f_Insert();
								BuildInfo.m_Build = fg_Move(_Entry.m_Build);
								BuildInfo.m_Flags = fg_Move(_Entry.m_Flags);
								BuildInfo.m_pRule = &RuleInfo;
							}
						;


						for (auto &Rule : OtherWorkspaceRules)
							fAddRuleEntry(Rule.f_GetMergedEntry(m_BuildSystem));

						for (auto &Project : _Workspace.m_Projects)
						{
							if (YieldStopwatch.f_GetCycles() > g_CooperativeTimeSliceCycles)
							{
								co_await m_BuildSystem.f_CheckCancelled();
								co_await g_Yield;
							}

							auto *pTargetSettings = Project.m_EvaluatedTargetSettings.f_FindEqual(Config);
							if (pTargetSettings)
							{
								fAddRuleEntry(pTargetSettings->m_PreBuild.f_GetMergedEntry(m_BuildSystem));
								fAddRuleEntry(pTargetSettings->m_Build.f_GetMergedEntry(m_BuildSystem));
								fAddRuleEntry(pTargetSettings->m_PostBuild.f_GetMergedEntry(m_BuildSystem));
								for (auto &Rule : pTargetSettings->m_OtherRules)
									fAddRuleEntry(Rule.f_GetMergedEntry(m_BuildSystem));
							}

							// Get compile_commands.json path for this project/config (empty if not set)
							auto *pCompileCommandsPath = Project.m_CompileCommandsFilePath.f_FindEqual(Config);

							for (auto &File : Project.m_Files)
							{
								auto *pBuild = File.m_Builds.m_Builds.f_FindEqual(Config);

								if (!pBuild || pBuild->m_bDisabled)
									continue;

								TCMap<int64, TCVector<TCVariant<CStr, TCVector<CStr>>>> PrioritizedFlags;

								CBuildAndRuleEntry MergedEntry = pBuild->f_GetMergedEntry(m_BuildSystem, PrioritizedFlags);

								//if (!pBuild->m_bFullEval)
								{
									auto *pInherited = Project.m_Builds.f_FindEqual(pBuild->m_Type);
									if (pInherited)
									{
										auto pInheritedConfig = pInherited->m_Builds.f_FindEqual(Config);
										pBuild->f_InheritFrom(MergedEntry, m_BuildSystem, *pInheritedConfig, PrioritizedFlags);
									}
								}

								for (auto iFlags = PrioritizedFlags.f_GetIteratorReverse(); iFlags; ++iFlags)
									MergedEntry.m_Flags.f_Insert(fg_Move(*iFlags));

								if (!MergedEntry.m_Rule.m_Command)
								{
									auto Positions = MergedEntry.m_Positions;
									Positions.f_AddPositionFirst(File.m_Position, gc_ConstString_Context, gc_ConstString_Context);
									CBuildSystem::fs_ThrowError
										(
											Positions
											, "No rule specified to build '{}' with type '{}'. Entry:\n{}"_f
											<< Project.m_Files.fs_GetKey(File)
											<< pBuild->m_Type
											<< CStr::fs_ToStr(MergedEntry).f_Indent("    ")
										)
									;
								}

								// Collect compile commands for C/C++/ObjC/ObjC++ files (only if enabled for this target)
								if (pCompileCommandsPath && !pCompileCommandsPath->f_IsEmpty() && fg_IsCompileType(pBuild->m_Type) && !MergedEntry.m_Build.m_Inputs.f_IsEmpty() && !MergedEntry.m_Build.m_Outputs.f_IsEmpty())
								{
									auto &Entry = CompileCommandsByPath[*pCompileCommandsPath].f_Insert();
									Entry.m_File = MergedEntry.m_Build.m_Inputs[0];
									Entry.m_Output = MergedEntry.m_Build.m_Outputs[0];

									// Expand flags to a vector
									CJsonSorted FlagsVec(NEncoding::EJsonType_Array);
									bool bWasPCH = false;
									auto fAddFlag = [&](CStr const &_Flag)
										{
											if (_Flag.f_StartsWith(gc_Str<"/clang:-MF">.m_Str))
												return;

											if (_Flag == "/std:c++latest")
												FlagsVec.f_Insert(gc_Str<"/clang:-std=c++26">.m_Str);
											else
												FlagsVec.f_Insert(_Flag);
										}
									;

									for (auto &Flag : MergedEntry.m_Flags)
									{
										if (Flag.f_IsOfType<CStr>())
										{
											auto &FlagEntry = Flag.f_GetAsType<CStr>();
											if (FlagEntry == gc_Str<"-include-pch">.m_Str || FlagEntry == gc_Str<"/clang:-include-pch">.m_Str)
											{
												bWasPCH = true;
												continue;
											}
											if (bWasPCH)
											{
												bWasPCH = false;
												continue;
											}
											fAddFlag(FlagEntry);
										}
										else
										{
											for (auto &FlagEntry : Flag.f_GetAsType<TCVector<CStr>>())
											{
												if (FlagEntry == gc_Str<"-include-pch">.m_Str || FlagEntry == gc_Str<"/clang:-include-pch">.m_Str)
												{
													bWasPCH = true;
													continue;
												}
												if (bWasPCH)
												{
													bWasPCH = false;
													continue;
												}
												fAddFlag(FlagEntry);
											}
										}
									}

									// Parse command template and expand placeholders
									// The command template contains placeholders like {NinjaInternal_Input}, {NinjaInternal_Output}, {NinjaInternal_Flags}
									TCVector<CStr> ParsedArgs = fg_ParseNinjaRawCommand(MergedEntry.m_Rule.m_Command);

									auto &ArgumentsArray = Entry.m_Arguments.f_Array();

									// Helper lambda to expand a single placeholder argument
									auto fExpandArg = [&](CStr const &_Arg)
										{
											if (_Arg == gc_Str<"{uvi9RKGP}in">.m_Str)
												ArgumentsArray.f_Insert(Entry.m_File);
											else if (_Arg == gc_Str<"{uvi9RKGP}out">.m_Str)
												ArgumentsArray.f_Insert(Entry.m_Output);
											else if (_Arg == gc_Str<"{uvi9RKGP}flags">.m_Str)
												ArgumentsArray.f_Insert(fg_Move(FlagsVec.f_Array()));
											else
												ArgumentsArray.f_Insert(_Arg);
										}
									;

									for (auto &Arg : ParsedArgs)
									{
										// Handle @{NinjaInternal_RspFile} - expand rspfile_content inline (clang @filename format)
										if (Arg == gc_Str<"@{uvi9RKGP}rspfile">.m_Str)
										{
											// Get rspfile_content from rule's OtherProperties
											if (CStr const *pRspContent = MergedEntry.m_Rule.m_OtherProperties.f_FindEqual(gc_Str<"rspfile_content">.m_Str))
											{
												// Parse rspfile_content and expand each argument
												TCVector<CStr> RspArgs = fg_ParseNinjaRawCommand(*pRspContent);
												for (auto &RspArg : RspArgs)
													fExpandArg(RspArg);
											}
										}
										// Skip bare {NinjaInternal_RspFile} - not a clang-supported format
										else if (Arg == gc_Str<"{uvi9RKGP}rspfile">.m_Str)
										{
											// Skip - response file path not meaningful for compile_commands.json
										}
										else
											fExpandArg(Arg);
									}
								}

								fAddRuleEntry(fg_Move(MergedEntry));
							}
						}

						for (auto &RuleInfo : AllRules)
						{
							auto &Rule = AllRules.fs_GetKey(RuleInfo);

							if (RuleInfo.m_bPhony || !Rule.m_Command)
								continue;

							if (YieldStopwatch.f_GetCycles() > g_CooperativeTimeSliceCycles)
							{
								co_await m_BuildSystem.f_CheckCancelled();
								co_await g_Yield;
							}

							Appender += "rule ";
							fg_EscapeNinjaString(Appender, RuleInfo.m_Name, true);
							Appender += "\n  command = ";
							fg_EscapeNinjaString(Appender, Rule.m_Command, false);
							Appender += "\n  description = $malterlib_desc\n";

							if (!Rule.m_Environment.f_IsEmpty())
							{
								CStr Environment;
								{
									CStr::CAppender Appender(Environment);
									bool bFirst = true;
									for (auto &Entry : Rule.m_Environment.f_Entries())
									{
										if (!bFirst)
											Appender += "\n";
										else
											bFirst = false;

										Appender += Entry.f_Key();
										Appender += '=';
										fg_EscapeEnvironmentValue(Appender, Entry.f_Value());
									}
								}
								Appender += "  environment = ";
								fg_EscapeNinjaString(Appender, Environment, false);
								Appender += '\n';
							}

							bool bRawSet = false;
							for (auto &Value : Rule.m_OtherProperties)
							{
								auto &Key = Rule.m_OtherProperties.fs_GetKey(Value);
								if (Key == gc_Str<"command_raw">.m_Str)
									bRawSet = true;
								Appender += "  ";
								Appender += Key;
								Appender += " = ";
								fg_EscapeNinjaString(Appender, Value, false);
								Appender += '\n';
							}

							if (!bRawSet)
								Appender += "  command_raw = true\n";

							Appender += '\n';
						}

						for (auto &BuildInfo : AllBuilds)
						{
							auto &Build = BuildInfo.m_Build;
							auto &Rule = *BuildInfo.m_pRule;

							if (YieldStopwatch.f_GetCycles() > g_CooperativeTimeSliceCycles)
							{
								co_await m_BuildSystem.f_CheckCancelled();
								co_await g_Yield;
							}

							// Build set of outputs to filter from implicit dependencies
							TCSet<CStr> Outputs;
							for (auto &Output : Build.m_Outputs)
								Outputs.f_Insert(Output);
							for (auto &Output : Build.m_ImplicitOutputs)
								Outputs.f_Insert(Output);

							Appender += "build ";
							fg_AddNinjaArray(Appender, Build.m_Outputs);
							fg_AddNinjaArray(Appender, Build.m_ImplicitOutputs, gc_Str<"|">);

							Appender += ": ";
							Appender += Rule.m_Name;
							Appender += " ";

							fg_AddNinjaArray(Appender, Build.m_Inputs);
							fg_AddNinjaArrayFiltered(Appender, Build.m_ImplicitDependencies, Outputs, gc_Str<"|">);
							fg_AddNinjaArray(Appender, Build.m_OrderDependencies, gc_Str<"||">);
							fg_AddNinjaArray(Appender, Build.m_Validations, gc_Str<"|@">);

							if (!Rule.m_bPhony)
							{
								Appender += "\n  malterlib_desc = ";
								fg_EscapeNinjaString(Appender, Build.m_Description, false);
								TCVector<CStr> Flags;
								for (auto &Flag : BuildInfo.m_Flags)
								{
									if (Flag.f_IsOfType<CStr>())
										Flags.f_Insert(fg_Move(Flag.f_GetAsType<CStr>()));
									else
										Flags.f_Insert(fg_Move(Flag.f_GetAsType<TCVector<CStr>>()));
								}
								if (!Flags.f_IsEmpty())
								{
									bool bIsRaw = true;
									if (auto *pRaw = AllRules.fs_GetKey(Rule).m_OtherProperties.f_FindEqual(gc_Str<"command_raw">.m_Str))
										bIsRaw = *pRaw == "true";

									Appender += "\n  flags = ";
									if (bIsRaw)
										fg_EscapeNinjaString(Appender, NProcess::CProcessLaunchParams::fs_GetParamsUnix(Flags), false);
									else
										fg_EscapeNinjaString(Appender, NProcess::CProcessLaunchParams::fs_GetParams(Flags), false);
								}

								for (auto &VariableEntry : Build.m_Variables.f_Entries())
								{
									Appender += "\n  ";
									Appender += VariableEntry.f_Key();
									Appender += " = ";
									fg_EscapeNinjaString(Appender, VariableEntry.f_Value(), false);
								}
							}

							Appender += "\n\n";
						}

						if (DefaultTarget)
						{
							Appender += "default ";
							fg_EscapeNinjaString(Appender, *DefaultTarget, true);
							Appender += "\n\n";
						}
					}

					co_await m_BuildSystem.f_CheckCancelled();
					co_await g_Yield;

					NinjaFileConents = NinjaFileConents.f_Replace(gc_Str<"{uvi9RKGP}">.m_Str, gc_Str<"$">.m_Str);

					if (YieldStopwatch.f_GetCycles() > g_CooperativeTimeSliceCycles)
						co_await g_Yield;

					TCFutureVector<void> FileWrites;
					TCLinkedList<CBlockingActorCheckout> Checkouts;

					{
						auto &BlockingActorCheckout = Checkouts.f_Insert(fg_BlockingActor());

						g_Dispatch(BlockingActorCheckout) / [this, NinjaFile, NinjaFileConents = fg_Move(NinjaFileConents), WorkspaceName = _Workspace.f_GetName()]
							{
								bool bWasCreated;
								if (!m_BuildSystem.f_AddGeneratedFile(NinjaFile, NinjaFileConents, WorkspaceName, bWasCreated, EGeneratedFileFlag_NoDateCheck))
									DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << NinjaFile));

								if (bWasCreated)
								{
									CByteVector FileData;
									CFile::fs_WriteStringToVector(FileData, NinjaFileConents, false);
									m_BuildSystem.f_WriteFile(FileData, NinjaFile);
								}
							}
							> FileWrites
						;
					}

					// Generate compile_commands.json files (one per distinct output path)
					for (auto &CompileCommandsEntry : CompileCommandsByPath.f_Entries())
					{
						auto &CompileCommandsFilePath = CompileCommandsEntry.f_Key();
						auto &CompileCommands = CompileCommandsEntry.f_Value();

						if (YieldStopwatch.f_GetCycles() > g_CooperativeTimeSliceCycles)
						{
							co_await m_BuildSystem.f_CheckCancelled();
							co_await g_Yield;
						}

						NEncoding::CJsonSorted CompileCommandsJson(NEncoding::EJsonType_Array);
						for (auto &Entry : CompileCommands)
						{
							auto &JsonEntry = CompileCommandsJson.f_Insert();
							JsonEntry["directory"] = CompileCommandsDirectory;
							JsonEntry["file"] = Entry.m_File;
							JsonEntry["output"] = Entry.m_Output;
							JsonEntry["arguments"] = fg_Move(Entry.m_Arguments);
						}

						CStr JsonContent = CompileCommandsJson.f_ToString(nullptr);

						auto &BlockingActorCheckout = Checkouts.f_Insert(fg_BlockingActor());

						g_Dispatch(BlockingActorCheckout) / [this, CompileCommandsFilePath, JsonContent = fg_Move(JsonContent), WorkspaceName = _Workspace.f_GetName()]
							{
								CFile::fs_CreateDirectoryForFile(CompileCommandsFilePath);

								bool bWasCreatedCompileCommands;
								if (!m_BuildSystem.f_AddGeneratedFile(CompileCommandsFilePath, JsonContent, WorkspaceName, bWasCreatedCompileCommands))
									DError(CStr(CStr::CFormat("File '{}' already generated with other contents") << CompileCommandsFilePath));

								if (bWasCreatedCompileCommands)
								{
									CByteVector FileData;
									CFile::fs_WriteStringToVector(FileData, JsonContent, false);
									m_BuildSystem.f_WriteFile(FileData, CompileCommandsFilePath);
								}
							}
							> FileWrites
						;
					}

					co_await fg_AllDone(FileWrites);

					co_return {};
				}
				, m_BuildSystem.f_SingleThreaded()
			)
		;

		co_return {};
	}
}

