// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"

namespace NMib::NBuildSystem::NVisualStudio
{
	TCVector<CGeneratorSettings::CVS_Setting> CGeneratorSettings::ms_ExcludedFromBuildVSSettingsTrue{CVS_Setting_Item{CStr(gc_ConstString_ExcludedFromBuild), gc_ConstString_true}};
	TCVector<CGeneratorSettings::CVS_Setting> CGeneratorSettings::ms_ExcludedFromBuildVSSettingsFalse{CVS_Setting_Item{CStr(gc_ConstString_ExcludedFromBuild), gc_ConstString_false}};

	void CGeneratorSettings::CVS_SettingShared::fs_FromJson(CVS_SettingShared &o_Value, NEncoding::CEJsonSorted &&_Json)
	{
		o_Value.m_Key = fg_Move(_Json["Key"].f_String());
		o_Value.m_Value = fg_Move(_Json["Value"].f_String());
	}

	auto CGeneratorSettings::CVS_Setting_PropertyGroup::fs_FromJson(NEncoding::CEJsonSorted &&_Json) -> CVS_Setting_PropertyGroup
	{
		CVS_Setting_PropertyGroup Return;
		CVS_SettingShared::fs_FromJson(Return, fg_Move(_Json));
		if (auto pValue = _Json.f_GetMember("Label"))
			Return.m_Label = fg_Move(pValue->f_String());

		return Return;
	}

	auto CGeneratorSettings::CVS_Setting_ItemDefinitionGroup::fs_FromJson(NEncoding::CEJsonSorted &&_Json) -> CVS_Setting_ItemDefinitionGroup
	{
		CVS_Setting_ItemDefinitionGroup Return;
		CVS_SettingShared::fs_FromJson(Return, fg_Move(_Json));
		Return.m_Name = fg_Move(_Json["Name"].f_String());

		return Return;
	}
}
