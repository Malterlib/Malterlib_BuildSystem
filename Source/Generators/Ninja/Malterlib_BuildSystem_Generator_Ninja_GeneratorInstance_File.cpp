// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Generator_Ninja.h"
#include "../../Malterlib_BuildSystem_DefinedProperties.hpp"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NNinja
{
	void CRuleAndBuild::f_FromGeneratorSetting(CBuildSystem const &_BuildSystem, CGeneratorSetting &&_Setting)
	{
		m_bFullEval = _Setting.m_bIsFullEval;
		m_bDisabled = _Setting.f_GetSettingWithoutPositions<bool>("Disabled");
		if (m_bDisabled)
			return;

		CRuleAndBuild::fs_FromJson(*this, _BuildSystem, fg_Move(_Setting.m_Value));
	}

	template <typename tf_CValue>
	[[noreturn]] static void fg_ReportAmbiguous(CBuildAndRuleEntry const &_Entry, CBuildAndRuleEntry &o_Entry, CStr const &_Name, tf_CValue const &_NewValue, tf_CValue const &_OldValue)
	{
		NContainer::TCVector<CBuildSystemError> OtherErrors;
		CStr ErrorMessage = "Ambiguous value for '{}' entry.\nValue:\n{}"_f << _Name << CStr::fs_ToStr(_NewValue).f_Indent("    ");
		if (!o_Entry.m_Positions.f_IsEmpty())
		{
			auto &Error = OtherErrors.f_Insert();
			Error.m_Error = "See previous value:\n{}"_f << CStr::fs_ToStr(_OldValue).f_Indent("    ");
			Error.m_Positions = o_Entry.m_Positions;
		}
		else
			ErrorMessage += "\nOld value:\n{}"_f << CStr::fs_ToStr(_OldValue).f_Indent("    ");

		CBuildSystem::fs_ThrowError
			(
				_Entry.m_Positions
				, ErrorMessage
				, OtherErrors
			)
		;
	}

	void CRule::f_MergeFrom(CBuildAndRuleEntry const &_Entry, CBuildAndRuleEntry &o_Entry)
	{
		if (_Entry.m_Rule.m_Command)
		{
			if (m_Command)
				fg_ReportAmbiguous(_Entry, o_Entry, gc_Str<"Command">, m_Command, _Entry.m_Rule.m_Command);
			m_Command = _Entry.m_Rule.m_Command;
		}

		for (auto &EnvironmentEntry : _Entry.m_Rule.m_Environment.f_Entries())
		{
			auto &Name = EnvironmentEntry.f_Key();
			if (auto *pOldValue = m_Environment.f_FindEqual(Name))
				fg_ReportAmbiguous(_Entry, o_Entry, "environment.{}"_f << Name, EnvironmentEntry.f_Value(), *pOldValue);

			m_Environment[Name] = EnvironmentEntry.f_Value();
		}

		for (auto &PropertyEntry : _Entry.m_Rule.m_OtherProperties.f_Entries())
		{
			auto &PropertyName = PropertyEntry.f_Key();
			if (auto *pOldValue = m_OtherProperties.f_FindEqual(PropertyName))
				fg_ReportAmbiguous(_Entry, o_Entry, PropertyName, PropertyEntry.f_Value(), *pOldValue);

			m_OtherProperties[PropertyName] = PropertyEntry.f_Value();
		}
	}

	void CBuild::f_MergeFrom(CBuildAndRuleEntry const &_Entry, CBuildAndRuleEntry &o_Entry)
	{
		if (_Entry.m_Build.m_Description)
		{
			if (m_Description)
				fg_ReportAmbiguous(_Entry, o_Entry, gc_Str<"Description">, m_Description, _Entry.m_Build.m_Description);
			m_Description = _Entry.m_Build.m_Description;
		}

		m_Inputs.f_Insert(_Entry.m_Build.m_Inputs);
		m_Outputs.f_Insert(_Entry.m_Build.m_Outputs);
		m_ImplicitOutputs.f_Insert(_Entry.m_Build.m_ImplicitOutputs);
		m_ImplicitDependencies.f_Insert(_Entry.m_Build.m_ImplicitDependencies);
		m_OrderDependencies.f_Insert(_Entry.m_Build.m_OrderDependencies);
		m_Validations.f_Insert(_Entry.m_Build.m_Validations);

		for (auto &VariableEntry : _Entry.m_Build.m_Variables.f_Entries())
		{
			auto &VariableName = VariableEntry.f_Key();
			if (auto *pOldValue = m_Variables.f_FindEqual(VariableName))
				fg_ReportAmbiguous(_Entry, o_Entry, "{}"_f << VariableName, VariableEntry.f_Value(), *pOldValue);

			m_Variables[VariableName, VariableEntry.f_Value()];
		}
	}

	void CBuildAndRuleEntry::f_MergeFrom(CBuildSystem const &_BuildSystem, CBuildAndRuleEntry const &_Entry, TCMap<int64, TCVector<TCVariant<CStr, TCVector<CStr>>>> &o_PrioritizedFlags)
	{
		m_Build.f_MergeFrom(_Entry, *this);
		m_Rule.f_MergeFrom(_Entry, *this);
		if (!_Entry.m_Flags.f_IsEmpty())
			o_PrioritizedFlags[_Entry.m_FlagsPriority].f_Insert(_Entry.m_Flags);

		if (_BuildSystem.f_EnablePositions())
			m_Positions.f_AddPositions(_Entry.m_Positions);
	}

	CBuildAndRuleEntry CRuleAndBuild::f_GetMergedEntry(CBuildSystem const &_BuildSystem, TCMap<int64, TCVector<TCVariant<CStr, TCVector<CStr>>>> &o_PrioritizedFlags) const
	{
		CBuildAndRuleEntry Return;

		for (auto &Entry : m_Entries)
			Return.f_MergeFrom(_BuildSystem, Entry, o_PrioritizedFlags);

		return Return;
	}

	CBuildAndRuleEntry CRuleAndBuild::f_GetMergedEntry(CBuildSystem const &_BuildSystem) const
	{
		TCMap<int64, TCVector<TCVariant<CStr, TCVector<CStr>>>> PrioritizedFlags;

		auto Return = f_GetMergedEntry(_BuildSystem, PrioritizedFlags);

		for (auto iFlags = PrioritizedFlags.f_GetIteratorReverse(); iFlags; ++iFlags)
			Return.m_Flags.f_Insert(fg_Move(*iFlags));

		return Return;
	}

	void CRuleAndBuild::f_InheritFrom
		(
			CBuildAndRuleEntry &o_Entry
			, CBuildSystem const &_BuildSystem
			, CRuleAndBuild const &_Other
			, TCMap<int64, TCVector<TCVariant<CStr, TCVector<CStr>>>> &o_PrioritizedFlags
		) const
	{
		for (auto &Entry : _Other.m_Entries)
		{
			auto &EntryName = _Other.m_Entries.fs_GetKey(Entry);
			if (m_Entries.f_FindEqual(EntryName))
				continue;
			o_Entry.f_MergeFrom(_BuildSystem, Entry, o_PrioritizedFlags);
		}
	}

	auto CGeneratorInstance::fp_GenerateProjectFile_File(CProject &_Project) const -> TCUnsafeFuture<TCMap<CStr, CCompileType>>
	{
		co_await ECoroutineFlag_CaptureExceptions;

		for (auto &File : _Project.m_Files)
		{
			for (auto &PerConfig : File.m_EnabledConfigs)
				File.m_Builds.m_Builds[File.m_EnabledConfigs.fs_GetKey(PerConfig)];
		}

		co_await g_Yield;

		co_await fg_ParallelForEach
			(
				_Project.m_EnabledProjectConfigs
				, [&](auto &_ProjectEntity) -> TCUnsafeFuture<void>
				{
					co_await ECoroutineFlag_CaptureExceptions;
					co_await m_BuildSystem.f_CheckCancelled();

					auto &Config = _Project.m_EnabledProjectConfigs.fs_GetKey(_ProjectEntity);
					auto *pTargetSettings = _Project.m_EvaluatedTargetSettings.f_FindEqual(Config);
					DMibFastCheck(pTargetSettings);
					auto &TargetSettings = *pTargetSettings;

					CGeneratorThreadLocalConfigScope ConfigScope(*ms_ThreadLocal, &TargetSettings);

					NTime::CCyclesStopwatch YieldStopwatch(true);

					auto OnResume = co_await fg_OnResume
						(
							[&]() -> CExceptionPointer
							{
								YieldStopwatch.f_Start();
								return {};
							}
						)
					;

					for (auto &File : _Project.m_Files)
					{
						auto *pResult = File.m_Builds.m_Builds.f_FindEqual(Config);
						if (!pResult)
							continue;

						auto &Entity = **File.m_EnabledConfigs.f_FindEqual(Config);

						CGeneratorSetting Result;
						CGeneratorSettings::fs_PopulateSetting(gc_ConstKey_GeneratorSetting_Compile, EPropertyType_Compile, m_BuildSystem, Entity, Result);

						CBuildSystemPropertyInfo PropertyInfoValidateSettings;
						m_BuildSystem.f_EvaluateEntityProperty
							(
								Entity
								, gc_ConstKey_GeneratorSetting_CompileValidateSettings
								, PropertyInfoValidateSettings
							)
						;

						pResult->f_FromGeneratorSetting(m_BuildSystem, fg_Move(Result));

						if (YieldStopwatch.f_GetCycles() > g_CooperativeTimeSliceCycles)
							co_await g_Yield;
					}

					co_return {};
				}
				, m_BuildSystem.f_SingleThreaded()
			)
		;

		co_await g_Yield;

		co_await m_BuildSystem.f_CheckCancelled();

		TCMap<CStr, CCompileType> CompileTypes;

		for (auto &File : _Project.m_Files)
		{
			for (auto &Build : File.m_Builds.m_Builds)
			{
				auto &CompileType = CompileTypes[Build.m_Type];
				CompileType.m_EnabledConfigs[File.m_Builds.m_Builds.fs_GetKey(Build)];
			}
		}

		co_return fg_Move(CompileTypes);
	}
}


