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
		TCMap<CConfiguration, CEntityPointer> m_EnabledConfigs;
		CFilePosition m_Position;
		CStr m_VSType;
		CStr m_VSFile;
		zbool m_bWasGenerated;
	};

	struct CProjectDependency
	{
		CStr const &f_GetName() const;
		
		TCMap<CConfiguration, CEntityPointer> m_EnabledConfigs;
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
		TCMap<CConfiguration, CEntityPointer> m_EnabledProjectConfigs;
		TCMap<CConfiguration, CEntityPointer> m_EnabledConfigs;
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
		
		TCMap<CStr, CGroup> m_Groups;
		TCMap<CStr, CProject> m_Projects;
		TCMap<CStr, CSolutionFile> m_SolutionFiles;

		CFilePosition m_Position;

		CStr m_EntityName;

		TCMap<CConfiguration, CConfigDisplay> m_EnabledConfigs;

		DMibIntrusiveLink(CSolution, TCAVLLink<>, m_Link);
	};

	struct CGeneratorState
	{
		TCMap<CStr, CSolution> m_Solutions;
		TCAVLTree<CSolution::CLinkTraits_m_Link, CSolution::CCompare> m_SolutionsByEntity;
	};

	struct CGeneratorInstance : public CGeneratorInterface
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
			zbool m_bDisabled;
			zbool m_bConvertPath;
			zbool m_bEscapeSeparated;
			EPropertyValidity m_Validity;
			
			CStr m_Substitute;
		};

		struct CConfigValue
		{
			CStr m_Parent;
			CStr m_Entity;
			CStr m_Property;
			CStr m_Value;

			zbool m_bMainValue;

			inline bool operator < (CConfigValue const &_Right) const;
		};

		struct CPrefixHeader
		{
			TCSet<CConfiguration> m_Configurations;
			TCMap<CStr, TCSet<CXMLElement *>> m_Elements;
			zbool m_bUsed;
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
			
			CStr m_CurrentOutputDir;
			TCMap<TCMap<CStr, CStr>, TCMap<CStr, CPrefixHeader>> m_PrefixHeaders;
			ELanguageType m_LanguageType;
			TCMap<CStr, CStr> m_CurrentCompileTypes;

			TCMap<CStr, zbool> m_FileExistsCache;
			TCSet<CStr> m_CreateDirectoryCache;
		};

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

		CStr f_GetToolsVersion() const;

		void f_SetEvaluatedValues
			(
				TCMap<CStr, CXMLElement *> const &_Parents
				, TCMap<CConfiguration, CEntityPointer> const &_Configs
				, TCMap<CConfiguration, CEntityPointer> const &_AllConfigs
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
				TCMap<CConfiguration, CEntityPointer> const &_Configs
				, TCMap<CConfiguration, CEntityPointer> const &_AllConfigs
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
		void f_GenerateSolutionFile(CSolution &_Solution, CStr const &_OutputDir, mint _MaxSolutionNameLength) const;

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
		CEntityPointer m_pTargetSettings;
		
		mutable TCThreadLocal<CThreadLocal> m_ThreadLocal;
		
		CStr m_Win32Platfrom;				

		zbool m_bEnableSourceControl;

		uint32 m_Version; // 2012, 2013 etc
		
	private:
		template <typename tf_CSet0, typename tf_CSet1>
		bool fp_IsSameConfig(tf_CSet0 const &_Configs, tf_CSet1 const &_AllConfigs) const;
	};
}

#include "Malterlib_BuildSystem_Generator_VisualStudio.hpp"
