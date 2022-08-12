// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>
#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NBuildSystem::NXcode
{
	void CGeneratorInstance::fp_SetEvaluatedValuesCompile
		(
			CEntityMutablePointer const &_Config
			, bool _bFile
			, CConfigResultCompile &o_Result
		) const
	{
		auto pTopConfig = _Config.f_Get();

		CEJSONSorted GeneratorSettings;

		if (_bFile)
			GeneratorSettings = m_BuildSystem.f_GetDefinedProperties<true>(*pTopConfig, EPropertyType_Compile);
		else
			GeneratorSettings = m_BuildSystem.f_GetDefinedProperties<false>(*pTopConfig, EPropertyType_Compile);

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
					*pTopConfig // *pCompileSettingsEntity
					, gc_ConstKey_GeneratorSetting_Compile
					, PropertyInfo
				)
			;
			auto ValueMove = Value.f_Move();

			auto Cleanup = g_OnScopeExit / [&]
				{
					pTopConfig->m_EvaluatedProperties.m_Properties.f_Remove(gc_ConstKey_GeneratorSetting_DefinedProperties);
					pTopConfig->m_EvaluatedProperties.m_Properties.f_Remove(gc_ConstKey_GeneratorSetting_Compile);
				}
			;

			o_Result.m_Type.f_SetFrom(fg_Move(ValueMove), gc_ConstString_Type, PropertyInfo);

			if (auto pReference = ValueMove.f_GetMember(gc_ConstString_PBXFileReference, EJSONType_Object))
			{
				o_Result.m_PBXFileReference.m_LastKnownFileType.f_SetFrom(fg_Move(*pReference), gc_ConstString_lastKnownFileType, PropertyInfo, CStr());
				o_Result.m_PBXFileReference.m_ExplicitFileType.f_SetFrom(fg_Move(*pReference), gc_ConstString_explicitFileType, PropertyInfo, CStr());
				o_Result.m_PBXFileReference.m_FileEncoding.f_SetFrom(fg_Move(*pReference), gc_ConstString_fileEncoding, PropertyInfo, 0);
				o_Result.m_PBXFileReference.m_TabWidth.f_SetFrom(fg_Move(*pReference), gc_ConstString_tabWidth, PropertyInfo, 0);
				o_Result.m_PBXFileReference.m_UsesTabs.f_SetFrom(fg_Move(*pReference), gc_ConstString_usesTabs, PropertyInfo, 0);
				o_Result.m_PBXFileReference.m_IndentWidth.f_SetFrom(fg_Move(*pReference), gc_ConstString_indentWidth, PropertyInfo, 0);
			}

			if (auto pXCConfigs = ValueMove.f_GetMember(gc_ConstString_XCConfigs, EEJSONType_Object))
			{
				for (auto &Member : pXCConfigs->f_Object())
				{
					auto &Element = o_Result.m_Element[Member.f_Name()];

					auto &Value = Member.f_Value();

					Element.m_bXcodeProperty = true;

					if (Value.f_IsArray())
					{
						Element.m_bUseValues = true;
						for (auto &Value : Value.f_Array())
						{
							if (Value.f_IsString())
								Element.m_ValueSet[Value.f_String()];
							else if (Value.f_IsStringArray())
								Element.m_ValueSet[Value.f_StringArray()];
							else
								m_BuildSystem.fs_ThrowError(CFilePosition(), CStr::CFormat("XCConfig value is of unsupported type: {}") << Value);
						}
					}
					else if (Value.f_IsString())
						Element.m_Value = Value.f_String();
					else if (Value.f_IsBoolean())
					{
						if (Value.f_Boolean())
							Element.m_Value = gc_ConstString_YES;
						else
							Element.m_Value = gc_ConstString_NO;
					}
					else
						m_BuildSystem.fs_ThrowError(CFilePosition(), CStr::CFormat("XCConfig value is of unsupported type: {}") << Value);
				}
			}

			if (auto pFlags = ValueMove.f_GetMember(gc_ConstString_Flags, EEJSONType_Object))
			{
				for (auto &Object : pFlags->f_Object())
				{
					auto &Element = o_Result.m_Element[Object.f_Name()];
					Element.m_bUseValues = true;

					auto &ValueObject = Object.f_Value();
					auto &Positions = ValueObject[gc_ConstString_Positions.m_String];

					if (m_BuildSystem.f_EnablePositions())
						Element.m_Positions.f_AddPositions(CBuildSystemUniquePositions::fs_FromJSON(Positions));

					auto &Value = ValueObject[gc_ConstString_Value.m_String];

					if (Value.f_IsStringArray())
					{
						for (auto &String : Value.f_StringArray())
							Element.m_ValueSet[String];
					}
					else if (Value.f_IsString())
						Element.m_ValueSet[Value.f_String()];
					else if (Value.f_IsArray())
					{
						for (auto &StringArray : Value.f_Array())
						{
							if (!StringArray.f_IsStringArray())
								m_BuildSystem.fs_ThrowError(Element.m_Positions, CStr::CFormat("Flags value is of unsupported type: {}") << Value);
							Element.m_ValueSet[StringArray.f_StringArray()];
						}
					}
					else
						m_BuildSystem.fs_ThrowError(Element.m_Positions, CStr::CFormat("Flags value is of unsupported type: {}") << Value);
				}
			}
		}
	}

	void CGeneratorInstance::fp_SetEvaluatedValuesTarget
		(
			CEntityMutablePointer const &_Entity
			, CGeneratorInstance::CConfigResultTarget &o_Result
			, CStr const &_Name
		) const
	{
		auto *pConfig = _Entity.f_Get();
		auto pTopConfig = pConfig;

		CEJSONSorted GeneratorSettings = m_BuildSystem.f_GetDefinedProperties<false>(*pTopConfig, EPropertyType_Target);

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

			o_Result.m_TargetType.f_SetFrom(fg_Move(ValueMove), gc_ConstString_Type, PropertyInfo);
			o_Result.m_ProductType.f_SetFrom(fg_Move(ValueMove), gc_ConstString_ProductType, PropertyInfo);
			o_Result.m_Name.f_SetFrom(fg_Move(ValueMove), gc_ConstString_Name, PropertyInfo);

			if (auto pXCConfigs = ValueMove.f_GetMember(gc_ConstString_XCConfigs, EEJSONType_Object))
			{
				for (auto &Member : pXCConfigs->f_Object())
				{
					auto &Element = o_Result.m_Element[Member.f_Name()];

					auto &Value = Member.f_Value();
					Element.m_bXcodeProperty = true;

					if (Value.f_IsArray())
					{
						Element.m_bUseValues = true;
						for (auto &Value : Value.f_Array())
						{
							if (Value.f_IsString())
								Element.m_ValueSet[Value.f_String()];
							else if (Value.f_IsStringArray())
								Element.m_ValueSet[Value.f_StringArray()];
							else
								m_BuildSystem.fs_ThrowError(CFilePosition(), CStr::CFormat("XCConfig value is of unsupported type: {}") << Value);
						}
					}
					else if (Value.f_IsString())
						Element.m_Value = Value.f_String();
					else if (Value.f_IsBoolean())
					{
						if (Value.f_Boolean())
							Element.m_Value = gc_ConstString_YES;
						else
							Element.m_Value = gc_ConstString_NO;
					}
					else
						m_BuildSystem.fs_ThrowError(CFilePosition(), CStr::CFormat("XCConfig value is of unsupported type: {}") << Value);
				}
			}
		}
	}

	TCVector<CStr> CElement::fs_ValueArray(TCSet<TCVariant<CStr, TCVector<CStr>>> const &_Values)
	{
		TCVector<CStr> Vector;
		for (auto Value : _Values)
		{
			if (Value.f_IsOfType<TCVector<CStr>>())
				Vector.f_Insert(Value.f_GetAsType<TCVector<CStr>>());
			else
				Vector.f_Insert(Value.f_GetAsType<CStr>());
		}

		return Vector;
	}

	CStr CElement::f_GetCombinedValue() const
	{
		if (m_bUseValues)
			return CStr::fs_Join(f_ValueArray(), " ");

		return m_Value;
	}

	CStr CGeneratorInstance::fs_EscapeCommandLineArgument(CStr const &_String)
	{
		if (_String.f_FindChars(" \\\"") >= 0)
			return _String.f_EscapeStr();
		else
			return _String;
	}

	auto CGeneratorInstance::fp_GetConfigValues
		(
			TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
			, CPropertyKeyReference const &_PropertyKey
			, EEJSONType _ExpectedType
			, bool _bOptional
		) const
		-> TCMap<CConfiguration, CSingleValue>
	{
		TCMap<CConfiguration, CSingleValue> RetValues;

		for (auto iConfig = _Configs.f_GetIterator(); iConfig; ++iConfig)
		{
			CBuildSystemPropertyInfo PropertyInfo;

			auto &Ret = *RetValues(iConfig.f_GetKey(), m_BuildSystem.f_EvaluateEntityProperty(**iConfig, _PropertyKey, PropertyInfo));

			if (m_BuildSystem.f_EnablePositions())
			{
				if (PropertyInfo.m_pPositions)
					Ret.m_Positions = *PropertyInfo.m_pPositions;
				else
					Ret.m_Positions.f_AddPosition((*iConfig)->f_Data().m_Position, gc_ConstString_Entity);
			}

			if (_ExpectedType != EEJSONType_Invalid)
				m_BuildSystem.f_CheckPropertyTypeValue(_PropertyKey, Ret.m_Value.f_Get(), _ExpectedType, Ret.m_Positions, _bOptional);
		}

		return RetValues;
	}

	auto CGeneratorInstance::fp_GetConfigValue
		(
		 	TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
		 	, CConfiguration const &_Configuration
		 	, CPropertyKeyReference const &_PropertyKey
			, EEJSONType _ExpectedType
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

	CGeneratorInstance::CSingleValue CGeneratorInstance::fp_GetSingleConfigValue
		(
			TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
			, CPropertyKeyReference const &_PropertyKey
			, EEJSONType _ExpectedType
			, bool _bOptional
		) const
	{
		bool bFirst = true;

		TCOptional<CGeneratorInstance::CSingleValue> Ret;
		CConfiguration const *pFromConfig = nullptr;

		for (auto iConfig = _Configs.f_GetIterator(); iConfig; ++iConfig)
		{
			CBuildSystemPropertyInfo PropertyInfo;
			CBuildSystemUniquePositions Positions;
			auto Value = m_BuildSystem.f_EvaluateEntityProperty(**iConfig, _PropertyKey, PropertyInfo);

			if (!PropertyInfo.m_pPositions && m_BuildSystem.f_EnablePositions())
				Positions.f_AddPosition((*iConfig)->f_Data().m_Position, gc_ConstString_Entity);

			m_BuildSystem.f_CheckPropertyTypeValue(_PropertyKey, Value.f_Get(), _ExpectedType, PropertyInfo.m_pPositions ? *PropertyInfo.m_pPositions : Positions, _bOptional);

			if (bFirst)
			{
				bFirst = false;
				Ret = fg_Move(Value);

				if (m_BuildSystem.f_EnablePositions())
				{
					if (PropertyInfo.m_pPositions)
						Ret->m_Positions.f_AddPositions(*PropertyInfo.m_pPositions);
					else
						Ret->m_Positions.f_AddPosition((*iConfig)->f_Data().m_Position, gc_ConstString_Entity);
				}

				pFromConfig = &iConfig.f_GetKey();
			}
			else
			{
				if (Value.f_Get() != Ret->m_Value.f_Get())
				{
					CBuildSystemUniquePositions Positions;
					if (PropertyInfo.m_pPositions)
						Positions = *PropertyInfo.m_pPositions;
					else
						Positions.f_AddPosition((*iConfig)->f_Data().m_Position, gc_ConstString_Entity);

					TCVector<CBuildSystemError> OtherErrors;
					CBuildSystemError &Error = OtherErrors.f_Insert();
					Error.m_Positions = Ret->m_Positions;
					Error.m_Error = CStr::CFormat("note: Differs from ({}): {}") << pFromConfig->f_GetFullName() << Ret->m_Value.f_Get();
					m_BuildSystem.fs_ThrowError
						(
							Positions
							, CStr::CFormat("Property value cannot be varied per configuration ({}): {}") << iConfig.f_GetKey().f_GetFullName() << Value.f_Get()
							, OtherErrors
						)
					;
				}
			}
		}

		if (Ret)
			return fg_Move(*Ret);
		else
			m_BuildSystem.fs_ThrowError(CFilePosition(), CStr::CFormat("No value found for '{}' and one is required") << _PropertyKey);
	}
}
