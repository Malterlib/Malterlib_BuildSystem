// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Registry.h"

#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NContainer
{
	using namespace NEncoding;
	using namespace NEncoding::NJSON;
	using namespace NBuildSystem;
	using namespace NStr;

	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::f_ParseValue(CJSON &o_Value, uch8 const *&o_pParse) const
	{
		auto pParse = o_pParse;
		if (*pParse == '`')
		{
			CJSON Value = f_ParseEvalStringToken(pParse);

			o_Value["$type"] = "BuildSystemToken";
			o_Value["$value"] = fg_Move(Value);

			o_pParse = pParse;
			return true;
		}
		else if (*pParse == '@')
		{
			++pParse;

			CStr Type;
			if (*pParse == '@')
			{
				++pParse;
				Type = "AppendExpression";
			}
			else
				Type = "Expression";

			fg_ParseWhiteSpace(pParse);

			o_Value["$type"] = "BuildSystemToken";
			o_Value["$value"] = f_ParseExpression(pParse, Type);

			o_pParse = pParse;
			return true;
		}

		return false;
	}

	template <>
	void TCRegistry_CustomValue<CBuildSystemRegistryValue>::fs_Generate<CStr>
		(
		 	CStr &o_Output
		 	, CBuildSystemRegistryValue const &_Value
		 	, bool _bForceEscape
		 	, mint _Level
		 	, CStr const &_PreData
		)
	{
		if (!_Value.f_IsValid())
		{
			if (_bForceEscape)
				o_Output += "undefined";
			return;
		}

		fg_GenerateJSONValue<CJSONParseContext>(o_Output, _Value.f_ToJSON(), _Level, "\t", true);
	}

	template <>
	bool TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext::fs_GenerateValue<TCRegistry_CustomValue<CBuildSystemRegistryValue>::CJSONParseContext, NStr::CStr>
		(
	   		CStr &o_String
		 	, CJSON const &_Value
		 	, mint _Depth
		 	, ch8 const *_pPrettySeparator
		)
	{
		// TODO: Handle without converting to EJSON
		CEJSON Value = CEJSON::fs_FromJSON(_Value);
		if (Value.f_IsDate())
		{
			auto DateTime = NTime::CTimeConvert(Value.f_Date()).f_ExtractDateTime();

			aint nComponents = 7;

			if (DateTime.m_Fraction == 0.0)
			{
				--nComponents;
				if (DateTime.m_Second == 0)
				{
					--nComponents;
					if (DateTime.m_Hour == 0 && DateTime.m_Minute == 0)
						nComponents -= 2;
				}
			}

			o_String += CStr::CFormat("Date({tc*})") << Value.f_Date() << nComponents;
			return true;
		}
		else if (Value.f_IsBinary())
		{
			o_String += CStr::CFormat("Binary({})") << fg_Base64Encode(Value.f_Binary());
			return true;
		}
		else if (Value.f_IsUserType())
		{
			auto &UserType = Value.f_UserType();
			if (UserType.m_Type == "BuildSystemToken")
			{
				CJSONParseContext::fs_GenerateExpression(o_String, UserType.m_Value, true, _Depth);
				return true;
			}
		}

		return false;
	}
}
