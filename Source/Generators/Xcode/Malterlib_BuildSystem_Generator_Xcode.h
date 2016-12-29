// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../../Malterlib_BuildSystem.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NXcode
{
	extern CUniversallyUniqueIdentifier g_GeneratorUUIDNamespace;

	extern CStr g_ReservedProductRefGroup;
	extern CStr g_ReservedConfigurationsGroup;
	extern CStr g_ReservedProjectDependenciesGroup;
	extern CStr g_ReservedGeneratorGroup;

	// These are ordered as Xcode requires them.
	enum EBuildFileTypes
	{
		ECustom,
		EMlTwk,
		EMalterlibFS,
		EQTRcc,
		EQTMoc,
		EQTUic,
		ECCompile_InitEarly,
		ECCompile,
		ECInclude,
		ENone,
	};

	struct CBuildConfiguration
	{
		CStr f_GetFile() const;
		CStr f_GetFileNoExt() const;
		CStr const& f_GetFileRefGUID();
		CStr const& f_GetGUID();

		CStr m_Name;
		CStr m_Path;
		bint m_bProject;
		
	private:
		CStr mp_GUID;
		CStr mp_FileRefGUID;
	};

	struct CBuildScript
	{
		CStr const& f_GetGUID();
		CStr const& f_GetScriptSetting();
		
		CStr m_Name;
		TCMap<CConfiguration, CStr> m_Script;
		TCMap<CConfiguration, CStr> m_ScriptNames;
		TCVector<CStr> m_Inputs;
		TCVector<CStr> m_Outputs;
		bool m_bPostBuild = false;
		bool m_bPreBuild = false;
		
	private:
		CStr mp_BuildSetting;
		CStr mp_GUID;
	};

	struct CNativeTarget
	{
		CStr const& f_GetGUID();
		CStr const& f_GetSourcesBuildPhaseGUID();
		CStr const& f_GetFrameworksBuildPhaseGUID();
		CStr const& f_GetProductReferenceGUID();
		CStr const& f_GetBuildConfigurationListGUID();
#if 0
		CStr const& f_GetHeadersBuildPhaseGUID();
#endif
		
		CStr m_Name;
		CStr m_ProductName;
		CStr m_ProductType;
		CStr m_ProductPath;
		CStr m_ProductSourceTree;
		int32 m_BuildActionMask;
		CStr m_Type;

		TCVector<CBuildConfiguration> m_BuildConfigurationList;
		TCMap<CStr, CBuildScript> m_BuildScripts;

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

	struct CProjectFile
	{
		TCPointer<CGroup> m_pGroup;
		TCMap<CConfiguration, CEntityPointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CStr m_Type;
		CStr m_LastKnownFileType;
		bool m_bWasGenerated = false;
		bool m_bHasCompilerFlags = false;
		uint8 m_TabWidth = 0; // 0 means unset
		uint8 m_IndentWidth = 0; // 0 means unset
		uint8 m_UsesTabs = 0; // 0 means unset, 1 = uses tabs, 2 = uses spaces

		CStr const &f_GetName() const;
		CStr f_GetGroupPath() const;
		CStr const& f_GetFileRefGUID();
		CStr const& f_GetBuildRefGUID();
		CStr const& f_GetLastKnownFileType();
		CStr const& f_GetCompileFlagsGUID();

	private:
		CStr mp_FileRefGUID;
		CStr mp_BuildRefGUID;
		CStr mp_CompileFlagsGUID;
	};

	struct CProjectDependency
	{
		struct CPerConfig
		{
			CStr m_CalculatedDependencyName;
			CStr m_CalculatedDependencyExtension;
			CStr m_CalculatedPath;
			CStr m_SearchPath;
			CStr m_LinkerGroup;
			bool m_bLink = false;
		};

		CStr const &f_GetName() const;
		CStr const& f_GetFileRefGUID();
		CStr const& f_GetBuildRefGUID();
		CStr const& f_GetContainerItemGUID();
		CStr const& f_GetContainerItemProductGUID();
		CStr const& f_GetTargetGUID();
		CStr const& f_GetReferenceProxyGUID();
		CStr const& f_GetProductRefGroupGUID();
		
		DLinkDS_Link(CProjectDependency, m_Link);
		TCMap<CConfiguration, CEntityPointer> m_EnabledConfigs;
		CFilePosition m_Position;
		
		CStr m_Type;
		TCMap<CConfiguration, CPerConfig> m_PerConfig;

	private:
		CStr mp_DependencyFileRefGUID;
		CStr mp_DependencyBuildRefGUID;
		CStr mp_DependencyContainerItemGUID;
		CStr mp_DependencyItemProductGUID;
		CStr mp_DependencyTargetGUID;
		CStr mp_DependencyReferenceProxyGUID;
		CStr mp_DependencyProductRefGroupGUID;
	};

	struct CSolution;

	struct CBuildFileRef
	{
		CStr m_FileName;
		CStr m_Name;
		CStr m_BuildGUID;
		CStr m_CompileFlagsGUID;
		CStr m_Type;
		CStr m_FileRefGUID;
		bint m_bHasCompilerFlags;
		TCVector<CStr> m_CustomOutputs;
		CStr m_CustomCommandLine;
		CStr m_CustomWorkingDirectory;
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
		CStr const& f_GetBuildConfigurationListGUID();
		void fr_FindRecursiveDependencies(CBuildSystem const &_BuildSystem, TCSet<CStr> &_Stack, CProjectDependency const *_pDepend, TCMap<CStr, CProject> const &_Projects) const;

		TCMap<CStr, CProjectFile> m_Files;
		TCMap<CStr, CGroup> m_Groups;
		TCMap<CStr, CProjectDependency> m_DependenciesMap;
		DLinkDS_List(CProjectDependency, m_Link) m_DependenciesOrdered;

		CNativeTarget m_NativeTarget;
		TCMap<EBuildFileTypes, TCVector<CBuildFileRef>> mp_OrderedBuildTypes;
		TCVector<CBuildConfiguration> m_BuildConfigurationList;

		CStr m_EntityName;
		bool m_GeneratedBuildScript = false;

		CSolution *m_pSolution;

		TCPointer<CGroup> m_pGroup;
		TCMap<CConfiguration, CEntityPointer> m_EnabledProjectConfigs;
		TCMap<CConfiguration, CEntityPointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CFilePosition m_ProjectPosition;
		CStr m_FileName;

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

		TCMap<CStr, CGroup> m_Groups;
		TCMap<CStr, CProject> m_Projects;
		TCMap<CStr, CSolutionFile> m_SolutionFiles;
		
		CFilePosition m_Position;

		CStr m_EntityName;

		TCMap<CConfiguration, CEntityPointer> m_EnabledConfigs;

		DMibIntrusiveLink(CSolution, TCAVLLink<>, m_Link);
	};

	struct CGeneratorState
	{
		TCMap<CStr, CSolution> m_Solutions;
		TCAVLTree<CSolution::CLinkTraits_m_Link, CSolution::CCompare> m_SolutionsByEntity;
	};

	struct CGeneratorInstance : public CGeneratorInterface
	{
		CGeneratorInstance
			(
				CBuildSystem const &_BuildSystem
				, CBuildSystemData const &_BuildSystemData
				, TCMap<CPropertyKey, CStr> const &_InitialValues
				, CStr const &_OutputDir
			)
		;
		~CGeneratorInstance();
		
		virtual bool f_GetBuiltin(CStr const &_Value, CStr &_Result) const override;
		virtual CStr f_GetExpandedPath(CStr const &_Path, CStr const& _Base) const override;
		void f_GenerateProjectFile(CProject &_Project, CStr const &_OutputDir, TCMap<CConfiguration, TCSet<CStr>> &_Runnables, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable) const;
		void f_GenerateWorkspaceFile(CSolution &_Solution, CStr const &_OutputDir, mint _MaxWorkspaceNameLen) const;
		
		// Members
		CBuildSystem const &m_BuildSystem;
		CBuildSystemData const &m_BuildSystemData;

		CBuildSystemData m_GeneratorSettingsData;
		
		CGeneratorState m_State;
		CStr m_OutputDir;
		CStr m_RelativeBasePathAbsolute;
		CStr m_RelativeBasePath;
		uint32 m_XcodeVersion;
		
		CEntityPointer m_pGeneratorSettings;
		CEntityPointer m_pTargetSettings;

	private:
		struct CValueProperties
		{
			TCPointer<CEvaluatedProperty const> m_pTranslatedPropertyName;
			CEntityPointer m_pTranslators;
			CEntityPointer m_pValueSet;
			CEntityPointer m_pProperties;
			CStr m_Substitute;
			bool m_bDisabled = false;
			bool m_bConvertSeperator = false;
			bool m_bQuoteSeperatedValues = false;
			bool m_bQuoteAfterEquals = false;
			bool m_bRemoveLastSlash = false;
			bool m_bIgnoreEmtpy = false;
			CStr m_Seperator;
			CStr m_OldSeperator;
			CStr m_Prefix;
		};

		struct CConfigValue
		{
			CStr m_Parent;
			CStr m_Entity;
			CStr m_Property;
			CStr m_Value;
			TCVector<CStr> m_Values;
			
			bool m_bXcodeProperty = false;
			bool m_bMainValue = false;
			bool m_bUseValues = false;

			bool operator < (CConfigValue const &_Right) const;
		};

		struct CElement
		{
			inline CStr const &f_GetValue() const;
			inline void f_SetValue(CStr const &_Value);
			inline bool f_IsSameValue(CElement const &_Other) const;
			
			CStr m_Property;
			TCSet<CStr> m_ValueSet;
			CStr m_Value;
			bool m_bXcodeProperty = false;
			bool m_bUseValues = false;
			CFilePosition m_Position;
		};

		struct CValueConfigs
		{
			TCSet<CConfiguration> m_Configurations;
			TCMap<CStr, TCLinkedList<CConfiguration>> m_ByPlatform;
			TCSet<CStr> m_OriginalValues;
		};

		struct CConfigResult
		{
			TCMap<CStr, CElement> m_Element;
		};

		struct CSingleValue
		{
			CFilePosition m_Position;
			CStr m_Value;
		};
		
		struct CThreadLocal
		{
			CThreadLocal();

			void f_CreateDirectory(CStr const &_Path);

			CXMLDocument *m_pXMLFile;
			TCMap<CStr> mp_EvaluatedTypesInUse;
			TCMap<CConfiguration, TCMap<CStr, TCVector<CStr>>> mp_XcodeSettingsFromFiles;
			TCMap<CConfiguration, TCSet<CStr>> mp_XcodeSettingsFromFilesExcluded;
			TCMap<CConfiguration, TCMap<CStr, CStr>> mp_XcodeSettingsFromTypes;
			TCMap<CStr, TCMap<CConfiguration, CStr>> mp_CompileFlagsValues;
			TCMap<CConfiguration, TCMap<CStr, CStr>> mp_EvaluatedOverriddenCompileFlags;
			TCMap<CConfiguration, CConfigResult> mp_EvaluatedTargetSettings;
			TCMap<CStr, TCMap<CConfiguration, CConfigResult>> mp_EvaluatedTypeCompileFlags;
			TCMap<CStr, TCMap<CConfiguration, CConfigResult>> mp_EvaluatedCompileFlags;
			TCMap<CConfiguration, CStr> mp_OtherCPPFlags; // Required for moc files
			TCMap<CConfiguration, CStr> mp_OtherObjCPPFlags; // Required for moc files
			TCMap<CConfiguration, CStr> mp_OtherCFlags; // Required for moc files
			TCMap<CConfiguration, CStr> mp_OtherObjCFlags; // Required for moc files
			TCMap<CConfiguration, CStr> mp_OtherAssemblerFlags; // Required for moc files
			TCSet<CStr> mp_BuildRules;
			CStr mp_MocOutputPatternCPP;
			CStr m_ProjectOutputDir;
			
			TCSet<CStr> m_CreateDirectoryCache;
		};
		
		bool fp_GenerateBuildAllSchemes(CSolution &_Solution, CStr const &_OutputDir, TCMap<CConfiguration, TCSet<CStr>> &_Runnables, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable) const;

		void fp_CalculateDependencyProductPath(CProject& _Project, CProjectDependency& _Dependency) const;

		void fp_GenerateBuildConfigurationFiles(CProject& _Project, CStr const& _OutputDir, TCVector<CBuildConfiguration>& _ConfigList, bint _bProject) const;
		void fp_GenerateBuildConfigurationFile(CProject& _Project, CConfiguration const& _Configuration, CStr const& _OutputFile, CStr const& _OutputDir) const;
		void fp_GenerateBuildConfigurationScriptFile(CProject& _Project, CConfiguration const& _Configuration, CStr const& _OutputFile, CStr const& _OutputDir, CStr const &_Contents) const;
		void fp_GenerateCompilerFlags(CProject& _Project) const;
		CStr fp_MakeNiceSharedFlagValue(CStr const& _Type) const;
#if 0
		void fp_GeneratePBXHeadersBuildPhaseSection(CProject &_Project, CStr& _Output) const;
#endif
		void fp_GeneratePBXSourcesBuildPhaseSection(CProject &_Project, CStr& _Output) const;
		void fp_GeneratePBXFrameworksBuildPhaseSection(CProject &_Project, CStr& _Output) const;

		void fp_GeneratePBXShellScriptBuildPhaseSection(CProject& _Project, CStr& _Output) const;

		void fp_GeneratePBXBuildFileSection(CProject &_Project, CStr &o_Output) const;
		void fp_GeneratePBXBuildRule(CProject &_Project, CStr& _Output) const;
		void fp_GeneratePBXFileReferenceSection(CProject &_Project, CStr const& _OutputDir, CStr& _Output) const;
		void fp_GeneratePBXGroupSection(CProject &_Project, CStr& _Output) const;
		void fp_GeneratePBXProjectSection(CProject &_Project, CStr& _Output) const;

		void fp_GeneratePBXReferenceProxySection(CProject &_Project, CStr& _Output) const;

		void fp_GeneratePBXLegacyTargetSection(CProject& _Project, CStr& _Output) const;
		void fp_GeneratePBXNativeTargetSection(CProject &_Project, CStr& _Output) const;
		void fp_GenerateXCConfigurationList(CProject &_Project, CStr& _Output) const;
		void fp_GenerateXCBuildConfigurationSection(CProject &_Project, CStr& _Output) const;
		void fp_GeneratePBXContainerItemProxySection(CProject& _Project, CStr& _Output) const;
		void fp_GeneratePBXTargetDependencySection(CProject& _Project, CStr& _Output) const;

		void fp_GenerateToolRunScript(CProject& _Project, CStr const& _OutputDir) const;

		static void fspr_MergeScheme(CXMLNode const* _pExistingNode, CXMLNode const* _pPrevNode, CXMLNode* _pNewNode);
		bool fp_GenerateSchemes(CProject& _Project, TCMap<CConfiguration, TCSet<CStr>> &_Runnables, TCMap<CConfiguration, TCMap<CStr, CStr>> &_Buildable) const;
		
		void fp_AddExcludedFile(CConfiguration const &_Config, CStr const &_File) const;
		void fp_ProcessExcludedFiles(CStr const &_OutputPath) const;

		// Values

		void fp_EvaluateFiles(CProject& _Project) const;
		void fp_EvaluateFileTypeCompileFlags(CProject& _Project) const;
		void fp_EvaluateTargetSettings(CProject& _Project) const;
		void fp_EvaluateDependencies(CProject& _Project) const;

		void fp_SetEvaluatedValues
			(
				TCMap<CConfiguration, CEntityPointer> const &_Configs
				, TCMap<CConfiguration, CEntityPointer> const &_AllConfigs
				, bool _bFile
				, EPropertyType _PropertyType
				, TCVector<CStr> const *_pSearchList
				, TCMap<CConfiguration, TCVector<CStr>> const *_pSearchListPerConfig
				, CStr const &_DefaultEntity
				, bool _bPropertyCondition
				, bool _bAddPropertyDefined
				, TCMap<CConfiguration, CConfigResult> &_Result
			) const
		;

		void fp_GetConfigValue
			(
				TCMap<CConfiguration, CEntityPointer> const &_Configs
				, TCMap<CConfiguration, CEntityPointer> const &_AllConfigs
				, CFilePosition const &_Position
				, EPropertyType _PropType
				, CStr const &_SourceType
				, bint _bFile
				, bool _bExcludeFromBuildCondition
				, TCVector<CStr> const *_pSearchList
				, TCMap<CConfiguration, TCVector<CStr>> const *_pSearchListPerConfig
				, CStr const &_DefaultEntity
				, CStr const &_ExtraCondition
				, TCMap<CConfiguration, CConfigResult> &_Result
			) const
		;

		CSingleValue fp_GetSingleConfigValue(TCMap<CConfiguration, CEntityPointer> const &_Configs, EPropertyType _PropType, CStr const &_Property) const;
		
		template <typename tf_CSet0, typename tf_CSet1>
		bool fp_IsSameConfig(tf_CSet0 const &_Configs, tf_CSet1 const &_AllConfigs) const;

		mutable TCThreadLocal<CThreadLocal> m_ThreadLocal;
	};
}

#include "Malterlib_BuildSystem_Generator_Xcode.hpp"
