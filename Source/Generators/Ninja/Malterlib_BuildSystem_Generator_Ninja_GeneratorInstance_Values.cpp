// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Generator_Ninja.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>
#include <Mib/Encoding/JsonShortcuts>

namespace NMib::NBuildSystem::NNinja
{
	CRule CRule::fs_FromJson(CEJsonSorted &&_Json)
	{
		CRule Return;

		for (auto &Member : _Json.f_Object())
		{
			auto &Name = Member.f_Name();
			auto &Value = Member.f_Value();

			if (Name == gc_Str<"command">.m_Str)
				Return.m_Command = Value.f_String();
			else if (Name == gc_Str<"environment">.m_Str)
			{
				for (auto &Entry : Value.f_Object())
					Return.m_Environment[Entry.f_Name()] = Entry.f_Value().f_String();
			}
			else
			{
				if (Value.f_IsBoolean())
					Return.m_OtherProperties[Name] = Value.f_AsString();
				else
					Return.m_OtherProperties[Name] = Value.f_String();
			}
		}

		return Return;
	}

	CBuild CBuild::fs_FromJson(CEJsonSorted &&_Json)
	{
		CBuild Return;

		for (auto &Member : _Json.f_Object())
		{
			auto &Name = Member.f_Name();
			auto &Value = Member.f_Value();

			if (Name == gc_Str<"description">.m_Str)
				Return.m_Description = Value.f_String();
			else if (Name == gc_Str<"inputs">.m_Str)
				Return.m_Inputs = fg_Move(Value).f_StringArray();
			else if (Name == gc_Str<"outputs">.m_Str)
				Return.m_Outputs = fg_Move(Value).f_StringArray();
			else if (Name == gc_Str<"implicit_outputs">.m_Str)
				Return.m_ImplicitOutputs = fg_Move(Value).f_StringArray();
			else if (Name == gc_Str<"implicit_dependencies">.m_Str)
				Return.m_ImplicitDependencies = fg_Move(Value).f_StringArray();
			else if (Name == gc_Str<"order_dependencies">.m_Str)
				Return.m_OrderDependencies = fg_Move(Value).f_StringArray();
			else if (Name == gc_Str<"validations">.m_Str)
				Return.m_Validations = fg_Move(Value).f_StringArray();
			else
				Return.m_Variables[Name, Value.f_String()];
		}

		return Return;
	}

	void CRuleAndBuild::fs_FromJson(CRuleAndBuild &o_This, CBuildSystem const &_BuildSystem, CEJsonSorted &&_Json)
	{
		if (auto *pValue = _Json.f_GetMember(gc_Str<"Type">.m_Str))
			o_This.m_Type = pValue->f_String();

		auto *pValue = _Json.f_GetMember(gc_Str<"Rule">.m_Str);
		if (!pValue)
			return;

		fs_RuleFromJson(o_This, _BuildSystem, fg_Move(*pValue));
	}

	void CRuleAndBuild::fs_RuleFromJson(CRuleAndBuild &o_This, CBuildSystem const &_BuildSystem, CEJsonSorted &&_Json, TCMap<CStr, TCVector<CStr>> *o_pSharedFlags)
	{
		for (auto &Entry : _Json.f_Object())
		{
			auto &OutEntry = o_This.m_Entries[Entry.f_Name()];

			auto &Value = Entry.f_Value();
			auto &Positions = Value(gc_ConstString_Positions.m_String);

			if (_BuildSystem.f_EnablePositions())
				OutEntry.m_Positions = CBuildSystemUniquePositions::fs_FromJson(Positions);

			OutEntry.m_FlagsPriority = Value.f_GetMemberValue(gc_Str<"FlagsPriority">.m_Str, int64(0)).f_Integer();

			if (auto *pValue = Value.f_GetMember(gc_Str<"Flags">.m_Str); pValue && pValue->f_IsValid())
			{
				if (pValue->f_IsString())
					OutEntry.m_Flags.f_Insert(pValue->f_String());
				else if (pValue->f_IsArray())
				{
					for (auto &ArrayEntry : pValue->f_Array())
					{
						if (ArrayEntry.f_IsString())
							OutEntry.m_Flags.f_Insert(ArrayEntry.f_String());
						else if (ArrayEntry.f_IsStringArray())
							OutEntry.m_Flags.f_Insert(ArrayEntry.f_StringArray());
						else
							_BuildSystem.fs_ThrowError(OutEntry.m_Positions, "Flags value is of unsupported type: {}"_f << Value);
					}
				}
				else
					_BuildSystem.fs_ThrowError(OutEntry.m_Positions, "Flags value is of unsupported type: {}"_f << Value);
			}

			if (o_pSharedFlags)
			{
				auto &SharedFlags = *o_pSharedFlags;
				if (auto *pValue = Value.f_GetMember(gc_Str<"FlagsShared">.m_Str); pValue && pValue->f_IsValid())
				{
					if (!pValue->f_IsObject())
						_BuildSystem.fs_ThrowError(OutEntry.m_Positions, "FlagsShared must be an object: {}"_f << *pValue);
					for (auto &Entry : pValue->f_Object())
					{
						auto &Value = Entry.f_Value();
						auto Names = Entry.f_Name().f_Split<true>(gc_Str<",">.m_Str);
						for (auto &Name : Names)
						{
							auto &FlagList = SharedFlags[Name];
							if (Value.f_IsString())
								FlagList.f_Insert(Value.f_String());
							else if (Value.f_IsArray())
							{
								for (auto &ArrayEntry : Value.f_Array())
								{
									if (ArrayEntry.f_IsString())
										FlagList.f_Insert(ArrayEntry.f_String());
									else if (ArrayEntry.f_IsStringArray())
										FlagList.f_Insert(ArrayEntry.f_StringArray());
									else
										_BuildSystem.fs_ThrowError(OutEntry.m_Positions, "FlagsShared value is of unsupported type: {}"_f << Value);

								}
							}
							else
								_BuildSystem.fs_ThrowError(OutEntry.m_Positions, "FlagsShared value is of unsupported type: {}"_f << Value);
						}
					}
				}
			}

			if (auto *pValue = Value.f_GetMember(gc_Str<"Rule">.m_Str))
				OutEntry.m_Rule = CRule::fs_FromJson(fg_Move(*pValue));

			if (auto *pValue = Value.f_GetMember(gc_Str<"Build">.m_Str))
				OutEntry.m_Build = CBuild::fs_FromJson(fg_Move(*pValue));
		}
	}

	void CGeneratorInstance::fp_SetEvaluatedValuesTarget
		(
			CEntityMutablePointer const &_Entity
			, CConfigResultTarget &o_Result
			, CStr const &_Name
		) const
	{
		auto *pConfig = _Entity.f_Get();
		auto pTopConfig = pConfig;

		bool bIsFullEval = false;
		CEJsonSorted GeneratorSettings = m_BuildSystem.f_GetDefinedProperties<false>(*pTopConfig, EPropertyType_Target, bIsFullEval);
		{
			{
				auto &EvalProperty = pTopConfig->m_EvaluatedProperties.m_Properties[gc_ConstKey_GeneratorSetting_DefinedProperties];
				EvalProperty.m_Value = fg_Move(GeneratorSettings);
				EvalProperty.m_Type = EEvaluatedPropertyType_External;
				EvalProperty.m_pProperty = &m_BuildSystem.f_ExternalProperty(EPropertyType_GeneratorSetting);
			}

			CBuildSystemPropertyInfo PropertyInfo;
			auto Value = m_BuildSystem.f_EvaluateEntityProperty
				(
					*pTopConfig
					, gc_ConstKey_GeneratorSetting_Target
					, PropertyInfo
				)
			;
			auto ValueMove = Value.f_Move();

			auto Cleanup = g_OnScopeExit / [&]
				{
					pTopConfig->m_EvaluatedProperties.m_Properties.f_Remove(gc_ConstKey_GeneratorSetting_DefinedProperties);
					pTopConfig->m_EvaluatedProperties.m_Properties.f_Remove(gc_ConstKey_GeneratorSetting_Target);
				}
			;

			CBuildSystemPropertyInfo PropertyInfoValidateSettings;
			m_BuildSystem.f_EvaluateEntityProperty
				(
					*pTopConfig
					, gc_ConstKey_GeneratorSetting_TargetValidateSettings
					, PropertyInfoValidateSettings
				)
			;

			CRuleAndBuild::fs_RuleFromJson(o_Result.m_PreBuild, m_BuildSystem, fg_Move(ValueMove[gc_Str<"Rule_PreBuild">.m_Str]));
			CRuleAndBuild::fs_RuleFromJson(o_Result.m_Build, m_BuildSystem, fg_Move(ValueMove[gc_Str<"Rule_Build">.m_Str]), &o_Result.m_SharedFlags);
			CRuleAndBuild::fs_RuleFromJson(o_Result.m_PostBuild, m_BuildSystem, fg_Move(ValueMove[gc_Str<"Rule_PostBuild">.m_Str]));

			for (auto &Rule : ValueMove[gc_Str<"OtherRules">.m_Str].f_Array())
				CRuleAndBuild::fs_RuleFromJson(o_Result.m_OtherRules.f_Insert(), m_BuildSystem, fg_Move(Rule));
		}
	}

	auto CGeneratorInstance::fp_GetConfigValue
		(
		 	TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
		 	, CConfiguration const &_Configuration
		 	, CPropertyKeyReference const &_PropertyKey
			, EEJsonType _ExpectedType
			, bool _bOptional
		) const
	 	-> CSingleValue
	{
		auto pEntity = _Configs.f_FindEqual(_Configuration);
		if (!pEntity)
		{
			CFilePosition Position;
			if (!_Configs.f_IsEmpty())
				Position = _Configs.f_FindSmallest()->f_Get()->f_Data().m_Position;

			m_BuildSystem.fs_ThrowError
				(
					Position
					, "When getting {}.{} Could not find config '{}' in enabled configs: {vs}"_f << _PropertyKey << _Configuration << _Configs.f_KeySet()
				)
			;
		}

		CBuildSystemPropertyInfo PropertyInfo;
		CGeneratorInstance::CSingleValue Ret(m_BuildSystem.f_EvaluateEntityProperty(**pEntity, _PropertyKey, PropertyInfo));
		if (m_BuildSystem.f_EnablePositions())
		{
			if (PropertyInfo.m_pPositions)
				Ret.m_Positions = *PropertyInfo.m_pPositions;
			else
				Ret.m_Positions.f_AddPosition((*pEntity)->f_Data().m_Position, gc_ConstString_Entity);
		}

		m_BuildSystem.f_CheckPropertyTypeValue(_PropertyKey, Ret.m_Value.f_Get(), _ExpectedType, Ret.m_Positions, _bOptional);

		return Ret;
	}
}
