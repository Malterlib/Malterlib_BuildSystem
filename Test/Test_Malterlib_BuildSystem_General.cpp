// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/BuildSystem/BuildSystem>
#include <Mib/Process/ProcessLaunch>
#include <Mib/Stream/Binary>
#include <Mib/Test/Exception>
#include "../Source/Malterlib_BuildSystem_RepositoryEditor.h"

namespace
{
	using namespace NMib;
	using namespace NMib::NBuildSystem;
	using namespace NMib::NStr;
	using namespace NMib::NContainer;
	using namespace NMib::NStream;

	class CGeneral_Tests : public NMib::NTest::CTest
	{
	public:
		void f_DoTests()
		{
			DMibTestSuite("RepositoryEditorCommandLine")
			{
#ifdef DPlatformFamily_Windows
				CStr Application = R"(C:\Program Files/Sublime Merge/smerge.exe)";
				CStr Repository = R"(C:\Source\repo with spaces\)";
#else
				CStr Application = "/opt/Repo Editor/bin/editor";
				CStr Repository = "/opt/Source/repo with spaces/";
#endif
				TCVector<CStr> Tokens{Application, "{}", Repository, ""};
				NRepository::CRepoEditor Editor;
				Editor.f_SetCommandLine(NProcess::CProcessLaunchParams::fs_GetParams(Tokens));

				DMibExpect(Editor.m_Application, ==, Application);
				DMibAssert(Editor.m_Params.f_GetLen(), ==, 3u);
				DMibExpect(Editor.m_Params[0], ==, "{}");
				DMibExpect(Editor.m_Params[1], ==, Repository);
				DMibExpect(Editor.m_Params[2], ==, "");

				{
					DMibTestPath("ReplaceCommand");
					Editor.f_SetCommandLine("editor {}");

					DMibExpect(Editor.m_Application, ==, "editor");
					DMibAssert(Editor.m_Params.f_GetLen(), ==, 1u);
					DMibExpect(Editor.m_Params[0], ==, "{}");
				}
			};

			DMibTestSuite("UserSettingComment")
			{
				auto fParse = [](CStr const &_Whitespace, CStr const &_LeadingWhitespace)
					{
						return CBuildSystem::fs_ParseUserSettingComment(_Whitespace, _LeadingWhitespace);
					}
				;

				{
					DMibTestPath("Trailing");
					auto Comment = fParse(" // User(Debug)=true: Enable it\n", "\n");
					DMibAssertTrue(!!Comment);
					DMibExpect(Comment->m_Section, ==, "Debug");
					DMibExpect(Comment->m_DefaultValue, ==, "true");
					DMibExpect(Comment->m_Description, ==, "Enable it");
				}
				{
					DMibTestPath("Above");
					auto Comment = fParse("\n", " // Previous\n\n\t// Banner\n\n\t// User(Linking)=true: Discard what no\n\t// symbol references\n");
					DMibAssertTrue(!!Comment);
					DMibExpect(Comment->m_Section, ==, "Linking");
					DMibExpect(Comment->m_DefaultValue, ==, "true");
					DMibExpect(Comment->m_Description, ==, "Discard what no symbol references");
				}
				{
					DMibTestPath("General");
					auto Comment = fParse(" // User: Described\n", "\n");
					DMibAssertTrue(!!Comment);
					DMibExpect(Comment->m_Section, ==, "General");
					DMibExpect(Comment->m_Description, ==, "Described");
				}

				// What follows a definition documents the next one, and the line in front of the definition's own lines is
				// the end of the one before.
				DMibExpectFalse(!!fParse(" // Plain\n\t// User(Tools): For the next one\n", "\n"));
				DMibExpectFalse(!!fParse("\n", " // User(Tools): Of the one before\n"));
				DMibExpectFalse(!!fParse("\n", "\n\t// User(Tools): Separated\n\n"));

				// The comment lines a file starts with stand above its first declaration.
				{
					DMibTestPath("FirstInFile");
					CStringCache StringCache;
					CBuildSystemRegistryParseContext ParseContext(StringCache);
					CBuildSystemRegistry Registry;
					Registry.f_ParseStrWithContext(ParseContext, "// User(Review)=true: Root setting\nProperty.RootSetting: bool = false\nOther 1\n", "Test.MSettings");
					auto iChild = Registry.f_GetChildIterator();
					DMibAssertTrue(!!iChild);
					auto Comment = fParse((*iChild).f_GetWhiteSpace(ERegistryWhiteSpaceLocation_After), CBuildSystem::fs_GetLeadingWhiteSpace(*iChild));
					DMibAssertTrue(!!Comment);
					DMibExpect(Comment->m_Section, ==, "Review");
					DMibExpect(Comment->m_Description, ==, "Root setting");
					++iChild;
					{
						DMibTestPath("Next");
						DMibAssertTrue(!!iChild);
						DMibExpectFalse(!!fParse((*iChild).f_GetWhiteSpace(ERegistryWhiteSpaceLocation_After), CBuildSystem::fs_GetLeadingWhiteSpace(*iChild)));
					}
				}
			};

			DMibTestSuite("FindContainingPath_FindsOwner")
			{
				TCMap<CStr, int32> Paths;
				Paths["/opt/Source/Malterlib"] = 1;
				Paths["/opt/Source/Malterlib/Malterlib/Mongo"] = 2;
				Paths["/opt/Source/Malterlib/External/libjpeg-turbo"] = 3;
				Paths["/opt/Source/Malterlib/External/mongo-c-driver"] = 4;
				Paths["/opt/Source/Malterlib/External/mongo-cxx-driver"] = 5;

				auto fCheckContainingPath =
					[&]
					(
						CStr const &_TestPath
						, CStr const &_Path
						, CStr const &_ExpectedOwnerPath
						, int32 _ExpectedOwnerValue
					)
					-> void
					{
						DMibTestPath(_TestPath);

						CStr OwnerPath = "stale";
						auto *pOwner = CBuildSystem::fs_FindContainingPath(Paths, _Path, OwnerPath);

						DMibExpectTrue(pOwner != nullptr);
						DMibExpect(OwnerPath, ==, _ExpectedOwnerPath);
						if (pOwner)
							DMibExpect(*pOwner, ==, _ExpectedOwnerValue);
					}
				;

				fCheckContainingPath("ExactRoot", "/opt/Source/Malterlib", "/opt/Source/Malterlib", 1);
				fCheckContainingPath("RootConfigFile", "/opt/Source/Malterlib/Malterlib.MRepo", "/opt/Source/Malterlib", 1);
				fCheckContainingPath("ExactNestedRepo", "/opt/Source/Malterlib/Malterlib/Mongo", "/opt/Source/Malterlib/Malterlib/Mongo", 2);
				fCheckContainingPath("NestedConfigFile", "/opt/Source/Malterlib/Malterlib/Mongo/External.MRepo", "/opt/Source/Malterlib/Malterlib/Mongo", 2);
				fCheckContainingPath("NestedSubdirectoryConfigFile", "/opt/Source/Malterlib/Malterlib/Mongo/Tool/BuildBinaries/External.MRepo", "/opt/Source/Malterlib/Malterlib/Mongo", 2);
				fCheckContainingPath("NearestAncestor", "/opt/Source/Malterlib/Malterlib/Mongo/Tool/BuildBinaries", "/opt/Source/Malterlib/Malterlib/Mongo", 2);
				fCheckContainingPath("ChildOfExternalRepo", "/opt/Source/Malterlib/External/mongo-c-driver/src", "/opt/Source/Malterlib/External/mongo-c-driver", 4);
				fCheckContainingPath("SingleSiblingShadow", "/opt/Source/Malterlib/External/libjpeg", "/opt/Source/Malterlib", 1);
				fCheckContainingPath("MultiSiblingShadow", "/opt/Source/Malterlib/External/mongo", "/opt/Source/Malterlib", 1);
			};

			DMibTestSuite("FindContainingPath_NoOwner")
			{
				TCMap<CStr, int32> Paths;
				Paths["/opt/Source/Malterlib"] = 1;
				Paths["/opt/Source/Malterlib/Malterlib/Mongo"] = 2;
				Paths["/opt/Source/Malterlib/External/libjpeg-turbo"] = 3;
				Paths["/opt/Source/Malterlib/External/mongo-c-driver"] = 4;
				Paths["/opt/Source/Malterlib/External/mongo-cxx-driver"] = 5;

				auto fCheckNoContainingPath =
					[&]
					(
						CStr const &_TestPath
						, CStr const &_Path
					)
					-> void
					{
						DMibTestPath(_TestPath);

						CStr OwnerPath = "stale";
						auto *pOwner = CBuildSystem::fs_FindContainingPath(Paths, _Path, OwnerPath);

						DMibExpectTrue(pOwner == nullptr);
						DMibExpectTrue(OwnerPath.f_IsEmpty());
					}
				;

				fCheckNoContainingPath("DifferentWorkspace", "/opt/Source/OtherProject/External/mongo");
				fCheckNoContainingPath("PrefixButNotChild", "/opt/Source/MalterlibSDK");
			};

			DMibTestSuite("GeneratorState")
			{
				CGeneratorArchiveState State;
				State.m_GeneratedFiles["Generated.txt"].m_Workspaces["Workspace"];
				State.m_SourceFiles["Source.MHeader"];
				State.m_Environment["Variable"] = "Value";

				auto fRead = [](CByteVector const &_Data)
					{
						CGeneratorArchiveState Read;
						CBinaryStreamMemory<> Stream;
						Stream.f_Open(_Data);
						Stream >> Read;
						return Read;
					}
				;

				{
					DMibTestPath("CurrentVersion");
					CBinaryStreamMemory<> Stream;
					Stream << State;

					auto Read = fRead(Stream.f_MoveVector());
					DMibExpect(Read.m_GeneratedFiles.f_GetLen(), ==, 1u);
					DMibExpectTrue(Read.m_GeneratedFiles.f_FindEqual("Generated.txt"));
					DMibExpect(Read.m_SourceFiles.f_GetLen(), ==, 1u);
					DMibExpect(Read.m_Environment, ==, State.m_Environment);
				}
				{
					DMibTestPath("GeneratedFilesLastVersion");
					CBinaryStreamMemory<> Stream;
					Stream << uint32(CGeneratorArchiveState::EFileVersion_GeneratedFilesLast);
					Stream << State.m_ExeFile;
					Stream << State.m_SourceFiles;
					Stream << State.m_ReferencedFiles;
					Stream << State.m_GeneratedFiles;
					Stream << State.m_SourceSearches;
					Stream << State.m_Environment;
					Stream << State.m_NumWorkspaceTargets;
					Stream << State.m_GenerationFlags;

					auto Read = fRead(Stream.f_MoveVector());
					DMibExpectTrue(Read.m_GeneratedFiles.f_FindEqual("Generated.txt"));
					DMibExpect(Read.m_SourceFiles.f_GetLen(), ==, 1u);
					DMibExpect(Read.m_Environment, ==, State.m_Environment);
				}
				{
					DMibTestPath("LaterVersion");
					CBinaryStreamMemory<> Stream;
					Stream << State;
					Stream << CStr("State that only the later version knows");

					CByteVector Data = Stream.f_MoveVector();
					uint32 LaterVersion = CGeneratorArchiveState::EFileVersion + 1;
					NMemory::fg_MemCopy(Data.f_GetArray(), &LaterVersion, sizeof(LaterVersion));

					auto Read = fRead(Data);
					DMibExpectTrue(Read.m_GeneratedFiles.f_FindEqual("Generated.txt"));
					DMibExpect(Read.m_SourceFiles.f_GetLen(), ==, 1u);
					DMibExpect(Read.m_Environment, ==, State.m_Environment);
				}
				{
					DMibTestPath("UnreadableLaterVersion");
					uint32 LaterVersion = CGeneratorArchiveState::EFileVersion + 1;
					CBinaryStreamMemory<> Stream;
					Stream << LaterVersion;
					Stream << LaterVersion;

					DMibExpectException
						(
							fRead(Stream.f_MoveVector())
							, DMibErrorInstance
							(
								CStr::CFormat("Generator state of version 0x{nh} needs a reader of version 0x{nh} or later") << LaterVersion << LaterVersion
							)
						)
					;
				}
				{
					DMibTestPath("TooOldVersion");
					CBinaryStreamMemory<> Stream;
					Stream << uint32(CGeneratorArchiveState::EFileVersion_GeneratedFilesLast - 1);

					DMibExpectException
						(
							fRead(Stream.f_MoveVector())
							, DMibErrorInstance(CStr::CFormat("Generator state of version 0x{nh} is too old to read") << uint32(CGeneratorArchiveState::EFileVersion_GeneratedFilesLast - 1))
						)
					;
				}
			};
		}
	};
}

DMibTestRegister(CGeneral_Tests, Malterlib::BuildSystem);
