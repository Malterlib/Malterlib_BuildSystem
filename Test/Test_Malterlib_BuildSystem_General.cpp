// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/BuildSystem/BuildSystem>
#include <Mib/Process/ProcessLaunch>
#include "../Source/Malterlib_BuildSystem_RepositoryEditor.h"

namespace
{
	using namespace NMib;
	using namespace NMib::NBuildSystem;
	using namespace NMib::NStr;
	using namespace NMib::NContainer;

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
		}
	};
}

DMibTestRegister(CGeneral_Tests, Malterlib::BuildSystem);
