// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#ifdef __cplusplus

#include "Malterlib_BuildSystem_Find.h"
#include "Malterlib_BuildSystem_Data.h"
#include <Mib/Concurrency/ThreadSafeQueue>
#include <Mib/Concurrency/AsyncResult>
#include <Mib/Concurrency/ParallellForEach>

namespace NMib::NBuildSystem
{
	class CBuildSystem;
	struct CWorkspaceInfo;
	struct CTargetInfo;
}

#include "Malterlib_BuildSystem_Generator.h"
#include "Malterlib_BuildSystem_Group.h"
#include "Malterlib_BuildSystem_Target.h"
#include "Malterlib_BuildSystem_Workspace.h"
#include "Malterlib_BuildSystem_ConfigurationData.h"
#include "Malterlib_BuildSystem_Error.h"
#include "Malterlib_BuildSystem_GenerateSettings.h"

namespace NMib::NBuildSystem
{
	class CBuildSystem
	{
	public:
		CBuildSystem();
		
		void f_SetGeneratorInterface(CGeneratorInterface *_pInterface) const; 
		bool f_Generate(CGenerateSettings const &_GenerateSettings, bool &o_bRetry);
		void f_GenerateBuildSystem
			(
				TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> &o_Configurations
				, TCMap<CPropertyKey, CStr> const &_Values
				, TCSet<CStr> const &_ReservedGroups
				, CStr const &_DependencyFilesName
			) const
		;
		inline_always CGenerateSettings const &f_GetGenerateSettings() const;
		CStr f_GetBaseDir() const;
		bool f_AddGeneratedFile(CStr const &_File, CStr const &_Data, CStr const &_Workspace, bool &_bWasCreated, bool _bNoDateCheck) const;
		void f_GenerateGlobalFiles(CBuildSystemData &_BuildSystemData) const;
		void f_GenerateWorkspaceFiles(CBuildSystemData &_BuildSystemData, CEntity & _Target) const;
		void f_GenerateTargetFiles(CBuildSystemData &_BuildSystemData, CEntity & _Target) const;
		void f_ExpandRepositoryEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandGlobalEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandDynamicImports(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandGlobalTargetsAndWorkspaces(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandTargetDependencies(CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const;
		void f_ExpandTargetGroups(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_ExpandTargetFiles(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_ExpandWorkspaceTargets(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_ExpandWorkspaceEntities(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		bool f_WriteFile(TCVector<uint8> const& _FileData, CStr const& _File, EFileAttrib _AddAttribs = EFileAttrib_None) const;
		void f_SetFileChanged(CStr const& _File) const;
		TCSet<CStr> f_GetSourceFiles() const;
		void f_ReEvaluateData(CEntity &_Entity) const;
		CEntity const *f_EvaluateData
			(
				CBuildSystemData &_Destination
				, TCMap<CPropertyKey, CStr> const &_InitialValues
				, CEntity const *_pStartEntity // = nullptr
				, TCMap<CPropertyKey, CStr> const *_pStartEntityInitialValues // = nullptr
				, TCVector<CEntityKey> const *_pStartEntityInitialValuesLocation // = nullptr
				, bool _bCopyTree // = true
				, bool _bAddEnvironment // = true
			) const
		;
		CEntity const *f_EvaluateDataMain
			(
				CBuildSystemData &_Destination
				, TCMap<CPropertyKey, CStr> const &_InitialValues
			) const
		;
		void f_EvaluateAllGeneratorSettings(CEntity &_Entity) const;
		void f_EvalGlobalWorkspaces
			(
				CBuildSystemData &_Destination
				, TCMap<CStr, CEntity *> &_Targets
			) const
		;
		void f_EvaluateTargetsInWorkspace
			(
				CBuildSystemData &_Destination
				, TCMap<CStr, CEntity *> const &_Targets
				, CEntity &_Workspace
			) const
		;
		void f_EvaluateTarget
			(
				CBuildSystemData &_Destination
				, TCMap<CStr, CEntity *> const &_Targets
				, CEntity &_Entity
			) const
		;
		TCMap<CPropertyKey, CStr> f_GetExternalValues(CEntity const &_Entity) const;
		CStr f_EvaluateEntityProperty(CEntity const &_Entity, EPropertyType _Type, CStr const &_Property) const;
		CStr f_EvaluateEntityProperty
			(
				CEntity const &_Entity
				, EPropertyType _Type
				, CStr const &_Property
				, CProperty const *&_pFromProperty
			) const
		;
		CStr f_EvaluateEntityPropertyUncached
			(
				CEntity const &_Entity
				, EPropertyType _Type
				, CStr const &_Property
				, CProperty const *&_pFromProperty
				, TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties = nullptr
			) const
		;
		void f_InitEntityForEvaluation(CEntity &_Entity, TCMap<CPropertyKey, CStr> const &_InitialValues) const;
		void f_InitEntityForEvaluationNoEnv(CEntity &_Entity, TCMap<CPropertyKey, CStr> const &_InitialValues) const;
		CStr f_GetExternalProperty(CEntity &_Entity, CPropertyKey const &_Key) const;
		void f_AddExternalProperty(CEntity &_Entity, CPropertyKey const &_Key, CStr const &_Value) const;
		TCVector<TCVector<CConfigurationTuple>> f_EvaluateConfigurationTuples(TCMap<CPropertyKey, CStr> const &_InitialValues) const;
		bool f_EvalCondition(CEntity const &_Context, CCondition const &_Condition) const;
		static void fs_ThrowError(CFilePosition const &_Position, CStr const &_Error);
		static void fs_ThrowError(CFilePosition const &_Position, CStr const &_Error, TCVector<CBuildSystemError> const &_Errors);
		void f_AddSourceFile(CStr const &_File) const;

		NStr::CStr f_GetEnvironmentVariable(NStr::CStr const &_Name, NStr::CStr const &_Default = {}, bool *o_pExists = nullptr) const;

		struct CRepoFilter
		{
			CStr m_NameWildcard;
			CStr m_Type;
			TCSet<CStr> m_Tags;
			bool m_bOnlyChanged = false;
		};

		enum ERepoCleanupBranchesFlag
		{
			ERepoCleanupBranchesFlag_None = 0
			, ERepoCleanupBranchesFlag_Pretend = DBit(0)
			, ERepoCleanupBranchesFlag_Remote = DBit(1)
		};

		enum ERepoStatusFlag
		{
			ERepoStatusFlag_None = 0
			, ERepoStatusFlag_Verbose = DBit(0)
			, ERepoStatusFlag_UpdateRemotes = DBit(1)
			, ERepoStatusFlag_ShowUntracked = DBit(2)
			, ERepoStatusFlag_Quiet = DBit(3)
			, ERepoStatusFlag_AllBranches = DBit(4)
			, ERepoStatusFlag_UseDefaultUpstreamBranch = DBit(5)
			, ERepoStatusFlag_OpenSourceTree = DBit(6)
		};

		enum ERepoListCommitsFlag
		{
			ERepoListCommitsFlag_None = 0
			, ERepoListCommitsFlag_UpdateRemotes = DBit(0)
			, ERepoListCommitsFlag_Color = DBit(1)
			, ERepoListCommitsFlag_Compact = DBit(2)
		};

		struct CWildcardColumn
		{
			CStr m_Name;
			CStr m_Wildcard;
		};

	private:
		struct CEvaluationContext
		{
			inline_always CEvaluationContext(TCMap<CPropertyKey, CEvaluatedProperty> *_pEvaluatedProperties);
			
			TCMap<CPropertyKey, TCSet<CEntity const *>> m_EvalStack;
			TCMap<CPropertyKey, CEvaluatedProperty> *m_pEvaluatedProperties;
			TCLinkedList<CStr> m_ExplodeListStack;
		};

		struct CChangePropertiesScope
		{
			inline_always CChangePropertiesScope(CEvaluationContext &_Context, TCMap<CPropertyKey, CEvaluatedProperty> *_pNewProperties);
			inline_always ~CChangePropertiesScope();
			
			TCMap<CPropertyKey, CEvaluatedProperty> *m_pOldProperties;
			CEvaluationContext &m_Context;
		};

		struct CGeneratedFile
		{
			CStr m_Contents;
			TCSet<CStr> m_Workspaces;
			bool m_bGeneral = false;
			bool m_bAdded = false;
			bool m_bNoDateCheck = false;
		};

		void fp_ParseCondition(CRegistryPreserveAndOrder_CStr &_Registry, CCondition &_ParentCondition, bool _bRoot = true) const;
		void fp_ParseConfigurationConditions(CRegistryPreserveAndOrder_CStr &_Registry, CBuildSystemConfiguration &_Configuration) const;
		void fp_ParseConfigurationType(CStr const &_Name, CRegistryPreserveAndOrder_CStr &_Registry, TCMap<CStr, CConfigurationType> &o_Configurations) const;
		void fp_ParsePropertyValue
			(
				EPropertyType _Type
				, CStr const &_PropertyName
				, TCLinkedList<CEntity *> &_Entities
				, CRegistryPreserveAndOrder_CStr &_Registry
				, CCondition const &_Conditions
			) const
		;
		void fp_ParseProperty(TCLinkedList<CEntity *> &_Entities, CRegistryPreserveAndOrder_CStr &_Registry) const;
		CEntity *fp_ParseEntity(CEntity &_Parent, CRegistryPreserveAndOrder_CStr &_Registry) const;
		static void fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error);
		static void fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error, TCVector<CBuildSystemError> const &_Errors);
		static void fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, CStr const &_Error);
		static void fsp_ThrowError(CRegistryPreserveAndOrder_CStr const &_Registry, CStr const &_Error);
		void fp_ParseData(CEntity &_RootEntity, CRegistryPreserveAndOrder_CStr &_Registry, TCMap<CStr, CConfigurationType> *_pConfigurations) const;
		bool fpr_EvalCondition
			(
				CEntity const &_Context
				, CEntity const &_OriginalContext
				, CCondition const &_Condition
				, CEvaluationContext &_EvalContext
				, mint _TraceDepth
			) const;
		bool fp_EvalConditionSubject
			(
				CEntity const &_Context
				, CEntity const &_OriginalContext
				, CCondition const &_Condition
				, CEvaluationContext &_EvalContext
				, mint _TraceDepth
			) const
		;
		CStr fp_EvaluateEntityProperty
			(
				CEntity const &_Entity
				, CEntity const &_OriginalEntity
				, CPropertyKey const &_Key
				, CEvaluationContext &_EvalContext
				, CProperty const *&_pFromProperty
			) const
		;
		CStr fp_GetPropertyValue
			(
				CEntity const &_Context
				, CEntity const &_OriginalContext
				, CStr const &_Value
				, CFilePosition const &_Position
				, CEvaluationContext &_EvalContext
			) const
		;
		CStr fp_EvaluatePropertyValue
			(
				CEntity const &_Context
				, CEntity const &_OriginalContext
				, CStr const &_Value
				, CFilePosition const &_Position
				, CEvaluationContext &_EvalContext
			) const
		;
		void fpr_EvaluateData(CEntity &_Entity) const;
		void fpr_EvaluateData(CEntity &_Entity, TCSet<CEntity *> &o_Deleted) const;
		void fp_EvaluateDataOrder(CEntity &_Entity) const;
		void fpr_EvaluateAllGeneratorSettings(CEntity &_Entity) const;
		void fp_EvaluateAllProperties(CEntity &_Entity) const;
		TCVector<TCVector<CConfigurationTuple>> fp_EvaluateConfigurationTuples(TCMap<CPropertyKey, CStr> const &_InitialValues) const;
		CEntity const *fp_EvaluateData
			(
				CBuildSystemData &_Destination
				, TCMap<CPropertyKey, CStr> const &_InitialValues
				, CEntity const *_pStartEntity
				, TCMap<CPropertyKey, CStr> const *_pStartEntityInitialValues
				, TCVector<CEntityKey> const *_pStartEntityInitialValuesLocation
				, bool _bCopyTree
				, bool _bAddEnvironment
				, bool _bAllChildren
			) const
		;
		void fp_EvalGlobalWorkspaces
			(
				CBuildSystemData &_Destination
				, TCMap<CStr, CEntity *> &_Targets
			) const
		;
		void fp_EvaluateTargetsInWorkspace
			(
				CBuildSystemData &_Destination
				, TCMap<CStr, CEntity *> const &_Targets
				, CEntity &_Workspace
			) const
		;
		CEntity *fpr_FindChildTarget(CEntity &_Entity, CEntityKey const &_EntityKey) const;
		void fp_EvaluateTarget
			(
				CBuildSystemData &_Destination
				, TCMap<CStr, CEntity *> const &_Targets
				, TCLinkedList<CEntity *> &_ToEval
				, CEntity &_Entity
			) const
		;
		void fp_EvaluateWorkspace(CBuildSystemData &_Destination, CEntity &_Entity) const;
		void fp_UsedExternal(CStr const &_Name) const;
		void fp_GenerateFiles(CBuildSystemData &_BuildSystemData, CEntity & _Entity, bool _bRecursive, EEntityType _Type) const;
		CEntity *fp_AddEntity
			(
				CEntity &_Entity
				, CEntity &_ParentEntity
				, CEntityKey const &_NewKey
				, CEntity *_pInsertAfter
				, TCMap<CPropertyKey, CEvaluatedProperty> const *_pExtraProperties
			) const
		;
		bool fp_ExpandEntity(CEntity &_Entity, CEntity &_ParentEntity, TCVector<CEntity *> *o_pCreated) const;
		void fp_ExpandImport(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const;
		CBuildSystemData::CImportData *fp_ExpandImportCMake(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const;
		CBuildSystemData::CImportData *fp_ExpandImportCMake_FromGeneratedDirectory(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData, CStr const &_Directory) const;
		void fp_TracePropertyEval(bool _bSuccess, CEntity const &_Entity, CProperty const &_Property, CStr const &_Value) const;

		bool fp_HandleRepositories(TCMap<CPropertyKey, CStr> const &_Values);

		void fp_Repository_ForEachRepo(CRepoFilter const &_Filter, bool _bParallell, TCVector<CStr> const &_Params);
		void fp_Repository_Branch(CRepoFilter const &_Filter, CStr const &_Branch);
		void fp_Repository_Unbranch(CRepoFilter const &_Filter);
		void fp_Repository_CleanupBranches(CRepoFilter const &_Filter, ERepoCleanupBranchesFlag _Flags);
		void fp_Repository_Status(CRepoFilter const &_Filter, ERepoStatusFlag _Flags);
		void fp_Repository_Push(CRepoFilter const &_Filter, TCVector<CStr> const &_Remotes);
		void fp_Repository_ListCommits(CRepoFilter const &_Filter, CStr const &_From, CStr const &_To, ERepoListCommitsFlag _Flags, TCVector<CWildcardColumn> const &_ColumnWildcards);
		void fp_HandleAction(CStr const &_Action, TCVector<CStr> const &_Params);

		CGenerateSettings mp_GenerateSettings;

		align_cacheline mutable CMutualManyRead mp_SourceFilesLock;
		mutable TCSet<CStr> mp_SourceFiles;
		CFindCache mp_FindCache;
		CRegistryPreserveAndOrder_CStr mp_Registry;

		CBuildSystemData mp_Data;
		CProperty mp_ExternalProperty;

		mutable TCPointer<CGeneratorInterface> mp_GeneratorInterface;

		mutable CRegistryPreserveAndOrder_CStr mp_UserSettingsRegistry;
		mutable TCMap<CPropertyKey, CRegistryPreserveAndOrder_CStr const *> mp_UserSettingsProperties;

		align_cacheline mutable CMutualManyRead mp_UsedExternalsLock;
		mutable TCSet<CStr> mp_UsedExternals;

		CStr mp_FileLocation;
		CStr mp_BaseDir;
		CStr mp_OutputDir;

		CTime mp_Now;
		CTime mp_NowUTC;
		
		CStr mp_UserSettingsFile;
		CStr mp_FileLocationFile;
		CStr mp_GeneratorStateFileName;
		TCMap<CStr, CStr> mp_Environment;
		CStr mp_GenerateWorkspace;
		zbool mp_ValidTargetsValid;
		mutable TCAtomic<bool> mp_FileChanged;
		TCSet<CStr> mp_ValidTargets;
		
		align_cacheline mutable CMutual mp_GeneratedFilesLock;
		mutable TCMap<CStr, CGeneratedFile> mp_GeneratedFiles;
		
		align_cacheline mutable CMutual mp_CMakeGenerateLock;
		mutable TCMap<CStr, CMutual> mp_CMakeGenerateLocks;
		mutable TCMap<CStr, CStr> mp_CMakeGenerated;
		mutable TCMap<CStr, CStr> mp_CMakeGeneratedContents;
		
		EFileAttrib mp_SupportedAttributes = CFile::fs_GetSupportedAttributes();
		EFileAttrib mp_ValidAttributes = CFile::fs_GetValidAttributes();
	};
}

#include "Malterlib_BuildSystem.hpp"

#endif
