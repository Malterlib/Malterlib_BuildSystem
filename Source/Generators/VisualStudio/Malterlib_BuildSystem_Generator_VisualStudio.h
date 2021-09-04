// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../../Malterlib_BuildSystem.h"
#include <Mib/XML/XML>

namespace NMib::NBuildSystem::NVisualStudio
{
	extern CUniversallyUniqueIdentifier g_GeneratorGroupUUIDNamespace;
	extern CUniversallyUniqueIdentifier g_GeneratorProjectUUIDNamespace;
	extern CUniversallyUniqueIdentifier g_GeneratorSolutionUUIDNamespace;
	extern CUniversallyUniqueIdentifier g_GeneratorPrefixHeaderUUIDNamespace;

	enum ELanguageType
	{
		ELanguageType_Native
		, ELanguageType_CSharp
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

	struct CProjectFile
	{
		CStr const &f_GetName() const;
		CStr f_GetGroupPath() const;

		TCPointer<CGroup> m_pGroup;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CStr m_VSType;
		CStr m_VSFile;
		bool m_bWasGenerated = false;
	};

	struct CProjectDependency
	{
		CStr const &f_GetName() const;

		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CStr m_VSFile;
	};

	struct CSolution;

	struct CProject
	{
		CProject(CSolution *_pSolution);

		CStr const &f_GetName() const;
		CStr f_GetPath() const;
		CStr const &f_GetGUID();
		void fr_FindRecursiveDependencies(CBuildSystem const &_BuildSystem, TCSet<CStr> &_Stack, CProjectDependency const *_pDepend, TCMap<CStr, CProject> const &_Projects) const;
		CStr f_GetSolutionTypeGUID() const;

		TCMap<CStr, CProjectFile> m_Files;
		TCMap<CStr, CGroup> m_Groups;
		TCMap<CStr, CProjectDependency> m_Dependencies;

		CStr m_EntityName;

		CSolution *m_pSolution;

		TCPointer<CGroup> m_pGroup;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledProjectConfigs;
		TCMap<CConfiguration, CEntityMutablePointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CFilePosition m_ProjectPosition;
		CStr m_FileName;
		ELanguageType m_LanguageType;
		TCMap<CConfiguration, CStr> m_Platforms;

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

		TCMap<CStr, CGroup> m_Groups;
		TCMap<CStr, CProject> m_Projects;
		TCMap<CStr, CSolutionFile> m_SolutionFiles;

		CFilePosition m_Position;

		CStr m_EntityName;

		TCMap<CConfiguration, CConfigDisplay> m_EnabledConfigs;
		TCMap<CConfiguration, TCUniquePointer<CWorkspaceInfo>> m_WorkspaceInfos;
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

			inline COrdering_Weak operator <=> (CConfigValue const &_Right) const;
		};

		struct CPrefixHeader
		{
			TCSet<CConfiguration> m_Configurations;
			TCMap<CStr, TCSet<CXMLElement *>> m_Elements;
			bool m_bUsed = false;
			CFilePosition m_Position;
		};

		struct CValueConfigs
		{
			TCMap<CConfiguration, TCPointer<CPrefixHeader>> m_Configurations;
			TCMap<CStr, TCLinkedList<CConfiguration>> m_ByPlatform;
			TCSet<CStr> m_OriginalValues;
		};

		struct CConfigResult
		{
			CXMLElement *m_pElement;
			TCSet<CStr> m_UntranslatedValues;
		};

		struct CThreadLocal
		{
			bool f_FileExists(CStr const &_Path);
			void f_CreateDirectory(CStr const &_Path);
			void f_Clear();

			CStr m_CurrentOutputDir;
			TCMap<TCMap<CStr, CStr>, TCMap<CStr, CPrefixHeader>> m_PrefixHeaders;
			ELanguageType m_LanguageType = ELanguageType_Native;
			TCMap<CStr, CStr> m_CurrentCompileTypes;

			TCMap<CStr, zbool> m_FileExistsCache;
			TCSet<CStr> m_CreateDirectoryCache;
		};

		CGeneratorInstance
			(
				CBuildSystem const &_BuildSystem
				, CBuildSystemData const &_BuildSystemData
				, TCMap<CPropertyKey, CEJSON> const &_InitialValues
				, CStr const &_OutputDir
			)
		;
		~CGeneratorInstance();

		void f_ClearThreadLocal() const;

		virtual bool f_GetBuiltin(CStr const &_Value, CStr &_Result) const override;
		virtual CStr f_GetExpandedPath(CStr const &_Path, CStr const &_Base) const override;
		virtual CSystemEnvironment f_GetBuildEnvironment(CStr const &_Platform, CStr const &_Architecture) const override;

		CStr f_GetToolsVersion() const;
		CStr f_GetVisualStudioRoot() const;

		void f_SetEvaluatedValues
			(
				TCMap<CStr, CXMLElement *> const &_Parents
				, TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
				, TCMap<CConfiguration, CEntityMutablePointer> const &_AllConfigs
				, bool _bFile
				, EPropertyType _PropertyType
				, TCVector<CStr> const *_pSearchList
				, TCMap<CConfiguration, TCVector<CStr>> const *_pSearchListPerConfig
				, CStr const &_DefaultEntity
				, bool _bPropertyCondition
				, bool _bAddPropertyDefined
				, bool _bDontAllowRedefinition
				, CProject &_Project
			) const
		;

		TCMap<CConfiguration, CConfigResult> f_AddConfigValue
			(
				TCMap<CConfiguration, CEntityMutablePointer> const &_Configs
				, TCMap<CConfiguration, CEntityMutablePointer> const &_AllConfigs
				, CFilePosition const &_Position
				, EPropertyType _PropType
				, CStr const &_SourceType
				, TCMap<CStr, CXMLElement *> const &_Parents
				, CStr const &_AddAsAttribute
				, bool _bExcludeFromBuildCondition
				, TCVector<CStr> const *_pSearchList
				, TCMap<CConfiguration, TCVector<CStr>> const *_pSearchListPerConfig
				, CStr const &_DefaultEntity
				, bool _bPropertyCondition
				, bool _bAddPropertyDefined
				, bool _bDontAllowRedefinition
				, bool _bFile
				, CProject &_Project
			) const
		;
		void f_GenerateProjectFile(CProject &_Project, CStr const &_OutputDir) const;
		void f_GenerateSolutionFile(CSolution &_Solution, CStr const &_OutputDir) const;

		CStr f_GetNativePlatform(CStr const &_Platform);
		CStr f_GetNativePlatform(ELanguageType _Language, CStr const &_Platform);

		CBuildSystem const &m_BuildSystem;
		CBuildSystemData const &m_BuildSystemData;
		CBuildSystemData m_GeneratorSettingsData;

		CGeneratorState m_State;
		CStr m_OutputDir;
		CStr m_RelativeBasePath;
		CStr m_RelativeBasePathAbsolute;

		CEntityPointer m_pGeneratorSettings;

		mutable TCThreadLocal<CThreadLocal> m_ThreadLocal;

		CStr m_Win32Platfrom;

		bool m_bEnableSourceControl = false;

		uint32 m_Version; // 2012, 2013 etc

		mutable CMutual m_GetEnvironmentLock;
		mutable TCMap<CStr, CMutual> m_GetEnvironmentLocks;
		mutable TCMap<CStr, TCMap<CStr, CStr>> m_CachedBuildEnvironment;

	private:
		template <typename tf_CSet0, typename tf_CSet1>
		bool fp_IsSameConfig(tf_CSet0 const &_Configs, tf_CSet1 const &_AllConfigs) const;
	};
}

#include "Malterlib_BuildSystem_Generator_VisualStudio.hpp"
