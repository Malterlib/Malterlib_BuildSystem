// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	template <bool tf_bFile>
	CEJSONSorted CBuildSystem::f_GetDefinedProperties(CEntity &_Entity, EPropertyType _PropertyType) const
	{
		using namespace NStr;
		
		auto *pTopConfig = &_Entity;

		bool bBelowFileLevel = false;
		bool bFullFileSettings = false;
		if constexpr (tf_bFile)
		{
			for (auto *pConfig = pTopConfig; pConfig; pConfig = pConfig->m_pParent)
			{
				if (!pConfig->f_HasFullEval(_PropertyType))
					continue;

				CBuildSystemPropertyInfo PropertyInfo;
				bFullFileSettings = f_EvaluateEntityPropertyBool(*pTopConfig, gc_ConstKeys_FullEval[_PropertyType], PropertyInfo, false);
				break;
			}
		}

		CEJSONSorted GeneratorSettings;
		auto &GeneratorSettingsArray = GeneratorSettings.f_Array();

		TCSet<CPropertyKey> AddedProperties;

		if constexpr (tf_bFile)
		{
			if (_PropertyType == EPropertyType_Compile)
			{
				AddedProperties[gc_ConstKey_Compile_Type];

				auto &Inserted = GeneratorSettingsArray.f_Insert();
				auto &Object = Inserted.f_Object();
				Object[gc_ConstString_Type] = fg_PropertyTypeToStr(gc_ConstKey_Compile_Type.f_GetType());
				Object[gc_ConstString_Name] = gc_ConstKey_Compile_Type.m_Name;
			}
		}

		for (auto pConfig = pTopConfig; pConfig; pConfig = pConfig->m_pParent)
		{
			auto &Key = pConfig->f_GetKey();
			if constexpr (tf_bFile)
			{
				if (!bBelowFileLevel)
				{
					if
						(
							Key.m_Type != EEntityType_Group
							&& Key.m_Type != EEntityType_GenerateFile
							&& Key.m_Type != EEntityType_File
						)
					{
						bBelowFileLevel = true;
					}
				}
			}

			bool bUseExplitPerFile = bBelowFileLevel && !bFullFileSettings;

			for (auto iProperty = pConfig->f_Data().m_Properties.f_GetIterator(); iProperty; ++iProperty)
			{
				auto &SourceProperties = *iProperty;
				auto Type = iProperty.f_GetKey().f_GetType();

				if (Type != _PropertyType)
					continue;

				if constexpr (tf_bFile)
				{
					if (bUseExplitPerFile) // Implies _bFile && bBelowFileLevel && !bFullFileSettings
					{
						bool bNeedPerFile = false;
						for (auto &Property : SourceProperties)
						{
							if (Property.f_NeedPerFile())
								bNeedPerFile = true;
						}
						if (!bNeedPerFile)
							continue;
					}

					if (bBelowFileLevel) // Implies _bFile
					{
						bool bFoundProperty = false;
						for (auto &Property : SourceProperties)
						{
							if (!Property.m_pCondition || f_EvalCondition(*pTopConfig, *Property.m_pCondition, Property.m_Flags & EPropertyFlag_TraceCondition))
							{
								bFoundProperty = true;
								break;
							}
						}
						if (!bFoundProperty)
							continue;
					}
				}

				CBuildSystemPropertyInfo PropertyInfo;
				if (!SourceProperties.f_IsEmpty())
					PropertyInfo.m_pFallbackPosition = &SourceProperties.f_GetFirst().m_Position;

				auto Value = f_EvaluateEntityPropertyNoDefault(*pTopConfig, iProperty.f_GetKey().f_Reference(), PropertyInfo);
				auto &ValueRef = Value.f_Get();

				if (PropertyInfo.m_pProperty == nullptr || !ValueRef.f_IsValid())
					continue; // This means that the property is not valid at the top level

				if (AddedProperties(iProperty.f_GetKey()).f_WasCreated())
				{
					auto &Inserted = GeneratorSettingsArray.f_Insert();
					auto &Object = Inserted.f_Object();
					Object[gc_ConstString_Type] = fg_PropertyTypeToStr(iProperty.f_GetKey().f_GetType());
					Object[gc_ConstString_Name] = iProperty.f_GetKey().m_Name;
				}
			}

			if constexpr (tf_bFile)
			{
				if (bBelowFileLevel) // Implies _bFile
					continue;
			}

			for (auto &VariableDefinitions : pConfig->f_Data().m_VariableDefinitions)
			{
				auto &Key = pConfig->f_Data().m_VariableDefinitions.fs_GetKey(VariableDefinitions);
				if (Key.f_GetType() != _PropertyType)
					continue;

				if (AddedProperties.f_FindEqual(Key))
					continue;

				for (auto &VariableDefinition : VariableDefinitions)
				{
					auto TypePosition = VariableDefinition.m_Type.m_Position;
					CBuildSystemSyntax::CType const *pType = f_GetCanonicalDefaultedType(*pConfig, &VariableDefinition.m_Type.m_Type, TypePosition);
					if (!pType->f_IsDefaulted())
						continue;

					if (VariableDefinition.m_pConditions && !f_EvalCondition(*pTopConfig, *VariableDefinition.m_pConditions, false))
						continue;

					CBuildSystemPropertyInfo PropertyInfo;
					PropertyInfo.m_pFallbackPosition = &VariableDefinition.m_Type.m_Position;

					auto Value = f_EvaluateEntityProperty(*pTopConfig, Key.f_Reference(), PropertyInfo);
					auto &ValueRef = Value.f_Get();

					if (!ValueRef.f_IsValid())
						continue;

					if (AddedProperties(Key).f_WasCreated())
					{
						auto &Inserted = GeneratorSettingsArray.f_Insert();
						auto &Object = Inserted.f_Object();
						Object[gc_ConstString_Type] = fg_PropertyTypeToStr(Key.f_GetType());
						Object[gc_ConstString_Name] = Key.m_Name;
					}
				}
			}
		}

		if constexpr (tf_bFile)
		{
			if (!bFullFileSettings)
				return GeneratorSettings;
		}

		f_ForEachDefaultedBuiltinVariableDefinition
			(
				_PropertyType
				, [&](CPropertyKey const &_Key, CTypeWithPosition const &_Type)
				{
					if (AddedProperties.f_FindEqual(_Key))
						return;

					CBuildSystemPropertyInfo PropertyInfo;
					PropertyInfo.m_pFallbackPosition = &_Type.m_Position;

					auto Value = f_EvaluateEntityProperty(*pTopConfig, _Key.f_Reference(), PropertyInfo);
					auto &ValueRef = Value.f_Get();

					if (!ValueRef.f_IsValid())
						return;

					if (PropertyInfo.m_pProperty == nullptr)
						return;

					if (AddedProperties(_Key).f_WasCreated())
					{
						auto &Inserted = GeneratorSettingsArray.f_Insert();
						auto &Object = Inserted.f_Object();
						Object[gc_ConstString_Type] = fg_PropertyTypeToStr(_Key.f_GetType());
						Object[gc_ConstString_Name] = _Key.m_Name;
					}
				}
			)
		;

		return GeneratorSettings;
	}
}
