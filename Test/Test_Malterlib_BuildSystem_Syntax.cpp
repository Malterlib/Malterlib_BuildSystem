// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Time/Time>
#include <Mib/Test/Exception>
#include <Mib/BuildSystem/Registry>
#include <Mib/BuildSystem/BuildSystem>
#include <Mib/Encoding/JsonShortcuts>

#ifdef DPlatformFamily_Windows
#define DPrefixLocation_1_7 "Test.MHeader(1,7):"
#define DPrefixLocation_1_12 "Test.MHeader(1,12):"
#define DPrefixLocation_1_14 "Test.MHeader(1,14):"
#else
#define DPrefixLocation_1_7 "Test.MHeader:1:7:"
#define DPrefixLocation_1_12 "Test.MHeader:1:12:"
#define DPrefixLocation_1_14 "Test.MHeader:1:14:"
#endif

namespace
{
	using namespace NMib;
	using namespace NMib::NEncoding;
	using namespace NMib::NStr;
	using namespace NMib::NContainer;
	using namespace NMib::NBuildSystem;
	using namespace NMib::NTime;
	using namespace NMib::NException;
	using namespace NMib::NStorage;

	struct CBuildSystemSyntax_Tests : public NTest::CTest
	{
		CBuildSystemSyntax_Tests()
		{
		}

		CBuildSystemSyntax::CRootValue fs_ParseString(CStr const &_String)
		{
			CBuildSystemSyntax::CIdentifier Identifier;
			CBuildSystemRegistry RegistryTest;

			CStringCache StringCache;
			CBuildSystemRegistryParseContext ParseContext(StringCache);

			RegistryTest.f_ParseStrWithContext(ParseContext, "Value " + _String, "Test.MHeader");
			return RegistryTest.f_GetChildren().f_GetRoot()->f_GetThisValue();
		}

		void f_TestJson()
		{
			DMibTestSuite("String")
			{
				auto const &Value = fs_ParseString("\"String\"").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
				DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsString());
				DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_String(), ==, "String");
				DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_String().f_GetUserData(), ==, EJsonStringType_DoubleQuote);
			};
			DMibTestSuite("SingleQuoteString")
			{
				auto const &Value = fs_ParseString("'String'").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
				DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsString());
				DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_String(), ==, "String");
				DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_String().f_GetUserData(), ==, EJsonStringType_SingleQuote);
			};
			DMibTestSuite("Integer")
			{
				auto const &Value = fs_ParseString("55").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
				DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsInteger());
				DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_Integer(), ==, 55);
			};
			DMibTestSuite("Float")
			{
				auto const &Value = fs_ParseString("55.5").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
				DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsFloat());
				DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_Float(), ==, 55.5);
			};
			DMibTestSuite("Null")
			{
				auto const &Value = fs_ParseString("null").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
				DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsNull());
			};
			DMibTestSuite("Boolean")
			{
				auto const &Value = fs_ParseString("true").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
				DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsBoolean());
				DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_Boolean(), ==, true);
			};
		}

		void f_TestDate()
		{
			auto const &Value = fs_ParseString("date(2019-05-29)").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsDate());
			DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_Date(), ==, CTimeConvert::fs_CreateTime(2019, 05, 29));
			DMibExpectException
				(
					fs_ParseString("date(-1900-11-15)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"-\" as a date: Failed to parse \"-\" as a integer. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;

			DMibExpectException
				(
					fs_ParseString("date()")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"\" as a date: Missing year. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
			DMibExpectException
				(
					fs_ParseString("date(2019)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019\" as a date: Missing month. ""Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
			DMibExpectException
				(
					fs_ParseString("date(2019-11)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019-11\" as a date: Missing day. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
			DMibExpectException
				(
					fs_ParseString("date(2019-13-15)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019-13-15\" as a date: Invalid month. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
			DMibExpectException
				(
					fs_ParseString("date(2019-11-33)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019-11-33\" as a date: Invalid day. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
		}

		void f_TestDateMinute()
		{
			auto const &Value = fs_ParseString("date(2019-05-29 11:11)").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsDate());
			DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_Date(), ==, CTimeConvert::fs_CreateTime(2019, 05, 29, 11, 11));

			DMibExpectException
				(
					fs_ParseString("date(2019-11-15 33)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019-11-15 33\" as a date: Invalid hour. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
			DMibExpectException
				(
					fs_ParseString("date(2019-11-15 13:61)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019-11-15 13:61\" as a date: Invalid minute. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
		}

		void f_TestDateSecond()
		{
			auto const &Value = fs_ParseString("date(2019-05-29 11:11:06)").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsDate());
			DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_Date(), ==, CTimeConvert::fs_CreateTime(2019, 05, 29, 11, 11, 06));
			DMibExpectException
				(
					fs_ParseString("date(2019-11-15 13:50:62)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019-11-15 13:50:62\" as a date: Invalid second. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
		}

		void f_TestDateSecondFraction()
		{
			auto const &Value = fs_ParseString("date(2019-05-29 11:00:05.545)").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsDate());
			DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_Date(), ==, CTimeConvert::fs_CreateTime(2019, 05, 29, 11, 00, 05, 0.545));
			DMibExpectException
				(
					fs_ParseString("date(2019-11-15 13:50:50.o56)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019-11-15 13:50:50.\" as a date: Failed to parse \".\" as a float. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
			DMibExpectException
				(
					fs_ParseString("date(2019-11-15 13:50:50.1.647)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_12 " Failed to parse \"2019-11-15 13:50:50.\" as a date: Failed to parse \".\" as a float. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]"_f
						, {}
					)
				)
			;
		}

		void f_TestBinary()
		{
			auto const &Value = fs_ParseString("binary(VGhpcyBpcyBpdAo=)").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Value.f_GetAsType<CEJsonSorted>().f_IsBinary());
			DMibExpect(Value.f_GetAsType<CEJsonSorted>().f_Binary(), ==, (CByteVector{'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'i', 't', '\n'}));
			DMibExpectException
				(
					fs_ParseString("binary(156..6)")
					, DMibExceptionInstanceParse
					(
						DPrefixLocation_1_14 " Unexpected character in Base64 string: ."_f
						, {}
					)
				)
			;
		}

		void f_TestObjectDynamic()
		{
			auto const &Value = fs_ParseString
				(
					"{ \"doubleQuote\": 1, 'singleQuote': 2, noQuote: 3, `evalString`: 4, <<: [{'inner': 5}, Expression, ExpressionAppend...] }"
				)
				.m_Value.m_Value
			;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CObject>());

			auto &Object = Value.f_GetAsType<CBuildSystemSyntax::CObject>();

			DMibAssertTrue(Object.m_Object.f_FindEqual(CBuildSystemSyntax::CObjectKey{"doubleQuote"}));
			DMibAssertTrue(Object.m_Object.f_FindEqual(CBuildSystemSyntax::CObjectKey{"singleQuote"}));
			DMibAssertTrue(Object.m_Object.f_FindEqual(CBuildSystemSyntax::CObjectKey{"noQuote"}));

			auto EvalStringKey = CBuildSystemSyntax::CObjectKey
				{
					CBuildSystemSyntax::CEvalString{NContainer::TCVector<CBuildSystemSyntax::CEvalStringToken>{CBuildSystemSyntax::CEvalStringToken{"evalString"}}}
				}
			;
			DMibAssertTrue(Object.m_Object.f_FindEqual(EvalStringKey));

			auto AppendKey = CBuildSystemSyntax::CObjectKey{CBuildSystemSyntax::CAppendObject()};
			DMibAssertTrue(Object.m_Object.f_FindEqual(AppendKey));

			auto &AppendArrayValue = fg_Const(Object.m_Object)[AppendKey].m_Value.f_Get();
			DMibAssertTrue(AppendArrayValue.m_Value.f_IsOfType<CBuildSystemSyntax::CArray>());

			auto &AppendArray = AppendArrayValue.m_Value.f_GetAsType<CBuildSystemSyntax::CArray>();
			DMibAssert(AppendArray.m_Array.f_GetLen(), ==, 3);

			DMibExpectTrue(AppendArray.m_Array[0].f_Get().m_Value.f_IsOfType<NEncoding::CEJsonSorted>());
			DMibExpectTrue(AppendArray.m_Array[1].f_Get().m_Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
			DMibExpectTrue(AppendArray.m_Array[2].f_Get().m_Value.f_IsOfType<CBuildSystemSyntax::CExpressionAppend>());

			DMibExpectException(fs_ParseString("{ <<: 5 }"), DMibExceptionInstanceParse(DPrefixLocation_1_7 " error: Append object only supports objects, arrays or expressions", {}));
			DMibExpectException
				(
					fs_ParseString("{ <<: [5] }"), DMibExceptionInstanceParse(DPrefixLocation_1_7 " error: Append object array only supports objects expressions or append expressions", {})
				)
			;
		}

		void f_TestArrayStatic()
		{
			auto const &Value = fs_ParseString("[1, 2, 3]").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<NEncoding::CEJsonSorted>());
			auto &Array = Value.f_GetAsType<NEncoding::CEJsonSorted>();

			DMibExpect(Array, ==, (_[1, 2, 3]));
		}

		void f_TestObjectStatic()
		{
			auto const &Value = fs_ParseString
				(
					"{ \"doubleQuote\": 1, 'singleQuote': 2, noQuote: 3}"
				)
				.m_Value.m_Value
			;
			DMibAssertTrue(Value.f_IsOfType<NEncoding::CEJsonSorted>());

			auto &Object = Value.f_GetAsType<NEncoding::CEJsonSorted>();

			DMibExpect(Object, ==, (_={"doubleQuote"_= 1, "singleQuote"_= 2, "noQuote"_= 3}));
		}

		void f_TestArrayDynamic()
		{
			auto const &Value = fs_ParseString("[1, 2, Exprission]").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CArray>());
			auto &Object = Value.f_GetAsType<CBuildSystemSyntax::CArray>();

			DMibAssert(Object.m_Array.f_GetLen(), ==, 3);
			DMibAssertTrue(Object.m_Array[0].f_Get().m_Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Object.m_Array[1].f_Get().m_Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Object.m_Array[2].f_Get().m_Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
			DMibAssertTrue(Object.m_Array[0].f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_IsInteger());
			DMibAssertTrue(Object.m_Array[1].f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_IsInteger());
			DMibAssert(Object.m_Array[0].f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_Integer(), ==, 1);
			DMibAssert(Object.m_Array[1].f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_Integer(), ==, 2);
		}

		void f_TestEvalString()
		{
			auto const &Value = fs_ParseString("`String @(Value)@(Target:Target.Test->Function())@(\"Test\"->Function())@(Value<@(DynamicProp).ArrayProp[0].Prop2>)`").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CEvalString>());
			auto &EvalString = Value.f_GetAsType<CBuildSystemSyntax::CEvalString>();
			DMibAssert(EvalString.m_Tokens.f_GetLen(), ==, 5);
			auto &StringToken = EvalString.m_Tokens[0];
			DMibAssertTrue(StringToken.m_Token.f_IsOfType<CStr>());
			DMibExpect(StringToken.m_Token.f_GetAsType<CStr>(), ==,  "String ");
			DMibAssertTrue(EvalString.m_Tokens[1].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			DMibAssertTrue(EvalString.m_Tokens[2].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			DMibAssertTrue(EvalString.m_Tokens[3].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			DMibAssertTrue(EvalString.m_Tokens[4].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
		}

		void f_TestEvalStringEscape()
		{
			auto const &Value = fs_ParseString("`String @(Value)@(Target:Target.Test->Function())@(\"Test\"->Function())@@@@(Value<@@(DynamicProp).ArrayProp[0].Prop2>)@@E`").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CEvalString>());
			auto &EvalString = Value.f_GetAsType<CBuildSystemSyntax::CEvalString>();
			DMibAssert(EvalString.m_Tokens.f_GetLen(), ==, 5);
			auto &StringToken = EvalString.m_Tokens[0];
			DMibAssertTrue(StringToken.m_Token.f_IsOfType<CStr>());
			DMibExpect(StringToken.m_Token.f_GetAsType<CStr>(), ==,  "String ");
			DMibAssertTrue(EvalString.m_Tokens[1].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			DMibAssertTrue(EvalString.m_Tokens[2].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			DMibAssertTrue(EvalString.m_Tokens[3].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			DMibAssertTrue(EvalString.m_Tokens[4].m_Token.f_IsOfType<CStr>());
			DMibExpect(EvalString.m_Tokens[4].m_Token.f_GetAsType<CStr>(), ==, "@@(Value<@(DynamicProp).ArrayProp[0].Prop2>)@E");
		}

		void f_TestExpression()
		{
			{
				DMibTestPath("Identifier");
				auto const &Value = fs_ParseString("Test").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Expression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Expression.f_IsOfType<CBuildSystemSyntax::CParam>());
				auto &Param = Expression.f_GetAsType<CBuildSystemSyntax::CParam>().m_Param;
				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.f_IsNameConstantString());
				DMibExpect(Identifier.f_NameConstantString(), ==, "Test");
			}
			{
				DMibTestPath("IdentifierSpecific");
				auto const &Value = fs_ParseString("Workspace:Target.Test").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Expression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Expression.f_IsOfType<CBuildSystemSyntax::CParam>());
				auto &Param = Expression.f_GetAsType<CBuildSystemSyntax::CParam>().m_Param;
				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.f_IsNameConstantString());
				DMibExpect(Identifier.f_NameConstantString(), ==, "Test");
				DMibExpect(Identifier.m_EntityType, ==, EEntityType_Workspace);
				DMibExpect(Identifier.m_PropertyType, ==, EPropertyType_Target);
			}
			{
				DMibTestPath("IdentifierAccessor");
				auto const &Value = fs_ParseString("Workspace:Target.Test<@(DynamicProp).ArrayProp[2].Prop2[SubscriptProp]>").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Expression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;

				DMibAssertTrue(Expression.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>());
				auto &JsonAccessor = Expression.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>().f_Get();
				auto &Param = JsonAccessor.m_Param.m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.f_IsNameConstantString());
				DMibExpect(Identifier.f_NameConstantString(), ==, "Test");
				DMibExpect(Identifier.m_EntityType, ==, EEntityType_Workspace);
				DMibExpect(Identifier.m_PropertyType, ==, EPropertyType_Target);

				DMibAssert(JsonAccessor.m_Accessors.f_GetLen(), ==, 5);
				DMibExpectTrue(JsonAccessor.m_Accessors[0].m_Accessor.f_IsOfType<CBuildSystemSyntax::CExpression>());
				DMibExpectTrue(JsonAccessor.m_Accessors[1].m_Accessor.f_IsOfType<CStr>());
				DMibAssertTrue(JsonAccessor.m_Accessors[2].m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>());
				DMibAssertTrue(JsonAccessor.m_Accessors[2].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<uint32>());
				DMibExpect(JsonAccessor.m_Accessors[2].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_GetAsType<uint32>(), ==, 2);
				DMibExpectTrue(JsonAccessor.m_Accessors[3].m_Accessor.f_IsOfType<CStr>());
				DMibExpectTrue(JsonAccessor.m_Accessors[4].m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>());
				DMibAssertTrue(JsonAccessor.m_Accessors[4].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<CBuildSystemSyntax::CExpression>());
			}
			{
				DMibTestPath("IdentifierArrayAccessor");
				auto const &Value = fs_ParseString("Workspace:Target.Test<[0]>").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Expression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;

				DMibAssertTrue(Expression.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>());
				auto &JsonAccessor = Expression.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>().f_Get();
				auto &Param = JsonAccessor.m_Param.m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.f_IsNameConstantString());
				DMibExpect(Identifier.f_NameConstantString(), ==, "Test");
				DMibExpect(Identifier.m_EntityType, ==, EEntityType_Workspace);
				DMibExpect(Identifier.m_PropertyType, ==, EPropertyType_Target);

				DMibExpect(JsonAccessor.m_Accessors.f_GetLen(), ==, 1);
				DMibAssertTrue(JsonAccessor.m_Accessors[0].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<uint32>());
				DMibExpect(JsonAccessor.m_Accessors[0].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_GetAsType<uint32>(), ==, 0);
			}
			{
				DMibTestPath("DynamicExpression");
				auto const &Value = fs_ParseString("@('Test')").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Expression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Expression.f_IsOfType<CBuildSystemSyntax::CParam>());
				auto &Param = Expression.f_GetAsType<CBuildSystemSyntax::CParam>().m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.m_Name.f_IsOfType<CBuildSystemSyntax::CEvalString>());
				auto &EvalStringTokens = Identifier.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>().m_Tokens;

				DMibAssert(EvalStringTokens.f_GetLen(), ==, 1);
				DMibExpectTrue(EvalStringTokens[0].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			}
			{
				DMibTestPath("DynamicExpressionPrefix");
				auto const &Value = fs_ParseString("Prefix_@('Test')").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Expression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Expression.f_IsOfType<CBuildSystemSyntax::CParam>());
				auto &Param = Expression.f_GetAsType<CBuildSystemSyntax::CParam>().m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.m_Name.f_IsOfType<CBuildSystemSyntax::CEvalString>());
				auto &EvalStringTokens = Identifier.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>().m_Tokens;

				DMibAssert(EvalStringTokens.f_GetLen(), ==, 2);
				DMibAssertTrue(EvalStringTokens[0].m_Token.f_IsOfType<NStr::CStr>());
				DMibExpect(EvalStringTokens[0].m_Token.f_GetAsType<NStr::CStr>(), ==, "Prefix_");
				DMibExpectTrue(EvalStringTokens[1].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			}
			{
				DMibTestPath("DynamicExpressionSuffix");
				auto const &Value = fs_ParseString("@('Test')_Suffix").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Expression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Expression.f_IsOfType<CBuildSystemSyntax::CParam>());
				auto &Param = Expression.f_GetAsType<CBuildSystemSyntax::CParam>().m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.m_Name.f_IsOfType<CBuildSystemSyntax::CEvalString>());
				auto &EvalStringTokens = Identifier.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>().m_Tokens;

				DMibAssert(EvalStringTokens.f_GetLen(), ==, 2);
				DMibExpectTrue(EvalStringTokens[0].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
				DMibAssertTrue(EvalStringTokens[1].m_Token.f_IsOfType<NStr::CStr>());
				DMibExpect(EvalStringTokens[1].m_Token.f_GetAsType<NStr::CStr>(), ==, "_Suffix");
			}
		}

		void f_TestExpressionFunction()
		{
			{
				DMibTestPath("String");
				auto const &Value = fs_ParseString("\"Test\"->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CEJsonSorted>());
				auto &String = Param.f_GetAsType<CEJsonSorted>();
				DMibAssertTrue(String.f_IsString());
				DMibExpect(String.f_String(), ==, "Test");
			}
			{
				DMibTestPath("SingleQuoteString");
				auto const &Value = fs_ParseString("'Test'->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CEJsonSorted>());
				auto &String = Param.f_GetAsType<CEJsonSorted>();
				DMibAssertTrue(String.f_IsString());
				DMibExpect(String.f_String(), ==, "Test");
			}
			{
				DMibTestPath("Integer");
				auto const &Value = fs_ParseString("5->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CEJsonSorted>());
				auto &String = Param.f_GetAsType<CEJsonSorted>();
				DMibAssertTrue(String.f_IsInteger());
				DMibExpect(String.f_Integer(), ==, 5);
			}
			{
				DMibTestPath("Float");
				auto const &Value = fs_ParseString("5.5->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CEJsonSorted>());
				auto &String = Param.f_GetAsType<CEJsonSorted>();
				DMibAssertTrue(String.f_IsFloat());
				DMibExpect(String.f_Float(), ==, 5.5);
			}
			{
				DMibTestPath("Null");
				auto const &Value = fs_ParseString("null->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CEJsonSorted>());
				auto &String = Param.f_GetAsType<CEJsonSorted>();
				DMibAssertTrue(String.f_IsNull());
			}
			{
				DMibTestPath("Boolean");
				auto const &Value = fs_ParseString("true->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CEJsonSorted>());
				auto &String = Param.f_GetAsType<CEJsonSorted>();
				DMibAssertTrue(String.f_IsBoolean());
				DMibExpect(String.f_Boolean(), ==, true);
			}
			{
				DMibTestPath("EvalString");
				auto const &Value = fs_ParseString("`Test`->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CBuildSystemSyntax::CEvalString>());
				auto &EvalStringTokens = Param.f_GetAsType<CBuildSystemSyntax::CEvalString>().m_Tokens;
				DMibAssert(EvalStringTokens.f_GetLen(), ==, 1);
				DMibAssertTrue(EvalStringTokens[0].m_Token.f_IsOfType<NStr::CStr>());
				DMibExpect(EvalStringTokens[0].m_Token.f_GetAsType<NStr::CStr>(), ==, "Test");
			}
			{
				DMibTestPath("Identifier");
				auto const &Value = fs_ParseString("Test->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibExpect(Identifier.f_NameConstantString(), ==, "Test");
			}
			{
				DMibTestPath("IdentifierSpecific");
				auto const &Value = fs_ParseString("Workspace:Target.Test->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibExpect(Identifier.f_NameConstantString(), ==, "Test");
				DMibExpect(Identifier.m_EntityType, ==, EEntityType_Workspace);
				DMibExpect(Identifier.m_PropertyType, ==, EPropertyType_Target);
			}
			{
				DMibTestPath("IdentifierAccessor");
				auto const &Value = fs_ParseString("Workspace:Target.Test<@(DynamicProp).ArrayProp[2].Prop2[SubscriptProp]>->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &FunctionParam = FunctionCall.m_Params[0].m_Param;

				DMibAssertTrue(FunctionParam.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>());
				auto &Expression = FunctionParam.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>().f_Get().m_Expression;

				DMibAssertTrue(Expression.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>());
				auto &JsonAccessor = Expression.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>().f_Get();
				auto &Param = JsonAccessor.m_Param.m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibExpect(Identifier.f_NameConstantString(), ==, "Test");
				DMibExpect(Identifier.m_EntityType, ==, EEntityType_Workspace);
				DMibExpect(Identifier.m_PropertyType, ==, EPropertyType_Target);

				DMibExpect(JsonAccessor.m_Accessors.f_GetLen(), ==, 5);
				DMibExpectTrue(JsonAccessor.m_Accessors[0].m_Accessor.f_IsOfType<CBuildSystemSyntax::CExpression>());
				DMibExpectTrue(JsonAccessor.m_Accessors[1].m_Accessor.f_IsOfType<CStr>());
				DMibAssertTrue(JsonAccessor.m_Accessors[2].m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>());
				DMibAssertTrue(JsonAccessor.m_Accessors[2].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<uint32>());
				DMibExpect(JsonAccessor.m_Accessors[2].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_GetAsType<uint32>(), ==, 2);
				DMibExpectTrue(JsonAccessor.m_Accessors[3].m_Accessor.f_IsOfType<CStr>());
				DMibExpectTrue(JsonAccessor.m_Accessors[4].m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>());
				DMibAssertTrue(JsonAccessor.m_Accessors[4].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<CBuildSystemSyntax::CExpression>());
			}
			{
				DMibTestPath("PostFunctionAccessor");
				auto const &Value = fs_ParseString("Workspace:Target.Test->Func()<@(DynamicProp).ArrayProp[2].Prop2[SubscriptProp]>").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());

				auto &JsonAccessorExpression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(JsonAccessorExpression.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>());
				auto &JsonAccessor = JsonAccessorExpression.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>().f_Get();

				DMibAssertTrue(JsonAccessor.m_Param.m_Param.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>());
				auto &Function = JsonAccessor.m_Param.m_Param.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>().f_Get().m_Expression;

				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());

				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);

				auto &Param = FunctionCall.m_Params[0].m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibExpect(Identifier.f_NameConstantString(), ==, "Test");
				DMibExpect(Identifier.m_EntityType, ==, EEntityType_Workspace);
				DMibExpect(Identifier.m_PropertyType, ==, EPropertyType_Target);

				DMibExpect(JsonAccessor.m_Accessors.f_GetLen(), ==, 5);
				DMibExpectTrue(JsonAccessor.m_Accessors[0].m_Accessor.f_IsOfType<CBuildSystemSyntax::CExpression>());
				DMibExpectTrue(JsonAccessor.m_Accessors[1].m_Accessor.f_IsOfType<CStr>());
				DMibAssertTrue(JsonAccessor.m_Accessors[2].m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>());
				DMibAssertTrue(JsonAccessor.m_Accessors[2].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<uint32>());
				DMibExpect(JsonAccessor.m_Accessors[2].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_GetAsType<uint32>(), ==, 2);
				DMibExpectTrue(JsonAccessor.m_Accessors[3]. m_Accessor.f_IsOfType<CStr>());
				DMibExpectTrue(JsonAccessor.m_Accessors[4].m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>());
				DMibAssertTrue(JsonAccessor.m_Accessors[4].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<CBuildSystemSyntax::CExpression>());
			}
			{
				DMibTestPath("FunctionAccessor");
				auto const &Value = fs_ParseString("Func(5)<@(DynamicProp).ArrayProp[2].Prop2[SubscriptProp]>").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());

				auto &JsonAccessorExpression = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(JsonAccessorExpression.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>());
				auto &JsonAccessor = JsonAccessorExpression.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CJsonAccessor>>().f_Get();

				DMibAssertTrue(JsonAccessor.m_Param.m_Param.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>());
				auto &Function = JsonAccessor.m_Param.m_Param.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CExpression>>().f_Get().m_Expression;

				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());

				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);

				auto &Param = FunctionCall.m_Params[0].m_Param;

				DMibAssertTrue(Param.f_IsOfType<CEJsonSorted>());
				auto &Constant = Param.f_GetAsType<CEJsonSorted>();
				DMibExpect(Constant, ==, CEJsonSorted(5));

				DMibExpect(JsonAccessor.m_Accessors.f_GetLen(), ==, 5);
				DMibExpectTrue(JsonAccessor.m_Accessors[0].m_Accessor.f_IsOfType<CBuildSystemSyntax::CExpression>());
				DMibExpectTrue(JsonAccessor.m_Accessors[1].m_Accessor.f_IsOfType<CStr>());
				DMibAssertTrue(JsonAccessor.m_Accessors[2].m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>());
				DMibAssertTrue(JsonAccessor.m_Accessors[2].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<uint32>());
				DMibExpect(JsonAccessor.m_Accessors[2].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_GetAsType<uint32>(), ==, 2);
				DMibExpectTrue(JsonAccessor.m_Accessors[3]. m_Accessor.f_IsOfType<CStr>());
				DMibExpectTrue(JsonAccessor.m_Accessors[4].m_Accessor.f_IsOfType<CBuildSystemSyntax::CJsonSubscript>());
				DMibAssertTrue(JsonAccessor.m_Accessors[4].m_Accessor.f_GetAsType<CBuildSystemSyntax::CJsonSubscript>().m_Index.f_IsOfType<CBuildSystemSyntax::CExpression>());
			}
			{
				DMibTestPath("Object");
				auto const &Value = fs_ParseString("{Test: 5}->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<NEncoding::CEJsonSorted>());
				auto &Object = Param.f_GetAsType<NEncoding::CEJsonSorted>();
				DMibExpect(Object, ==, _={"Test"_= 5});
			}
			{
				DMibTestPath("Array");
				auto const &Value = fs_ParseString("[5]->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<NEncoding::CEJsonSorted>());
				auto &Array = Param.f_GetAsType<NEncoding::CEJsonSorted>();
				DMibExpect(Array, ==, _[5]);
			}
			{
				DMibTestPath("WildcardString");
				auto const &Value = fs_ParseString("~'Test'->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CBuildSystemSyntax::CWildcardString>());
				auto &String = Param.f_GetAsType<CBuildSystemSyntax::CWildcardString>().m_String;
				DMibAssertTrue(String.f_IsOfType<NStr::CStr>());
				DMibExpect(String.f_GetAsType<NStr::CStr>(), ==, "Test");
			}
			{
				DMibTestPath("WildcardStringEval");
				auto const &Value = fs_ParseString("~`Test`->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CBuildSystemSyntax::CWildcardString>());
				auto &String = Param.f_GetAsType<CBuildSystemSyntax::CWildcardString>().m_String;
				DMibAssertTrue(String.f_IsOfType<CBuildSystemSyntax::CEvalString>());
				auto &EvalString = String.f_GetAsType<CBuildSystemSyntax::CEvalString>();
				DMibAssert(EvalString.m_Tokens.f_GetLen(), ==, 1);
				auto &StringToken = EvalString.m_Tokens[0];
				DMibAssertTrue(StringToken.m_Token.f_IsOfType<CStr>());
				DMibExpect(StringToken.m_Token.f_GetAsType<CStr>(), ==,  "Test");
			}
			{
				DMibTestPath("DynamicExpression");
				auto const &Value = fs_ParseString("@('Test')->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.m_Name.f_IsOfType<CBuildSystemSyntax::CEvalString>());
				auto &EvalStringTokens = Identifier.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>().m_Tokens;

				DMibAssert(EvalStringTokens.f_GetLen(), ==, 1);
				DMibExpectTrue(EvalStringTokens[0].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			}
			{
				DMibTestPath("DynamicExpressionPrefix");
				auto const &Value = fs_ParseString("Prefix_@('Test')->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.m_Name.f_IsOfType<CBuildSystemSyntax::CEvalString>());
				auto &EvalStringTokens = Identifier.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>().m_Tokens;

				DMibAssert(EvalStringTokens.f_GetLen(), ==, 2);
				DMibAssertTrue(EvalStringTokens[0].m_Token.f_IsOfType<NStr::CStr>());
				DMibExpect(EvalStringTokens[0].m_Token.f_GetAsType<NStr::CStr>(), ==, "Prefix_");
				DMibExpectTrue(EvalStringTokens[1].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
			}
			{
				DMibTestPath("DynamicExpressionSuffix");
				auto const &Value = fs_ParseString("@('Test')_Suffix->Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibAssertTrue(Identifier.m_Name.f_IsOfType<CBuildSystemSyntax::CEvalString>());
				auto &EvalStringTokens = Identifier.m_Name.f_GetAsType<CBuildSystemSyntax::CEvalString>().m_Tokens;

				DMibAssert(EvalStringTokens.f_GetLen(), ==, 2);
				DMibExpectTrue(EvalStringTokens[0].m_Token.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpression>>());
				DMibAssertTrue(EvalStringTokens[1].m_Token.f_IsOfType<NStr::CStr>());
				DMibExpect(EvalStringTokens[1].m_Token.f_GetAsType<NStr::CStr>(), ==, "_Suffix");
			}
			{
				DMibTestPath("AppendExpression");
				auto const &Value = fs_ParseString("Func(ValueArray...)").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Property);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>());
				auto &ExpressionAppend = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>().f_Get();

				DMibAssertTrue(ExpressionAppend.m_Expression.f_IsOfType<CBuildSystemSyntax::CParam>());
				auto &ExpressionParam = ExpressionAppend.m_Expression.f_GetAsType<CBuildSystemSyntax::CParam>().m_Param;

				DMibAssertTrue(ExpressionParam.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = ExpressionParam.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibExpect(Identifier.f_NameConstantString(), ==, "ValueArray");
				DMibExpect(Identifier.m_EntityType, ==, EEntityType_Invalid);
				DMibExpect(Identifier.m_PropertyType, ==, EPropertyType_Property);
			}
			{
				DMibTestPath("AppendExpressionWithPropertyType");
				auto const &Value = fs_ParseString("Compile.Func(Compile.ValueArray...)").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Compile);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;

				DMibAssertTrue(Param.f_IsOfType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>());
				auto &ExpressionAppend = Param.f_GetAsType<TCIndirection<CBuildSystemSyntax::CExpressionAppend>>().f_Get();

				DMibAssertTrue(ExpressionAppend.m_Expression.f_IsOfType<CBuildSystemSyntax::CParam>());
				auto &ExpressionParam = ExpressionAppend.m_Expression.f_GetAsType<CBuildSystemSyntax::CParam>().m_Param;

				DMibAssertTrue(ExpressionParam.f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CIdentifier>>());
				auto &Identifier = ExpressionParam.f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CIdentifier>>().f_Get();
				DMibExpect(Identifier.f_NameConstantString(), ==, "ValueArray");
				DMibExpect(Identifier.m_EntityType, ==, EEntityType_Invalid);
				DMibExpect(Identifier.m_PropertyType, ==, EPropertyType_Compile);
			}
			{
				DMibTestPath("StringWithPropertyType");
				auto const &Value = fs_ParseString("\"Test\"->Compile.Func()").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CExpression>());
				auto &Function = Value.f_GetAsType<CBuildSystemSyntax::CExpression>().m_Expression;
				DMibAssertTrue(Function.f_IsOfType<CBuildSystemSyntax::CFunctionCall>());
				auto &FunctionCall = Function.f_GetAsType<CBuildSystemSyntax::CFunctionCall>();
				DMibExpect(FunctionCall.m_PropertyKey.m_Name, ==, "Func");
				DMibExpect(FunctionCall.m_PropertyKey.f_GetType(), ==, EPropertyType_Compile);
				DMibAssert(FunctionCall.m_Params.f_GetLen(), ==, 1);
				auto &Param = FunctionCall.m_Params[0].m_Param;
				DMibAssertTrue(Param.f_IsOfType<CEJsonSorted>());
				auto &String = Param.f_GetAsType<CEJsonSorted>();
				DMibAssertTrue(String.f_IsString());
				DMibExpect(String.f_String(), ==, "Test");
			}
			DMibExpectException
				(
					fs_ParseString("InvalidPropertyType.Func(@(ValueArray)...)")
					, DMibExceptionInstanceParse(DPrefixLocation_1_7 " error: Unknown property type 'InvalidPropertyType'"_f, {})
				)
			;
			DMibExpectException
				(
					fs_ParseString("\"Test\"->InvalidPropertyType.Func()")
					, DMibExceptionInstanceParse(DPrefixLocation_1_7 " error: Unknown property type 'InvalidPropertyType'"_f, {})
				)
			;
		}

		void f_TestDefineDefaultType()
		{
			DMibTestPath("DefaultType");
			{
				DMibTestPath("int");
				auto const &Value = fs_ParseString(": int").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Define.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);
			}
			{
				DMibTestPath("float");
				auto const &Value = fs_ParseString(": float").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Define.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_FloatingPoint);
			}
			{
				DMibTestPath("bool");
				auto const &Value = fs_ParseString(": bool").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Define.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_Boolean);
			}
			{
				DMibTestPath("string");
				auto const &Value = fs_ParseString(": string").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Define.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_String);
			}
			{
				DMibTestPath("binary");
				auto const &Value = fs_ParseString(": binary").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Define.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_Binary);
			}
			{
				DMibTestPath("date");
				auto const &Value = fs_ParseString(": date").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Define.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_Date);
			}
			{
				DMibTestPath("any");
				auto const &Value = fs_ParseString(": any").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Define.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_Any);
			}
			{
				DMibTestPath("void");
				auto const &Value = fs_ParseString(": void").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Define.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_Void);
			}
			DMibExpectException
				(
					fs_ParseString("any")
					, DMibExceptionInstanceParse(DPrefixLocation_1_7 " Type 'any' can only be used inside define statements"_f, {})
				)
			;
		}

		void f_TestDefineDefaulted()
		{
			DMibTestPath("Defaulted");
			{
				DMibTestPath("int");
				auto const &Value = fs_ParseString(": int = 5").m_Value.m_Value;
				DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
				auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;

				DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CTypeDefaulted>());
				auto &Defaulted = Define.f_GetAsType<CBuildSystemSyntax::CTypeDefaulted>();

				DMibAssertTrue(Defaulted.m_Type.f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &DefaultType = Defaulted.m_Type.f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
				DMibExpect(DefaultType, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

				DMibAssertTrue(Defaulted.m_DefaultValue.m_Param.f_IsOfType<NEncoding::CEJsonSorted>());
				DMibExpect(Defaulted.m_DefaultValue.m_Param.f_GetAsType<NEncoding::CEJsonSorted>(), ==, CEJsonSorted(5));
			}
		}

		void f_TestDefineArray()
		{
			DMibTestPath("Array");
			auto const &Value = fs_ParseString(": [int]").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
			auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CArrayType>());
			auto &ArrayType = Define.f_GetAsType<CBuildSystemSyntax::CArrayType>().m_Type.f_Get().m_Type;
			DMibAssertTrue(ArrayType.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			auto &ArrayInnerType = ArrayType.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type;
			DMibExpect(ArrayInnerType, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);
			DMibExpectException(fs_ParseString(": [55]"), DMibExceptionInstanceParse(DPrefixLocation_1_7 " error: Invalid type: 55", {}));
			DMibExpectException(fs_ParseString(": [int, float]"), DMibExceptionInstanceParse(DPrefixLocation_1_7 " error: Array definitions should have one entry", {}));
		}

		void f_TestDefineUserType()
		{
			DMibTestPath("UserType");
			auto const &Value = fs_ParseString(": type(CTest)").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
			auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CUserType>());
			auto &UserType = Define.f_GetAsType<CBuildSystemSyntax::CUserType>().m_Name;
			DMibExpect(UserType, ==, "CTest");
		}

		void f_TestDefineOneOf()
		{
			DMibTestPath("OneOf");
			auto const &Value = fs_ParseString(": one_of(int, float, 5)").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
			auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::COneOf>());
			auto &OneOf = Define.f_GetAsType<CBuildSystemSyntax::COneOf>().m_OneOf;
			DMibAssert(OneOf.f_GetLen(), ==, 3);

			DMibAssertTrue(OneOf[0].f_IsOfType<TCIndirection<CBuildSystemSyntax::CType>>());
			auto &Type0 = OneOf[0].f_GetAsType<TCIndirection<CBuildSystemSyntax::CType>>().f_Get().m_Type;
			DMibAssertTrue(Type0.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(Type0.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibAssertTrue(OneOf[1].f_IsOfType<TCIndirection<CBuildSystemSyntax::CType>>());
			auto &Type1 = OneOf[1].f_GetAsType<TCIndirection<CBuildSystemSyntax::CType>>().f_Get().m_Type;
			DMibAssertTrue(Type1.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(Type1.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_FloatingPoint);

			DMibAssertTrue(OneOf[2].f_IsOfType<CEJsonSorted>());
			auto &Type2 = OneOf[2].f_GetAsType<CEJsonSorted>();
			DMibAssertTrue(Type2.f_IsInteger());
			DMibExpect(Type2.f_Integer(), ==, 5);
		}

		void f_TestDefineObject()
		{
			DMibTestPath("Object");
			auto const &Value = fs_ParseString(": {mem1: int, \"mem2\": float, 'mem3': any, mem4: [any], mem5: one_of(int, float), mem6: {...: any}, 'mem7?': any}").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());
			auto &Define = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Define.f_IsOfType<CBuildSystemSyntax::CClassType>());
			auto const &ClassType = Define.f_GetAsType<CBuildSystemSyntax::CClassType>();

			DMibAssertFalse(ClassType.m_OtherKeysType);
			DMibAssert(ClassType.m_Members.f_GetLen(), ==, 7);

			{
				DMibAssertTrue(ClassType.m_Members.f_Exists("mem1"));
				auto &Member1 = ClassType.m_Members["mem1"];
				DMibExpectFalse(Member1.m_bOptional);
				DMibAssertTrue(Member1.m_Type.f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &Member1Type = Member1.m_Type.f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>();
				DMibExpect(Member1Type.m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);
			}
			{
				DMibAssertTrue(ClassType.m_Members.f_Exists("mem2"));
				auto &Member2 = ClassType.m_Members["mem2"];
				DMibExpectFalse(Member2.m_bOptional);
				DMibAssertTrue(Member2.m_Type.f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &Member2Type = Member2.m_Type.f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>();
				DMibExpect(Member2Type.m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_FloatingPoint);
			}
			{
				DMibAssertTrue(ClassType.m_Members.f_Exists("mem3"));
				auto &Member3 = ClassType.m_Members["mem3"];
				DMibExpectFalse(Member3.m_bOptional);
				DMibAssertTrue(Member3.m_Type.f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &Member3Type = Member3.m_Type.f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>();
				DMibExpect(Member3Type.m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Any);
			}
			{
				DMibAssertTrue(ClassType.m_Members.f_Exists("mem4"));
				auto &Member4 = ClassType.m_Members["mem4"];
				DMibExpectFalse(Member4.m_bOptional);
				DMibAssertTrue(Member4.m_Type.f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CArrayType>());
				auto &Member4Type = Member4.m_Type.f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CArrayType>().m_Type.f_Get().m_Type;
				DMibAssertTrue(Member4Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				DMibExpect(Member4Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Any);
			}
			{
				DMibAssertTrue(ClassType.m_Members.f_Exists("mem5"));
				auto &Member5 = ClassType.m_Members["mem5"];
				DMibExpectFalse(Member5.m_bOptional);
				DMibAssertTrue(Member5.m_Type.f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::COneOf>());
				auto &Member5Type = Member5.m_Type.f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::COneOf>();
				DMibAssert(Member5Type.m_OneOf.f_GetLen(), ==, 2);
				DMibAssertTrue(Member5Type.m_OneOf[0].f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>());
				DMibAssertTrue(Member5Type.m_OneOf[0].f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>().f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				DMibExpect
					(
						Member5Type.m_OneOf[0].f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>().f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type
						, ==
						, CBuildSystemSyntax::CDefaultType::EType_Integer
					)
				;
				DMibAssertTrue(Member5Type.m_OneOf[1].f_IsOfType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>());
				DMibAssertTrue(Member5Type.m_OneOf[1].f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>().f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				DMibExpect
					(
						Member5Type.m_OneOf[1].f_GetAsType<NStorage::TCIndirection<CBuildSystemSyntax::CType>>().f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type
						, ==
						, CBuildSystemSyntax::CDefaultType::EType_FloatingPoint
					)
				;
			}
			{
				DMibAssertTrue(ClassType.m_Members.f_Exists("mem6"));
				auto &Member6 = ClassType.m_Members["mem6"];
				DMibExpectFalse(Member6.m_bOptional);
				DMibAssertTrue(Member6.m_Type.f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CClassType>());
				auto &Member6Class = Member6.m_Type.f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CClassType>();
				DMibAssertTrue(Member6Class.m_OtherKeysType);
				auto &OtherKeysType = Member6Class.m_OtherKeysType.f_Get().f_Get().m_Type;
				DMibAssertTrue(OtherKeysType.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				DMibExpect(OtherKeysType.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Any);
			}
			{
				DMibAssertTrue(ClassType.m_Members.f_Exists("mem7"));
				auto &Member7 = ClassType.m_Members["mem7"];
				DMibExpectTrue(Member7.m_bOptional);
				DMibAssertTrue(Member7.m_Type.f_Get().m_Type.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
				auto &Member7Type = Member7.m_Type.f_Get().m_Type.f_GetAsType<CBuildSystemSyntax::CDefaultType>();
				DMibExpect(Member7Type.m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Any);
			}
		}

		void f_TestOperator()
		{
			auto const &Value = fs_ParseString("< 5").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::COperator>());
			auto &Operator = Value.f_GetAsType<CBuildSystemSyntax::COperator>();
			DMibAssert(Operator.m_Operator, ==, CBuildSystemSyntax::COperator::EOperator_LessThan);
			DMibAssertTrue(Operator.m_Right.f_Get().m_Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Operator.m_Right.f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_IsInteger());
			DMibExpect(Operator.m_Right.f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_Integer(), ==, 5);
		}

		void f_TestOperatorAppend()
		{
			auto const &Value = fs_ParseString("=+ 5").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::COperator>());
			auto &Operator = Value.f_GetAsType<CBuildSystemSyntax::COperator>();
			DMibAssert(Operator.m_Operator, ==, CBuildSystemSyntax::COperator::EOperator_Append);
			DMibAssertTrue(Operator.m_Right.f_Get().m_Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Operator.m_Right.f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_IsInteger());
			DMibExpect(Operator.m_Right.f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_Integer(), ==, 5);
		}

		void f_TestOperatorPrepend()
		{
			auto const &Value = fs_ParseString("+= 5").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::COperator>());
			auto &Operator = Value.f_GetAsType<CBuildSystemSyntax::COperator>();
			DMibAssert(Operator.m_Operator, ==, CBuildSystemSyntax::COperator::EOperator_Prepend);
			DMibAssertTrue(Operator.m_Right.f_Get().m_Value.f_IsOfType<CEJsonSorted>());
			DMibAssertTrue(Operator.m_Right.f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_IsInteger());
			DMibExpect(Operator.m_Right.f_Get().m_Value.f_GetAsType<CEJsonSorted>().f_Integer(), ==, 5);
		}

		void f_TestDefine()
		{
			f_TestDefineDefaultType();
			f_TestDefineArray();
			f_TestDefineUserType();
			f_TestDefineOneOf();
			f_TestDefineObject();
			f_TestDefineDefaulted();
		}

		void f_TestFunctionNoParam()
		{
			DMibTestPath("NoParam");
			auto const &Value = fs_ParseString("function() int").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());

			auto &Type = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>());

			auto &FunctionType = Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

			DMibExpectTrue(FunctionType.m_Parameters.f_IsEmpty());

			auto &ReturnType = FunctionType.m_Return.f_Get().m_Type;
			DMibAssertTrue(ReturnType.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ReturnType.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);
		}

		void f_TestFunctionParams()
		{
			DMibTestPath("Params");
			auto const &Value = fs_ParseString("function(int _Param0, float _Param1) int").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());

			auto &Type = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>());

			auto &FunctionType = Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

			auto &ReturnType = FunctionType.m_Return.f_Get().m_Type;
			DMibAssertTrue(ReturnType.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ReturnType.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibAssert(FunctionType.m_Parameters.f_GetLen(), ==, 2);

			DMibExpect(FunctionType.m_Parameters[0].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_None);
			DMibExpect(FunctionType.m_Parameters[0].m_Name, ==, "_Param0");
			auto &ParamType0 = FunctionType.m_Parameters[0].m_Type.f_Get().m_Type;
			DMibAssertTrue(ParamType0.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType0.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibExpect(FunctionType.m_Parameters[1].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_None);
			DMibExpect(FunctionType.m_Parameters[1].m_Name, ==, "_Param1");
			auto &ParamType1 = FunctionType.m_Parameters[1].m_Type.f_Get().m_Type;
			DMibAssertTrue(ParamType1.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType1.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_FloatingPoint);
		}

		void f_TestFunctionParamsEllipsis()
		{
			DMibTestPath("ParamsEllipsis");
			auto const &Value = fs_ParseString("function(int _Param0, float _Param1, any... p_Params) int").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());

			auto &Type = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>());

			auto &FunctionType = Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

			auto &ReturnType = FunctionType.m_Return.f_Get().m_Type;
			DMibAssertTrue(ReturnType.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ReturnType.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibAssert(FunctionType.m_Parameters.f_GetLen(), ==, 3);

			DMibExpect(FunctionType.m_Parameters[0].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_None);
			DMibExpect(FunctionType.m_Parameters[0].m_Name, ==, "_Param0");
			auto &ParamType0 = FunctionType.m_Parameters[0].m_Type.f_Get().m_Type;
			DMibAssertTrue(ParamType0.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType0.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibExpect(FunctionType.m_Parameters[1].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_None);
			DMibExpect(FunctionType.m_Parameters[1].m_Name, ==, "_Param1");
			auto &ParamType1 = FunctionType.m_Parameters[1].m_Type.f_Get().m_Type;
			DMibAssertTrue(ParamType1.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType1.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_FloatingPoint);

			DMibExpect(FunctionType.m_Parameters[2].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_Ellipsis);
			DMibExpect(FunctionType.m_Parameters[2].m_Name, ==, "p_Params");
			auto &ParamType2 = FunctionType.m_Parameters[2].m_Type.f_Get().m_Type;
			DMibAssertTrue(ParamType2.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType2.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Any);
		}

		void f_TestFunctionParamsOptional()
		{
			DMibTestPath("ParamsOptional");
			auto const &Value = fs_ParseString("function(int _Param0, float? _Param1) int").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());

			auto &Type = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>());

			auto &FunctionType = Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

			auto &ReturnType = FunctionType.m_Return.f_Get().m_Type;
			DMibAssertTrue(ReturnType.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ReturnType.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibAssert(FunctionType.m_Parameters.f_GetLen(), ==, 2);

			DMibExpect(FunctionType.m_Parameters[0].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_None);
			DMibExpect(FunctionType.m_Parameters[0].m_Name, ==, "_Param0");
			auto &ParamType0 = FunctionType.m_Parameters[0].m_Type.f_Get().m_Type;
			DMibAssertTrue(ParamType0.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType0.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibExpect(FunctionType.m_Parameters[1].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_Optional);
			DMibExpect(FunctionType.m_Parameters[1].m_Name, ==, "_Param1");
			auto &OptionalParamType1 = FunctionType.m_Parameters[1].m_Type.f_Get();
			DMibAssertTrue(OptionalParamType1.m_bOptional);

			auto &ParamType1 = OptionalParamType1.m_Type;
			DMibAssertTrue(ParamType1.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType1.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_FloatingPoint);
		}

		void f_TestFunctionParamsDefaulted()
		{
			DMibTestPath("ParamsDefaulted");
			auto const &Value = fs_ParseString("function(int _Param0, float _Param1 = 5.5) int = 5").m_Value.m_Value;
			DMibAssertTrue(Value.f_IsOfType<CBuildSystemSyntax::CDefine>());

			auto &Type = Value.f_GetAsType<CBuildSystemSyntax::CDefine>().m_Type.m_Type;
			DMibAssertTrue(Type.f_IsOfType<CBuildSystemSyntax::CFunctionType>());

			auto &FunctionType = Type.f_GetAsType<CBuildSystemSyntax::CFunctionType>();

			auto &ReturnTypeDefaulted = FunctionType.m_Return.f_Get().m_Type;
			DMibAssertTrue(ReturnTypeDefaulted.f_IsOfType<CBuildSystemSyntax::CTypeDefaulted>());

			auto &Defaulted = ReturnTypeDefaulted.f_GetAsType<CBuildSystemSyntax::CTypeDefaulted>();
			DMibAssertTrue(Defaulted.m_DefaultValue.m_Param.f_IsOfType<NEncoding::CEJsonSorted>());
			DMibExpect(Defaulted.m_DefaultValue.m_Param.f_GetAsType<NEncoding::CEJsonSorted>(), ==, CEJsonSorted(5));

			auto &ReturnType = Defaulted.m_Type.f_Get().m_Type;
			DMibAssertTrue(ReturnType.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ReturnType.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibAssert(FunctionType.m_Parameters.f_GetLen(), ==, 2);

			DMibExpect(FunctionType.m_Parameters[0].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_None);
			DMibExpect(FunctionType.m_Parameters[0].m_Name, ==, "_Param0");
			auto &ParamType0 = FunctionType.m_Parameters[0].m_Type.f_Get().m_Type;
			DMibAssertTrue(ParamType0.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType0.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_Integer);

			DMibExpect(FunctionType.m_Parameters[1].m_ParamType, ==, CBuildSystemSyntax::CFunctionParameter::EParamType_None);
			DMibExpect(FunctionType.m_Parameters[1].m_Name, ==, "_Param1");
			auto &OptionalParamType1 = FunctionType.m_Parameters[1].m_Type.f_Get().m_Type;
			DMibAssertTrue(OptionalParamType1.f_IsOfType<CBuildSystemSyntax::CTypeDefaulted>());

			auto &DefaultedParam = OptionalParamType1.f_GetAsType<CBuildSystemSyntax::CTypeDefaulted>();
			DMibAssertTrue(DefaultedParam.m_DefaultValue.m_Param.f_IsOfType<NEncoding::CEJsonSorted>());
			DMibExpect(DefaultedParam.m_DefaultValue.m_Param.f_GetAsType<NEncoding::CEJsonSorted>(), ==, CEJsonSorted(5.5));

			auto &ParamType1 = DefaultedParam.m_Type.f_Get().m_Type;
			DMibAssertTrue(ParamType1.f_IsOfType<CBuildSystemSyntax::CDefaultType>());
			DMibExpect(ParamType1.f_GetAsType<CBuildSystemSyntax::CDefaultType>().m_Type, ==, CBuildSystemSyntax::CDefaultType::EType_FloatingPoint);
		}

		void f_TestFunction()
		{
			f_TestFunctionNoParam();
			f_TestFunctionParams();
			f_TestFunctionParamsEllipsis();
			f_TestFunctionParamsOptional();
			f_TestFunctionParamsDefaulted();
		}

		void f_TestFormat()
		{
			DMibExpect
				(
					("{}"_f << fs_ParseString("\"String\"")).f_GetStr()
					, ==
					, "\"String\""
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("'String'")).f_GetStr()
					, ==
					, "'String'"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("'String\\n'\\\n'String'")).f_GetStr()
					, ==
					, "'String\\n'\\\n'String'"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("55")).f_GetStr()
					, ==
					, "55"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("55.5")).f_GetStr()
					, ==
					, "55.5"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("true")).f_GetStr()
					, ==
					, "true"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("null")).f_GetStr()
					, ==
					, "null"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("undefined")).f_GetStr()
					, ==
					, "undefined"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("date(2019-05-29 11:00:05.545)")).f_GetStr()
					, ==
					, "date(2019-05-29 11:00:05.545)"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("binary(VGhpcyBpcyBpdAo=)")).f_GetStr()
					, ==
					, "binary(VGhpcyBpcyBpdAo=)"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("[\"Test\", 5]")).f_GetStr()
					, ==
					, "[\n"
					"	\"Test\",\n"
					"	5\n"
					"]"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("{test: 5, 'test2': 5, \"test3\": 5, `test4 @(Test->Test())`: 5}")).f_GetStr()
					, ==
					, "{test: 5, 'test2': 5, \"test3\": 5, `test4 @(Test->Test())`: 5}"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("{test: 5, 'test2': 5, \"test3\": 5, `test4 @(Test->Test(`Other @(Other)`))`: 5}")).f_GetStr()
					, ==
					, "{test: 5, 'test2': 5, \"test3\": 5, `test4 @(Test->Test(`Other @(Other)`))`: 5}"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("~\"*Value*\"")).f_GetStr()
					, ==
					, "~\"*Value*\""
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("~'*Value*'")).f_GetStr()
					, ==
					, "~'*Value*'"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("~`*Value*`")).f_GetStr()
					, ==
					, "~`*Value*`"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("< Value<Property.ArrayProp[0].Prop2>")).f_GetStr()
					, ==
					, "< Value<Property.ArrayProp[0].Prop2>"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("Value<@(DynamicProp).ArrayProp[IntProp].Prop2>")).f_GetStr()
					, ==
					, "Value<@(DynamicProp).ArrayProp[IntProp].Prop2>"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("`String`")).f_GetStr()
					, ==
					, "`String`"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("`Test @(Value<@(DynamicProp).ArrayProp[IntProp].Prop2>)`")).f_GetStr()
					, ==
					, "`Test @(Value<@(DynamicProp).ArrayProp[IntProp].Prop2>)`"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString(": {mem1: int, \"mem2\": float, 'mem3': any, mem4: [any], mem5: one_of(int, float), mem6: {...: any}, 'mem7?': any}")).f_GetStr()
					, ==
					, ": {mem1: int, \"mem2\": float, 'mem3': any, mem4: [any], mem5: one_of(int, float), mem6: {...: any}, 'mem7?': any}"
				)
			;
			DMibExpect
				(
					("{}"_f << fs_ParseString("define {mem1: int, \"mem2\": float, 'mem3': any, mem4: [any], mem5: one_of(int, float), mem6: {...: any}, 'mem7?': any}")).f_GetStr()
					, ==
					, "define {mem1: int, \"mem2\": float, 'mem3': any, mem4: [any], mem5: one_of(int, float), mem6: {...: any}, 'mem7?': any}"
				)
			;
		}

		void f_TestCompare()
		{
			DMibExpect
				(
					fs_ParseString(": {mem1: int, \"mem2\": float, 'mem3': any, mem4: [any], mem5: one_of(int, float), mem6: {...: any}, 'mem7?': any}")
					, ==
					, fs_ParseString(": {mem1: int, \"mem2\": float, 'mem3': any, mem4: [any], mem5: one_of(int, float), mem6: {...: any}, 'mem7?': any}")
				)
			;
			DMibExpect
				(
					fs_ParseString("5")
					, <
					, fs_ParseString(": {mem1: int, \"mem2\": float, 'mem3': any, mem4: [any], mem5: one_of(int, float), mem6: {...: any}, 'mem7?': any}")
				)
			;
			DMibExpect
				(
					fs_ParseString("5")
					, <
					, fs_ParseString("6")
				)
			;
		}

		void f_DoTests()
		{
			f_TestJson();

			DMibTestSuite("Date")
			{
				f_TestDate();
			};
			DMibTestSuite("DateMinute")
			{
				f_TestDateMinute();
			};
			DMibTestSuite("DateSecond")
			{
				f_TestDateSecond();
			};
			DMibTestSuite("DateSecondFraction")
			{
				f_TestDateSecondFraction();
			};
			DMibTestSuite("Binary")
			{
				f_TestBinary();
			};
			DMibTestSuite("EvalString")
			{
				f_TestEvalString();
			};
			DMibTestSuite("EvalStringEscape")
			{
				f_TestEvalStringEscape();
			};
			DMibTestSuite("ObjectDynamic")
			{
				f_TestObjectDynamic();
			};
			DMibTestSuite("ArrayDynamic")
			{
				f_TestArrayDynamic();
			};
			DMibTestSuite("ObjectStatic")
			{
				f_TestObjectStatic();
			};
			DMibTestSuite("ArrayStatic")
			{
				f_TestArrayStatic();
			};
			DMibTestSuite("Expression")
			{
				f_TestExpression();
			};
			DMibTestSuite("ExpressionFunction")
			{
				f_TestExpressionFunction();
			};
			DMibTestSuite("Define")
			{
				f_TestDefine();
			};
			DMibTestSuite("Operator")
			{
				f_TestOperator();
			};
			DMibTestSuite("OperatorAppend")
			{
				f_TestOperatorAppend();
			};
			DMibTestSuite("OperatorPrepend")
			{
				f_TestOperatorPrepend();
			};
			DMibTestSuite("Format")
			{
				f_TestFormat();
			};
			DMibTestSuite("Compare")
			{
				f_TestCompare();
			};
			DMibTestSuite("Function")
			{
				f_TestFunction();
			};
		}

	private:
		CStr mp_TestFilePath;
	};

	DMibTestRegister(CBuildSystemSyntax_Tests, Malterlib::BuildSystem);
}
