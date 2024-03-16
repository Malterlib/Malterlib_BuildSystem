// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#ifdef __cplusplus

#include "Malterlib_BuildSystem_Find.h"
#include "Malterlib_BuildSystem_Data.h"
#include <Mib/Concurrency/ThreadSafeQueue>
#include <Mib/Concurrency/AsyncResult>
#include <Mib/Concurrency/ParallellForEach>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/CommandLine/AnsiEncoding>

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
#include "Malterlib_BuildSystem_GeneratorState.h"
#include "Malterlib_BuildSystem_Syntax.h"
#include "Malterlib_BuildSystem_ParallelForEach.h"
#include "Malterlib_BuildSystem_ValuePotentiallyByRef.h"

namespace NMib::NBuildSystem
{
	struct CBuildSystemPropertyInfo
	{
		CFilePosition const &f_FallbackPosition() const;
		CBuildSystemUniquePositions f_GetPositions() const;

		CProperty const *m_pProperty = nullptr;
		NStorage::TCSharedPointer<CBuildSystemUniquePositions> m_pPositions;
		CFilePosition const *m_pFallbackPosition = nullptr;
	};

	class CBuildSystem : public NConcurrency::CAllowUnsafeThis
	{
	public:
		CBuildSystem
			(
				NCommandLine::EAnsiEncodingFlag _AnsiFlags
				, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
				, NStorage::TCSharedPointer<NAtomic::TCAtomic<bool>> const &_pCancelled
			)
		;
		CBuildSystem(CBuildSystem const &) = delete;
		CBuildSystem(CBuildSystem &&) = delete;

		enum ERetry
		{
			ERetry_None
			, ERetry_Again
			, ERetry_Again_NoReconcileOptions
			, ERetry_Again_EnablePositions
			, ERetry_Relaunch
			, ERetry_Relaunch_NoReconcileOptions
		};

		struct CRepoFilter
		{
			static CRepoFilter fs_ParseParams(NEncoding::CEJSONSorted const &_Params);

			NStr::CStr m_NameWildcard;
			NStr::CStr m_Type;
			NStr::CStr m_Branch;
			NContainer::TCSet<NStr::CStr> m_Tags;
			bool m_bOnlyChanged = false;
		};

		enum ERepoCleanupBranchesFlag
		{
			ERepoCleanupBranchesFlag_None = 0
			, ERepoCleanupBranchesFlag_Pretend = DMibBit(0)
			, ERepoCleanupBranchesFlag_AllRemotes = DMibBit(1)
			, ERepoCleanupBranchesFlag_UpdateRemotes = DMibBit(2)
			, ERepoCleanupBranchesFlag_Verbose = DMibBit(3)
			, ERepoCleanupBranchesFlag_Force = DMibBit(4)
		};

		enum ERepoCleanupTagsFlag
		{
			ERepoCleanupTagsFlag_None = 0
			, ERepoCleanupTagsFlag_Pretend = DMibBit(0)
			, ERepoCleanupTagsFlag_AllRemotes = DMibBit(1)
			, ERepoCleanupTagsFlag_UpdateRemotes = DMibBit(2)
			, ERepoCleanupTagsFlag_Verbose = DMibBit(3)
			, ERepoCleanupTagsFlag_Force = DMibBit(4)
		};

		enum ERepoStatusFlag
		{
			ERepoStatusFlag_None = 0
			, ERepoStatusFlag_Verbose = DMibBit(0)
			, ERepoStatusFlag_UpdateRemotes = DMibBit(1)
			, ERepoStatusFlag_OnlyTracked = DMibBit(2)
			, ERepoStatusFlag_ShowUnchanged = DMibBit(3)
			, ERepoStatusFlag_AllBranches = DMibBit(4)
			, ERepoStatusFlag_UseDefaultUpstreamBranch = DMibBit(5)
			, ERepoStatusFlag_OpenEditor = DMibBit(6)
			, ERepoStatusFlag_NonDefaultToAll = DMibBit(7)
			, ERepoStatusFlag_NeedActionOnPush = DMibBit(8)
			, ERepoStatusFlag_NeedActionOnPull = DMibBit(9)
			, ERepoStatusFlag_NeedActionOnLocalChanges = DMibBit(10)
		};

		enum ERepoListCommitsFlag
		{
			ERepoListCommitsFlag_None = 0
			, ERepoListCommitsFlag_UpdateRemotes = DMibBit(0)
			, ERepoListCommitsFlag_Compact = DMibBit(1)
			, ERepoListCommitsFlag_Changelog = DMibBit(2)
		};

		enum ERepoPushFlag
		{
			ERepoPushFlag_None = 0
			, ERepoPushFlag_Pretend = DMibBit(0)
			, ERepoPushFlag_FollowTags = DMibBit(1)
			, ERepoPushFlag_NonDefaultToAll = DMibBit(2)
			, ERepoPushFlag_Force = DMibBit(3)
		};

		enum ERepoBranchFlag
		{
			ERepoBranchFlag_None = 0
			, ERepoBranchFlag_Pretend = DMibBit(0)
			, ERepoBranchFlag_Force = DMibBit(1)
		};

		struct CWildcardColumn
		{
			NStr::CStr m_Name;
			NStr::CStr m_Wildcard;
		};

		struct CExplodeStackEntry
		{
			NEncoding::CEJSONSorted const *m_pExplodedValue;
			NEncoding::CEJSONSorted m_Value;
		};

		struct CEvaluationContext
		{
			inline_always CEvaluationContext(CEvaluatedProperties *_pEvaluatedProperties);

			NContainer::TCMap<CPropertyKey, NContainer::TCSet<CEntity const *>> m_EvalStack;
			CEvaluatedProperties *m_pEvaluatedProperties = nullptr;
			NContainer::TCLinkedList<CExplodeStackEntry> m_ExplodeListStack;
			CBuildSystemSyntax::CFunctionType const *m_pCallingFunction = nullptr;
			NContainer::TCMap<CPropertyKey, CTypeWithPosition> m_OverriddenTypes;
			bool m_bFailUndefinedTypeCheck = true;
		};

		struct CEvalPropertyValueContext
		{
			CEntity &m_Context;
			CEntity &m_OriginalContext;
			CFilePosition const &m_Position;
			CEvaluationContext &m_EvalContext;
			CEvalPropertyValueContext const *m_pParentContext;
			CBuildSystemUniquePositions *m_pStorePositions = nullptr;
		};

		using FEvalPropertyFunction = NFunction::TCFunction
			<
				NEncoding::CEJSONSorted (CBuildSystem const &_This, CEvalPropertyValueContext &_Context, NContainer::TCVector<NEncoding::CEJSONSorted> &&_Params)
			>
		;

		struct CBuiltinFunction
		{
			CBuildSystemSyntax::CFunctionType m_Type;
			FEvalPropertyFunction m_fFunction;
			CFilePosition m_Position;
		};

		struct CExpandEntityState
		{
			CExpandEntityState();
			~CExpandEntityState();

			NContainer::TCSet<CEntityMutablePointer> m_OldEntitiesToRemove;
			bool m_bEnabled = false;
		};

		void f_SetGeneratorInterface(ICGeneratorInterface *_pInterface) const;
		NConcurrency::TCFuture<void> f_GenerateBuildSystem
			(
				NContainer::TCMap<CConfiguration, NStorage::TCUniquePointer<CConfiguraitonData>> *o_pConfigurations
				, NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const *_pValues
			) const
		;
		void f_GenerateBuildSystem_Workspace
			(
				CConfiguration const &_Config
				, CConfiguraitonData const &_ConfigData
				, CWorkspaceInfo *_pWorkspace
				, NContainer::TCSet<NStr::CStr> const &_ReservedGroups
				, NStr::CStr const &_DependencyFilesName
			) const
		;

		inline_always CGenerateSettings const &f_GetGenerateSettings() const;
		inline_always CGenerateOptions const &f_GetGenerateOptions() const;
		NStr::CStr const &f_GetBaseDir() const;
		bool f_AddGeneratedFile(NStr::CStr const &_File, NStr::CStr const &_Data, NStr::CStr const &_Workspace, bool &_bWasCreated, EGeneratedFileFlag _Flags = EGeneratedFileFlag_None) const;
		void f_GenerateGlobalFiles(CBuildSystemData &_BuildSystemData, bool _bBeforeImports) const;
		void f_GenerateWorkspaceFiles(CBuildSystemData &_BuildSystemData, CEntity &_Target) const;
		bool f_GenerateTargetFiles(CBuildSystemData &_BuildSystemData, CEntity &_Target) const;
		void f_ExpandRepositoryEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandCreateTemplateEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandGlobalEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandDynamicImports(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandGlobalTargetsAndWorkspaces(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandTargetDependenciesBackup(CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const;
		void f_ExpandTargetDependencies(CWorkspaceInfo &_Workspace, CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const;
		bool f_ExpandTargetGroups(CExpandEntityState &_ExpandState, CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		bool f_ExpandTargetFiles(CExpandEntityState &_ExpandState, CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_ExpandWorkspaceTargets(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_ExpandWorkspaceEntities(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_PopulateTargetAllFiles(CEntity &o_Target) const;
		bool f_WriteFile(NContainer::CByteVector const &_FileData, NStr::CStr const &_File, NFile::EFileAttrib _AddAttribs = NFile::EFileAttrib_None) const;
		void f_SetFileChanged(NStr::CStr const &_File) const;
		NContainer::TCSet<NStr::CStr> f_GetSourceFiles() const;
		CEntity const *f_EvaluateData
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const &_InitialValues
				, CEntity const *_pStartEntity // = nullptr
				, bool _bCopyTree // = true
			) const
		;
		CEntity const *f_EvaluateDataMain
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const &_InitialValues
			) const
		;
		void f_EvalGlobalWorkspaces
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<NStr::CStr, CEntityMutablePointer> &_Targets
			) const
		;
		NContainer::TCMap<CEntity *, NContainer::TCSet<NStr::CStr>> f_EvaluateTargetsInWorkspace
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<NStr::CStr, CEntityMutablePointer> const &_Targets
				, CEntity &_Workspace
			) const
		;
		void f_EvaluateTarget
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<NStr::CStr, CEntityMutablePointer> const &_Targets
				, CEntity &_Entity
				, NContainer::TCSet<NStr::CStr> &_AlreadyAddedGroups
			) const
		;
		NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> f_GetExternalValues(CEntity const &_Entity) const;
		CValuePotentiallyByRef f_EvaluateEntityProperty(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey) const;
		CValuePotentiallyByRef f_EvaluateEntityPropertyObject
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, NStorage::TCOptional<NEncoding::CEJSONSorted> &&_Default = {}
			) const
		;
		NStr::CStr f_EvaluateEntityPropertyString(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<NStr::CStr> &&_Default = {}) const;
		NStr::CStr f_EvaluateEntityPropertyString
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, CBuildSystemPropertyInfo &o_PropertyInfo
				, NStorage::TCOptional<NStr::CStr> &&_Default = {}
			) const
		;
		bool f_EvaluateEntityPropertyTryBool(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<bool> const &_Default = {}) const;
		bool f_EvaluateEntityPropertyBool(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<bool> const &_Default = {}) const;
		bool f_EvaluateEntityPropertyBool
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, CBuildSystemPropertyInfo &o_PropertyInfo
				, NStorage::TCOptional<bool> const &_Default = {}
				, CEvaluatedProperties *_pEvaluatedProperties = nullptr
			) const
		;
		fp64 f_EvaluateEntityPropertyFloat(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<fp64> const &_Default = {}) const;
		int64 f_EvaluateEntityPropertyInteger(CEntity &_Entity, CPropertyKeyReference const &_PropertyKey, NStorage::TCOptional<int64> const &_Default = {}) const;
		NContainer::TCVector<NStr::CStr> f_EvaluateEntityPropertyStringArray
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, NStorage::TCOptional<NContainer::TCVector<NStr::CStr>> &&_Default = {}
			) const
		;
		CValuePotentiallyByRef f_EvaluateEntityProperty
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, CBuildSystemPropertyInfo &o_PropertyInfo
				, CEvaluatedProperties *_pEvaluatedProperties = nullptr
			) const
		;
		CValuePotentiallyByRef f_EvaluateEntityPropertyNoDefault
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, CBuildSystemPropertyInfo &o_PropertyInfo
				, CEvaluatedProperties *_pEvaluatedProperties = nullptr
			) const
		;
		NEncoding::CEJSONSorted f_EvaluateEntityPropertyUncached
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, CBuildSystemPropertyInfo &o_PropertyInfo
				, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties = nullptr
				, CEvaluatedProperties *_pEvaluatedProperties = nullptr
			) const
		;
		NStr::CStr f_EvaluateEntityPropertyUncachedString
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, CBuildSystemPropertyInfo &o_PropertyInfo
				, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties = nullptr
				, NStorage::TCOptional<NStr::CStr> const &_Default = {}
			) const
		;
		NContainer::TCVector<NStr::CStr> f_EvaluateEntityPropertyUncachedStringArray
			(
				CEntity &_Entity
				, CPropertyKeyReference const &_PropertyKey
				, CBuildSystemPropertyInfo &o_PropertyInfo
				, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties = nullptr
				, NStorage::TCOptional<NContainer::TCVector<NStr::CStr>> const &_Default = {}
			) const
		;
		void f_InitEntityForEvaluation(CEntity &_Entity, NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const &_InitialValues) const;
		void f_InitEntityForEvaluationNoEnv(CEntity &_Entity, NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const &_InitialValues, EEvaluatedPropertyType _Type) const;
		NEncoding::CEJSONSorted f_GetExternalProperty(CEntity &_Entity, CPropertyKeyReference const &_Key) const;
		bool f_AddExternalProperty(CEntity &_Entity, CPropertyKeyReference const &_Key, NEncoding::CEJSONSorted &&_Value) const;
		NContainer::TCVector<NContainer::TCVector<CConfigurationTuple>> f_EvaluateConfigurationTuples(NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const &_InitialValues) const;
		bool f_EvalCondition(CEntity &_Context, CCondition const &_Condition, bool _bTrace = false, CEvaluatedProperties *_pEvaluatedProperties = nullptr) const;
		[[noreturn]] static void fs_ThrowError(CFilePosition const &_Position, NStr::CStr const &_Error);
		[[noreturn]] static void fs_ThrowError(CBuildSystemPropertyInfo const &_PropertyInfo, NStr::CStr const &_Error);
		[[noreturn]] static void fs_ThrowError(CBuildSystemUniquePositions const &_Positions, NStr::CStr const &_Error);
		[[noreturn]] static void fs_ThrowError(CBuildSystemPropertyInfo const &_PropertyInfo, CFilePosition const &_Position, NStr::CStr const &_Error);
		[[noreturn]] static void fs_ThrowError(CBuildSystemPropertyInfo const &_PropertyInfo, CBuildSystemUniquePositions const &_Positions, NStr::CStr const &_Error);
		[[noreturn]] static void fs_ThrowError(CFilePosition const &_Position, NStr::CStr const &_Error, NContainer::TCVector<CBuildSystemError> const &_Errors);
		[[noreturn]] static void fs_ThrowError(CBuildSystemUniquePositions const &_Positions, NStr::CStr const &_Error, NContainer::TCVector<CBuildSystemError> const &_Errors);
		[[noreturn]] static void fs_ThrowError(CBuildSystemRegistry const &_Registry, NStr::CStr const &_Error);
		[[noreturn]] static void fs_ThrowError
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemUniquePositions const &_Positions
				, NStr::CStr const &_Error
				, NContainer::TCVector<CBuildSystemError> const &_Errors = {}
			)
		;
		[[noreturn]] static void fs_ThrowError
			(
				CEvalPropertyValueContext &_Context
				, CFilePosition const &_Position
				, NStr::CStr const &_Error
				, NContainer::TCVector<CBuildSystemError> const &_Errors = {}
			)
		;
		[[noreturn]] static void fs_ThrowError
			(
				CEvalPropertyValueContext &_Context
				, NStr::CStr const &_Error
				, NContainer::TCVector<CBuildSystemError> const &_Errors = {}
			)
		;
		static NStr::CStr fs_GetNameIdentifierString(CBuildSystemRegistry const &_Registry);
		void f_AddSourceFile(NStr::CStr const &_File) const;
		NStr::CStr f_ReadFile(NStr::CStr const &_File) const;
		void f_CheckPropertyTypeValue
			(
				CPropertyKeyReference const &_PropertyKey
				, NEncoding::CEJSONSorted const &_Value
				, NEncoding::EEJSONType _ExpectedType
				, CBuildSystemUniquePositions const &_Positions
				, bool _bOptional
			) const
		;

		NStr::CStr f_GetEnvironmentVariable(NStr::CStr const &_Name, NStr::CStr const &_Default = {}, bool *o_pExists = nullptr) const;

		void f_NoReconcileOptions();

		bool f_SingleThreaded() const;

		static NConcurrency::TCFuture<ERetry> fs_RunBuildSystem
			(
				NFunction::TCFunctionMovable<NConcurrency::TCFuture<CBuildSystem::ERetry> (CBuildSystem &_BuildSystem)> _fCommand
				, NStorage::TCSharedPointer<NConcurrency::CCommandLineControl> const &_pCommandLine
				, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
				, CGenerateOptions const &_GenerateOptions
			)
		;

		NConcurrency::TCFuture<bool> f_Action_Generate(CGenerateOptions const &_GenerateOptions, ERetry &o_Retry);
		NConcurrency::TCFuture<ERetry> f_Action_Create(CGenerateOptions const &_GenerateOptions);

		NConcurrency::TCFuture<ERetry> f_Action_Repository_Update(CGenerateOptions const &_GenerateOptions);
		NConcurrency::TCFuture<ERetry> f_Action_Repository_Status
			(
				CGenerateOptions const &_GenerateOptions
				, CRepoFilter const &_Filter
				, ERepoStatusFlag _Flags
				, NContainer::TCVector<NStr::CStr> const &_HideBranches
				, NStorage::TCSharedPointer<NConcurrency::CCommandLineControl> const &_pCommandLine
			)
		;
		NConcurrency::TCFuture<ERetry> f_Action_Repository_ForEachRepo
			(
				CGenerateOptions const &_GenerateOptions
				, CRepoFilter const &_Filter
				, bool _bParallell
				, NContainer::TCVector<NStr::CStr> const &_Params
			)
		;

		struct CForEachRepoDirOptions
		{
			NContainer::TCVector<NStr::CStr> m_Params;
			NStr::CStr m_Application;
			bool m_bParallel = true;
		};
		NConcurrency::TCFuture<ERetry> f_Action_Repository_ForEachRepoDir(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, CForEachRepoDirOptions const &_Options);

		NConcurrency::TCFuture<ERetry> f_Action_Repository_Branch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, NStr::CStr const &_Branch, ERepoBranchFlag _Flags);
		NConcurrency::TCFuture<ERetry> f_Action_Repository_Unbranch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoBranchFlag _Flags);

		NConcurrency::TCFuture<ERetry> f_Action_Repository_CleanupBranches
			(
				CGenerateOptions const &_GenerateOptions
				, CRepoFilter const &_Filter
				, ERepoCleanupBranchesFlag _Flags
				, NContainer::TCVector<NStr::CStr> const &_Branches
			)
		;
		NConcurrency::TCFuture<ERetry> f_Action_Repository_CleanupTags
			(
				CGenerateOptions const &_GenerateOptions
				, CRepoFilter const &_Filter
				, ERepoCleanupTagsFlag _Flags
				, NContainer::TCVector<NStr::CStr> const &_Tags
			)
		;
		NConcurrency::TCFuture<ERetry> f_Action_Repository_Push(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, NContainer::TCVector<NStr::CStr> const &_Remotes, ERepoPushFlag _PushFlags);

		NConcurrency::TCFuture<ERetry> f_Action_Repository_ListCommits
			(
				CGenerateOptions const &_GenerateOptions
				, CRepoFilter const &_Filter
				, NStr::CStr const &_From
				, NStr::CStr const &_To
				, ERepoListCommitsFlag _Flags
				, NContainer::TCVector<CWildcardColumn> const &_ColumnWildcards
				, NStr::CStr const &_Prefix
				, uint32 _MaxCommitsMainRepo
				, uint32 _MaxCommits
				, uint32 _MaxMessageWidth
				, NStorage::TCSharedPointer<NConcurrency::CCommandLineControl> const &_pCommandLine
			)
		;

		NCommandLine::EAnsiEncodingFlag f_AnsiFlags() const;

		void f_RegisterFunctions(NContainer::TCMap<NStr::CStr, CBuiltinFunction> &&_Functions);
		void f_RegisterBuiltinVariables(NContainer::TCMap<CPropertyKey, CTypeWithPosition> &&_Variables) const;
		void f_OutputConsole(NStr::CStr const &_Output, bool _bError = false) const;
		NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &f_OutputConsoleFunctor() const;

		static void fs_AddEntityVariableDefinition
			(
				CBuildSystem const *_pBuildSystem
				, CEntity &_DestinationEntity
				, CPropertyKeyReference const &_VariableName
				, CBuildSystemSyntax::CType const &_Type
				, CFilePosition const &_Position
				, NStr::CStr const &_Whitespace
				, NStorage::TCSharedPointer<CCondition> const &_pConditions
				, EPropertyFlag _Flags
			)
		;
		static void fs_AddEntityUserType
			(
				CEntity &_DestinationEntity
				, NStr::CStr const &_TypeName
				, CBuildSystemSyntax::CType const &_Type
				, CFilePosition const &_Position
				, NStorage::TCSharedPointer<CCondition> const &_pConditions
				, EPropertyFlag _Flags
			)
		;
		CBuildSystemSyntax::CType const *f_GetCanonicalDefaultedType
			(
				CEntity &_Entity
				, CBuildSystemSyntax::CType const *_pType
				, CFilePosition &o_TypePosition
			) const
		;

		CProperty const &f_ExternalProperty(EPropertyType _Type) const;
		void f_ForEachDefaultedBuiltinVariableDefinition
			(
				EPropertyType _Type
				, NFunction::TCFunction<void (CPropertyKey const &_Key, CTypeWithPosition const &_Type)> const &_fOnDefinition
			) const
		;

		bool f_EnablePositions() const;
		CBuildSystemUniquePositions *f_EnablePositions(NStorage::TCSharedPointer<CBuildSystemUniquePositions> &o_pPositions) const;
		CBuildSystemUniquePositions *f_EnablePositions(CBuildSystemUniquePositions *_pPositions) const;
		void f_SetEnablePositions();

		bool f_EnableValues() const;
		void f_SetEnableValues();

		CStringCache &f_StringCache() const;

		template <bool tf_bFile>
		NEncoding::CEJSONSorted f_GetDefinedProperties(CEntity &_Entity, EPropertyType _PropertyType, bool &o_bIsFullEval) const;

		NConcurrency::TCFuture<void> f_CheckCancelled() const;
		void f_CheckCancelledException() const;
		NStorage::TCSharedPointer<NAtomic::TCAtomic<bool>> f_GetCancelledPointer() const;
		
		constexpr static uint32 mc_MToolVersion = 2;

	private:
		struct CChangePropertiesScope
		{
			inline_always CChangePropertiesScope(CEvaluationContext &_Context, CEvaluatedProperties *_pNewProperties);
			inline_always ~CChangePropertiesScope();

			CEvaluatedProperties *m_pOldProperties;
			CEvaluationContext &m_Context;
		};

		struct CGeneratedFile
		{
			NStr::CStr m_Contents;
			NContainer::TCSet<NStr::CStr> m_Workspaces;
			bool m_bGeneral = false;
			bool m_bAdded = false;
			EGeneratedFileFlag m_Flags = EGeneratedFileFlag_None;
		};

		struct CUserSettingsState
		{
			void f_Parse(CStringCache &o_StringCache);
			CBuildSystemRegistry *f_GetSection(CPropertyKey const &_Section);

			CBuildSystemRegistry m_Registry;
			NContainer::TCSet<CPropertyKey> m_Defined;
			NContainer::TCMap<CPropertyKey, CBuildSystemRegistry *> m_Sections;
			NContainer::TCMap<CPropertyKey, CBuildSystemRegistry const *> m_Properties;
			bool m_bChanged = false;
		};

		struct CGenerateEphemeralState
		{
			NTime::CClock m_Clock{true};
			NStorage::TCUniquePointer<CBuildSystemGenerator> m_pGenerator;
			NStr::CStr m_FileLocation;
			NStr::CStr m_OutputDir;
			NStr::CStr m_RelativeFileLocation;
			NStr::CStr m_GlobalGeneratorStateFileName;
			NStr::CStr m_WorkspaceGeneratorStateFileName;
			NStr::CStr m_EnvironmentStateFile;
			CGeneratorArchiveState m_GlobalState;
			CGeneratorArchiveState m_BeforeGlobalState;
			NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> m_GeneratorValues;
			NStorage::TCUniquePointer<ICGeneratorInterface> m_pLocalGeneratorInterface;
			COnScopeExitShared m_LocalGeneratorInterfaceCleanup;
			bool m_bUseCachedEnvironment = false;
			bool m_bDisableUserSettings = false;

			align_cacheline NAtomic::TCAtomic<bool> m_bDependenciesChanged;
		};

		struct CTypeWithPositionReference
		{
			CBuildSystemSyntax::CType const *m_pType;
			CFilePosition m_Position;
		};

		enum EBuiltinFunctionGetProperty
		{
			EBuiltinFunctionGetProperty_GetProperty
			, EBuiltinFunctionGetProperty_HasProperty
			, EBuiltinFunctionGetProperty_HasEntity
		};

		enum EFindFilesFunctionType
		{
			EFindFilesFunctionType_File
			, EFindFilesFunctionType_Directory
			, EFindFilesFunctionType_RecursiveFile
			, EFindFilesFunctionType_RecursiveDirectory
		};

		struct CTypeConformError
		{
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			NStr::CStr m_Error;
			NContainer::TCVector<NStr::CStr> m_ErrorPath;
		};

		struct CApplyAccessorsHelper;

	private:
		NConcurrency::TCFuture<ERetry> fp_GeneratePrepare
			(
				CGenerateOptions const &_GenerateOptions
				, CGenerateEphemeralState &_GenerateState
				, NFunction::TCFunction<NConcurrency::TCFuture<bool> ()> &&_fPreParse
			)
		;

		void fp_ParseConfigurationConditions(CBuildSystemRegistry &_Registry, CBuildSystemConfiguration &_Configuration) const;
		void fp_ParseConfigurationType(NStr::CStr const &_Name, CBuildSystemRegistry &_Registry, NContainer::TCMap<NStr::CStr, CConfigurationType> &o_Configurations) const;
		void fp_ParseConditionsAndDebug
			(
				CBuildSystemRegistry &_Registry
				, NStorage::TCSharedPointer<CCondition> const &_pConditions
				, EPropertyFlag &o_Flags
				, NStorage::TCSharedPointer<CCondition> &o_pConditions
			) const
		;
		void fp_ParsePropertyValue
			(
				CPropertyKeyReference const &_PropertyKey
				, CEntity &o_Entity
				, CBuildSystemRegistry &_Registry
				, NStorage::TCSharedPointer<CCondition> const &_pConditions
			) const
		;
		void fp_ParsePropertyValueDefines
			(
				CPropertyKeyReference const &_PropertyKey
				, CEntity &o_Entity
				, CBuildSystemRegistry &_Registry
				, NStorage::TCSharedPointer<CCondition> const &_pConditions
			) const
		;
		CTypeWithPosition const *fp_GetTypeForProperty(CEvalPropertyValueContext &_Context, CPropertyKeyReference const &_VariableName) const;
		CTypeWithPosition const *fp_GetUserTypeWithPositionForProperty(CEvalPropertyValueContext &_Context, NStr::CStr const &_UserType) const;

		enum EDoesValueConformToTypeFlag
		{
			EDoesValueConformToTypeFlag_None = 0
			, EDoesValueConformToTypeFlag_CanApplyDefault = DMibBit(0)
			, EDoesValueConformToTypeFlag_ConvertFromString = DMibBit(1)
		};
		bool fp_DoesValueConformToType
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CType const &_Type
				, CFilePosition const &_TypePosition
				, NEncoding::CEJSONSorted &o_Value
				, EDoesValueConformToTypeFlag _Flags
				, CTypeConformError *o_pError = nullptr
				, NFunction::TCFunctionNoAlloc<NStr::CStr ()> const *_pGetErrorContext = nullptr
			) const
		;
		void fp_CheckValueConformToPropertyType
			(
				CEvalPropertyValueContext &_Context
				, CPropertyKeyReference const &_Property
				, NEncoding::CEJSONSorted &o_Value
				, CFilePosition const &_Position
				, EDoesValueConformToTypeFlag _Flags
			) const
		;
		void fp_CheckValueConformToType
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CType const &_Type
				, NEncoding::CEJSONSorted &o_Value
				, CFilePosition const &_Position
				, CFilePosition const &_TypePosition
				, NFunction::TCFunctionNoAlloc<NStr::CStr ()> const &_fGetErrorContext
				, EDoesValueConformToTypeFlag _Flags
			) const
		;
		CBuildSystemSyntax::CType const *fp_GetCanonicalType
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CType const *_pType
				, CFilePosition const *&o_pTypePosition
			) const
		;
		CBuildSystemSyntax::CType const *fp_ApplyAccessorsToType
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CType const *_pType
				, NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors
				, CFilePosition const *o_pTypePosition
			) const
		;
		void fp_ApplyAccessors
			(
				CEvalPropertyValueContext &_Context
				, NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors
				, NFunction::TCFunctionNoAlloc<void (NStr::CStr const &_Member, bool _bOptionalChain)> const &_fApplyMemberName
				, NFunction::TCFunctionNoAlloc<void (int64 _Index, bool _bOptionalChain)> const &_fApplyArrayIndex
			) const
		;
		void fp_ParseProperty
			(
				CEntity &o_Entity
				, CBuildSystemSyntax::CIdentifier const &_Identifier
				, CBuildSystemRegistry &_Registry
				, NStorage::TCSharedPointer<CCondition> const &_pConditions
				, bool _bDefines
			) const
		;
		CEntity *fp_ParseEntity(CEntity &_Parent, CBuildSystemSyntax::CIdentifier const &_Identifier, CBuildSystemRegistry &_Registry) const;
		[[noreturn]] static void fsp_ThrowError(CFilePosition const &_Position, NStr::CStr const &_Error);
		[[noreturn]] static void fsp_ThrowError(CFilePosition const &_Position, NStr::CStr const &_Error, NContainer::TCVector<CBuildSystemError> const &_Errors);
		[[noreturn]] static void fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, NStr::CStr const &_Error);
		[[noreturn]] static void fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, NStr::CStr const &_Error, NContainer::TCVector<CBuildSystemError> const &_Errors);
		[[noreturn]] static void fsp_ThrowError(CBuildSystemRegistry const &_Registry, NStr::CStr const &_Error);

		static EPropertyFlag fsp_ParseDebugFlags(CFilePosition const &_Position, NStr::CStr const &_String);

		enum EHandleKeyFlag
		{
			EHandleKeyFlag_None = 0
			, EHandleKeyFlag_AllowPropertyType = DMibBit(0)
		};

		void fp_HandleKey
			(
				CBuildSystemRegistry &_Registry
				, NFunction::TCFunction<void (CBuildSystemSyntax::CKeyPrefixOperator const &_PrefixOprator)> const &_fOnPrefix
				, NFunction::TCFunction<void (CBuildSystemSyntax::CIdentifier const &_Identifier)> const &_fOnIdentifier
				, ch8 const *_pTypeError
				, EHandleKeyFlag _Flags
			) const
		;
		void fp_ParseData(CEntity &_RootEntity, CBuildSystemRegistry &_Registry, NContainer::TCMap<NStr::CStr, CConfigurationType> *_pConfigurations) const;
		bool fpr_EvalCondition
			(
				CEntity &_Context
				, CEntity &_OriginalContext
				, CCondition const &_Condition
				, CEvaluationContext &_EvalContext
				, mint _TraceDepth
				, CEvalPropertyValueContext const *_pParentContext
				, CBuildSystemUniquePositions *o_pPositions
			) const;
		bool fp_EvalConditionSubject
			(
				CEntity &_Context
				, CEntity &_OriginalContext
				, CCondition const &_Condition
				, CEvaluationContext &_EvalContext
				, mint _TraceDepth
				, EConditionType _ConditionType
				, CEvalPropertyValueContext const *_pParentContext
				, CBuildSystemUniquePositions *o_pPositions
			) const
		;
		CValuePotentiallyByRef fp_EvaluateEntityProperty
			(
				CEntity &_Entity
				, CEntity &_OriginalEntity
				, CPropertyKeyReference const &_Key
				, CEvaluationContext &_EvalContext
				, CBuildSystemPropertyInfo &o_PropertyInfo
				, CFilePosition const &_FallbackPosition
				, CEvalPropertyValueContext const *_pParentContext
				, bool _bMoveCache
			) const
		;
		CValuePotentiallyByRef fp_EvaluateRootValue
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CRootValue const &_Value
				, CPropertyKey const *_pProperty
				, bool &o_bTypeAlreadyChecked
			 ) const
		;

		struct CWritePropertyContext
		{
			CPropertyKey const &m_Property;
			bool &m_bTypeAlreadyChecked;
			NEncoding::CEJSONSorted *m_pWriteDestination = nullptr;
			NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const *m_pAccessors = nullptr;
		};

		CValuePotentiallyByRef fp_EvaluatePropertyValue(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CValue const &_Value, CWritePropertyContext *_pWriteContext) const;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueObject(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CObject const &_Value) const;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueArray(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CArray const &_Value) const;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueWildcardString(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CWildcardString const &_Value) const;
		NStr::CStr fp_EvaluatePropertyValueEvalString(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CEvalString const &_Value) const;
		CValuePotentiallyByRef fp_EvaluatePropertyValueExpression(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CExpression const &_Value) const;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueExpressionAppend(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CExpressionAppend const &_Value) const;
		CValuePotentiallyByRef fp_EvaluatePropertyValueTernary(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CTernary const &_Value) const;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValuePrefixOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CPrefixOperator const &_Value) const;
		CValuePotentiallyByRef fp_EvaluatePropertyValueBinaryOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CBinaryOperator const &_Value) const;
		bool fsp_CompareValueRecursive
			(
				NEncoding::CEJSONSorted const &_Left
				, NEncoding::CEJSONSorted const &_Right
				, EConditionType _ConditionType
				, NFunction::TCFunctionNoAlloc<void (NStr::CStr const &_Error)> const &_fOnError
			) const
		;

		void fp_EvaluatePropertyValueFunctionCallCollectParams
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CFunctionCall const &_Value
				, CBuildSystemSyntax::CFunctionType const &_FunctionType
				, NFunction::TCFunctionNoAlloc
				<
					void (NEncoding::CEJSONSorted &&_Param, CBuildSystemSyntax::CFunctionParameter const &_FunctionParam, bool _bEllipsis, bool _bAddDefault)
				> const &_fConsumeParam
				, CFilePosition const &_TypePosition
			) const
		;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueFunctionCallBuiltin
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CFunctionCall const &_Value
				, CBuiltinFunction const *_pFunction
			) const
		;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueFunctionCall(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CFunctionCall const &_Value, bool _bMoveCache) const;
		CValuePotentiallyByRef fp_EvaluatePropertyValueParam(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CParam const &_Value) const;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueIdentifierReference(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CIdentifierReference const &_Value) const;
		CValuePotentiallyByRef fp_EvaluatePropertyValueIdentifier(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CIdentifier const &_Value, bool _bMoveCache) const;
		CValuePotentiallyByRef fp_EvaluatePropertyValueJSONAccessor(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CJSONAccessor const &_Value) const;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::COperator const &_Value, CWritePropertyContext *_pWriteContext) const;
		NEncoding::CEJSONSorted fp_EvaluatePropertyValueDefine(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CDefine const &_Value) const;
		CValuePotentiallyByRef fp_EvaluatePropertyValueAccessors
			(
				CEvalPropertyValueContext &_Context
				, CValuePotentiallyByRef &&_Value
				, NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors
			) const
		;

		void fpr_EvaluateData(CEntity &_Entity) const;
		void fpr_EvaluateData(CEntity &_Entity, NContainer::TCSet<CEntity *> &o_Deleted) const;
		void fp_EvaluateAllProperties(CEntity &_Entity, bool _bDoTypeChecks) const;
		NContainer::TCVector<NContainer::TCVector<CConfigurationTuple>> fp_EvaluateConfigurationTuples(NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const &_InitialValues) const;
		CEntity const *fp_EvaluateData
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const &_InitialValues
				, CEntity const *_pStartEntity
				, bool _bCopyTree
				, bool _bAllChildren
			) const
		;
		void fp_EvalGlobalWorkspaces
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<NStr::CStr, CEntityMutablePointer> &_Targets
			) const
		;
		NContainer::TCMap<CEntity *, NContainer::TCSet<NStr::CStr>> fp_EvaluateTargetsInWorkspace
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<NStr::CStr, CEntityMutablePointer> const &_Targets
				, CEntity &_Workspace
			) const
		;
		CEntity *fpr_FindChildTarget(CEntity &_Entity, CEntityKey const &_EntityKey) const;
		void fp_EvaluateTarget
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<NStr::CStr, CEntityMutablePointer> const &_Targets
				, NContainer::TCLinkedList<CEntity *> &_ToEval
				, CEntity &_Entity
				, NContainer::TCSet<NStr::CStr> &_AlreadyAddedGroups
			) const
		;
		void fp_UpdateDependenciesNames(CEntity *_pTargetOuterEntity) const;
		void fp_EvaluateWorkspace(CBuildSystemData &_Destination, CEntity &_Entity) const;
		void fp_UsedExternal(CPropertyKeyReference const &_PropertyKey) const;
		bool fp_GenerateFiles
			(
				CBuildSystemData &_BuildSystemData
				, CEntity &_Entity
				, bool _bRecursive
				, EEntityType _Type
				, CPropertyKeyReference const *_pConditionalProperty
				, bool _bConditional
			) const
		;
		CEntity *fp_AddEntity
			(
				CEntity const &_Entity
				, CEntity &_ParentEntity
				, CEntityKey const &_NewKey
				, CEntity *_pInsertAfter
				, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pExtraProperties
			) const
		;
		CEntity *fp_ExpandEntity(CEntity &_Entity, CEntity &_ParentEntity, NContainer::TCVector<CEntity *> *o_pCreated, bool &o_bChanged) const;
		void fp_ExpandImport(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const;
		CBuildSystemData::CImportData fp_ExpandImportCMake(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const;
		CBuildSystemData::CImportData fp_ExpandImportCMake_FromGeneratedDirectory
			(
				CEntity &_Entity
				, CBuildSystemData &_BuildSystemData
				, NStr::CStr const &_Directory
			) const
		;
		void fp_TracePropertyEval(bool _bSuccess, CEntity const &_Entity, CPropertyKey const &_PropertyKey, CProperty const &_Property, NEncoding::CEJSONSorted const &_Value) const;

		NConcurrency::TCFuture<ERetry> fp_HandleRepositories(NContainer::TCMap<CPropertyKey, NEncoding::CEJSONSorted> const &_Values);

		void fp_SaveEnvironment();

		void fp_RegisterBuiltinFunctions();
		void fp_RegisterBuiltinFunctions_Execute();
		void fp_RegisterBuiltinFunctions_List();
		void fp_RegisterBuiltinFunctions_Windows();
		void fp_RegisterBuiltinFunctions_File();
		void fp_RegisterBuiltinFunctions_Misc();
		void fp_RegisterBuiltinFunctions_Color();
		void fp_RegisterBuiltinFunctions_Compare();
		void fp_RegisterBuiltinFunctions_Path();
		void fp_RegisterBuiltinFunctions_Property();
		void fp_RegisterBuiltinFunctions_String();

		void fp_RegisterBuiltinVariables();

		NEncoding::CEJSONSorted fp_BuiltinFunction_FindFiles(CEvalPropertyValueContext &_Context, NContainer::TCVector<NEncoding::CEJSONSorted> &&_Params, EFindFilesFunctionType _Function) const;
		NEncoding::CEJSONSorted fp_BuiltinFunction_GetProperty
			(
				CEvalPropertyValueContext &_Context
				, NContainer::TCVector<NEncoding::CEJSONSorted> &&_Params
				, EBuiltinFunctionGetProperty _Function
				, CEvalPropertyValueContext const *_pParentContext
			) const
		;

		CBuildSystemSyntax::CIdentifier fp_IdentifierFromJson(CBuildSystem::CEvalPropertyValueContext &_Context, NEncoding::CEJSONSorted const &_Value, bool _bSupportEntityType) const;
		NEncoding::CEJSONSorted fp_BuiltinFunction_OverridingType
			(
				CBuildSystem::CEvalPropertyValueContext &_Context
				, NContainer::TCVector<NEncoding::CEJSONSorted> &&_Params
				, bool _bPositions
				, bool _bType
			) const
		;

		struct CCachedFile
		{
			NThread::CLowLevelLock m_Lock;
			NStr::CStr m_Contents;
			bool m_bRead = false;
		};

		mutable NThread::CLowLevelLock mp_CachedFilesLock;
		mutable NContainer::TCMap<NStr::CStr, CCachedFile> mp_CachedFiles;

		struct CExecuteCommand
		{
			NThread::CLowLevelLock m_Lock;
			NStr::CStr m_Executable;
			NContainer::TCVector<NStr::CStr> m_FunctionParams;
			NContainer::TCVector<NStr::CStr> m_Inputs;
			NStr::CStr m_Result;
			NStr::CStr m_Error;
			bool m_bInitialized = false;
		};

		mutable NThread::CLowLevelLock mp_ExecuteCommandsLock;
		mutable NContainer::TCMap<NStr::CStr, CExecuteCommand> mp_ExecuteCommands;

		NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> mp_fOutputConsole;

		NContainer::TCMap<NStr::CStr, CBuiltinFunction> mp_BuiltinFunctions;
		mutable NContainer::TCMap<CPropertyKey, CTypeWithPosition> mp_BuiltinVariablesDefinitions;
		mutable NContainer::TCSet<CTypeWithPosition const *> mp_DefaultedBuiltinVariablesDefinitions[EPropertyType_Max];
		CTypeWithPosition mp_TypeForPropertyAny;

		CGenerateOptions mp_GenerateOptions;
		bool mp_bNoReconcileOptions = false;
		bool mp_bSingleThreaded = false;

		align_cacheline mutable NThread::CMutualManyRead mp_SourceFilesLock;
		mutable NContainer::TCSet<NStr::CStr> mp_SourceFiles;
		CFindCache mp_FindCache;
		CBuildSystemRegistry mp_Registry;

		CBuildSystemData mp_Data;
		CProperty mp_ExternalProperty[EPropertyType_Max];

		mutable CStringCache mp_StringCache;

		mutable NStorage::TCPointer<ICGeneratorInterface> mp_GeneratorInterface;

		mutable CUserSettingsState mp_UserSettingsLocal;
		mutable CUserSettingsState mp_UserSettingsGlobal;

		align_cacheline mutable NThread::CMutualManyRead mp_UsedExternalsLock;
		mutable NContainer::TCSet<CPropertyKey> mp_UsedExternals;

		NEncoding::CEJSONSorted mp_FileLocation;
		NEncoding::CEJSONSorted mp_BaseDir;
		NStr::CStr mp_OutputDir;

		NTime::CTime mp_Now;
		NTime::CTime mp_NowUTC;

		NStr::CStr mp_UserSettingsFileLocal;
		NStr::CStr mp_UserSettingsFileGlobal;
		NEncoding::CEJSONSorted mp_FileLocationFile;
		NEncoding::CEJSONSorted mp_GeneratorStateFileName;
		NEncoding::CEJSONSorted mp_MToolExe;
		NEncoding::CEJSONSorted mp_CMakeRoot;
		NEncoding::CEJSONSorted mp_MalterlibExe;

		CSystemEnvironment mp_SaveEnvironment;
		CSystemEnvironment mp_Environment;
		NStr::CStr mp_GenerateWorkspace;
		mutable NAtomic::TCAtomic<bool> mp_FileChanged;

		align_cacheline mutable NThread::CMutual mp_GeneratedFilesLock;
		mutable NContainer::TCMap<NStr::CStr, CGeneratedFile> mp_GeneratedFiles;

		align_cacheline mutable NThread::CMutual mp_CMakeGenerateLock;
		struct CCMakeGenerateState
		{
			NThread::CMutual m_Lock;
			bool m_bTried = false;
		};
		mutable NContainer::TCMap<NStr::CStr, CCMakeGenerateState> mp_CMakeGenerateState;
		mutable NContainer::TCMap<NStr::CStr, NStr::CStr> mp_CMakeGenerated;
		mutable NContainer::TCMap<NStr::CStr, NStr::CStr> mp_CMakeGeneratedContents;
		mutable NAtomic::TCAtomic<mint> mp_LogSequence;

		NFile::EFileAttrib mp_SupportedAttributes = NFile::CFile::fs_GetSupportedAttributes();
		NFile::EFileAttrib mp_ValidAttributes = NFile::CFile::fs_GetValidAttributes();
		bool mp_bDebugFileLocks = fg_GetSys()->f_GetEnvironmentVariable("MalterlibBuildSystemDebugFileLocks", "false") == gc_ConstString_true.m_String;
		NCommandLine::EAnsiEncodingFlag mp_AnsiFlags = NCommandLine::EAnsiEncodingFlag_None;
		bool mp_bEnablePositions = false;
		bool mp_bEnableValues = false;

		NStorage::TCSharedPointer<NAtomic::TCAtomic<bool>> mp_pCancelled;
	};
}

#include "Malterlib_BuildSystem.hpp"
#include "Malterlib_BuildSystem_Property.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NBuildSystem;
#endif

#endif

#include "Malterlib_BuildSystem_ValuePotentiallyByRef.hpp"
#include "Malterlib_BuildSystem_FilePosition.hpp"
#include "Malterlib_BuildSystem_GeneratorSettings.h"
