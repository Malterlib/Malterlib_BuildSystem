// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/BuildSystem/BuildSystem>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/File/ExeFS>
#include <Mib/File/VirtualFSs/MalterlibFS>
#include <Mib/Test/Exception>

namespace
{
	using namespace NMib;
	using namespace NMib::NEncoding;
	using namespace NMib::NStr;
	using namespace NMib::NFile;
	using namespace NMib::NContainer;
	using namespace NMib::NBuildSystem;
	using namespace NMib::NStorage;
	using namespace NMib::NConcurrency;

	struct CCommandLineControlActorTest : public ICCommandLineControl
	{
		TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput _fOnInput, NProcess::EStdInReaderFlag _Flags) override
		{
			co_return {};
		}

		TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput _fOnInput, NProcess::EStdInReaderFlag _Flags) override
		{
			co_return {};
		}

		TCFuture<TCActorSubscriptionWithID<>> f_RegisterForCancellation(FOnCancel _fOnCancel) override
		{
			co_return {};
		}

		TCFuture<NContainer::CIOByteVector> f_ReadBinary() override
		{
			co_return {};
		}

		TCFuture<NStr::CStrIO> f_ReadLine() override
		{
			co_return {};
		}

		TCFuture<NStr::CStrIO> f_ReadPrompt(NProcess::CStdInReaderPromptParams _Params) override
		{
			co_return {};
		}

		TCFuture<void> f_AbortReads() override
		{
			co_return {};
		}

		TCFuture<void> f_StdOut(NStr::CStrIO _Output) override
		{
			co_return {};
		}

		TCFuture<void> f_StdOutBinary(NContainer::CIOByteVector _Output) override
		{
			co_return {};
		}

		TCFuture<void> f_StdErr(NStr::CStrIO _Output) override
		{
			co_return {};
		}

		void f_Clear()
		{
		}

	private:
		TCFuture<void> fp_Destroy() override
		{
			co_return {};
		}
	};

	class CGenerate_Tests : public NMib::NTest::CTest
	{
		CEJsonSorted fp_Generate(CStr const &_TestName, TCOptional<TCMap<CStr, CStr>> const &_Environment)
		{
			CExeFS SourceFS;
			if (!fg_OpenExeFS(SourceFS))
				DMibError("Could not open ExeFS");

			CFileSystemInterface_VirtualFS SourceVirtualFS(SourceFS.m_FileSystem);
			CFileSystemInterface_Disk DestinationFS;

			CStr TempDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTests" / _TestName;
			CStr OutputDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTestsOutput" / _TestName;

			if (CFile::fs_FileExists(OutputDirectory))
				CFile::fs_DeleteDirectoryRecursive(OutputDirectory);

			CFile::fs_CreateDirectory(OutputDirectory);

			CFile::fs_CreateDirectory(TempDirectory);

			[[maybe_unused]] auto Files = SourceVirtualFS.f_FindFiles("*", EFileAttrib_File | EFileAttrib_Directory, true);

			SourceVirtualFS.f_CopyFiles(CStr("TestFiles") / _TestName / "*", DestinationFS, TempDirectory);

			CGenerateOptions GenerateOptions;
			auto &GenerateSettings = GenerateOptions.m_Settings;
			GenerateSettings.m_SourceFile = TempDirectory / "Test.MBuildSystem";
			GenerateSettings.m_OutputDir = OutputDirectory;
			GenerateSettings.m_Generator = "Test";
			if (_Environment)
				GenerateSettings.m_Environment = *_Environment;

			GenerateSettings.m_GenerationFlags |= EGenerationFlag_AbsoluteFilePaths | EGenerationFlag_SingleThreaded | EGenerationFlag_DisableUserSettings;

			GenerateOptions.m_DetailedPositions = EDetailedPositions_Enable;
			GenerateOptions.m_bDetailedValues = true;

			bool bChanged = false;
			CBuildSystem::ERetry Retry;

			{
				CActorRunLoopTestHelper RunLoopHelper;

				TCDistributedActor<CCommandLineControlActorTest> pCommandLineControlActor = fg_Construct();
				TCSharedPointer<CCommandLineControl> pCommandLineControl = fg_Construct();
				pCommandLineControl->m_ControlActor = pCommandLineControlActor->f_ShareInterface<ICCommandLineControl>();

				(
					g_Dispatch / [&]() -> TCUnsafeFuture<void>
					{
						co_await ECoroutineFlag_CaptureMalterlibExceptions;

						Retry = co_await CBuildSystem::fs_RunBuildSystem
							(
								[&](NBuildSystem::CBuildSystem *_pBuildSystem) -> TCUnsafeFuture<CBuildSystem::ERetry>
								{
									co_await ECoroutineFlag_CaptureMalterlibExceptions;

									CBuildSystem::ERetry Retry = CBuildSystem::ERetry_None;
									if (co_await _pBuildSystem->f_Action_Generate(GenerateOptions, Retry))
										bChanged = true;
									co_return Retry;
								}
								, pCommandLineControl
								, [](CStr const &_Output, bool _bError)
								{
								}
								, GenerateOptions
							)
						;

						co_return {};
					}
				)
				.f_CallSync(RunLoopHelper.m_pRunLoop);
			}

			DMibExpect(Retry, ==, CBuildSystem::ERetry_None)(ETestFlag_Aggregated);

			auto GeneratorFile = OutputDirectory / "BuildSystemData.json";

			return CEJsonSorted::fs_FromString(CFile::fs_ReadStringFromFile(GeneratorFile), GeneratorFile);
		}

		struct CTestGenerate
		{
			CStr m_OutputDirectory;
			CStr m_SourceDirectory;
			CStr m_ExpectedContents;
			CEJsonSorted m_BuildSystemData;
		};

		CTestGenerate fp_TestGenerate(CStr const &_TestName, TCOptional<TCMap<CStr, CStr>> const &_Environment = {})
		{
			CTestGenerate TestGenerate;
			TestGenerate.m_SourceDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTests" / _TestName;
			TestGenerate.m_OutputDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTestsOutput" / _TestName;

			fg_TestAddCleanupPath(TestGenerate.m_SourceDirectory);
			fg_TestAddCleanupPath(TestGenerate.m_OutputDirectory);

			auto BuildSystemData = fp_Generate(_TestName, _Environment);
			CStr BuildSystemDataFile = TestGenerate.m_SourceDirectory / "ExpectedBuildSystemData.json";

			CStr StringContents = CFile::fs_ReadStringFromFile(BuildSystemDataFile, true);
			StringContents = StringContents.f_Replace(("/Deploy/Tests/BuildSystemTests/{}/"_f << _TestName).f_GetStr(), ("{}/"_f << TestGenerate.m_SourceDirectory).f_GetStr());
			StringContents = StringContents.f_Replace(("/Deploy/Tests/BuildSystemTestsOutput/{}/"_f << _TestName).f_GetStr(), ("{}/"_f << TestGenerate.m_OutputDirectory).f_GetStr());

			CEJsonSorted ExpectedBuildSystemData = CEJsonSorted::fs_FromString(StringContents, BuildSystemDataFile);
			DMibAssert(BuildSystemData, ==, ExpectedBuildSystemData)(ETestFlag_Aggregated);
			TestGenerate.m_BuildSystemData = fg_Move(ExpectedBuildSystemData);
			CStr ExpectedContentsFile = TestGenerate.m_SourceDirectory / "ExpectedContents.txt";

			if (CFile::fs_FileExists(ExpectedContentsFile))
				TestGenerate.m_ExpectedContents = CFile::fs_ReadStringFromFile(ExpectedContentsFile, true);

			return TestGenerate;
		}

		CParseLocation fp_GetLocationInFile(CStr const &_FileName, CStr const &_String)
		{
			CExeFS SourceFS;
			if (!fg_OpenExeFS(SourceFS))
				DMibError("Could not open ExeFS");

			CStr BuildSystemSourceDir = CFile::fs_GetExpandedPath(CStr(DMibPFile) + "/../../Source");
#ifdef DPlatformFamily_Windows
			BuildSystemSourceDir = BuildSystemSourceDir.f_ReplaceChar('\\', '/');
#endif

			CFileSystemInterface_VirtualFS SourceVirtualFS(SourceFS.m_FileSystem);

			auto FileContents = SourceVirtualFS.f_ReadStringFromFile(CStr("Source") / _FileName, true);
			uint32 iLine = 0;
			for (auto &Line : FileContents.f_SplitLine())
			{
				++iLine;
				if (auto iFound = Line.f_Find(_String); iFound >= 0)
					return CParseLocation{BuildSystemSourceDir / _FileName, 0, iLine, 0};
			}

			DMibError("String '{}' not found in file '{}'"_f << _String << _FileName);
		}

	public:
		void f_DoTests()
		{
			DMibTestSuite("Empty")
			{
				fp_TestGenerate("Empty");
			};
			DMibTestSuite("Simple")
			{
				fp_TestGenerate("Simple");
			};
			DMibTestSuite("Expand")
			{
				fp_TestGenerate("Expand");
			};
			DMibTestSuite("GenerateFile")
			{
				auto TestGenerate = fp_TestGenerate("GenerateFile");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, "Generate File Contents");
			};
			DMibTestSuite("Types")
			{
				auto TestGenerate = fp_TestGenerate("Types");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				auto ExpectedContents = CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt");
				DMibExpect(ExpectedContents, ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("Append")
			{
				auto TestGenerate = fp_TestGenerate("Append");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile1.txt"));
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile2.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile1.txt"), ==, TestGenerate.m_ExpectedContents);
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile2.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("FunctionCallAppend")
			{
				auto TestGenerate = fp_TestGenerate("FunctionCallAppend");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("Accessors")
			{
				auto TestGenerate = fp_TestGenerate("Accessors");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("DynamicValue")
			{
				auto TestGenerate = fp_TestGenerate("DynamicValue");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("FunctionCall")
			{
				auto TestGenerate = fp_TestGenerate("FunctionCall");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("FunctionDefaulted")
			{
				auto TestGenerate = fp_TestGenerate("FunctionDefaulted");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("FunctionCallAsVariable")
			{
				CStr TempDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTests/FunctionCallAsVariable";
				CStr BuildSystemFile = TempDirectory / "Test.MBuildSystem";
				TCVector<CParseError> ExpectedParseErrors =
					{
						{"error: Trying to access a function as a variable, you need to call it", {BuildSystemFile, 1, 44, 13}, 0}
						, {"Contributing values", {}, 1, true}
						, {"Contributing to value", {BuildSystemFile, 1, 44, 13}, 2}
						, {"Context contributing values", {}, 1, true}
						, 
						{
							"Property.CalcValue\n"
							"    {\n"
							"        \"Test\": undefined,\n"
							"        \"TestOptional\": {\n"
							"            \"InnerValue\": \"Default\"\n"
							"        },\n"
							"        \"TestOptional.InnerValue\": \"Not inner\"\n"
							"    }"
							, {BuildSystemFile, 1, 44, 13}
							, 2
						}
						, 
						{
							"Property.GetDefaulted\n"
							"    \"Default\""
							, {BuildSystemFile, 1, 35, 16}
							, 2
						}
						, 
						{
							"Condition\n"
							"    true"
							, {BuildSystemFile, 1, 37, 13}
							, 2
						}
						, {"Other messages", {}, 1, true}
						, {"See definition of function type", {BuildSystemFile, 1, 26, 13}, 2}
						, {"Other message contributing values", {}, 2, true}
						, {"Type", {BuildSystemFile, 1, 26, 13}, 3}
						, {"Parent contexts", {}, 1, true}
						, {"Context", {BuildSystemFile, 1, 60, 12}, 2}
						, {"Parent context contributing values", {}, 2, true}
						, 
						{
							"Call builtin 'ToString'"
							, fp_GetLocationInFile("Malterlib_BuildSystem_Evaluate_BuiltinFunctions_String.cpp", "DMibBuildSystemFilePosition // ToString")
							, 3
						}
						, {"Entity and parents", {}, 2, true}
						, {"Path=Root->GenerateFile:TestFile", {BuildSystemFile, 1, 18, 15}, 3}
					}
				;

				DMibExpectException(fp_TestGenerate("FunctionCallAsVariable"), DMibExceptionInstanceParse(CParseError::fs_ToString(ExpectedParseErrors), {}));
			};
			DMibTestSuite("ForEach")
			{
				auto TestGenerate = fp_TestGenerate("ForEach");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("ForEachRecursive")
			{
				auto TestGenerate = fp_TestGenerate("ForEachRecursive");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("Ternary")
			{
				auto TestGenerate = fp_TestGenerate("Ternary");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("DynamicDefault")
			{
				auto TestGenerate = fp_TestGenerate("DynamicDefault");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("FunctionCallWithCondition")
			{
				CStr TempDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTests/FunctionCallWithCondition";
				CStr BuildSystemFile = TempDirectory / "Test.MBuildSystem";
				TCVector<CParseError> ExpectedParseErrors = 
					{
						{"error: No such function: Property.TestFunction", {BuildSystemFile, 1, 32, 12}, 0}
						, {"Contributing values", {}, 1, true}
						, {"Contributing to value", {BuildSystemFile, 1, 32, 12}, 2}
						, {"Context contributing values", {}, 1, true}
						, 
						{
							"Condition\n"
							"    false"
							, {BuildSystemFile, 1, 26, 11}
							, 2
						}
						, 
						{
							"Apply default\n"
							"    false"
							, {BuildSystemFile, 1, 22, 7}
							, 2
						}
					}
				;

				DMibExpectException(fp_TestGenerate("FunctionCallWithCondition"), DMibExceptionInstanceParse(CParseError::fs_ToString(ExpectedParseErrors), {}));
			};

			DMibTestSuite("TypeWithCondition")
			{
				CStr TempDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTests/TypeWithCondition";
				CStr BuildSystemFile = TempDirectory / "Test.MBuildSystem";
				CStr BuildSystemSourceDir = CFile::fs_GetExpandedPath(CStr(DMibPFile) + "/../../Source") + "/";
#ifdef DPlatformFamily_Windows
				BuildSystemSourceDir = BuildSystemSourceDir.f_ReplaceChar('/', '\\');
#endif
				TCVector<CParseError> ExpectedParseErrors = 
					{
						{"error: Could not find user type of name 'CTest'", {BuildSystemFile, 1, 40, 12}, 0}
						, {"Contributing values", {}, 1, true}
						, {"Contributing to value", {BuildSystemFile, 1, 40, 12}, 2}
						, {"Context contributing values", {}, 1, true}
						, 
						{
							"Apply default\n"
							"    {\n"
							"        \"Test\": \"Test\"\n"
							"    }"
							, {BuildSystemFile, 1, 35, 16}
							, 2
						}
						, 
						{
							"Condition\n"
							"    false"
							, {BuildSystemFile, 1, 25, 15}
							, 2
						}
						, 
						{
							"Initial value: Property.Platform\n"
							"    \"TestPlatform\""
							, fp_GetLocationInFile("Malterlib_BuildSystem.cpp", "DMibBuildSystemFilePosition, \"Initial value")
							, 2
						}
						, {"Parent contexts", {}, 1, true}
						, {"Context", {BuildSystemFile, 1, 40, 12}, 2}
						, {"Entity and parents", {}, 2, true}
						, {"Path=Root->GenerateFile:TestFile", {BuildSystemFile, 1, 29, 15}, 3}
					}
				;

				DMibExpectException(fp_TestGenerate("TypeWithCondition"), DMibExceptionInstanceParse(CParseError::fs_ToString(ExpectedParseErrors), {}));
			};

			DMibTestSuite("Environment")
			{
				TCMap<CStr, CStr> Environment =
					{
						{"TestString", "String1"}
						, {"TestBoolTRUE", "TRUE"}
						, {"TestBoolFALSE", "FALSE"}
						, {"TestBool1", "1"}
						, {"TestBool0", "0"}
						, {"TestBool_true", "true"}
						, {"TestBool_false", "false"}
						, {"TestInt0", "0"}
						, {"TestInt50", "50"}
						, {"TestIntNeg50", "-50"}
						, {"TestFloat0", "0.0"}
						, {"TestFloat55_5", "55.5"}
						, {"TestFloatNeg55_5", "-55.5"}
						, {"TestFloatInf", "Inf"}
						, {"TestDate", "2020-03-22 11:11"}
						, {"TestBinary", "VGVzdEJhc2U2NAo="}
						, {"TestArrayString", "Test1;Test2;Test3"}
						, {"TestArrayInt", "1;2;3"}
						, {"TestObject", "TestString=Test;TestInt=5;TestArrayString=[\"Test\"];TestArrayInt=[5]"}
						, {"TestOneOf", "5"}
					}
				;
				auto TestGenerate = fp_TestGenerate("Environment", Environment);

				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("DynamicString")
			{
				auto TestGenerate = fp_TestGenerate("DynamicString");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("Import")
			{
				auto TestGenerate = fp_TestGenerate("Import");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("DefaultedTypes")
			{
				auto TestGenerate = fp_TestGenerate("DefaultedTypes");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("BinaryOperators")
			{
				auto TestGenerate = fp_TestGenerate("BinaryOperators");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("FormatString")
			{
				auto TestGenerate = fp_TestGenerate("FormatString");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("Namespace")
			{
				auto TestGenerate = fp_TestGenerate("Namespace");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
		}
	};
}

DMibTestRegister(CGenerate_Tests, Malterlib::BuildSystem);
