// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib::NBuildSystem
{
	struct CGeneratorSetting
	{
		template <typename tf_CValue>
		TCValueWithPositions<tf_CValue> f_GetSetting(NStr::CStr const &_Name) const;

		template <typename tf_CValue>
		tf_CValue f_GetSettingWithoutPositions(NStr::CStr const &_Name) const;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("{}") << m_Value;
		}

		NEncoding::CEJSONSorted m_Value;
		CBuildSystemPropertyInfo m_PropertyInfo;
	};

	struct CGeneratorSettings
	{
		NConcurrency::TCFuture<void> f_PopulateSettings
			(
				CPropertyKeyReference const &_GeneratorSetting
				, EPropertyType _PropertyType
				, CBuildSystem const &_BuildSystem
				, NContainer::TCMap<CConfiguration, CEntityMutablePointer> const &_EntitiesPerConfig
			)
		;

		void f_PopulateSetting
			(
				CPropertyKeyReference const &_GeneratorSetting
				, EPropertyType _PropertyType
				, CBuildSystem const &_BuildSystem
				, NContainer::TCMap<CConfiguration, CEntityMutablePointer> const &_EntitiesPerConfig
				, CGeneratorSetting &o_Result
			)
		;

		template <typename tf_CValue, typename tf_CThis>
		static TCValueWithPositions<tf_CValue> fsp_GetSingleSetting(tf_CThis &&_This, NStr::CStr const &_Name);

		template <typename tf_CValue>
		TCValueWithPositions<tf_CValue> f_GetSingleSetting(NStr::CStr const &_Name) const &;

		template <typename tf_CValue>
		TCValueWithPositions<tf_CValue> f_GetSingleSetting(NStr::CStr const &_Name) &&;

		template <typename tf_CValue>
		tf_CValue f_GetSingleSettingWithoutPositions(NStr::CStr const &_Name) const;

		template <typename tf_CValue>
		TCValueWithPositions<tf_CValue> f_GetSetting(CConfiguration const &_Config, NStr::CStr const &_Name) const &;

		template <typename tf_CValue>
		tf_CValue f_GetSettingWithoutPositions(CConfiguration const &_Config, NStr::CStr const &_Name) const &;

		NContainer::TCMap<CConfiguration, CGeneratorSetting> &f_ConstructSettings()
		{
#if DMibEnableSafeCheck > 0
			DMibFastCheck(!m_Settings);
			m_Settings = fg_Construct();
			return *m_Settings;
#else
			return m_Settings;
#endif
		}

		void f_DestructSettings()
		{
			m_Settings.f_Clear();
		}

		NContainer::TCMap<CConfiguration, CGeneratorSetting> &f_Settings()
		{
#if DMibEnableSafeCheck > 0
			return *m_Settings;
#else
			return m_Settings;
#endif
		}

		NContainer::TCMap<CConfiguration, CGeneratorSetting> const &f_Settings() const
		{
#if DMibEnableSafeCheck > 0
			return *m_Settings;
#else
			return m_Settings;
#endif
		}

#if DMibEnableSafeCheck > 0
		NStorage::TCOptional<NContainer::TCMap<CConfiguration, CGeneratorSetting>> m_Settings;
#else
		NContainer::TCMap<CConfiguration, CGeneratorSetting> m_Settings;
#endif
	};
}

#include "Malterlib_BuildSystem_GeneratorSettings.hpp"
