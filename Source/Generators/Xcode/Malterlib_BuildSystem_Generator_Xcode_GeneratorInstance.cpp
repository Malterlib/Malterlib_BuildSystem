// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include <AOCC/AOXMLUtils.h>

namespace NMib::NBuildSystem::NXcode
{
	CGeneratorInstance::~CGeneratorInstance()
	{
		m_BuildSystem.f_SetGeneratorInterface(nullptr);
	}

	CGeneratorInstance::CGeneratorInstance
		(
			CBuildSystem const &_BuildSystem
			, CBuildSystemData const &_BuildSystemData
			, TCMap<CPropertyKey, CStr> const &_InitialValues
			, CStr const &_OutputDir
		)
		: m_BuildSystem(_BuildSystem)
		, m_BuildSystemData(_BuildSystemData)
		, m_OutputDir(_OutputDir)
	{
		m_RelativeBasePath = CFile::fs_MakePathRelative(m_BuildSystem.f_GetBaseDir(), m_OutputDir + "/Files/Temp");
		m_RelativeBasePathAbsolute = "$SRCROOT/" + m_RelativeBasePath;
		m_BuildSystem.f_SetGeneratorInterface(this);
		CEntityKey Key;
		Key.m_Type = EEntityType_GeneratorSetting;
		Key.m_Name = "Xcode";
		CStr Generator = _InitialValues[CPropertyKey(EPropertyType_Property, "Generator")];
		m_XcodeVersion = Generator.f_Replace("Xcode", "").f_ToInt(uint32(4));
		// DConOut("Generator: {} Version: {}" DNewLine, Generator << m_XcodeVersion);

		auto pEntity = _BuildSystemData.m_RootEntity.m_ChildEntitiesMap.f_FindEqual(Key);
		if (!pEntity)
			m_BuildSystem.fs_ThrowError(CFilePosition(), "No Xcode generator settings found");
		_BuildSystem.f_EvaluateData(m_GeneratorSettingsData, _InitialValues, pEntity, nullptr, nullptr, true, true);

		auto pSettings = m_GeneratorSettingsData.m_RootEntity.m_ChildEntitiesMap.f_FindEqual(Key);
		if (!pSettings)
			m_BuildSystem.fs_ThrowError(CFilePosition(), "No Xcode generator settings found");
		m_pGeneratorSettings = fg_Explicit(pSettings);
		_BuildSystem.f_EvaluateAllGeneratorSettings(*pSettings);
	}

	CStr CGeneratorInstance::f_GetExpandedPath(CStr const &_Path, CStr const& _Base) const
	{
		return CFile::fs_GetExpandedPath(_Path.f_Replace(m_RelativeBasePathAbsolute, m_BuildSystem.f_GetBaseDir()), _Base);
	}

	bool CGeneratorInstance::f_GetBuiltin(CStr const &_Value, CStr &_Result) const
	{
		// TODO
		if (_Value == "BasePathRelativeProject")
		{
			_Result = m_RelativeBasePath;
			return true;
		}
		else if (_Value == "BasePath")
		{
			_Result = m_RelativeBasePathAbsolute;
			return true;
		}
		else if (_Value == "GeneratedBuildSystemDir")
		{
			_Result = m_OutputDir;
			return true;
		}
		else if (_Value == "ProjectPath")
		{
			_Result = ".";
			return true;
		}
		else if (_Value == "Inherit")
		{
			_Result = "";
			return true;
		}
		else if (_Value == "IntermediateDirectory")
		{
			_Result = "$CONFIGURATION_TEMP_DIR/";
			return true;
		}
		else if (_Value == "OutputDirectory")
		{
			_Result = "$CONFIGURATION_BUILD_DIR/";
			return true;
		}
		else if (_Value == "SourceFileName")
		{
			_Result = "$(InputFileBase)";
			return true;
		}
		return false;
	}

	bool CGeneratorInstance::CConfigValue::operator < (CConfigValue const &_Right) const
	{
		if (m_Parent < _Right.m_Parent)
			return true;
		else if (m_Parent > _Right.m_Parent)
			return false;
		if (m_Entity < _Right.m_Entity)
			return true;
		else if (m_Entity > _Right.m_Entity)
			return false;
		if (m_Property < _Right.m_Property)
			return true;
		else if (m_Property > _Right.m_Property)
			return false;

		if (m_Value < _Right.m_Value)
			return true;
		else if (m_Value > _Right.m_Value)
			return false;
		
		return m_Values < _Right.m_Values;
	}
	
	CGeneratorInstance::CThreadLocal::CThreadLocal()
		: m_pXMLFile(nullptr)
	{
	}
	
	void CGeneratorInstance::CThreadLocal::f_CreateDirectory(CStr const &_Path)
	{
		auto Mapped = m_CreateDirectoryCache(_Path);
		if (Mapped.f_WasCreated())
			CFile::fs_CreateDirectory(_Path);
	}
}
