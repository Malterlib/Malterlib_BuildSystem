// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem
{
	template <typename t_CType>
	struct TCRemoveOptional
	{
		using CType = t_CType;
	};

	template <typename t_CType>
	struct TCRemoveOptional<NStorage::TCOptional<t_CType>>
	{
		using CType = t_CType;
	};

	template <typename tf_CValue>
	TCValueWithPositions<tf_CValue> CGeneratorSetting::f_GetSetting(NStr::CStr const &_Name) const
	{
		TCValueWithPositions<tf_CValue> ReturnValue;
		ReturnValue.f_SetFrom(m_Value, _Name, m_PropertyInfo);

		return ReturnValue;
	}

	template <typename tf_CValue, typename tf_CThis>
	TCValueWithPositions<tf_CValue> CGeneratorSettings::fsp_GetSingleSetting(tf_CThis &&_This, NStr::CStr const &_Name)
	{
		using namespace NStr;

		TCValueWithPositions<tf_CValue> ReturnValue;
		bool bFirst = true;
		CConfiguration const *pFirstConfig;

		auto &Settings = _This.f_Settings();
		for (auto &Setting : Settings)
		{
			auto &Config = Settings.fs_GetKey(Setting);

			TCValueWithPositions<tf_CValue> ThisValue;
			ThisValue.f_SetFrom(fg_ForwardAs<tf_CThis>(Setting.m_Value), _Name, Setting.m_PropertyInfo);

			if (bFirst)
			{
				bFirst = false;
				pFirstConfig = &Config;
				ReturnValue = fg_Move(ThisValue);
			}
			else if (ThisValue.m_Value != ReturnValue.m_Value)
			{
				NContainer::TCVector<CBuildSystemError> OtherErrors;
				auto &Other = OtherErrors.f_Insert();
				Other.m_Error = "See other value ('{}')"_f << *pFirstConfig;
				Other.m_Positions = ReturnValue.m_Positions;

				CBuildSystem::fs_ThrowError
					(
						ThisValue.m_Positions
						, CStr::CFormat("You cannot specify '{}' with different values for different configurations ('{}').") << _Name << Config
						, OtherErrors
					)
				;
			}
		}
		return ReturnValue;
	}

	template <typename tf_CValue>
	TCValueWithPositions<tf_CValue> CGeneratorSettings::f_GetSingleSetting(NStr::CStr const &_Name) const &
	{
		return fsp_GetSingleSetting<tf_CValue>(*this, _Name);
	}

	template <typename tf_CValue>
	TCValueWithPositions<tf_CValue> CGeneratorSettings::f_GetSingleSetting(NStr::CStr const &_Name) &&
	{
		return fsp_GetSingleSetting<tf_CValue>(fg_Move(*this), _Name);
	}

	template <typename tf_CValue>
	tf_CValue CGeneratorSetting::f_GetSettingWithoutPositions(NStr::CStr const &_Name) const
	{
		using namespace NStr;

		tf_CValue ReturnValue = {};
		auto *pValue = m_Value.f_GetMember(_Name);

		using CType = typename TCRemoveOptional<tf_CValue>::CType;

		if constexpr (!NStorage::cIsOptional<tf_CValue>)
		{
			if (!pValue)
				CBuildSystem::fs_ThrowError(m_PropertyInfo, "Missing member for '{}'"_f << _Name);
		}

		if (pValue)
		{
			if constexpr (NTraits::cIsSame<CType, NEncoding::CEJsonSorted>)
				ReturnValue = fg_Move(fg_RemoveQualifiers(*pValue));
			else if constexpr (NTraits::cIsSame<CType, NStr::CStr>)
			{
				if (!pValue->f_IsString())
					CBuildSystem::fs_ThrowError(m_PropertyInfo, "Expected value to be string for '{}'"_f << _Name);

				ReturnValue = fg_Move(fg_RemoveQualifiers(pValue->f_String()));
			}
			else if constexpr (NTraits::cIsSame<CType, NContainer::TCVector<NStr::CStr>>)
			{
				if (!pValue->f_IsStringArray())
					CBuildSystem::fs_ThrowError(m_PropertyInfo, "Expected value to be a string array for '{}'"_f << _Name);

				ReturnValue = fg_Move(fg_RemoveQualifiers(pValue->f_StringArray()));
			}
			else if constexpr (NTraits::cIsSame<CType, int32>)
			{
				if (!pValue->f_IsInteger())
					CBuildSystem::fs_ThrowError(m_PropertyInfo, "Expected value to be integer for '{}'"_f << _Name);

				ReturnValue = pValue->f_Integer();
			}
			else if constexpr (NTraits::cIsSame<CType, bool>)
			{
				if (!pValue->f_IsBoolean())
					CBuildSystem::fs_ThrowError(m_PropertyInfo, "Expected value to be boolean for '{}'"_f << _Name);

				ReturnValue = pValue->f_Boolean();
			}
			else
				static_assert(NTraits::cIsSame<CType, CVoidTag>, "Unsupported type");
		}

		return ReturnValue;
	}

	template <typename tf_CValue>
	tf_CValue CGeneratorSettings::f_GetSingleSettingWithoutPositions(NStr::CStr const &_Name) const
	{
		using namespace NStr;

		tf_CValue ReturnValue;
		bool bFirst = true;
		CConfiguration const *pFirstConfig;

		auto &Settings = f_Settings();

		for (auto &Setting : Settings)
		{
			auto &Config = Settings.fs_GetKey(Setting);

			tf_CValue ThisValue = Setting.f_GetSettingWithoutPositions<tf_CValue>(_Name);

			if (bFirst)
			{
				bFirst = false;
				pFirstConfig = &Config;
				ReturnValue = fg_Move(ThisValue);
			}
			else if (ThisValue != ReturnValue)
			{
				NContainer::TCVector<CBuildSystemError> OtherErrors;
				auto &Other = OtherErrors.f_Insert();
				Other.m_Error = "See other value ('{}')"_f << *pFirstConfig;
				Other.m_Positions = Settings[*pFirstConfig].m_PropertyInfo.f_GetPositions();

				CBuildSystem::fs_ThrowError
					(
						Setting.m_PropertyInfo.f_GetPositions()
						, CStr::CFormat("You cannot specify '{}' with different values for different configurations ('{}').") << _Name << Config
						, OtherErrors
					)
				;
			}
		}

		return ReturnValue;
	}

	template <typename tf_CValue>
	TCValueWithPositions<tf_CValue> CGeneratorSettings::f_GetSetting(CConfiguration const &_Config, NStr::CStr const &_Name) const &
	{
		using namespace NStr;

		auto &Settings = f_Settings();
		auto *pSetting = Settings.f_FindEqual(_Config);

		if constexpr (NStorage::cIsOptional<tf_CValue>)
		{
			if (!pSetting)
				return {};
		}
		else
		{
			if (!pSetting)
				CBuildSystem::fs_ThrowError(CBuildSystemUniquePositions(), "Missing member for '{}'"_f << _Name);
		}

		TCValueWithPositions<tf_CValue> ThisValue;
		ThisValue.f_SetFrom(pSetting->m_Value, _Name, pSetting->m_PropertyInfo);

		return ThisValue;
	}

	template <typename tf_CValue>
	tf_CValue CGeneratorSettings::f_GetSettingWithoutPositions(CConfiguration const &_Config, NStr::CStr const &_Name) const &
	{
		using namespace NStr;

		auto &Settings = f_Settings();
		auto *pSetting = Settings.f_FindEqual(_Config);

		if constexpr (NStorage::cIsOptional<tf_CValue>)
		{
			if (!pSetting)
				return {};
		}
		else
		{
			if (!pSetting)
				CBuildSystem::fs_ThrowError(CBuildSystemUniquePositions(), "Missing member for '{}'"_f << _Name);
		}

		return pSetting->f_GetSettingWithoutPositions<tf_CValue>(_Name);
	}
}
