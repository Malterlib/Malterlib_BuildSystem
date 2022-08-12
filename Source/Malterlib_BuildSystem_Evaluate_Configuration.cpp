// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	namespace
	{
		struct CConfigKey
		{
			CStr m_ConfigType;
			CStr m_ConfigName;

			auto operator <=> (CConfigKey const &_Right) const = default;
		};

		struct CConfigTuple
		{
			TCMap<CConfigKey, CBuildSystemConfiguration const *> m_Keys;
			auto operator <=> (CConfigTuple const &_Right) const = default;
		};

	}

	CBuildSystemRegistry *CBuildSystem::CUserSettingsState::f_GetSection(CPropertyKey const &_Section)
	{
		if (auto pSection = m_Sections.f_FindEqual(_Section))
			return *pSection;

		auto &pNewSection = m_Sections[_Section];

		CBuildSystemRegistry TempRegistry;
		CStringCache StringCache;
		CBuildSystemRegistryParseContext Context(StringCache);

		TempRegistry.f_ParseStrWithContext(Context, "{} false"_f << fg_PropertyTypeToStr(_Section.f_GetType()));

		auto pChild = m_Registry.f_CreateChildNoPath(TempRegistry.f_GetChildren().f_GetRoot()->f_GetName(), true);
		pChild->f_SetThisValue({});
		pChild->f_SetWhiteSpace(ERegistryWhiteSpaceLocation_After, " // {}{\n}"_f << _Section.m_Name);

		pNewSection = pChild;

		m_bChanged = true;

		return pChild;
	}

	void CBuildSystem::CUserSettingsState::f_Parse(CStringCache &o_StringCache)
	{
		CStr AllWhitespace;

		for (auto &Property : m_Properties)
			m_Defined[m_Properties.fs_GetKey(Property)];

		auto fIsRoot = [&](CBuildSystemRegistry const &_Registry)
			{
				return &_Registry == &m_Registry;
			}
		;
		auto fDepth = [&](CBuildSystemRegistry const &_Registry)
			{
				mint Depth = 0;
				for (auto pReg = &_Registry; pReg; pReg = pReg->f_GetParent())
					++Depth;

				return Depth;
			}
		;
		auto fCheckWhitespace = [&](CPropertyKey const &_Section, CBuildSystemRegistry const &_Registry)
			{
				CStr WhiteSpace = _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_BeforeKey).f_Trim();
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_Between).f_Trim(), "\n");

				auto After = _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_After).f_Trim().f_SplitLine();
				if (!After.f_IsEmpty())
					After.f_Remove(0);

				fg_AddStrSep(WhiteSpace, CStr::fs_Join(After, "\n"), "\n");
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_BeforeChildScopeStart).f_Trim(), "\n");
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_AfterChildScopeStart).f_Trim(), "\n");
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_BeforeChildScopeEnd).f_Trim(), "\n");
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(ERegistryWhiteSpaceLocation_AfterChildScopeEnd).f_Trim(), "\n");

				for (auto &Line : WhiteSpace.f_SplitLine())
				{
					auto pParse = Line.f_GetStr();
					fg_ParseWhiteSpace(pParse);

					if (!fg_StrStartsWith(pParse, "//"))
						continue;

					pParse += 2;

					fg_ParseWhiteSpace(pParse);

					auto pParseStart = pParse;
					while (*pParse && !fg_CharIsWhiteSpace(*pParse))
						++pParse;

					if (!fg_CharIsWhiteSpace(*pParse))
						continue;

					CStr PropertyName(pParseStart, pParse - pParseStart);

					if (aint iDot = PropertyName.f_Find("."); iDot >= 0)
					{
						CStr PropertyType = PropertyName.f_Left(iDot);
						CStr Name = PropertyName.f_Extract(iDot + 1);
						m_Defined[CPropertyKey(o_StringCache, fg_PropertyTypeFromStr(PropertyType), Name)];
					}
					else
						m_Defined[CPropertyKey(o_StringCache, _Section.f_GetType(), PropertyName)];
				}
			}
		;
		auto fStringFromIdentifier = [](CBuildSystemRegistry &_Registry, CStr &o_String)
			{
				auto &Name = _Registry.f_GetName();
				if (!Name.f_IsValue())
					fs_ThrowError(_Registry, "Expected an identifier");

				auto &Value = Name.f_Value();

				if (!Value.f_IsIdentifier())
					fs_ThrowError(_Registry, "Expected an identifier");

				auto &Identifier = Value.f_Identifier();

				if (!Identifier.f_IsNameConstantString())
					fs_ThrowError(_Registry, "Only constant identifiers supported");

				if (Identifier.m_EntityType != EEntityType_Invalid)
					fs_ThrowError(_Registry, "Entity specifiers not supported supported");

				if (!Identifier.m_bEmptyPropertyType)
					return false;

				o_String = Identifier.f_NameConstantString();
				return true;
			}
		;
		auto fGetSection = [&](CBuildSystemRegistry &_Registry) -> CPropertyKey
			{
				CStr String;

				if (!fStringFromIdentifier(_Registry, String))
					return CPropertyKey{o_StringCache, "General"};

				auto pGetFrom = &_Registry;
				auto pParent = _Registry.f_GetParent();
				if (pParent && !fIsRoot(*pParent))
					pGetFrom = pParent;

				CStr GetFromNameString;
				if (!fStringFromIdentifier(*pGetFrom, GetFromNameString))
					return CPropertyKey{o_StringCache, "General"};

				auto WhiteSpace = pGetFrom->f_GetWhiteSpace(ERegistryWhiteSpaceLocation_After).f_Trim();
				if (!WhiteSpace.f_StartsWith("// "))
					return CPropertyKey{o_StringCache, "General"};

				WhiteSpace = WhiteSpace.f_Extract(3);

				CPropertyKey Section{o_StringCache, fg_PropertyTypeFromStr(GetFromNameString), WhiteSpace};
				if (auto Mapped = m_Sections(Section); Mapped.f_WasCreated())
					*Mapped = pGetFrom;

				return Section;
			}
		;

		m_Registry.f_ForEachInTree
			(
			 	[&](CBuildSystemRegistry const &_Registry)
			 	{
					if (fIsRoot(_Registry))
					{
						fCheckWhitespace(CPropertyKey{o_StringCache, "General"}, _Registry);
						return;
					}

					if (fDepth(_Registry) > 3)
						return;

					CPropertyKey Section = fGetSection(fg_RemoveQualifiers(_Registry));

					fCheckWhitespace(Section, _Registry);
				}
			)
		;
	}

	TCVector<TCVector<CConfigurationTuple>> CBuildSystem::fp_EvaluateConfigurationTuples(TCMap<CPropertyKey, CEJSONSorted> const &_InitialValues) const
	{
		TCSet<CConfigTuple> Tuples;

		for (auto iConfigType = mp_Data.m_ConfigurationTypes.f_GetIterator(); iConfigType; ++iConfigType)
		{
			TCSet<CConfigTuple> NewTuples;
			for (auto iConfig = iConfigType->m_Configurations.f_GetIterator(); iConfig; ++iConfig)
			{
				CConfigTuple Tuple;
				CConfigKey Key;
				Key.m_ConfigType = iConfigType->f_GetName();
				Key.m_ConfigName = iConfig->f_GetName();
				Tuple.m_Keys[Key] = iConfig;

				NewTuples[Tuple];
			}
			TCSet<CConfigTuple> OldTuples = fg_Move(Tuples);

			if (OldTuples.f_IsEmpty())
				Tuples = fg_Move(NewTuples);
			else
			{
				for (auto iOldTuple = OldTuples.f_GetIterator(); iOldTuple; ++iOldTuple)
				{
					for (auto iNewTuple = NewTuples.f_GetIterator(); iNewTuple; ++iNewTuple)
					{
						CConfigTuple Tuple = *iOldTuple;
						Tuple.m_Keys += iNewTuple->m_Keys;
						Tuples[Tuple];
					}
				}
			}
		}

		TCSet<CConfigTuple> FinalTuples;

		CBuildSystemData TempData;
		f_EvaluateData(TempData, _InitialValues, &mp_Data.m_RootEntity, false);

		CEntity Entity(&TempData.m_RootEntity);
		auto &RootEntityData = TempData.m_RootEntity.f_Data();

		for (auto &Properties : RootEntityData.m_Properties)
		{
			auto &PropertyKey = RootEntityData.m_Properties.fs_GetKey(Properties);
			for (auto &Property : Properties)
			{
				if (Property.m_pRegistry->f_GetLocation().m_File == mp_UserSettingsFileLocal)
				{
					auto Mapped = mp_UserSettingsLocal.m_Properties(PropertyKey);
					*Mapped = Property.m_pRegistry;
				}

				if (Property.m_pRegistry->f_GetLocation().m_File == mp_UserSettingsFileGlobal)
				{
					auto Mapped = mp_UserSettingsGlobal.m_Properties(PropertyKey);
					*Mapped = Property.m_pRegistry;
				}
			}
		}

		mp_UserSettingsLocal.f_Parse(mp_StringCache);
		mp_UserSettingsGlobal.f_Parse(mp_StringCache);

		struct CUserVariable
		{
			CStr m_Description;
			CStr m_DefaultValue;
		};

		TCMap<CPropertyKey, TCMap<CPropertyKey, CUserVariable, CPropertyKey::CCompareByString>, CPropertyKey::CCompareByString> UserVariablesBySection;

		for (auto &Definitions : RootEntityData.m_VariableDefinitions)
		{
			auto &VariableKey = RootEntityData.m_VariableDefinitions.fs_GetKey(Definitions);

			for (auto &Definition : Definitions)
			{
				auto iWhiteStart = Definition.m_Type.m_Whitespace.f_Find("// User");
				if (iWhiteStart < 0)
					continue;

				CStr Section = "General";
				CStr DefaultValue;

				CStr LineCandidate = Definition.m_Type.m_Whitespace.f_GetStr() + iWhiteStart;
				CStr Line = fg_GetStrLineSep(LineCandidate);
				CStr ToParse = Line.f_Extract(7);

				auto pParse = ToParse.f_GetStr();
				fg_ParseWhiteSpace(pParse);
				if (*pParse == '(')
				{
					++pParse;
					auto pStart = pParse;
					while (*pParse && *pParse != ')')
						++pParse;

					if (*pParse != ')')
						continue;
					Section = CStr(pStart, pParse - pStart);
					++pParse;
				}

				fg_ParseWhiteSpace(pParse);

				if (*pParse == '=')
				{
					++pParse;
					fg_ParseWhiteSpace(pParse);
					if (fg_StrStartsWith(pParse, "---("))
					{
						pParse += 4;
						auto pStart = pParse;
						while (*pParse && !fg_StrStartsWith(pParse, ")---"))
							++pParse;

						DefaultValue = CStr(pStart, pParse - pStart).f_Trim();

						if (fg_StrStartsWith(pParse, ")---"))
							pParse += 4;

						fg_ParseWhiteSpace(pParse);
					}
					else if (*pParse == '\"')
					{
						auto pStart = pParse;
						fg_ParseEscape<'\"'>(pParse, '\"');

						DefaultValue = CStr(pStart, pParse - pStart);

						fg_ParseWhiteSpace(pParse);
					}
					else
					{
						auto pStart = pParse;
						while (*pParse && *pParse != ':' && !fg_CharIsNewLine(*pParse))
							++pParse;

						DefaultValue = CStr(pStart, pParse - pStart).f_Trim();

						fg_ParseWhiteSpace(pParse);
					}
				}

				if (*pParse != ':')
					continue;

				++pParse;
				fg_ParseWhiteSpace(pParse);

				CStr Description = pParse;

				UserVariablesBySection[CPropertyKey{mp_StringCache, VariableKey.f_GetType(), Section}][VariableKey] 
					= {.m_Description = fg_Move(Description), .m_DefaultValue = fg_Move(DefaultValue)}
				;
			}
		}

		auto fAddSettings = [&](CUserSettingsState &o_State, CPropertyKey const &_VariableKey, CStr const &_Description, CPropertyKey const &_Section, CStr const &_DefaultValue)
			{
				auto Mapped = o_State.m_Defined(_VariableKey);

				if (Mapped.f_WasCreated())
				{
					o_State.m_bChanged = true;

					auto pSection = o_State.f_GetSection(_Section);
					auto pCommentReg = pSection;
					auto CommentType = ERegistryWhiteSpaceLocation_AfterChildScopeStart;
					if (pCommentReg->f_HasChildren())
					{
						pCommentReg = pCommentReg->f_GetChildren().f_FindLargest();
						if (pCommentReg->f_HasScope())
							CommentType = ERegistryWhiteSpaceLocation_AfterChildScopeEnd;
						else
							CommentType = ERegistryWhiteSpaceLocation_After;
					}

					CStr Comments = pCommentReg->f_GetWhiteSpace(CommentType).f_TrimRight("\r\n");
					CBuildSystemRegistry TempRegistry;
					CStringCache StringCache;
					CBuildSystemRegistryParseContext Context(StringCache);
					TempRegistry.f_ParseStrWithContext(Context, "{} false"_f << _VariableKey.m_Name);

					CStr ToAdd = "{\n}\t//{} {} // {}{\n}"_f
						<< TempRegistry.f_GenerateStr().f_Trim().f_RemoveSuffix(" false")
						<< (_DefaultValue.f_IsEmpty() ? CStr("\"\"") : _DefaultValue)
						<< _Description
					;

					Comments += ToAdd;

					pCommentReg->f_SetWhiteSpace(CommentType, Comments);
				}
			}
		;


		for (auto &UserVariables : UserVariablesBySection)
		{
			auto &Section = UserVariablesBySection.fs_GetKey(UserVariables);
			for (auto &UserVariable : UserVariables)
			{
				auto &VariableKey = UserVariables.fs_GetKey(UserVariable);

				fAddSettings(mp_UserSettingsLocal, VariableKey, UserVariable.m_Description, Section, UserVariable.m_DefaultValue);
				fAddSettings(mp_UserSettingsGlobal, VariableKey, UserVariable.m_Description, Section, UserVariable.m_DefaultValue);
			}
		}

		CEntityKey EntityKey;
		EntityKey.m_Type = EEntityType_Root;
		EntityKey.m_Name.m_Value = "Temp Configuration Child";
		auto &TupleEntity = Entity.m_ChildEntitiesMap(EntityKey, &Entity).f_GetResult();
		TupleEntity.f_DataWritable().m_Position = DMibBuildSystemFilePosition;

		for (auto iTuple = Tuples.f_GetIterator(); iTuple; ++iTuple)
		{
			TupleEntity.m_EvaluatedProperties.m_Properties.f_Clear();
			for (auto iKey = iTuple->m_Keys.f_GetIterator(); iKey; ++iKey)
			{
				CPropertyKey Key(mp_StringCache, EPropertyType_Property, iKey.f_GetKey().m_ConfigType);
				auto &Evaluated = TupleEntity.m_EvaluatedProperties.m_Properties[Key];
				if ((*iKey)->f_GetName().f_IsEmpty())
					Evaluated.m_Value = CEJSONSorted{};
				else
					Evaluated.m_Value = (*iKey)->f_GetName();
				Evaluated.m_Type = EEvaluatedPropertyType_External;
				Evaluated.m_pProperty = &mp_ExternalProperty[Key.f_GetType()];
			}

			bool bFailed = false;
			for (auto iKey = iTuple->m_Keys.f_GetIterator(); iKey; ++iKey)
			{
				if (!f_EvalCondition(TupleEntity, (*iKey)->m_Condition))
				{
					bFailed = true;
					break;
				}
			}

			if (!bFailed)
				FinalTuples[*iTuple];
		}

		TCVector<TCVector<CConfigurationTuple>> Ret;

		for (auto iTuple = FinalTuples.f_GetIterator(); iTuple; ++iTuple)
		{
			auto &ConfigTuple = Ret.f_Insert();
			for (auto iKey = iTuple->m_Keys.f_GetIterator(); iKey; ++iKey)
			{
				auto &Config = ConfigTuple.f_Insert();
				Config.m_Type = iKey.f_GetKey().m_ConfigType;
				Config.m_Name = (*iKey)->f_GetName();
				Config.m_Position = (*iKey)->m_Position;
			}
		}

		return Ret;
	}
}
