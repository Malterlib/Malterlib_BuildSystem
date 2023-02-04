// Copyright © 2026 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../../Malterlib_BuildSystem.h"
#include "../Shared/Malterlib_BuildSystem_Generator_VisualStudioRootHelper.h"

namespace NMib::NBuildSystem::NNinja
{
	struct CBuildAndRuleEntry;

	extern int64 g_CooperativeTimeSliceCycles;

	struct CRule
	{
		auto operator <=> (CRule const &_Right) const = default;

		static CRule fs_FromJson(CEJsonSorted &&_Json);

		template <typename tf_CStream>
		void f_Stream(tf_CStream &o_Stream)
		{
			o_Stream % m_Command;
			o_Stream % m_Environment;
			o_Stream % m_OtherProperties;
		}

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("Command: {} Environment: {vs} Other: {vs}\n") << m_Command << m_Environment << m_OtherProperties;
		}

		bool f_IsPhony() const
		{
			auto pPhonyValue = m_OtherProperties.f_FindEqual(gc_Str<"phony">.m_Str);
			return pPhonyValue && *pPhonyValue == "true";
		}

		CStr m_Command;
		TCMap<CStr, CStr> m_Environment;
		TCMap<CStr, CStr> m_OtherProperties;

		void f_MergeFrom(CBuildAndRuleEntry const &_Entry, CBuildAndRuleEntry &o_Entry);
	};

	struct CBuild
	{
		auto operator <=> (CBuild const &_Right) const = default;

		static CBuild fs_FromJson(CEJsonSorted &&_Json);

		template <typename tf_CStream>
		void f_Stream(tf_CStream &o_Stream)
		{
			o_Stream % m_Description;
			o_Stream % m_Inputs;
			o_Stream % m_Outputs;
			o_Stream % m_ImplicitOutputs;
			o_Stream % m_ImplicitDependencies;
			o_Stream % m_OrderDependencies;
			o_Stream % m_Validations;
			o_Stream % m_Variables;
		}

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("Description: {} Inputs: {} Outputs: {} ImplicitDependencies: {} OrderDependencies: {} Validations: {} Variables: {vs}\n")
				<< m_Description
				<< m_Inputs
				<< m_Outputs
				<< m_ImplicitDependencies
				<< m_OrderDependencies
				<< m_Validations
				<< m_Variables
			;
		}

		void f_MergeFrom(CBuildAndRuleEntry const &_Entry, CBuildAndRuleEntry &o_Entry);

		CStr m_Description;
		TCVector<CStr> m_Inputs;
		TCVector<CStr> m_Outputs;
		TCVector<CStr> m_ImplicitOutputs;
		TCVector<CStr> m_ImplicitDependencies;
		TCVector<CStr> m_OrderDependencies;
		TCVector<CStr> m_Validations;
		TCMap<CStr, CStr> m_Variables;
	};

	struct CBuildAndRuleEntry
	{
		void f_MergeFrom(CBuildSystem const &_BuildSystem, CBuildAndRuleEntry const &_Entry, TCMap<int64, TCVector<TCVariant<CStr, TCVector<CStr>>>> &o_PrioritizedFlags);

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("Rule: {}\nBuild: {}\nFlags: {}") << m_Rule << m_Build << m_Flags;
		}

		CBuildSystemUniquePositions m_Positions;
		CRule m_Rule;
		CBuild m_Build;
		TCVector<TCVariant<CStr, TCVector<CStr>>> m_Flags;
		int64 m_FlagsPriority = 0;
	};

	struct CRuleAndBuild
	{
		static void fs_FromJson(CRuleAndBuild &o_This, CBuildSystem const &_BuildSystem, CEJsonSorted &&_Json);
		static void fs_RuleFromJson(CRuleAndBuild &o_This, CBuildSystem const &_BuildSystem, CEJsonSorted &&_Json, TCMap<CStr, TCVector<CStr>> *o_pSharedFlags = nullptr);
		void f_FromGeneratorSetting(CBuildSystem const &_BuildSystem, CGeneratorSetting &&_Setting);
		CBuildAndRuleEntry f_GetMergedEntry(CBuildSystem const &_BuildSystem, TCMap<int64, TCVector<TCVariant<CStr, TCVector<CStr>>>> &o_PrioritizedFlags) const;
		CBuildAndRuleEntry f_GetMergedEntry(CBuildSystem const &_BuildSystem) const;
		void f_InheritFrom
			(
				CBuildAndRuleEntry &o_Entry
				, CBuildSystem const &_BuildSystem
				, CRuleAndBuild const &_Other
				, TCMap<int64, TCVector<TCVariant<CStr, TCVector<CStr>>>> &o_PrioritizedFlags
			) const
		;

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("Type: {}\nEntries: {}\n") << m_Type << m_Entries;
		}

		TCMap<CStr, CBuildAndRuleEntry> m_Entries;
		CStr m_Type;
		bool m_bDisabled = false;
		bool m_bFullEval = false;
	};

	struct CBuilds
	{
		TCMap<CConfiguration, CRuleAndBuild> m_Builds;
	};

	struct CConfigResultTarget
	{
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("PreBuild: {} Build: {} PostBuild: {}\n") << m_PreBuild << m_Build << m_PostBuild;
		}

		CRuleAndBuild m_PreBuild;
		CRuleAndBuild m_Build;
		CRuleAndBuild m_PostBuild;
		TCVector<CRuleAndBuild> m_OtherRules;
		TCMap<CStr, TCVector<CStr>> m_SharedFlags;
	};

	struct CConfigResultCompile
	{
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("Build: {}\n") << m_Build;
		}

		CRuleAndBuild m_Build;
	};

	struct CProjectFile
	{
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		CBuilds m_Builds;
		CFilePosition m_Position;
	};

	struct CWorkspace;

	struct align_cacheline CProject
	{
		CProject(CWorkspace *_pWorkspace);

		CStr const &f_GetName() const;

		TCMap<CFileKey, CProjectFile> m_Files;

		TCMap<CConfiguration, CConfigResultTarget> m_EvaluatedTargetSettings;
		TCMap<CConfiguration, CStr> m_CompileCommandsFilePath;

		TCMap<CStr, CBuilds> m_Builds;

		CStr m_EntityName;

		CWorkspace *m_pWorkspace;

		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledProjectConfigs;
		CFilePosition m_Position;
		CFilePosition m_ProjectPosition;
	};

	struct align_cacheline CWorkspace
	{
		CStr const &f_GetName() const;

		CWorkspace(CStr const &_Name)
			: m_Name(_Name)
		{
		}

		CStr m_Name;

		TCMap<CConfiguration, TCUniquePointer<CWorkspaceInfo>> m_WorkspaceInfos;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		TCMap<CStr, CProject> m_Projects;

		mint m_nTotalTargets = 0;

		CFilePosition m_Position;

		CStr m_EntityName;
	};

	struct CGeneratorState
	{
		TCMap<CStr, TCUniquePointer<CWorkspace>> m_Workspaces;
	};

	struct CGeneratorThreadLocal
	{
		CConfigResultTarget *m_pCurrentConfigResult = nullptr;
	};

	struct CGeneratorThreadLocalConfigScope final : public CCoroutineThreadLocalHandler
	{
		CGeneratorThreadLocalConfigScope(TCThreadLocal<CGeneratorThreadLocal> &_ThreadLocal, CConfigResultTarget *_pConfigResult);
		~CGeneratorThreadLocalConfigScope();

		void f_Suspend() noexcept override;
		void f_ResumeNoExcept() noexcept override;

		CConfigResultTarget *m_pOldConfigResult = nullptr;
		CConfigResultTarget *m_pNewConfigResult = nullptr;
		TCThreadLocal<CGeneratorThreadLocal> &m_ThreadLocal;
	};

	struct CGeneratorInstance : public ICGeneratorInterface
	{
		CGeneratorInstance
			(
				CBuildSystem const &_BuildSystem
				, CBuildSystemData const &_BuildSystemData
				, TCMap<CPropertyKey, CEJsonSorted> const &_InitialValues
				, CStr const &_OutputDir
			)
		;
		~CGeneratorInstance();

		CValuePotentiallyByRef f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const override;
		CStr f_GetExpandedPath(CStr const &_Path, CStr const &_Base) const override;
		CSystemEnvironment f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const override;

		TCUnsafeFuture<void> f_GenerateProjectFile(CProject &_Project) const;
		TCUnsafeFuture<void> f_GenerateWorkspaceFile(CWorkspace &_Workspace, CStr const &_OutputDir) const;

		// Members
		CBuildSystem const &m_BuildSystem;
		CBuildSystemData const &m_BuildSystemData;

		CGeneratorState m_State;
		CEJsonSorted m_OutputDir;
		CEJsonSorted m_RelativeBasePathAbsolute;
		CEJsonSorted m_RelativeBasePath;

		CEJsonSorted m_Builtin_ProjectPath;
		CEJsonSorted m_Builtin_Inherit;

		CEJsonSorted m_Builtin_Input;
		CEJsonSorted m_Builtin_Output;
		CEJsonSorted m_Builtin_Flags;
		CEJsonSorted m_Builtin_Dollar;
		CEJsonSorted m_Builtin_RspFile;

		mutable CVisualStudioRootHelper m_VisualStudioRootHelper;
		uint32 m_VisualStudioVersion = 2022;

		static TCAggregate<TCThreadLocal<CGeneratorThreadLocal>> ms_ThreadLocal;

	private:
		struct CCompileType
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const
			{
				o_Str += typename tf_CStr::CFormat("{vs}") << m_EnabledConfigs;
			}

			CStr const &f_GetType() const
			{
				return TCMap<CStr, CCompileType>::fs_GetKey(*this);
			}

			TCSet<CConfiguration> m_EnabledConfigs;
		};

		struct CSingleValue
		{
			CSingleValue(CValuePotentiallyByRef &&_Value)
				: m_Value(fg_Move(_Value))
			{
			}

			CBuildSystemUniquePositions m_Positions;
			CValuePotentiallyByRef m_Value;
		};

		TCUnsafeFuture<TCMap<CStr, CCompileType>> fp_GenerateProjectFile_File(CProject &_Project) const;
		TCUnsafeFuture<void> fp_GenerateProjectFile_FileTypes
			(
				CProject &_Project
				, TCMap<CStr, CCompileType> const &_CompileTypes
			) const
		;

		TCUnsafeFuture<void> fp_EvaluateTargetSettings(CProject &_Project) const;

		void fp_SetEvaluatedValuesTarget
			(
				CEntityMutablePointer const &_Entity
				, CConfigResultTarget &o_Result
				, CStr const &_Name
			) const
		;

		CSingleValue fp_GetConfigValue
			(
				TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
				, CConfiguration const &_Configuration
				, CPropertyKeyReference const &_PropertyKey
				, EEJsonType _ExpectedType
				, bool _bOptional
			) const
		;
	};
}
