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

			bool operator < (CConfigKey const &_Right) const
			{
				if (m_ConfigType < _Right.m_ConfigType)
					return true;
				else if (m_ConfigType > _Right.m_ConfigType)
					return false;

				return m_ConfigName < _Right.m_ConfigName;
			}
		};

		struct CConfigTuple
		{
			TCMap<CConfigKey, CBuildSystemConfiguration const *> m_Keys;
			bool operator < (CConfigTuple const &_Right) const
			{
				return m_Keys < _Right.m_Keys;
			}
		};

	}

	CRegistryPreserveAndOrder_CStr *CBuildSystem::CUserSettingsState::f_GetSection(CPropertyKey const &_Section)
	{
		if (auto pSection = m_Sections.f_FindEqual(_Section))
			return *pSection;

		auto &pNewSection = m_Sections[_Section];

		CStr Name = CStr::CFormat("{}") << fg_PropertyTypeToStr(_Section.m_Type);
		//o_Registry.f_GetChildren().
		auto pChild = m_Registry.f_CreateChild(Name, true);
		pChild->f_SetThisValue("");
		pChild->f_SetWhiteSpace(EWhiteSpaceLocation_After, " // {}{\n}"_f << _Section.m_Name);

		pNewSection = pChild;
		return pChild;
	}

	void CBuildSystem::CUserSettingsState::f_Parse()
	{
		CStr AllWhitespace;

		for (auto &Property : m_Properties)
			m_Defined[m_Properties.fs_GetKey(Property)];

		auto fIsRoot = [&](CRegistryPreserveAndOrder_CStr const &_Registry)
			{
				return &_Registry == &m_Registry;
			}
		;
		auto fCheckWhitespace = [&](CPropertyKey const &_Section, CRegistryPreserveAndOrder_CStr const &_Registry)
			{
				CStr WhiteSpace = _Registry.f_GetWhiteSpace(EWhiteSpaceLocation_BeforeKey).f_Trim();
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(EWhiteSpaceLocation_Between).f_Trim(), "\n");

				auto After = _Registry.f_GetWhiteSpace(EWhiteSpaceLocation_After).f_Trim().f_SplitLine();
				if (!After.f_IsEmpty())
					After.f_Remove(0);

				fg_AddStrSep(WhiteSpace, CStr::fs_Join(After, "\n"), "\n");
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(EWhiteSpaceLocation_BeforeChildScopeStart).f_Trim(), "\n");
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(EWhiteSpaceLocation_AfterChildScopeStart).f_Trim(), "\n");
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(EWhiteSpaceLocation_BeforeChildScopeEnd).f_Trim(), "\n");
				fg_AddStrSep(WhiteSpace, _Registry.f_GetWhiteSpace(EWhiteSpaceLocation_AfterChildScopeEnd).f_Trim(), "\n");

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
						m_Defined[CPropertyKey{fg_PropertyTypeFromStr(PropertyType), Name}];
					}
					else
						m_Defined[CPropertyKey{_Section.m_Type, PropertyName}];
				}
			}
		;
		auto fGetSection = [&](CRegistryPreserveAndOrder_CStr &_Registry) -> CPropertyKey
			{
				if (_Registry.f_GetName().f_Find(".") >= 0)
					return CPropertyKey{"General"};

				auto pGetFrom = &_Registry;
				auto pParent = _Registry.f_GetParent();
				if (pParent && !fIsRoot(*pParent))
					pGetFrom = pParent;

				if (pGetFrom->f_GetName().f_Find(".") >= 0)
					return CPropertyKey{"General"};

				auto WhiteSpace = pGetFrom->f_GetWhiteSpace(EWhiteSpaceLocation_After).f_Trim();
				if (!WhiteSpace.f_StartsWith("// "))
					return CPropertyKey{"General"};

				WhiteSpace = WhiteSpace.f_Extract(3);

				CPropertyKey Section{fg_PropertyTypeFromStr(pGetFrom->f_GetName()), WhiteSpace};
				if (auto Mapped = m_Sections(Section); Mapped.f_WasCreated())
					*Mapped = pGetFrom;

				return Section;
			}
		;

		m_Registry.f_ForEachInTree
			(
			 	[&](CRegistryPreserveAndOrder_CStr const &_Registry)
			 	{
					if (fIsRoot(_Registry))
					{
						fCheckWhitespace(CPropertyKey{"General"}, _Registry);
						return;
					}

					CStr Path = _Registry.f_GetPath();
					CPropertyKey Section = fGetSection(fg_RemoveQualifiers(_Registry));

					fCheckWhitespace(Section, _Registry);
				}
			)
		;


	}

	TCVector<TCVector<CConfigurationTuple>> CBuildSystem::fp_EvaluateConfigurationTuples(TCMap<CPropertyKey, CStr> const &_InitialValues) const
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
		f_EvaluateData(TempData, _InitialValues, &mp_Data.m_RootEntity, nullptr, nullptr, false, true);

		CEntity Entity(&TempData.m_RootEntity);

		for (auto iProp = TempData.m_RootEntity.m_PropertiesEvalOrder.f_GetIterator(); iProp; ++iProp)
		{
			if (iProp->m_pRegistry->f_GetFile() == mp_UserSettingsFileLocal)
			{
				auto Mapped = mp_UserSettingsLocal.m_Properties(iProp->m_Key);
				*Mapped = iProp->m_pRegistry;
			}
			if (iProp->m_pRegistry->f_GetFile() == mp_UserSettingsFileGlobal)
			{
				auto Mapped = mp_UserSettingsGlobal.m_Properties(iProp->m_Key);
				*Mapped = iProp->m_pRegistry;
			}
		}

		mp_UserSettingsLocal.f_Parse();
		mp_UserSettingsGlobal.f_Parse();


		for (auto iProp = TempData.m_RootEntity.m_PropertiesEvalOrder.f_GetIterator(); iProp; ++iProp)
		{
			CStr WhiteSpace = iProp->m_pRegistry->f_GetWhiteSpace(EWhiteSpaceLocation_After);

			auto fAddSettings = [&](CUserSettingsState &o_State, CStr const &_Description, CPropertyKey const &_Section, CStr const &_DefaultValue)
				{
					auto Mapped = o_State.m_Defined(iProp->m_Key);

					if (Mapped.f_WasCreated())
					{
						auto pSection = o_State.f_GetSection(_Section);
						auto pCommentReg = pSection;
						auto CommentType = EWhiteSpaceLocation_AfterChildScopeStart;
						if (pCommentReg->f_HasChildren())
						{
							pCommentReg = pCommentReg->f_GetChildren().f_FindLargest();
							if (pCommentReg->f_HasScope())
								CommentType = EWhiteSpaceLocation_AfterChildScopeEnd;
							else
								CommentType = EWhiteSpaceLocation_After;
						}

						CStr Comments = pCommentReg->f_GetWhiteSpace(CommentType).f_TrimRight("\r\n");
						CRegistry_CStr TempReg;
						TempReg.f_SetValue(iProp->m_Key.m_Name, _DefaultValue);
						Comments += "{\n}\t//{} // {}{\n}"_f << TempReg.f_GenerateStr().f_Trim() << _Description;

						pCommentReg->f_SetWhiteSpace(CommentType, Comments);
					}
				}
			;

			{
				auto iWhiteStart = WhiteSpace.f_Find("// User");
				if (iWhiteStart >= 0)
				{
					CStr Section = "General";
					CStr DefaultValue = "\"\"";

					CStr LineCandidate = WhiteSpace.f_GetStr() + iWhiteStart;
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
						if (*pParse == '\"')
						{
							auto pStart = pParse;
							fg_ParseEscape<'\"'>(pParse, '\"');

							DefaultValue = CStr(pStart, pParse - pStart);
							DefaultValue = fg_RemoveEscape<'\"'>(DefaultValue);

							fg_ParseWhiteSpace(pParse);
						}
						else
						{
							auto pStart = pParse;
							while (*pParse && *pParse != ':' && !fg_CharIsWhiteSpace(*pParse))
								++pParse;

							DefaultValue = CStr(pStart, pParse - pStart);

							fg_ParseWhiteSpace(pParse);
						}
					}

					if (*pParse != ':')
						continue;


					++pParse;
					fg_ParseWhiteSpace(pParse);

					CStr Description = pParse;

					fAddSettings(mp_UserSettingsLocal, Description, {iProp->f_GetType(), Section}, DefaultValue);
					fAddSettings(mp_UserSettingsGlobal, Description, {iProp->f_GetType(), Section}, DefaultValue);
				}
			}
		}

		CEntityKey EntityKey;
		EntityKey.m_Type = EEntityType_Root;
		EntityKey.m_Name = "TempChild";
		auto &TupleEntity = Entity.m_ChildEntitiesMap(EntityKey, &Entity).f_GetResult();
		Entity.m_ChildEntitiesOrdered.f_Insert(TupleEntity);

		for (auto iTuple = Tuples.f_GetIterator(); iTuple; ++iTuple)
		{
			TupleEntity.m_EvaluatedProperties.f_Clear();
			CPropertyKey Key;
			for (auto iKey = iTuple->m_Keys.f_GetIterator(); iKey; ++iKey)
			{
				Key.m_Type = EPropertyType_Property;
				Key.m_Name = iKey.f_GetKey().m_ConfigType;
				auto &Evaluated = TupleEntity.m_EvaluatedProperties[Key];
				Evaluated.m_Value = (*iKey)->f_GetName();
				Evaluated.m_Type = EEvaluatedPropertyType_External;
				Evaluated.m_pProperty = &mp_ExternalProperty;
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
