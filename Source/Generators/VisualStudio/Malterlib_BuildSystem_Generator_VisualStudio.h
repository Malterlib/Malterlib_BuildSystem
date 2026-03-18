// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../../Malterlib_BuildSystem.h"
#include "../Shared/Malterlib_BuildSystem_Generator_VisualStudioRootHelper.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NVisualStudio
{
	extern CUniversallyUniqueIdentifier const gc_GeneratorGroupUUIDNamespace;
	extern CUniversallyUniqueIdentifier const gc_GeneratorProjectUUIDNamespace;
	extern CUniversallyUniqueIdentifier const gc_GeneratorSolutionUUIDNamespace;
	extern CUniversallyUniqueIdentifier const gc_GeneratorPrefixHeaderUUIDNamespace;

	enum ELanguageType
	{
		ELanguageType_Native
		, ELanguageType_CSharp
	};

	struct CProject;

	struct CProjectXMLState
	{
		CXMLDocument m_XMLFile;
		CXMLDocument m_PropsXMLFile;

		CXMLElement *m_pProject = nullptr;

		CXMLElement *m_pPreProject = nullptr;
		CXMLElement *m_pGlobals = nullptr;
		CXMLElement *m_pConfiguration = nullptr;

		CXMLElement *m_pPropsProject = nullptr;

		CXMLElement *m_pPropsPropertyGroup = nullptr;
		CXMLElement *m_pPropsItemDefinitionGroup = nullptr;

		CXMLElement *m_pFileItemGroup = nullptr;

		CXMLElement *m_pDependenciesItemGroup = nullptr;

		TCMap<CStr, CXMLElement *> m_PropertyGroupLabels;
		TCMap<CStr, CXMLElement *> m_ItemDefinitionGroups;

		bool m_bIsDotNet = false;
	};

	struct CItemState;
	struct CGeneratorSettingsVSType;

	struct CGeneratorSettings : public NBuildSystem::CGeneratorSettings
	{
		struct CVS_SettingShared
		{
			auto operator <=> (CVS_SettingShared const &_Right) const noexcept = default;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_String) const;
			static void fs_FromJson(CVS_SettingShared &o_Value, NEncoding::CEJsonSorted &&_Json);

			CStr m_Key;
			CStr m_Value;
		};

		struct CVS_Setting_PropertyGroup : public CVS_SettingShared
		{
			auto operator <=> (CVS_Setting_PropertyGroup const &_Right) const noexcept = default;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_String) const;
			static CVS_Setting_PropertyGroup fs_FromJson(NEncoding::CEJsonSorted &&_Json);

			// Type: one_of("PropertyGroup")
			CStr m_Label;
		};

		struct CVS_Setting_ItemDefinitionGroup : public CVS_SettingShared
		{
			auto operator <=> (CVS_Setting_ItemDefinitionGroup const &_Right) const noexcept = default;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_String) const;
			static CVS_Setting_ItemDefinitionGroup fs_FromJson(NEncoding::CEJsonSorted &&_Json);

			// Type: one_of("ItemDefinitionGroup")
			CStr m_Name;
		};

		struct CVS_Setting_Item : public CVS_SettingShared
		{
			auto operator <=> (CVS_Setting_Item const &_Right) const noexcept = default;

			template <typename tf_CStr>
			void f_Format(tf_CStr &o_String) const;
		};

		using CVS_Setting = TCVariant<CVS_Setting_Item, CVS_Setting_PropertyGroup, CVS_Setting_ItemDefinitionGroup>;

		struct CVSSettingAggregateProperties
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_String) const;

			TCSet<CConfiguration> m_Configurations;
			CBuildSystemUniquePositions m_Positions;
		};

		struct CVSSettingAggregate
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_String) const;

			TCMap<TCVector<CVS_Setting>, CVSSettingAggregateProperties> m_Settings;
		};

		struct CVSSettingAggregated
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_String) const;

			TCMap<CStr, CVSSettingAggregate> m_AggregatedSettings;
		};

		struct CParsedSetting
		{
			CBuildSystemUniquePositions m_Positions;
			TCVector<CVS_Setting> m_VSSettings;
		};

		struct CParsedGeneratorSetting
		{
			TCMap<CStr, CParsedSetting> m_Properties;
			TCMap<CStr, CParsedSetting> m_NonSharedProperties;
		};

		struct CParsedGeneratorSettings
		{
			bool f_SettingsConstructed()
			{
#if DMibEnableSafeCheck > 0
				return !!m_Settings;
#else
				return true;
#endif
			}

			TCMap<CConfiguration, CParsedGeneratorSetting> &f_ConstructSettings()
			{
#if DMibEnableSafeCheck > 0
				DMibFastCheck(!m_Settings);
				m_Settings = fg_Construct();
				return *m_Settings;
#else
				return m_Settings;
#endif
			}

			void f_DestructSettings()
			{
				m_Settings.f_Clear();
			}

			TCMap<CConfiguration, CParsedGeneratorSetting> &f_Settings()
			{
#if DMibEnableSafeCheck > 0
				return *m_Settings;
#else
				return m_Settings;
#endif
			}

			TCMap<CConfiguration, CParsedGeneratorSetting> const &f_Settings() const
			{
#if DMibEnableSafeCheck > 0
				return *m_Settings;
#else
				return m_Settings;
#endif
			}

			CStr m_VSType;
			CStr m_Type;
#if DMibEnableSafeCheck > 0
			TCOptional<TCMap<CConfiguration, CParsedGeneratorSetting>> m_Settings;
#else
			TCMap<CConfiguration, CParsedGeneratorSetting> m_Settings;
#endif
		};

		template <typename tf_CAllConfigs>
		static CStr fs_GetConditionString(TCSet<CConfiguration> const &_Configurations, tf_CAllConfigs const &_AllConfigurations, CProject const &_Project);

		template <bool tf_bCompile, bool tf_bIsItem>
		static void fs_AddToXMLFiles(CProjectXMLState &_XMLState, CProject const &_Project, CParsedGeneratorSettings &&_Parsed, CItemState const *_pItemState);

		template <bool tf_bType, bool tf_bIsItem>
		CParsedGeneratorSettings f_GetParsedVSSettings(TCMap<CStr, CGeneratorSettingsVSType> *_pCompileSettings);

		static TCVector<CVS_Setting> ms_ExcludedFromBuildVSSettingsTrue;
		static TCVector<CVS_Setting> ms_ExcludedFromBuildVSSettingsFalse;
	};

	struct CGeneratorSettingsVSType
	{
		CGeneratorSettings::CParsedGeneratorSettings m_SharedSettings;
		TCMap<CStr, CGeneratorSettings::CParsedGeneratorSettings> m_SpecificSettings;
	};

	struct CItemState
	{
		TCMap<CStr, CGeneratorSettingsVSType> const *m_pCompile;
		CXMLElement *m_pItemElement;
	};

	struct CLocalConfiguraitonData : public CConfiguraitonData
	{
		CStr m_FullConfiguration;
		CStr m_SolutionPlatform;
		CStr m_SolutionConfiguration;
	};

	struct CGroup
	{
		CStr const &f_GetPath() const;
		CStr f_GetGroupPath() const;
		CStr const &f_GetGUID();

		CStr m_Name;
		TCPointer<CGroup> m_pParent;

	private:
		CStr mp_GUID;

	};

	struct CPrefixHeader
	{
		TCSet<CConfiguration> m_Configurations;
		CStr m_PCHFile;
		CBuildSystemUniquePositions m_Positions;
		bool m_bUsed = false;
	};

	struct CProjectFile
	{
		CStr const &f_GetName() const;
		CStr f_GetGroupPath() const;

		TCPointer<CGroup> m_pGroup;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CStr m_VSType;
		CStr m_VSFile;
		CGeneratorSettings m_GeneratorSettings;
		CGeneratorSettings::CParsedGeneratorSettings m_ParsedSettings;
		TCMap<CConfiguration, CPrefixHeader * const> m_PrefixHeaders;
		bool m_bWasGenerated = false;
	};

	struct CProjectDependencyDebug
	{
		bool m_bIndirect;
		bool m_bIndirectOrdered;
	};

	struct CProjectDependency
	{
		CStr const &f_GetName() const;

		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		TCMap<CConfiguration, CProjectDependencyDebug> m_PerConfigDebug;
		CFilePosition m_Position;
		CGeneratorSettings m_GeneratorSettings;
		bool m_bExternal = false;
	};

	struct CSolution;

	struct CProject
	{
		CProject(CSolution *_pSolution);

		CStr const &f_GetName() const;
		CStr f_GetPath() const;
		CStr const &f_GetGUID();
		void fr_FindRecursiveDependencies(CBuildSystem const &_BuildSystem, TCSet<CStr> &_Stack, CProjectDependency const *_pDepend, TCMap<CStr, CProject> &_Projects);
		CStr const &f_GetSolutionTypeGUID() const;

		TCMap<CStr, CProjectFile> m_Files;
		TCMap<CStr, CGroup> m_Groups;
		TCMap<CStr, CProjectDependency> m_Dependencies;

		CStr m_EntityName;

		CSolution *m_pSolution;

		TCPointer<CGroup> m_pGroup;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledProjectConfigs;
		//TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CFilePosition m_ProjectPosition;
		CStr m_FileName;
		ELanguageType m_LanguageType = ELanguageType_Native;
		TCMap<CConfiguration, CStr> m_Platforms;

		bool m_bCheckedDependencies = false;

	private:
		CStr mp_GUID;
	};

	struct CSolutionFile
	{
		CStr const &f_GetName() const;
		CStr f_GetGroupPath() const;

		TCPointer<CGroup> m_pGroup;
		CFilePosition m_Position;
	};

	struct CSolution
	{
		struct CConfigDisplay
		{
			CStr m_Platform;
			CStr m_Config;

			CEntityPointer m_pEntity;
		};

		class CCompare
		{
		public:
			inline_small CStr const &operator() (CSolution const &_Node) const;
		};

		CStr const &f_GetName() const;
		void f_FindRecursiveDependencies(CBuildSystem const &_BuildSystem);

		CSolution(CStr const &_Name)
			: m_Name(_Name)
		{
		}

		CStr m_Name;

		TCMap<CConfiguration, TCUniquePointer<CWorkspaceInfo>> m_WorkspaceInfos;

		TCMap<CStr, CGroup> m_Groups;
		TCMap<CStr, CProject> m_Projects;
		TCMap<CStr, CSolutionFile> m_SolutionFiles;

		CFilePosition m_Position;

		CStr m_EntityName;

		TCMap<CConfiguration, CConfigDisplay> m_EnabledConfigs;

		mint m_nTotalTargets = 0;
	};

	struct CGeneratorState
	{
		TCMap<CStr, TCUniquePointer<CSolution>> m_Solutions;
	};

	struct CGeneratorInstance : public ICGeneratorInterface
	{
		enum EPropertyValidity
		{
			EPropertyValidity_Any
			, EPropertyValidity_File
			, EPropertyValidity_NotFile
		};

		struct CValueProperties
		{
			inline CValueProperties();

			TCPointer<CEvaluatedProperty const> m_pVSParentName;
			TCPointer<CEvaluatedProperty const> m_pVSEntityName;
			TCPointer<CEvaluatedProperty const> m_pVSPropertyName;
			CEntityPointer m_pTranslators;
			CEntityPointer m_pValueSet;
			CEntityPointer m_pProperties;
			bool m_bDisabled = false;
			bool m_bConvertPath = false;
			bool m_bEscapeSeparated = false;
			bool m_bShortenFilenames = false;
			EPropertyValidity m_Validity;

			CStr m_Substitute;
			CStr m_Separator;
		};

		struct CConfigValue
		{
			CStr m_Parent;
			CStr m_Entity;
			CStr m_Property;
			CStr m_Value;

			bool m_bMainValue = false;

			inline COrdering_Strong operator <=> (CConfigValue const &_Right) const noexcept;
		};

		struct CConfigResult
		{
			CXMLElement *m_pElement;
			TCSet<CStr> m_UntranslatedValues;
		};

		struct CProjectState
		{
			bool f_FileExists(CStr const &_Path);
			void f_CreateDirectory(CStr const &_Path);

			CStr m_CurrentOutputDir;
			TCMap<CStr, TCMap<CStr, CPrefixHeader>> m_PrefixHeaders;
			ELanguageType m_LanguageType = ELanguageType_Native;

			TCMap<CStr, zbool> m_FileExistsCache;
			TCSet<CStr> m_CreateDirectoryCache;
		};

		struct CCompileType
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const
			{
				o_Str += typename tf_CStr::CFormat("{}={vs}") << m_VSType << m_EnabledConfigs;
			}

			CStr m_VSType;
			TCSet<CConfiguration> m_EnabledConfigs;
		};

		CGeneratorInstance
			(
				CBuildSystem const &_BuildSystem
				, CBuildSystemData const &_BuildSystemData
				, TCMap<CPropertyKey, CEJsonSorted> const &_InitialValues
				, CStr const &_OutputDir
			)
		;
		~CGeneratorInstance();

		virtual CValuePotentiallyByRef f_GetBuiltin(CBuildSystemUniquePositions *_pStorePositions, CStr const &_Value, bool &o_bSuccess) const override;
		virtual CStr f_GetExpandedPath(CStr const &_Path, CStr const &_Base) const override;
		virtual CSystemEnvironment f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const override;

		CStr const &f_GetToolsVersion() const;
		CEJsonSorted const &f_GetVisualStudioRoot() const;

		TCUnsafeFuture<void> f_GenerateProjectFile(CProject &_Project, CStr const &_OutputDir) const;
		TCUnsafeFuture<void> f_GenerateSolutionFile(CSolution &_Solution, CStr const &_OutputDir) const;

		TCUnsafeFuture<TCMap<CStr, CCompileType>> f_GenerateProjectFile_File(CProject &_Project, CProjectState &_ProjectState) const;
		void f_GenerateProjectFile_AddPrefixHeaders(CProject &_Project, CProjectState &_ProjectState) const;
		TCUnsafeFuture<void> f_GenerateProjectFile_Dependency(CProject &_Project, CProjectState &_ProjectState) const;
		TCUnsafeFuture<TCMap<CStr, CGeneratorSettingsVSType>> f_GenerateProjectFile_FileTypes
			(
				CProject &_Project
				, CProjectState &_ProjectState
				, TCMap<CStr, CCompileType> const &_CompileTypes
			) const
		;
		void f_GenerateProjectFile_AddToXML_FileTypes(CProject &_Project, CProjectState &_ProjectState, CProjectXMLState &_XMLState, TCMap<CStr, CGeneratorSettingsVSType> &&_Compile) const;
		void f_GenerateProjectFile_AddToXML_File(CProject &_Project, CProjectState &_ProjectState, CProjectXMLState &_XMLState, TCMap<CStr, CGeneratorSettingsVSType> &_Compile) const;
		void f_GenerateProjectFile_AddToXML_Dependency(CProject &_Project, CProjectState &_ProjectState, CProjectXMLState &_XMLState) const;

		CStr f_GetNativePlatform(CProjectState &_ProjectState, CStr const &_Platform);
		CStr f_GetNativePlatform(ELanguageType _Language, CStr const &_Platform);

		CBuildSystem const &m_BuildSystem;
		CBuildSystemData const &m_BuildSystemData;

		CGeneratorState m_State;
		CEJsonSorted m_OutputDir;
		CEJsonSorted m_RelativeBasePath;
		CEJsonSorted m_RelativeBasePathAbsolute;

		CEJsonSorted m_Builtin_ProjectPath = ".";
		CEJsonSorted m_Builtin_Inherit = gc_ConstString_Symbol_Inherit;

		mutable CVisualStudioRootHelper m_VisualStudioRootHelper;

		CStr m_Win32Platfrom;

		bool m_bEnableSourceControl = false;

		uint32 m_Version; // 2012, 2013 etc

	private:
		template <typename tf_CSet0, typename tf_CSet1>
		bool fp_IsSameConfig(tf_CSet0 const &_Configs, tf_CSet1 const &_AllConfigs) const;
	};
}

#include "Malterlib_BuildSystem_Generator_VisualStudio.hpp"
