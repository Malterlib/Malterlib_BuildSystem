// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_VisualStudio.h"

namespace NMib::NBuildSystem::NVisualStudio
{
	TCVector<CGeneratorSettings::CVS_Setting> CGeneratorSettings::ms_ExcludedFromBuildVSSettings{CVS_Setting_Item{CStr(gc_ConstString_ExcludedFromBuild), gc_ConstString_true}};

	void CGeneratorSettings::CVS_SettingShared::fs_FromJson(CVS_SettingShared &o_Value, NEncoding::CEJSONSorted &&_JSON)
	{
		o_Value.m_Key = fg_Move(_JSON["Key"].f_String());
		o_Value.m_Value = fg_Move(_JSON["Value"].f_String());
	}

	auto CGeneratorSettings::CVS_Setting_PropertyGroup::fs_FromJson(NEncoding::CEJSONSorted &&_JSON) -> CVS_Setting_PropertyGroup
	{
		CVS_Setting_PropertyGroup Return;
		CVS_SettingShared::fs_FromJson(Return, fg_Move(_JSON));
		if (auto pValue = _JSON.f_GetMember("Label"))
			Return.m_Label = fg_Move(pValue->f_String());
				
		return Return;
	}

	auto CGeneratorSettings::CVS_Setting_ItemDefinitionGroup::fs_FromJson(NEncoding::CEJSONSorted &&_JSON) -> CVS_Setting_ItemDefinitionGroup
	{
		CVS_Setting_ItemDefinitionGroup Return;
		CVS_SettingShared::fs_FromJson(Return, fg_Move(_JSON));
		Return.m_Name = fg_Move(_JSON["Name"].f_String());
				
		return Return;
	}
}
