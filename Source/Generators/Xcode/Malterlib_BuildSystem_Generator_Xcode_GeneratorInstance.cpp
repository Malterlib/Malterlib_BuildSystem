// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Xcode.h"
#include "../../Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"
#include <Mib/XML/XML>

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
			, TCMap<CPropertyKey, CEJSONSorted> const &_InitialValues
			, CStr const &_OutputDir
		)
		: m_BuildSystem(_BuildSystem)
		, m_BuildSystemData(_BuildSystemData)
		, m_OutputDir(_OutputDir)
	{
		m_RelativeBasePath = CFile::fs_MakePathRelative(m_BuildSystem.f_GetBaseDir(), m_OutputDir.f_String() + "/Files/Temp");
		m_RelativeBasePathAbsolute = "$SRCROOT/" + m_RelativeBasePath.f_String();
		m_BuildSystem.f_SetGeneratorInterface(this);
		CStr Generator = _InitialValues[CPropertyKey(_BuildSystem.f_StringCache(), EPropertyType_Property, gc_ConstString_Generator)].f_String();
		m_XcodeVersion = Generator.f_Replace("Xcode", "").f_ToInt(uint32(13));
		
		_BuildSystem.f_RegisterBuiltinVariables
			(
				{
					{CPropertyKey(_BuildSystem.f_StringCache(), gc_ConstString_XcodeGeneratorDependencyFiles), DMibBuildSystemTypeWithPosition(g_StringArray)}
				}
			)
		;
	}

	CStr CGeneratorInstance::f_GetExpandedPath(CStr const &_Path, CStr const &_Base) const
	{
		return CFile::fs_GetExpandedPath(_Path.f_Replace(m_RelativeBasePathAbsolute.f_String(), m_BuildSystem.f_GetBaseDir()), _Base);
	}

	CSystemEnvironment CGeneratorInstance::f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const
	{
		return fg_GetSys()->f_Environment();
	}

	CValuePotentiallyByRef CGeneratorInstance::f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const
	{
		// TODO
		if (_Value == gc_ConstString_BasePathRelativeProject.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePathRelativeProject")->f_AddValue(m_RelativeBasePath, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_RelativeBasePath;
		}
		else if (_Value == gc_ConstString_BasePath.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePath")->f_AddValue(m_RelativeBasePathAbsolute, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_RelativeBasePathAbsolute;
		}
		else if (_Value == gc_ConstString_GeneratedBuildSystemDir.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.GeneratedBuildSystemDir")->f_AddValue(m_OutputDir, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_OutputDir;
		}
		else if (_Value == gc_ConstString_ProjectPath.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.ProjectPath")->f_AddValue(m_Builtin_ProjectPath, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_ProjectPath;
		}
		else if (_Value == gc_ConstString_Inherit.m_String)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Inherit")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_Inherit;
		}

		return CEJSONSorted();
	}
	
	void CGeneratorInstance::CSharedState::f_CreateDirectory(CStr const &_Path)
	{
		auto Mapped = m_CreateDirectoryCache(_Path);
		if (Mapped.f_WasCreated())
			CFile::fs_CreateDirectory(_Path);
	}
}
