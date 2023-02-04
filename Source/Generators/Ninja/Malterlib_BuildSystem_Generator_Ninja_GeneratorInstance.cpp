// Copyright © 2026 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_Generator_Ninja.h"
#include "../../Malterlib_BuildSystem_Evaluate_BuiltinFunctions.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NNinja
{
	constinit TCAggregate<TCThreadLocal<CGeneratorThreadLocal>> CGeneratorInstance::ms_ThreadLocal{DAggregateInit};

	CGeneratorInstance::~CGeneratorInstance()
	{
		m_BuildSystem.f_SetGeneratorInterface(nullptr);
	}

	CGeneratorInstance::CGeneratorInstance
		(
			CBuildSystem const &_BuildSystem
			, CBuildSystemData const &_BuildSystemData
			, TCMap<CPropertyKey, CEJsonSorted> const &_InitialValues
			, CStr const &_OutputDir
		)
		: m_BuildSystem(_BuildSystem)
		, m_BuildSystemData(_BuildSystemData)
		, m_OutputDir(_OutputDir)
		, m_Builtin_ProjectPath(gc_Str<".">.m_Str)
		, m_Builtin_Inherit(gc_Str<"">.m_Str)
		, m_Builtin_Input(gc_Str<"{uvi9RKGP}in">.m_Str)
		, m_Builtin_Output(gc_Str<"{uvi9RKGP}out">.m_Str)
		, m_Builtin_Flags(gc_Str<"{uvi9RKGP}flags">.m_Str)
		, m_Builtin_Dollar(gc_Str<"{uvi9RKGP}">.m_Str)
		, m_Builtin_RspFile(gc_Str<"{uvi9RKGP}rspfile">.m_Str)
		, m_VisualStudioRootHelper(_BuildSystem)
	{
		m_RelativeBasePath = CFile::fs_MakePathRelative(m_BuildSystem.f_GetBaseDir(), m_OutputDir.f_String() + "/Files/Temp");
		m_RelativeBasePathAbsolute = "$SRCROOT/" + m_RelativeBasePath.f_String();
		m_BuildSystem.f_SetGeneratorInterface(this);
		CStr Generator = _InitialValues[gc_ConstKey_Generator].f_String();
		m_VisualStudioVersion = _BuildSystem.f_GetEnvironmentVariable("MalterlibVisualStudioVersion").f_ToInt(uint32(2022));

		const_cast<CBuildSystem &>(_BuildSystem).f_RegisterBuiltinVariables
			(
				{
					{CPropertyKey(gc_ConstKey_Builtin_VisualStudioRoot), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_Output), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_Input), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_Flags), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_Dollar), DMibBuildSystemTypeWithPosition(g_String)}
					, {CPropertyKey(gc_ConstKey_Builtin_RspFile), DMibBuildSystemTypeWithPosition(g_String)}
				}
			)
		;

		const_cast<CBuildSystem &>(_BuildSystem).f_RegisterFunctions
			(
				{
					{
						gc_Str<"GetSharedFlags">.m_Str
						, CBuildSystem::CBuiltinFunction
						{
							fg_FunctionType
							(
								g_StringArray
								, fg_FunctionParam(g_String, gc_Str<"_Tool">.m_Str)
							)
							, [](CBuildSystem const &_This, CBuildSystem::CEvalPropertyValueContext &_Context, TCVector<CEJsonSorted> &&_Params) -> CEJsonSorted
							{
								auto &ThreadLocal = **ms_ThreadLocal;
								if (!ThreadLocal.m_pCurrentConfigResult)
									CBuildSystem::fs_ThrowError(_Context, "GetSharedFlags can only be called when evaluating a target configuration");

								CStr const &Tool = _Params[0].f_String();
								auto &SharedFlags = ThreadLocal.m_pCurrentConfigResult->m_SharedFlags;

								if (auto *pFlags = SharedFlags.f_FindEqual(Tool))
									return CEJsonSorted(*pFlags);

								return CEJsonSorted(EJsonType_Array);
							}
							, DMibBuildSystemFilePosition
						}
					}
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
		return m_VisualStudioRootHelper.f_GetBuildEnvironment(m_VisualStudioVersion, _Platform, _Architecture);
	}

	CValuePotentiallyByRef CGeneratorInstance::f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const
	{
		if (_Value == gc_ConstKey_Builtin_BasePathRelativeProject.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePathRelativeProject")->f_AddValue(m_RelativeBasePath, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_RelativeBasePath;
		}
		else if (_Value == gc_ConstKey_Builtin_BasePath.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.BasePath")->f_AddValue(m_RelativeBasePathAbsolute, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_RelativeBasePathAbsolute;
		}
		else if (_Value == gc_ConstKey_Builtin_GeneratedBuildSystemDir.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.GeneratedBuildSystemDir")->f_AddValue(m_OutputDir, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_OutputDir;
		}
		else if (_Value == gc_ConstKey_Builtin_ProjectPath.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.ProjectPath")->f_AddValue(m_Builtin_ProjectPath, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_ProjectPath;
		}
		else if (_Value == gc_ConstKey_Builtin_Inherit.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Inherit")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_Inherit;
		}
		else if (_Value == gc_ConstKey_Builtin_Input.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Input")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_Input;
		}
		else if (_Value == gc_ConstKey_Builtin_Output.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Output")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_Output;
		}
		else if (_Value == gc_ConstKey_Builtin_Flags.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Flags")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_Flags;
		}
		else if (_Value == gc_ConstKey_Builtin_Dollar.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Dollar")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_Dollar;
		}
		else if (_Value == gc_ConstKey_Builtin_RspFile.m_Name)
		{
			if (_pStorePositions)
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.Dollar")->f_AddValue(m_Builtin_Inherit, m_BuildSystem.f_EnableValues());
			o_bSuccess = true;
			return &m_Builtin_RspFile;
		}
		else if (_Value == gc_ConstKey_Builtin_VisualStudioRoot.m_Name)
		{
			if (_pStorePositions)
			{
				_pStorePositions->f_AddPosition(DMibBuildSystemFilePosition, "Builtin.VisualStudioRoot")
					->f_AddValue(m_VisualStudioRootHelper.f_GetVisualStudioRoot(m_VisualStudioVersion), m_BuildSystem.f_EnableValues())
				;
			}
			o_bSuccess = true;
			return &m_VisualStudioRootHelper.f_GetVisualStudioRoot(m_VisualStudioVersion);
		}

		return CEJsonSorted();
	}
}
