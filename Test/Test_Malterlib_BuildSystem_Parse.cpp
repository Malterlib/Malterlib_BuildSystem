// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Time/Time>
#include <Mib/BuildSystem/Registry>

namespace
{
	using namespace NMib;
	using namespace NMib::NEncoding;
	using namespace NMib::NStr;
	using namespace NMib::NContainer;
	using namespace NMib::NBuildSystem;

	struct CBuildSystemRegistry_Tests : public NTest::CTest
	{
		CBuildSystemRegistry_Tests()
		{
		}

		void f_DoTests()
		{
			DMibTestSuite("Parse")
			{
				auto fParseGenerate = [&](CStr const &_Source)
					{
						CBuildSystemRegistry RegistryTest;
						RegistryTest.f_ParseStr(_Source, "/Temp/Test.reg");
						return RegistryTest.f_GenerateStr();
					}
				;
				NStr::CStr RegistryStr = R"---(Root
{
	TestStr /* Comment */ "Test Str" // Comment
	TestMultilineStr "Line 1\n"\
	                 "Line 2\n"\
	                 "Line 3\n"\
	                 ""
	TestInt 65
	TestFloat 65.5
	TestBoolean true
	TestNull null
	TestUndefined undefined
	TestStringSingle 'String'
	TestStringSingle2 'String "Test"'
	TestStringSingle3 'String \''
	TestStringEmpty ""
	TestStringEmpty2 ''
	TestStringEval `String`
	TestStringEval2 `String @(Target:Target.Test->Function()) String`
	TestStringEval3 `String @("Test"->Function()) String`
	TestStringEval4 `String @('Test'->Function()) String`
	TestStringEval5 `String @(\`@(Test)\`->Function()) String`
	TestStringEval6 `String @(\`@(Test)\`->Function("Param")) String`
	TestStringEval7 `String @(\`@(Test)\`->Function(@(Param))) String`
	TestStringEval8 `String @(Function(@(Param->Function("Param")))) String`
	TestStringEval9 `String @(Function(@(Param->Function("Param")), \`TestLine\\n\`\\\n`\
	                `                                               \`TestLine2\`)) String`
	TestStringEval10 `String @(Function(@(Param->Function("Param")), "TestLine\\n"\\\n`\
	                 `                                               "TestLine2")) String`
	TestReference @(Other)
	Children
	{
		"Test Test" "Test Test 2"
	}
	TestDate Date(2019-05-29)
	TestMinute Date(2019-05-29 11:11)
	TestSecond Date(2019-05-29 11:11:06)
	TestFraction Date(2019-05-29 11:00:05.545)
	TestBinary Binary(VGhpcyBpcyBpdAo=)
	TestArray [
		"Test",
		"Test3\n"\
		"Test2",
		@@(AppendArray)
	]
	TestObject {
		"Test2": "Test",
		"Test4\n"\
		"TestLine": "Test\n"\
		            "TestLine",
		'Test5': 'Test',
		'': 'Test',
		Test6: "Without quotes",
		`@(Test) Test`: "Evaluated"
	}  // Comment
	TestObjectReference {
		"Test2": "Test",
		"Test4": @(Other->Function('Str', "Str2", @(Other3), `@(Other4) Test`, true, [
			"Array",
			false
		], {
			Object: "Value"
		}, "TestLine\n"\
		   "TestLine2\n"\
		   "TestLine3")),
		'Test5': 'Test',
		'': 'Test',
		Test6: "Without quotes",
		`@(Test) Test`: "Evaluated",
		`@(DynamicValue)`: @(Other->Function('Str', "Str2", @(Other3), `@(Other4) Test`, {
			"JSON": "Value"
		}, [
			"JSON",
			"Array"
		])),
		<<: [
			{
				"Test5": "Value"
			},
			@(Target:Compile.AppendObject),
			@(Target:Compile.AppendObject2),
			@@(Target:Compile.AppendObjectArray)
		]
	}  // Comment
	{
		!!Condition true
	}
}
)---";
				NFile::CFile::fs_WriteStringToFile("/Temp/Test.reg", RegistryStr);
				DMibExpect(fParseGenerate(RegistryStr), ==, RegistryStr);
			};
		}

	private:
		NStr::CStr mp_TestFilePath;
	};

	DMibTestRegister(CBuildSystemRegistry_Tests, Malterlib::Encoding);
}
