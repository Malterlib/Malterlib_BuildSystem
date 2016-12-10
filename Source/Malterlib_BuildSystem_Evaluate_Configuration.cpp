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
			if (iProp->m_pRegistry->f_GetFile() == mp_UserSettingsFile)
			{
				auto Mapped = mp_UserSettingsProperties(iProp->m_Key);
				*Mapped = iProp->m_pRegistry;
			}
		}


		for (auto iProp = TempData.m_RootEntity.m_PropertiesEvalOrder.f_GetIterator(); iProp; ++iProp)
		{
			CStr WhiteSpace = iProp->m_pRegistry->f_GetWhiteSpace(EWhiteSpaceLocation_After);

			auto iWhiteStart = WhiteSpace.f_Find("// User: ");
			if (iWhiteStart >= 0)
			{
				//if (f_EvalCondition(Entity, iProp->m_Condition))
				{
					CStr Description = WhiteSpace.f_Extract(iWhiteStart + 9);

					auto Mapped = mp_UserSettingsProperties(iProp->m_Key);

					if (Mapped.f_WasCreated())
					{
						CStr Name = CStr::CFormat("{}.{}") << fg_PropertyTypeToStr(iProp->m_Key.m_Type) << iProp->m_Key.m_Name;
						auto pChild = mp_UserSettingsRegistry.f_CreateChild(Name, true);
						pChild->f_SetThisValue("");
						pChild->f_SetWhiteSpace(EWhiteSpaceLocation_After, CStr::CFormat(" // {}") << Description);
						*Mapped = pChild;
					}
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
