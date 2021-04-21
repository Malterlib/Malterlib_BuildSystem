// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/BuildSystem/BuildSystem>
#include <Mib/File/ExeFS>
#include <Mib/File/VirtualFSs/MalterlibFS>
#include <Mib/Encoding/JSONShortcuts>
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

	class CGenerate_Tests : public NMib::NTest::CTest
	{
		CEJSON fp_Generate(CStr const &_TestName, TCOptional<TCMap<CStr, CStr>> const &_Environment)
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

			bool bChanged = false;

			auto Retry = CBuildSystem::fs_RunBuildSystem
				(
					[&](NBuildSystem::CBuildSystem &_BuildSystem)
					{
						CBuildSystem::ERetry Retry = CBuildSystem::ERetry_None;
						if (_BuildSystem.f_Action_Generate(GenerateOptions, Retry))
							bChanged = true;
						return Retry;
					}
					, NCommandLine::EAnsiEncodingFlag_None
					, [](CStr const &_Output)
					{
					}
				)
			;

			DMibExpect(Retry, ==, CBuildSystem::ERetry_None)(ETestFlag_Aggregated);

			auto GeneratorFile = OutputDirectory / "BuildSystemData.json";

			return CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(GeneratorFile), GeneratorFile);
		}

		struct CTestGenerate
		{
			CStr m_OutputDirectory;
			CStr m_SourceDirectory;
			CStr m_ExpectedContents;
			CEJSON m_BuildSystemData;
		};

		CTestGenerate fp_TestGenerate(CStr const &_TestName, TCOptional<TCMap<CStr, CStr>> const &_Environment = {})
		{
			CTestGenerate TestGenerate;

			auto BuildSystemData = fp_Generate(_TestName, _Environment);
			TestGenerate.m_SourceDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTests" / _TestName;
			TestGenerate.m_OutputDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTestsOutput" / _TestName;
			CStr BuildSystemDataFile = TestGenerate.m_SourceDirectory / "ExpectedBuildSystemData.json";

			CStr StringContents = CFile::fs_ReadStringFromFile(BuildSystemDataFile, true);
			StringContents = StringContents.f_Replace(("/Deploy/Tests/BuildSystemTests/{}/"_f << _TestName).f_GetStr(), ("{}/"_f << TestGenerate.m_SourceDirectory).f_GetStr());
			StringContents = StringContents.f_Replace(("/Deploy/Tests/BuildSystemTestsOutput/{}/"_f << _TestName).f_GetStr(), ("{}/"_f << TestGenerate.m_OutputDirectory).f_GetStr());

			CEJSON ExpectedBuildSystemData = CEJSON::fs_FromString(StringContents, BuildSystemDataFile);
			DMibAssert(BuildSystemData, ==, ExpectedBuildSystemData)(ETestFlag_Aggregated);
			TestGenerate.m_BuildSystemData = fg_Move(ExpectedBuildSystemData);
			CStr ExpectedContentsFile = TestGenerate.m_SourceDirectory / "ExpectedContents.txt";

			if (CFile::fs_FileExists(ExpectedContentsFile))
				TestGenerate.m_ExpectedContents = CFile::fs_ReadStringFromFile(ExpectedContentsFile, true);

			return TestGenerate;
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
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
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
				DMibExpectException
					(
						fp_TestGenerate("FunctionCallAsVariable")
						, DMibErrorInstance
						(
							"{}/Test.MBuildSystem" DMibPFileLineColumnFormat " error: Trying to access a function as a variable, you need to call it{\n}"
							DMibPFileLineFormatIndent "{}/Test.MBuildSystem" DMibPFileLineColumnFormat " See definition of function type{\n}"
							DMibPFileLineFormatIndent "{}/Test.MBuildSystem" DMibPFileLineColumnFormat " --- \"TestFile\""_f
							<< TempDirectory << "" << 44 << 13
							<< TempDirectory << "" << 26 << 13
							<< TempDirectory << "" << 60 << 13
						)
					)
				;
			};
			DMibTestSuite("ForEach")
			{
				auto TestGenerate = fp_TestGenerate("ForEach");
				DMibAssertTrue(CFile::fs_FileExists(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"));
				DMibExpect(CFile::fs_ReadStringFromFile(TestGenerate.m_OutputDirectory / "TestGenerateFile.txt"), ==, TestGenerate.m_ExpectedContents);
			};
			DMibTestSuite("l")
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
				DMibExpectException
					(
						fp_TestGenerate("FunctionCallWithCondition")
						, DMibErrorInstance("{}/Test.MBuildSystem" DMibPFileLineColumnFormat " error: Define does not support conditions or debug"_f << TempDirectory << "" << 26 << 11)
					)
				;
			};
			DMibTestSuite("TypeWithCondition")
			{
				CStr TempDirectory = CFile::fs_GetProgramDirectory() / "BuildSystemTests/TypeWithCondition";
				DMibExpectException
					(
						fp_TestGenerate("TypeWithCondition")
						, DMibErrorInstance("{}/Test.MBuildSystem" DMibPFileLineColumnFormat " error: Type does not support conditions or debug"_f << TempDirectory << "" << 25 << 15)
					)
				;
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
						, {"TestBooltrue", "true"}
						, {"TestBoolfalse", "false"}
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
		}
	};
}

DMibTestRegister(CGenerate_Tests, Malterlib::BuildSystem);
