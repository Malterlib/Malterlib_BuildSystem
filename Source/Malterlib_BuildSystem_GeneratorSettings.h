// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMib::NBuildSystem
{
	struct CGeneratorSetting
	{
		template <typename tf_CValue>
		TCValueWithPositions<tf_CValue> f_GetSetting(NStr::CStr const &_Name) const;
		template <typename tf_CValue>
		tf_CValue f_GetSettingWithoutPositions(NStr::CStr const &_Name) const;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const;

		NEncoding::CEJsonSorted m_Value;
		CBuildSystemPropertyInfo m_PropertyInfo;
		bool m_bIsFullEval = false;
	};

	struct DMibRunsToCompletion CGeneratorSettings
	{
		auto f_PopulateSettings
			(
				CPropertyKeyReference const &_GeneratorSetting
				, EPropertyType _PropertyType
				, CBuildSystem const &_BuildSystem
				, NContainer::TCMap<CConfiguration, CEntityMutablePointer> const &_EntitiesPerConfig
			)
			-> NConcurrency::TCUnsafeFuture<void>
		;

		static void fs_PopulateSetting
			(
				CPropertyKeyReference const &_GeneratorSetting
				, EPropertyType _PropertyType
				, CBuildSystem const &_BuildSystem
				, CEntity &_Entity
				, CGeneratorSetting &o_Result
			)
		;

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

		NContainer::TCMap<CConfiguration, CGeneratorSetting> &f_ConstructSettings();
		void f_DestructSettings();

		NContainer::TCMap<CConfiguration, CGeneratorSetting> &f_Settings();
		NContainer::TCMap<CConfiguration, CGeneratorSetting> const &f_Settings() const;

#if DMibEnableSafeCheck > 0
		NStorage::TCOptional<NContainer::TCMap<CConfiguration, CGeneratorSetting>> m_Settings;
#else
		NContainer::TCMap<CConfiguration, CGeneratorSetting> m_Settings;
#endif

	private:
		template <typename tf_CValue, typename tf_CThis>
		static TCValueWithPositions<tf_CValue> fsp_GetSingleSetting(tf_CThis &&_This, NStr::CStr const &_Name);
	};
}

#include "Malterlib_BuildSystem_GeneratorSettings.hpp"
