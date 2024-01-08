// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../../Malterlib_BuildSystem.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NXcode
{
	extern CUniversallyUniqueIdentifier const gc_GeneratorUUIDNamespace;

	// These are ordered as Xcode requires them.
	enum EBuildFileType
	{
		EBuildFileType_Custom,
		EBuildFileType_MlTwk,
		EBuildFileType_MalterlibFS,
		EBuildFileType_QTRcc,
		EBuildFileType_QTMoc,
		EBuildFileType_QTUic,
		EBuildFileType_CCompileInitEarly,
		EBuildFileType_CCompile,
		EBuildFileType_CInclude,
		EBuildFileType_GenericCompile,
		EBuildFileType_GenericCompileWithFlags,
		EBuildFileType_GenericNonCompile,
		EBuildFileType_GenericCustom,
		EBuildFileType_None,
	};

	struct CBuildConfiguration
	{
		CStr f_GetFile() const;
		CStr f_GetFileNoExt() const;
		CStr const &f_GetFileRefGUID() const;
		CStr const &f_GetGUID() const;

		CStr m_ConfigName;
		CStr m_ConfigFileName;
		CStr m_Path;
		bool m_bProject;

	private:
		mutable CStr mp_GUID;
		mutable CStr mp_FileRefGUID;
	};

	struct CBuildScript
	{
		CStr const &f_GetGUID(CConfiguration const &_Configuration) const;
		CStr const &f_GetScriptSetting() const;

		CStr m_Name;
		CStr m_Script;
		TCVector<CStr> m_Inputs;
		TCVector<CStr> m_Outputs;
		TCSet<CStr> m_OutputTypes;

		bool m_bPostBuild = false;
		bool m_bPreBuild = false;
		bool m_bCustom = false;

	private:
		mutable CStr mp_BuildSetting;
		mutable CStr mp_GUID;
	};

	struct CNativeTarget
	{
		CStr const &f_GetGUID();
		CStr const &f_GetSourcesBuildPhaseGUID();
		CStr const &f_GetFrameworksBuildPhaseGUID();
		CStr const &f_GetProductReferenceGUID();
		CStr const &f_GetBuildConfigurationListGUID();
#if 0
		CStr const &f_GetHeadersBuildPhaseGUID();
#endif

		CStr m_Name;
		CStr m_XcodeProductName;
		CStr m_ProductName;
		CStr m_ProductType;
		CStr m_ProductPath;
		CStr m_ProductSourceTree;
		CStr m_Type;
		CStr m_CType;

		TCVector<CNativeTarget *> m_CTargets;

		CBuildConfiguration m_BuildConfiguration;
		TCMap<CStr, CBuildScript> m_BuildScripts;
		TCVector<CBuildScript> m_CustomBuildScripts;
		CStr m_ScriptExport;

		TCSet<CStr> m_IncludedTypes;
		TCSet<CStr> m_ExcludedTypes;

		int32 m_BuildActionMask;
		bool m_bGeneratedBuildScript = false;
		bool m_bDefaultTarget = false;
		bool m_bGenerateScheme = false;

	private:
		CStr mp_GUID;
		CStr mp_SourcesBuildPhaseGUID;
		CStr mp_FrameworksBuildPhaseGUID;
#if 0
		CStr mp_HeadersBuildPhaseGUID;
#endif
		CStr mp_BuildConfigurationListGUID;
		CStr mp_ProductReferenceGUID;
	};

	struct CGroup
	{
		CStr const &f_GetPath() const;
		CStr f_GetGroupPath() const;
		CStr const &f_GetGUID();

		CStr m_Name;
		TCPointer<CGroup> m_pParent;
		bool m_OutputToWorkspace = false;

	private:
		CStr mp_GUID;
	};

	struct CPBXFileReference
	{
		TCValueWithPositions<CStr> m_LastKnownFileType;
		TCValueWithPositions<CStr> m_ExplicitFileType;
		TCValueWithPositions<int32> m_FileEncoding = {0};
		TCValueWithPositions<int32> m_TabWidth = {0};
		TCValueWithPositions<int32> m_UsesTabs = {0};
		TCValueWithPositions<int32> m_IndentWidth = {0};
	};

	struct CElement
	{
		inline CStr const &f_GetValue() const;
		CStr f_GetCombinedValue() const;
		inline TCVector<CStr> f_ValueArray() const;
		static TCVector<CStr> fs_ValueArray(TCSet<TCVariant<CStr, TCVector<CStr>>> const &_Values);
		inline void f_SetValue(CStr const &_Value);
		inline bool f_IsSameValue(CElement const &_Other) const;
		inline bool f_IsEmpty() const;
		CStr const &f_GetProperty() const
		{
			return TCMap<CStr, CElement>::fs_GetKey(*this);
		}

		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("m_ValueSet {vs}   m_Value {}   m_bXcodeProperty {}   m_bUseValues {}\n")
				<< m_ValueSet
				<< m_Value
				<< m_bXcodeProperty
				<< m_bUseValues
			;
		}

		TCSet<TCVariant<CStr, TCVector<CStr>>> m_ValueSet;
		CStr m_Value;
		CBuildSystemUniquePositions m_Positions;
		bool m_bXcodeProperty = false;
		bool m_bUseValues = false;
	};

	struct CConfigResultCompile
	{
		template <typename tf_CStr>
		void f_Format(tf_CStr &o_Str) const
		{
			o_Str += typename tf_CStr::CFormat("Elements: {}\n") << m_Element;
		}

		TCValueWithPositions<CStr> m_Type;
		CPBXFileReference m_PBXFileReference;
		TCMap<CStr, CElement> m_Element;
	};

	struct CProjectFile
	{
		TCPointer<CGroup> m_pGroup;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		TCMap<CConfiguration, CConfigResultCompile> m_CompileResults;
		CFilePosition m_Position;
		CStr m_Type;
		CStr m_LastKnownFileType;
		CStr m_ExplicitFileType;
		int32 m_FileEncoding = 0;
		bool m_bHasCompilerFlags = false;
		uint8 m_TabWidth = 0; // 0 means unset
		uint8 m_IndentWidth = 0; // 0 means unset
		uint8 m_UsesTabs = 0; // 0 means unset, 1 = uses tabs, 2 = uses spaces

		CStr const &f_GetName() const;
		CStr const &f_GetNameGroupPath() const;
		CStr f_GetGroupPath() const;
		CStr const &f_GetFileRefGUID();
		CStr const &f_GetBuildRefGUID(CConfiguration const &_Configuration);
		CStr const &f_GetBuildRuleGUID(CConfiguration const &_Configuration);
		CStr const &f_GetLastKnownFileType();
		CStr const &f_GetCompileFlagsGUID();

	private:
		CStr mp_FileRefGUID;
		CStr mp_FileNameGUID;
		TCMap<CConfiguration, CStr> mp_BuildRefGUIDs;
		TCMap<CConfiguration, CStr> mp_BuildRuleGUIDs;
		CStr mp_CompileFlagsGUID;
	};

	struct CProjectDependency
	{
		struct CPerConfig
		{
			CConfiguration const &f_Configuration() const;

			CStr f_GetName(CProjectDependency const &_Dependency, ch8 const *_pIdentifier) const;

			CStr const &f_GetContainerItemGUID(CProjectDependency const &_Dependency);
			CStr const &f_GetTargetGUID(CProjectDependency const &_Dependency);

			CStr m_CalculatedDependencyName;
			CStr m_CalculatedDependencyExtension;
			CStr m_CalculatedPath;
			CStr m_SearchPath;
			CStr m_LinkerGroup;
			bool m_bLink = false;
			bool m_bObjectLibrary = false;

		private:
			CStr mp_DependencyContainerItemGUID;
			CStr mp_DependencyTargetGUID;
		};

		CStr const &f_GetName() const;
		CStr const &f_GetProductRefGroupGUID();
		CStr const &f_GetFileRefGUID();

		DLinkDS_Link(CProjectDependency, m_Link);
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		CFilePosition m_Position;

		CStr m_Type;
		TCMap<CConfiguration, CPerConfig> m_PerConfig;
		bool m_bInternal = false;
		bool m_bExternal = false;

	private:
		CStr mp_DependencyProductRefGroupGUID;
		CStr mp_DependencyFileRefGUID;
	};

	struct CSolution;

	struct CCustomBuildRule
	{
		CStr m_GUID;
		CStr m_MalterlibCustomBuildCommandLine;
		CStr m_WorkingDirectory;
		TCSet<CStr> m_OutputTypes;
		TCVector<CStr> m_Outputs;
		TCVector<CStr> m_Inputs;
	};

	struct CBuildFileRef
	{
		CConfiguration m_Configuration;
		CStr m_FileName;
		CStr m_Name;
		TCMap<CConfiguration, CStr> m_BuildGUIDs;
		TCMap<CConfiguration, CCustomBuildRule> m_BuildRules;
		TCSet<CConfiguration> m_Disabled;
		CStr m_CompileFlagsGUID;
		CStr m_Type;
		CStr m_FileRefGUID;
		bool m_bHasCompilerFlags = false;
	};

	struct align_cacheline CProject
	{
		CProject(CSolution *_pSolution);

		CStr const &f_GetName() const;
		CStr f_GetPath() const;
		CStr const &f_GetGUID();
		CStr const &f_GetMainGroupGUID();
		CStr const &f_GetGeneratorGroupGUID();
		CStr const &f_GetProductRefGroupGUID();
		CStr const &f_GetProjectDependenciesGroupGUID();
		CStr const &f_GetConfigurationsGroupGUID();
		CStr const &f_GetBuildConfigurationListGUID();
		void fr_FindRecursiveDependencies(CBuildSystem const &_BuildSystem, TCSet<CStr> &_Stack, CProjectDependency const *_pDepend, TCMap<CStr, CProject> &_Projects);

		CNativeTarget &f_GetDefaultNativeTarget(CConfiguration const &_Configuration);

		TCMap<CFileKey, CProjectFile> m_Files;
		TCMap<CStr, CGroup> m_Groups;
		TCMap<CStr, CProjectDependency> m_DependenciesMap;
		DLinkDS_List(CProjectDependency, m_Link) m_DependenciesOrdered;

		TCMap<CConfiguration, TCLinkedList<CNativeTarget>> m_NativeTargets;

		TCMap<EBuildFileType, TCVector<CBuildFileRef>> mp_OrderedBuildTypes;
		TCVector<CBuildConfiguration> m_BuildConfigurationList;

		CStr m_EntityName;

		CSolution *m_pSolution;

		TCPointer<CGroup> m_pGroup;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledProjectConfigs;
		//TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CFilePosition m_ProjectPosition;
		CStr m_FileName;

		bool m_bCheckedDependencies = false;

	private:
		CStr mp_GUID;
		CStr mp_MainGroupGUID;
		CStr mp_GeneratorGroupGUID;
		CStr mp_ProductRefGroupGUID;
		CStr mp_ProjectDependenciesGroupGUID;
		CStr mp_ConfigurationsGroupGUID;
		CStr mp_BuildConfigurationListGUID;
	};

	struct CSolutionFile
	{
		CStr const &f_GetName() const;
		CStr f_GetGroupPath() const;

		TCPointer<CGroup> m_pGroup;
		CFilePosition m_Position;
	};

	struct align_cacheline CSolution
	{
		class CCompare
		{
		public:
			inline_always CStr const &operator() (CSolution const &_Node) const;
		};

		CStr const &f_GetName() const;
		void f_FindRecursiveDependencies(CBuildSystem const &_BuildSystem);

		CSolution(CStr const &_Name)
			: m_Name(_Name)
		{
		}

		CStr m_Name;

		TCMap<CConfiguration, TCUniquePointer<CWorkspaceInfo>> m_WorkspaceInfos;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		TCMap<CStr, CSolutionFile> m_SolutionFiles;
		TCMap<CStr, CProject> m_Projects;
		TCMap<CStr, CGroup> m_Groups;
		CStr m_IntermediatePath;
		CStr m_OutputPath;

		mint m_nTotalTargets = 0;

		CFilePosition m_Position;

		CStr m_EntityName;
	};

	struct CGeneratorState
	{
		TCMap<CStr, TCUniquePointer<CSolution>> m_Solutions;
	};

	struct CGeneratorInstance : public ICGeneratorInterface
	{
		CGeneratorInstance
			(
				CBuildSystem const &_BuildSystem
				, CBuildSystemData const &_BuildSystemData
				, TCMap<CPropertyKey, CEJSONSorted> const &_InitialValues
				, CStr const &_OutputDir
			)
		;
		~CGeneratorInstance();

		CValuePotentiallyByRef f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const override;
		CStr f_GetExpandedPath(CStr const &_Path, CStr const &_Base) const override;
		CSystemEnvironment f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const override;

		TCFuture<void> f_GenerateProjectFile(CProject &_Project, CStr const &_OutputDir, TCMap<CConfiguration, TCSet<CStr>> &_Runnables, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable) const;
		TCFuture<void> f_GenerateWorkspaceFile(CSolution &_Solution, CStr const &_OutputDir) const;

		// Members
		CBuildSystem const &m_BuildSystem;
		CBuildSystemData const &m_BuildSystemData;

		CBuildSystemData m_GeneratorSettingsData;

		CGeneratorState m_State;
		CEJSONSorted m_OutputDir;
		CEJSONSorted m_RelativeBasePathAbsolute;
		CEJSONSorted m_RelativeBasePath;
		uint32 m_XcodeVersion;

		CEJSONSorted m_Builtin_ProjectPath = ".";
		CEJSONSorted m_Builtin_Inherit = "";

	private:
		struct CValueConfigs
		{
			TCSet<CConfiguration> m_Configurations;
			TCMap<CStr, TCLinkedList<CConfiguration>> m_ByPlatform;
			TCSet<CStr> m_OriginalValues;
		};

		struct CConfigResultTarget
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const
			{
				o_Str += typename tf_CStr::CFormat("Elements: {}\n") << m_Element;
			}

			TCValueWithPositions<CStr> m_TargetType;
			TCValueWithPositions<CStr> m_ProductType;
			TCValueWithPositions<CStr> m_Name;
			TCMap<CStr, CElement> m_Element;
		};

		struct CSingleValue
		{
			CSingleValue(CValuePotentiallyByRef &&_Value)
				: m_Value(fg_Move(_Value))
			{
			}

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const
			{
				o_Str += typename tf_CStr::CFormat("{}") << m_Value;
			}

			CBuildSystemUniquePositions m_Positions;
			CValuePotentiallyByRef m_Value;
		};

		struct CSharedState
		{
			void f_CreateDirectory(CStr const &_Path);

			TCSet<CStr> m_CreateDirectoryCache;
		};

		struct CWorkspaceState : public CSharedState
		{
		};

		struct CProjectState : public CSharedState
		{
			TCMap<CConfiguration, TCSet<CStr>> m_UsedCTypes;
			TCSet<CStr> m_EvaluatedTypesInUse;
			TCMap<CConfiguration, TCMap<CStr, CStr>> m_XcodeSettingsFromTypes;
			TCMap<CStr, TCMap<CConfiguration, CStr>> m_CompileFlagsValues;
			TCMap<CConfiguration, TCMap<CStr, CStr>> m_EvaluatedOverriddenCompileFlags;
			TCMap<CConfiguration, CConfigResultTarget> m_EvaluatedTargetSettings;
			TCMap<CStr, TCMap<CConfiguration, CConfigResultCompile>> m_EvaluatedTypeCompileFlags;
			TCMap<CStr, TCMap<CConfiguration, CConfigResultCompile>> m_EvaluatedCompileFlags;
			TCMap<CConfiguration, CStr> m_OtherCPPFlags; // Required for moc files
			TCMap<CConfiguration, CStr> m_OtherObjCPPFlags; // Required for moc files
			TCMap<CConfiguration, CStr> m_OtherCFlags; // Required for moc files
			TCMap<CConfiguration, CStr> m_OtherObjCFlags; // Required for moc files
			TCMap<CConfiguration, CStr> m_OtherAssemblerFlags; // Required for moc files
			TCMap<CConfiguration, TCMap<CStr, TCSet<CStr>>> m_BuildRules;
			CStr m_MocOutputPatternCPP;
			CStr m_ProjectOutputDir;
		};

		bool fp_GenerateBuildAllSchemes
			(
				CWorkspaceState &_WorkspaceState
				, CSolution &_Solution
				, CStr const &_OutputDir
				, TCMap<CConfiguration, TCSet<CStr>> &_Runnables
				, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable
			) const
		;

		void fp_CalculateDependencyProductPath(CProject &_Project, CProjectDependency &_Dependency, TCMap<CConfiguration, CEntityMutablePointer> const &_EnabledConfigurations) const;

		void fp_GenerateBuildConfigurationFilesList(CProjectState &_ProjectState, CProject& _Project, CStr const &_OutputDir, TCVector<CBuildConfiguration>& _ConfigList) const;
		void fp_GenerateBuildConfigurationFiles(CProjectState &_ProjectState, CProject& _Project, CStr const &_OutputDir) const;
		void fp_GenerateBuildConfigurationFile(CProjectState &_ProjectState,CProject& _Project, CConfiguration const &_Configuration, CStr const &_OutputFile, CStr const &_OutputDir, CNativeTarget const &_NativeTarget) const;
		void fp_GenerateCompilerFlags(CProjectState &_ProjectState, CProject& _Project) const;
		CStr fp_MakeNiceSharedFlagValue(CStr const &_Type) const;
		void fp_GeneratePBXSourcesBuildPhaseSection(CProject &_Project, CStr& _Output) const;
		void fp_GeneratePBXFrameworksBuildPhaseSection(CProject &_Project, CStr& _Output) const;

		void fp_GeneratePBXShellScriptBuildPhaseSection(CProject& _Project, CStr& _Output) const;

		void fp_GeneratePBXBuildFileSection(CProjectState &_ProjectState, CProject &_Project, CStr &o_Output) const;
		void fp_GeneratePBXBuildRule(CProjectState &_ProjectState, CProject &_Project, CStr& _Output) const;
		void fp_GeneratePBXFileReferenceSection(CProject &_Project, CStr const &_OutputDir, CStr& _Output) const;
		void fp_GeneratePBXGroupSection(CProject &_Project, CStr& _Output) const;
		void fp_GeneratePBXProjectSection(CProject &_Project, CStr& _Output) const;

		void fp_GeneratePBXAggregateTargetSection(CProject& _Project, CStr& _Output) const;
		void fp_GeneratePBXNativeTargetSection(CProjectState &_ProjectState, CProject &_Project, CStr& _Output) const;
		void fp_GenerateXCConfigurationList(CProject &_Project, CStr& _Output) const;
		void fp_GenerateXCBuildConfigurationSection(CProject &_Project, CStr& _Output) const;
		void fp_GeneratePBXContainerItemProxySection(CProject& _Project, CStr& _Output) const;
		void fp_GeneratePBXTargetDependencySection(CProject& _Project, CStr& _Output) const;

		static void fspr_MergeScheme(CXMLNode const* _pExistingNode, CXMLNode const* _pPrevNode, CXMLNode* _pNewNode);
		bool fp_GenerateSchemes(CProjectState &_ProjectState, CProject& _Project, TCMap<CConfiguration, TCSet<CStr>> &_Runnables, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable) const;

		// Values

		TCFuture<void> fp_EvaluateFiles(CProjectState &_ProjectState, CProject &_Project) const;
		TCFuture<void> fp_EvaluateFileTypeCompileFlags(CProjectState &_ProjectState, CProject& _Project) const;
		TCFuture<void> fp_EvaluateTargetSettings(CProjectState &_ProjectState, CProject& _Project) const;
		void fp_EvaluateDependencies(CProject& _Project) const;

		static CStr fs_EscapeCommandLineArgument(CStr const &_String);

		void fp_SetEvaluatedValuesTarget
			(
				CEntityMutablePointer const &_Entity
				, CConfigResultTarget &o_Result
				, CStr const &_Name
			) const
		;
		void fp_SetEvaluatedValuesCompile
			(
				CEntityMutablePointer const &_Entity
				, bool _bFile
				, CConfigResultCompile &o_Result
			) const
		;

		CSingleValue fp_GetSingleConfigValue
			(
				TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
				, CPropertyKeyReference const &_PropertyKey
				, EEJSONType _ExpectedType
				, bool _bOptional
			) const
		;
		CSingleValue fp_GetConfigValue
			(
				TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
				, CConfiguration const &_Configuration
				, CPropertyKeyReference const &_PropertyKey
				, EEJSONType _ExpectedType
				, bool _bOptional
			) const
		;
		TCMap<CConfiguration, CSingleValue> fp_GetConfigValues
			(
				TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
				, CPropertyKeyReference const &_PropertyKey
				, EEJSONType _ExpectedType
				, bool _bOptional
			) const
		;

		template <typename tf_CSet0, typename tf_CSet1>
		bool fp_IsSameConfig(tf_CSet0 const &_Configs, tf_CSet1 const &_AllConfigs) const;

		template <typename tf_CValue, typename tf_FGetValue>
		static void fsp_GetSingleValue
			(
				TCMap<CConfiguration, CConfigResultCompile> const &_CompileEntities
				, tf_CValue &o_Value
				, tf_FGetValue const &_fGetValue
			)
		;
	};
}

#include "Malterlib_BuildSystem_Generator_Xcode.hpp"
