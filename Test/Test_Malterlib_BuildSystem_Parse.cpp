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
						auto DebugFile = NFile::CFile::fs_GetProgramDirectory() / "TestBuildSystemParse.reg";
						fg_TestAddCleanupPath(DebugFile);

						CBuildSystemRegistry RegistryTest;
						NFile::CFile::fs_WriteStringToFile(DebugFile, _Source);

						CStringCache StringCache;
						CBuildSystemRegistryParseContext ParseContext(StringCache);

						RegistryTest.f_ParseStrWithContext(ParseContext, _Source, DebugFile);
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
	TestInt 65 // Comment
	Type
	{
		namespace NTest
		{
			CTest: {
				"int": one_of(int, float)
			}
		}
	} // Comment
	TestDefine: {
		...: any,
		"array": [
			one_of("Value1", "Value2", {
				"int": one_of(int, float)
			})
		],
		"binary": binary,
		"bool": bool? = false ? true : false,
		"date": date,
		"float": float,
		"int": one_of(int, float),
		"object": {
			"test": string
		},
		"string": string,
		"test": type(CTest)?
	} // Comment
	TestFunction function(int _Param0, float _Param1) float // Comment
	TestFunctionDefaulted function(int _Param0, float _Param1 = 5.05) float = 10.0 // Comment
	TestFunctionDefaultedDynamic function(int _Param0, float _Param1 = 5.05) string = `@(TestStr) suffix @(_Param0->JsonToString()) @(_Param1->JsonToString())` // Comment
	TestFunctionDefaultedEquals function(int _Param0, float _Param1 = 5.05) float = 10.0 // Comment
	TestFunctionDefaultedObj function(int _Param0, type(CObject) _Param1 = {
		Test: "Testing",
		"Test1": "Testing 2",
		'Test2': "Testing 3"
	}) type(CObject) = {
		Test: "Testing",
		"Test1": "Testing 2",
		'Test2': "Testing 3"
	} // Comment
	TestFunctionOptional function(int _Param0, float? _Param1) float // Comment
	TestFunctionEllipsis function(int _Param0, float _Param0, any... p_Params) string // Comment
	TestFunctionEllipsisComplex function(int _Param0, float _Param0, one_of(type(CTest), int, "5")... p_Params) string // Comment
	TestDefineOneOf: one_of("Test", "Test2", int) // Comment
	TestDefineDefault: one_of("Test", "Test2", int, bool) = true // Comment
	TestDefineOptionalString: string? = true ? true : false // Comment
	TestDefineOptionalClass: {
		one: one_of("One"),
		two: one_of("Two")
	}? = true // Comment
	TestFloat 65.5 // Comment
	{
		TestFloat == undefined // Comment
	}
	TestBoolean true // Comment
	TestNull null // Comment
	TestUndefined undefined // Comment
	TestStringSingle 'String' // Comment
	TestStringSingle2 'String "Test"' // Comment
	TestStringSingle3 'String \'' // Comment
	TestStringEmpty "" // Comment
	TestStringEmpty2 '' // Comment
	TestStringEval `String` // Comment
	TestStringEvalEscape `String \` Post` // Comment
	TestStringEvalEscapeEval `String @@ String` // Comment
	TestStringEval2 `String @(Target:Target.Test->Function()) String` // Comment
	TestStringEval3 `String @("Test"->Function()) String` // Comment
	TestStringEval4 `String @('Test'->Function()) String` // Comment
	TestStringEval5 `String @(`@(Test)`->Function()) String` // Comment
	TestStringEval5 `String @(`@(Test)`->Function()->Function2()) String` // Comment
	TestStringEval6 `String @(`@(Test)`->Function("Param")) String` // Comment
	TestStringEval7 `String @(`@(Test)`->Function(Param)) String` // Comment
	TestStringEval8 `String @(Function(Param->Function("Param"))) String` // Comment
	TestStringEval9 `String @(Function(Param->Function("Param"), `TestLine\n`\
	                                                             `TestLine2`)) String` // Comment
	TestStringEval10 `String @(Function(Param->Function("Param"), "TestLine\n"\
	                                                              "TestLine2")) String` // Comment

	`TestStringEvalKey @(Target:Target.Test->Function()) String` "Value" // Comment
	`TestStringEvalKeyEscape \` Post` "Value" // Comment
	`TestStringEvalKey2 @(Target:Target.Test->Function()) String` "Value" // Comment
	`TestStringEvalKey3 @("Test"->Function()) String` "Value" // Comment
	`TestStringEvalKey4 @('Test'->Function()) String` "Value" // Comment
	`TestStringEvalKey5 @(`@(Test)`->Function()) String` "Value" // Comment
	`TestStringEvalKey5 @(`@(Test)`->Function()->Function2()) String` "Value" // Comment
	`TestStringEvalKey6 @(`@(Test)`->Function("Param")) String` "Value" // Comment
	`TestStringEvalKey7 @(`@(Test)`->Function(Param)) String` "Value" // Comment
	`TestStringEvalKey8 @(Function(Param->Function("Param"))) String` "Value" // Comment
	`TestStringEvalKey9 @(Function(Param->Function("Param"), `TestLine\n`\
	                                                         `TestLine2`)) String` "Value" // Comment
	`TestStringEvalKey10 @(Function(Param->Function("Param"), "TestLine\n"\
	                                                          "TestLine2")) String` "Value" // Comment
	TestReference Other // Comment
	TestReferenceFunc `Other`->ToString() // Comment
	TestObjectReference Other<Prop[0]> // Comment
	TestArrayReference Other<[0]> // Comment
	TestOptionalChainging1 Other<?.[0]> // Comment
	TestOptionalChainging2 Other<Test?.[0]> // Comment
	TestOptionalChainging3 Other<Test?.Test> // Comment
	TestOptionalChainging4 Other<?.Test> // Comment
	TestTernary Other ? Other1 : Other2 // Comment
	TestTernaryFunction function() string = Other ? Other1 : Other2 // Comment
	TestTernaryDefine: string = Other ? Other1 : Other2 // Comment
	TestTernaryWildcard (Other <==> ~"Test") ? Other1 : Other2 // Comment
	TestTernaryNotWildcard (Other <!=> ~"Test") ? Other1 : Other2 // Comment
	TestDynamicReferenceSuffix Prefix_@(Suffix) // Comment
	TestDynamicReferenceMiddle Prefix_@(Other)_Suffix // Comment
	TestDynamicReferencePrefix @(Other)_Suffix // Comment
	TestDynamicReferenceSuffixEscape Prefix\#_@(Suffix) // Comment
	TestDynamicReferenceMiddleEscape Prefix\#_@(Other)_\#Suffix // Comment
	TestDynamicReferencePrefixEscape @(Other)_\#Suffix // Comment
	TestDynamicReference @(Other) // Comment
	TestArrayFunctionExpansion Function(Other...) // Comment
	TestArrayFunctionExpansionCall Function("Test", Other()...) // Comment
	TestArrayFunctionExpansionConstant Function([
		"Test",
		"Test2"
	]...) // Comment
	TestFunctionIdentifierReference0 Function(&Test) // Comment
	TestFunctionIdentifierReference1 Function(&Property.Test) // Comment
	TestFunctionIdentifierReference2 Function(&Workspace:Property.Test) // Comment
	TestFunctionIdentifierReferenceDynamic0 Function(&Test_@(Test)) // Comment
	TestFunctionIdentifierReferenceDynamic1 Function(&Property.Test_@(Test)) // Comment
	TestFunctionIdentifierReferenceDynamic2 Function(&Workspace:Property.Test_@(Test)) // Comment
	TestFunctionIdentifierReferenceDynamicProp0 Function(&Test_@(Test)) // Comment
	TestFunctionIdentifierReferenceDynamicProp1 Function(&@(Test).Test_@(Test)) // Comment
	TestFunctionIdentifierReferenceDynamicProp2 Function(&Workspace:@(Test).Test_@(Test)) // Comment
	Children // Comment
	{
		"Test Test" "Test Test 2" // Comment
	}
	TestDate date(2019-05-29) // Comment
	TestMinute date(2019-05-29 11:11) // Comment
	TestSecond date(2019-05-29 11:11:06) // Comment
	TestFraction date(2019-05-29 11:00:05.545) // Comment
	TestBinary binary(VGhpcyBpcyBpdAo=) // Comment
	TestInObject {
		"Binary": binary(VGhpcyBpcyBpdAo=),
		"Date": date(2019-05-29)
	} // Comment
	TestArray [
		"Test",
		"Test3\n"\
		"Test2",
		AppendArray...
	] // Comment
	TestObject {
		'': 'Test',
		`@(Test(`Other @(Test)`)) Test`: "Evaluated",
		`@(Test) Test`: "Evaluated",
		"Test2": "Test",
		"Test4\n"\
		"TestLine": "Test\n"\
		            "TestLine",
		'Test5': 'Test',
		Test6: "Without quotes"
	} // Comment
	TestObjectReference {
		'': 'Test',
		<<: [
			{
				"Test5": "Value"
			},
			Target:Compile.AppendObject,
			Target:Compile.AppendObject2,
			Prop<Test>,
			Compile.AppendObject3<Test>,
			Target:Compile.AppendObject4<Test>,
			Target:Compile.AppendObjectArray...
		],
		`@(DynamicValue)`: Other->Function('Str', "Str2", Other3, `@(Other4) Test`, {
			"Json": "Value"
		}, [
			"Json",
			"Array"
		]),
		`@(Test) Test`: "Evaluated",
		"Test2": "Test",
		"Test4": Other->Function('Str', "Str2", Other3, `@(Other4) Test`, true, [
			"Array",
			false
		], {
			Object: "Value"
		}, "TestLine\n"\
		   "TestLine2\n"\
		   "TestLine3"),
		'Test5': 'Test',
		Test6: "Without quotes"
	}  // Comment
	{
		!!Condition true // Comment
	}
	TestArrayAppend += [
		"Test"
	] // Comment
	TestObjectAppend += {
		Key: "Value"
	} // Comment
	TestObjectAppendInner += {
		Key.Inner: "Value"
	} // Comment
	TestObjectAppendInnerAccessor #<Key.Inner> += "AppendString" // Comment
	TestObjectSetAccessor #<Key.Inner> "SetValue" // Comment
	TestArraySetAccessor #<[Other]> "SetValue" // Comment
	WildcardValue ~"*Value*" // Comment
	WildcardCondition "Value" // Comment
	{
		!!Value ~"*CompareTo*"
	}
	WildcardConditionInAnd "Value" // Comment
	{
		&
		{
			Value ~"*CompareTo*" // Comment
		}
	}
	WildcardInsideObject "Value" // Comment
	{
		!!Value {
			"Array": [
				~"*CompareTo*"
			],
			"Key": ~"*CompareTo*"
		} // Comment
	}
	Condition "Value"
	{
		!!Compile.Value<Test> true // Comment
		!!Target:Compile.Value<Test> true // Comment
		!!Value<Property.ArrayProp[0].Prop2> {
			"Array": [
				~'*CompareTo*'
			],
			"Key": ~"*CompareTo*"
		} // Comment
		!!Value<@(DynamicProp).ArrayProp[0].Prop2> {
			"Array": [
				~"*CompareTo*"
			],
			"Key": ~`*CompareTo*`
		} // Comment
		&
		{
			Value<Value1> < Value<Property.ArrayProp[0].Prop2> // Comment
			Value<Value1> != Value<@(DynamicProp).ArrayProp[0].Prop2> // Comment
			Value<Value1> >= Value<@(DynamicProp).ArrayProp[IntProp].Prop2> // Comment
		}
	}
	Condition2 "Value2" // Comment
	{ // Comment
		`Dynamic` == "Dynamic" // Comment
	} // Comment
	\true "Reserved" // Comment
	Test\.Test\#Test "Escaped" // Comment
	\ "Empty identifier" // Comment
	Test\ Space "Space identifier" // Comment
	TestBinaryOperator1 Value1 / Value2 // Comment
	TestBinaryOperator2 (Value1 / Value2) // Comment
	TestBinaryOperator3 Value1 / Value2 + Value3 // Comment
	TestBinaryOperator4 (Value1 / Value2 + Value3) // Comment
	TestBinaryOperator5 (Value1 / Value2 - Value3) // Comment
	TestBinaryOperator6 (Value1 / Value2 * Value3) // Comment
	TestBinaryOperator7 (Value1 / Value2 % Value3) // Comment
	TestBinaryOperator8 (Value1 / (Value2 % Value3)) // Comment
	TestBinaryOperator9 (Value1 ?? "Value") // Comment
	SearchPath Compile.SearchPath->f_RemoveElements("../../External/ninja"->MakeAbsolute() / "src") // Comment
	SearchPath2 "../../External/ninja"->MakeAbsolute() / "src" // Comment
	SearchPath3 true ? "../../External/ninja"->MakeAbsolute() / "src" : "" // Comment
	SearchPath4 "../../External/ninja"->MakeAbsolute() / "src" ? true : false // Comment
	TestAccessor0 {
		Value: "Yes"
	}<Value>
	TestAccessor1 _bValue ? {
		Value: "Yes"
	} : {
		Value: "No"
	}<Value>
	TestAccessor2 [
		{
			Value: "Yes"
		}
	]<[0].@(Value)>
}
)---";
				DMibExpect(fParseGenerate(RegistryStr), ==, RegistryStr);
			};
		}

	private:
		NStr::CStr mp_TestFilePath;
	};

	DMibTestRegister(CBuildSystemRegistry_Tests, Malterlib::BuildSystem);
}

