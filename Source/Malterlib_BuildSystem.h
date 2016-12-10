// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

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
		bint f_Generate(CGenerateSettings const &_GenerateSettings);
		void f_GenerateBuildSystem
			(
				TCMap<CConfiguration, TCUniquePointer<CConfiguraitonData>> &o_Configurations
				, TCMap<CPropertyKey, CStr> const &_Values
				, TCSet<CStr> const &_ReservedGroups
				, CStr const &_DependencyFilesName
			) const
		;
		inline_always CGenerateSettings const& f_GetGenerateSettings() const;
		CStr f_GetBaseDir() const;
		bool f_AddGeneratedFile(CStr const &_File, CStr const &_Data, CStr const &_Workspace, bool &_bWasCreated, bool _bNoDateCheck) const;
		void f_GenerateGlobalFiles(CBuildSystemData &_BuildSystemData) const;
		void f_GenerateWorkspaceFiles(CBuildSystemData &_BuildSystemData, CEntity & _Target) const;
		void f_GenerateTargetFiles(CBuildSystemData &_BuildSystemData, CEntity & _Target) const;
		void f_ExpandGlobalEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandGlobalTargetsAndWorkspaces(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandTargetDependencies(CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const;
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
			zbool m_bGeneral;
			zbool m_bAdded;
			zbool m_bNoDateCheck;
		};

		void fp_ParseCondition(CRegistryPreserveAndOrder_CStr &_Registry, CCondition &_ParentCondition, bool _bRoot = true);
		void fp_ParseConfigurationConditions(CRegistryPreserveAndOrder_CStr &_Registry, CBuildSystemConfiguration &_Configuration);
		void fp_ParseConfigurationType(CStr const &_Name, CRegistryPreserveAndOrder_CStr &_Registry);
		void fp_ParsePropertyValue
			(
				EPropertyType _Type
				, CStr const &_PropertyName
				, TCLinkedList<CEntity *> &_Entities
				, CRegistryPreserveAndOrder_CStr &_Registry
				, CCondition const &_Conditions
			)
		;
		void fp_ParseProperty(TCLinkedList<CEntity *> &_Entities, CRegistryPreserveAndOrder_CStr &_Registry);
		CEntity *fp_ParseEntity(CEntity &_Parent, CRegistryPreserveAndOrder_CStr &_Registry);
		static void fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error);
		static void fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error, TCVector<CBuildSystemError> const &_Errors);
		static void fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, CStr const &_Error);
		static void fsp_ThrowError(CRegistryPreserveAndOrder_CStr const &_Registry, CStr const &_Error);
		void fp_ParseData();
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
		bool fp_ExpandEntity(CEntity &_Entity, CEntity &_ParentEntity, TCVector<CEntity *> *o_pCreated) const;
		void fp_TracePropertyEval(bool _bSuccess, CEntity const &_Entity, CProperty const &_Property, CStr const &_Value) const;
		
		CGenerateSettings mp_GenerateSettings;

		TCSet<CStr> mp_SourceFiles;
		CFindCache mp_FindCache;
		CRegistryPreserveAndOrder_CStr mp_Registry;

		CBuildSystemData mp_Data;
		CProperty mp_ExternalProperty;

		mutable TCPointer<CGeneratorInterface> mp_GeneratorInterface;

		mutable CRegistryPreserveAndOrder_CStr mp_UserSettingsRegistry;
		mutable TCMap<CPropertyKey, CRegistryPreserveAndOrder_CStr const *> mp_UserSettingsProperties;

		mutable CMutualManyRead mp_UsedExternalsLock;
		mutable TCSet<CStr> mp_UsedExternals;

		CStr mp_FileLocation;
		CStr mp_BaseDir;
		
		CTime mp_Now;
		CTime mp_NowUTC;
		
		CStr mp_UserSettingsFile;
		CStr mp_FileLocationFile;
		CStr mp_GeneratorStateFileName;
		CStr mp_GenerateWorkspace;
		zbool mp_ValidTargetsValid;
		mutable TCAtomic<bool> mp_FileChanged;
		TCSet<CStr> mp_ValidTargets;
		
		mutable CMutual mp_GeneratedFilesLock;
		mutable TCMap<CStr, CGeneratedFile> mp_GeneratedFiles;
	};
}

#include "Malterlib_BuildSystem.hpp"
