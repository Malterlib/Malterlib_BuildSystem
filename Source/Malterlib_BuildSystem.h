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

namespace NMib::NBuildSystem
{
	class CBuildSystem
	{
	public:
		CBuildSystem(NCommandLine::EAnsiEncodingFlag _AnsiFlags, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole);
		CBuildSystem(CBuildSystem const &) = delete;
		CBuildSystem(CBuildSystem &&) = delete;

		enum ERetry
		{
			ERetry_None
			, ERetry_Again
			, ERetry_Again_NoReconcileOptions
			, ERetry_Relaunch
			, ERetry_Relaunch_NoReconcileOptions
		};

		struct CRepoFilter
		{
			static CRepoFilter fs_ParseParams(NEncoding::CEJSON const &_Params);

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
			, ERepoStatusFlag_NeedActionOnPush = DMibBit(7)
			, ERepoStatusFlag_NonDefaultToAll = DMibBit(8)
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
			NEncoding::CEJSON const *m_pExplodedValue;
			NEncoding::CEJSON m_Value;
		};

		struct CEvaluationContext
		{
			inline_always CEvaluationContext(CEvaluatedProperties *_pEvaluatedProperties);

			NContainer::TCMap<CPropertyKey, NContainer::TCSet<CEntity const *>> m_EvalStack;
			CEvaluatedProperties *m_pEvaluatedProperties = nullptr;
			NContainer::TCLinkedList<CExplodeStackEntry> m_ExplodeListStack;
			CBuildSystemSyntax::CFunctionType const *m_pCallingFunction = nullptr;
			NContainer::TCMap<CPropertyKey, CTypeWithPosition> m_OverriddenTypes;
		};

		struct CEvalPropertyValueContext
		{
			CEntity &m_Context;
			CEntity &m_OriginalContext;
			CFilePosition const &m_Position;
			CEvaluationContext &m_EvalContext;
			CEvalPropertyValueContext const *m_pParentContext;
		};

		using FEvalPropertyFunction = NFunction::TCFunction
			<
				NEncoding::CEJSON (CBuildSystem const &_This, CEvalPropertyValueContext &_Context, NContainer::TCVector<NEncoding::CEJSON> &&_Params)
			>
		;

		struct CBuiltinFunction
		{
			CBuildSystemSyntax::CFunctionType m_Type;
			FEvalPropertyFunction m_fFunction;
		};

		void f_SetGeneratorInterface(ICGeneratorInterface *_pInterface) const;
		void f_GenerateBuildSystem
			(
				NContainer::TCMap<CConfiguration, NStorage::TCUniquePointer<CConfiguraitonData>> &o_Configurations
				, NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_Values
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
		NStr::CStr f_GetBaseDir() const;
		bool f_AddGeneratedFile(NStr::CStr const &_File, NStr::CStr const &_Data, NStr::CStr const &_Workspace, bool &_bWasCreated, EGeneratedFileFlag _Flags = EGeneratedFileFlag_None) const;
		void f_GenerateGlobalFiles(CBuildSystemData &_BuildSystemData) const;
		void f_GenerateWorkspaceFiles(CBuildSystemData &_BuildSystemData, CEntity &_Target) const;
		void f_GenerateTargetFiles(CBuildSystemData &_BuildSystemData, CEntity &_Target) const;
		void f_ExpandRepositoryEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandCreateTemplateEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandGlobalEntities(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandDynamicImports(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandGlobalTargetsAndWorkspaces(CBuildSystemData &_BuildSystemData) const;
		void f_ExpandTargetDependenciesBackup(CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const;
		void f_ExpandTargetDependencies(CWorkspaceInfo &_Workspace, CBuildSystemData &_BuildSystemData, CEntity const &_Target, CDependenciesBackup &o_Backup) const;
		void f_ExpandTargetGroups(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_ExpandTargetFiles(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_ExpandWorkspaceTargets(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		void f_ExpandWorkspaceEntities(CBuildSystemData &_BuildSystemData, CEntity const &_Target) const;
		bool f_WriteFile(NContainer::CByteVector const &_FileData, NStr::CStr const &_File, NFile::EFileAttrib _AddAttribs = NFile::EFileAttrib_None) const;
		void f_SetFileChanged(NStr::CStr const &_File) const;
		NContainer::TCSet<NStr::CStr> f_GetSourceFiles() const;
		CEntity const *f_EvaluateData
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_InitialValues
				, CEntity const *_pStartEntity // = nullptr
				, bool _bCopyTree // = true
			) const
		;
		CEntity const *f_EvaluateDataMain
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_InitialValues
			) const
		;
		void f_EvaluateAllGeneratorSettings(CEntity &_Entity) const;
		void f_EvalGlobalWorkspaces
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<NStr::CStr, CEntityMutablePointer> &_Targets
			) const
		;
		void f_EvaluateTargetsInWorkspace
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
			) const
		;
		NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> f_GetExternalValues(CEntity const &_Entity) const;
		NEncoding::CEJSON f_EvaluateEntityProperty(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property) const;
		NEncoding::CEJSON f_EvaluateEntityPropertyObject
			(
				CEntity &_Entity
				, EPropertyType _Type
				, NStr::CStr const &_Property
				, NStorage::TCOptional<NEncoding::CEJSON> const &_Default = {}
			) const
		;
		NStr::CStr f_EvaluateEntityPropertyString(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<NStr::CStr> const &_Default = {}) const;
		NStr::CStr f_EvaluateEntityPropertyString
			(
				CEntity &_Entity
				, EPropertyType _Type
				, NStr::CStr const &_Property
				, CProperty const *&_pFromProperty
				, NStorage::TCOptional<NStr::CStr> const &_Default = {}
			) const
		;
		bool f_EvaluateEntityPropertyTryBool(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<bool> const &_Default = {}) const;
		bool f_EvaluateEntityPropertyBool(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<bool> const &_Default = {}) const;
		bool f_EvaluateEntityPropertyBool
			(
				CEntity &_Entity
				, EPropertyType _Type
				, NStr::CStr const &_Property
				, CProperty const *&_pFromProperty
				, NStorage::TCOptional<bool> const &_Default = {}
			) const
		;
		fp64 f_EvaluateEntityPropertyFloat(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<fp64> const &_Default = {}) const;
		int64 f_EvaluateEntityPropertyInteger(CEntity &_Entity, EPropertyType _Type, NStr::CStr const &_Property, NStorage::TCOptional<int64> const &_Default = {}) const;
		NContainer::TCVector<NStr::CStr> f_EvaluateEntityPropertyStringArray
			(
				CEntity &_Entity
				, EPropertyType _Type
				, NStr::CStr const &_Property
				, NStorage::TCOptional<NContainer::TCVector<NStr::CStr>> const &_Default = {}
			) const
		;
		NEncoding::CEJSON f_EvaluateEntityProperty
			(
				CEntity &_Entity
				, EPropertyType _Type
				, NStr::CStr const &_Property
				, CProperty const *&_pFromProperty
			) const
		;
		NEncoding::CEJSON f_EvaluateEntityPropertyUncached
			(
				CEntity &_Entity
				, EPropertyType _Type
				, NStr::CStr const &_Property
				, CProperty const *&_pFromProperty
				, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties = nullptr
			) const
		;
		NStr::CStr  f_EvaluateEntityPropertyUncachedString
			(
				CEntity &_Entity
				, EPropertyType _Type
				, NStr::CStr const &_Property
				, CProperty const *&_pFromProperty
				, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties = nullptr
				, NStorage::TCOptional<NStr::CStr> const &_Default = {}
			) const
		;
		NContainer::TCVector<NStr::CStr> f_EvaluateEntityPropertyUncachedStringArray
			(
				CEntity &_Entity
				, EPropertyType _Type
				, NStr::CStr const &_Property
				, CProperty const *&_pFromProperty
				, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pInitialProperties = nullptr
				, NStorage::TCOptional<NContainer::TCVector<NStr::CStr>> const &_Default = {}
			) const
		;
		void f_InitEntityForEvaluation(CEntity &_Entity, NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_InitialValues) const;
		void f_InitEntityForEvaluationNoEnv(CEntity &_Entity, NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_InitialValues, EEvaluatedPropertyType _Type) const;
		NEncoding::CEJSON f_GetExternalProperty(CEntity &_Entity, CPropertyKey const &_Key) const;
		bool f_AddExternalProperty(CEntity &_Entity, CPropertyKey const &_Key, NEncoding::CEJSON &&_Value) const;
		NContainer::TCVector<NContainer::TCVector<CConfigurationTuple>> f_EvaluateConfigurationTuples(NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_InitialValues) const;
		bool f_EvalCondition(CEntity &_Context, CCondition const &_Condition, bool _bTrace = false) const;
		[[noreturn]] static void fs_ThrowError(CFilePosition const &_Position, NStr::CStr const &_Error);
		[[noreturn]] static void fs_ThrowError(CFilePosition const &_Position, NStr::CStr const &_Error, NContainer::TCVector<CBuildSystemError> const &_Errors);
		[[noreturn]] static void fs_ThrowError(CBuildSystemRegistry const &_Registry, NStr::CStr const &_Error);
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
		void f_CheckPropertyTypeValue
			(
				EPropertyType _PropType
				, NStr::CStr const &_Property
				, NEncoding::CEJSON const &_Value
				, NEncoding::EEJSONType _ExpectedType
				, CFilePosition const &_Position
				, bool _bOptional
			) const
		;

		NStr::CStr f_GetEnvironmentVariable(NStr::CStr const &_Name, NStr::CStr const &_Default = {}, bool *o_pExists = nullptr) const;

		void f_NoReconcileOptions();

		static ERetry fs_RunBuildSystem
			(
				NFunction::TCFunction<CBuildSystem::ERetry (CBuildSystem &_BuildSystem)> &&_fCommand
				, NCommandLine::EAnsiEncodingFlag _AnsiFlags
				, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &_fOutputConsole
			)
		;

		bool f_Action_Generate(CGenerateOptions const &_GenerateOptions, ERetry &o_Retry);
		ERetry f_Action_Create(CGenerateOptions const &_GenerateOptions);

		ERetry f_Action_Repository_Update(CGenerateOptions const &_GenerateOptions);
		ERetry f_Action_Repository_Status(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoStatusFlag _Flags);
		NConcurrency::TCFuture<ERetry> f_Action_Repository_Status_Async(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoStatusFlag _Flags);
		ERetry f_Action_Repository_ForEachRepo(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, bool _bParallell, NContainer::TCVector<NStr::CStr> const &_Params);

		ERetry f_Action_Repository_Branch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, NStr::CStr const &_Branch, ERepoBranchFlag _Flags);
		ERetry f_Action_Repository_Unbranch(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, ERepoBranchFlag _Flags);

		ERetry f_Action_Repository_CleanupBranches
			(
				CGenerateOptions const &_GenerateOptions
				, CRepoFilter const &_Filter
				, ERepoCleanupBranchesFlag _Flags
				, NContainer::TCVector<NStr::CStr> const &_Branches
			)
		;
		ERetry f_Action_Repository_CleanupTags
			(
				CGenerateOptions const &_GenerateOptions
				, CRepoFilter const &_Filter
				, ERepoCleanupTagsFlag _Flags
				, NContainer::TCVector<NStr::CStr> const &_Tags
			)
		;
		ERetry f_Action_Repository_Push(CGenerateOptions const &_GenerateOptions, CRepoFilter const &_Filter, NContainer::TCVector<NStr::CStr> const &_Remotes, ERepoPushFlag _PushFlags);
		ERetry f_Action_Repository_ListCommits
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
				, NConcurrency::CDistributedAppCommandLineClient const &_CommandLineClient
			)
		;

		NCommandLine::EAnsiEncodingFlag f_AnsiFlags() const;

		void f_RegisterFunctions(NContainer::TCMap<NStr::CStr, CBuiltinFunction> &&_Functions);
		void f_RegisterBuiltinVariables(NContainer::TCMap<CPropertyKey, CBuildSystemSyntax::CType> &&_Variables) const;
		void f_OutputConsole(NStr::CStr const &_Output, bool _bError = false) const;
		NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> const &f_OutputConsoleFunctor() const;

		static void fs_AddEntityVariableDefinition
			(
				CBuildSystem const *_pBuildSystem
				, CEntity &_DestinationEntity
				, CPropertyKey const &_VariableName
				, CBuildSystemSyntax::CType const &_Type
				, CFilePosition const &_Position
				, NStr::CStr const &_Whitespace
				, NStorage::TCSharedPointer<CCondition> const &_pConditions
			)
		;
		static void fs_AddEntityUserType
			(
				CEntity &_DestinationEntity
				, NStr::CStr const &_TypeName
				, CBuildSystemSyntax::CType const &_Type
				, CFilePosition const &_Position
			)
		;
		CBuildSystemSyntax::CType const *f_GetCanonicalDefaultedType
			(
				CEntity const &_Entity
				, CBuildSystemSyntax::CType const *_pType
				, CFilePosition &o_TypePosition
			) const
		;

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
			CBuildSystemRegistry m_Registry;
			NContainer::TCSet<CPropertyKey> m_Defined;
			NContainer::TCMap<CPropertyKey, CBuildSystemRegistry *> m_Sections;
			NContainer::TCMap<CPropertyKey, CBuildSystemRegistry const *> m_Properties;

			void f_Parse();
			CBuildSystemRegistry *f_GetSection(CPropertyKey const &_Section);
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
			NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> m_GeneratorValues;
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
			, EBuiltinFunctionGetProperty_GetWithType
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
		ERetry fp_GeneratePrepare(CGenerateOptions const &_GenerateOptions, CGenerateEphemeralState &_GenerateState, NFunction::TCFunction<bool ()> &&_fPreParse);

		void fp_ParseConfigurationConditions(CBuildSystemRegistry &_Registry, CBuildSystemConfiguration &_Configuration) const;
		void fp_ParseConfigurationType(NStr::CStr const &_Name, CBuildSystemRegistry &_Registry, NContainer::TCMap<NStr::CStr, CConfigurationType> &o_Configurations) const;
		void fp_ParsePropertyValue
			(
				EPropertyType _Type
				, NStr::CStr const &_PropertyName
				, CEntity *o_pEntity
				, CBuildSystemRegistry &_Registry
				, CCondition const *_pParentConditions
			) const
		;
		void fp_ParsePropertyValueDefines
			(
				EPropertyType _Type
				, NStr::CStr const &_PropertyName
				, CEntity *o_pEntity
				, CBuildSystemRegistry &_Registry
				, NStorage::TCSharedPointer<CCondition> const &_pConditions
			) const
		;
		CTypeWithPosition const *fp_GetTypeForProperty(CBuildSystem::CEvalPropertyValueContext &_Context, CPropertyKey const &_VariableName) const;
		CTypeWithPosition const *fp_GetUserTypeWithPositionForProperty(CEntity const &_Entity, NStr::CStr const &_UserType) const;

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
				, NEncoding::CEJSON &o_Value
				, EDoesValueConformToTypeFlag _Flags
				, CTypeConformError *o_pError = nullptr
			) const
		;
		void fp_CheckValueConformToPropertyType
			(
				CEvalPropertyValueContext &_Context
				, CPropertyKey const &_Property
				, NEncoding::CEJSON &o_Value
				, CFilePosition const &_Position
				, EDoesValueConformToTypeFlag _Flags
			) const
		;
		void fp_CheckValueConformToType
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CType const &_Type
				, NEncoding::CEJSON &o_Value
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
				, CFilePosition &o_TypePosition
			) const
		;
		CBuildSystemSyntax::CType const *fp_ApplyAccessorsToType
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CType const *_pType
				, NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors
				, CFilePosition &o_TypePosition
			) const
		;
		void fp_ApplyAccessors
			(
				CEvalPropertyValueContext &_Context
				, NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors
				, NFunction::TCFunctionNoAlloc<void (NStr::CStr const &_Member)> const &_fApplyMemberName
				, NFunction::TCFunctionNoAlloc<void (int64 _Index)> const &_fApplyArrayIndex
			) const
		;
		void fp_ParseProperty
			(
				CEntity *o_pEntitiy
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
			) const
		;
		NEncoding::CEJSON fp_EvaluateEntityProperty
			(
				CEntity &_Entity
				, CEntity &_OriginalEntity
				, CPropertyKey const &_Key
				, CEvaluationContext &_EvalContext
				, CProperty const *&_pFromProperty
				, CFilePosition const &_FallbackPosition
				, CEvalPropertyValueContext const *_pParentContext
			) const
		;
		NEncoding::CEJSON fp_EvaluatePropertyValue(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CRootValue const &_Value, CPropertyKey const *_pProperty) const;

		struct CWritePropertyContext
		{
			CPropertyKey const &m_Property;
			NEncoding::CEJSON *m_pWriteDestination = nullptr;
			NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const *m_pAccessors = nullptr;
		};
		NEncoding::CEJSON fp_EvaluatePropertyValue(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CValue const &_Value, CWritePropertyContext *_pWriteContext) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueObject(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CObject const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueArray(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CArray const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueWildcardString(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CWildcardString const &_Value) const;
		NStr::CStr fp_EvaluatePropertyValueEvalString(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CEvalString const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueExpression(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CExpression const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueExpressionAppend(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CExpressionAppend const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueTernary(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CTernary const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValuePrefixOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CPrefixOperator const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueBinaryOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CBinaryOperator const &_Value) const;
		bool fsp_CompareValueRecursive
			(
				NEncoding::CEJSON const &_Left
				, NEncoding::CEJSON const &_Right
				, EConditionType _ConditionType
				, NFunction::TCFunctionNoAlloc<void (NStr::CStr const &_Error)> const &_fOnError
			) const
		;

		void fp_EvaluatePropertyValueFunctionCallCollectParams
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CFunctionCall const &_Value
				, CBuildSystemSyntax::CFunctionType const &_FunctionType
				, NFunction::TCFunctionNoAlloc<void (NEncoding::CEJSON &&_Param, CBuildSystemSyntax::CFunctionParameter const &_FunctionParam, bool _bEllipsis)> const &_fConsumeParam
				, CFilePosition const &_TypePosition
			) const
		;
		NEncoding::CEJSON fp_EvaluatePropertyValueFunctionCallBuiltin
			(
				CEvalPropertyValueContext &_Context
				, CBuildSystemSyntax::CFunctionCall const &_Value
				, CBuiltinFunction const *_pFunction
			) const
		;
		NEncoding::CEJSON fp_EvaluatePropertyValueFunctionCall(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CFunctionCall const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueParam(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CParam const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueIdentifier(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CIdentifier const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueJSONAccessor(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CJSONAccessor const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueOperator(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::COperator const &_Value, CWritePropertyContext *_pWriteContext) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueDefine(CEvalPropertyValueContext &_Context, CBuildSystemSyntax::CDefine const &_Value) const;
		NEncoding::CEJSON fp_EvaluatePropertyValueAccessors
			(
				CEvalPropertyValueContext &_Context
				, NEncoding::CEJSON &&_Value
				, NContainer::TCVector<CBuildSystemSyntax::CJSONAccessorEntry> const &_Accessors
			) const
		;

		void fpr_EvaluateData(CEntity &_Entity) const;
		void fpr_EvaluateData(CEntity &_Entity, NContainer::TCSet<CEntity *> &o_Deleted) const;
		void fpr_EvaluateAllGeneratorSettings(CEntity &_Entity) const;
		void fp_EvaluateAllProperties(CEntity &_Entity, bool _bDoTypeChecks) const;
		NContainer::TCVector<NContainer::TCVector<CConfigurationTuple>> fp_EvaluateConfigurationTuples(NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_InitialValues) const;
		CEntity const *fp_EvaluateData
			(
				CBuildSystemData &_Destination
				, NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_InitialValues
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
		void fp_EvaluateTargetsInWorkspace
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
			) const
		;
		void fp_UpdateDependenciesNames(CEntity *_pTargetOuterEntity) const;
		void fp_EvaluateWorkspace(CBuildSystemData &_Destination, CEntity &_Entity) const;
		void fp_UsedExternal(NStr::CStr const &_Name) const;
		void fp_GenerateFiles(CBuildSystemData &_BuildSystemData, CEntity &_Entity, bool _bRecursive, EEntityType _Type) const;
		CEntity *fp_AddEntity
			(
				CEntity const &_Entity
				, CEntity &_ParentEntity
				, CEntityKey const &_NewKey
				, CEntity *_pInsertAfter
				, NContainer::TCMap<CPropertyKey, CEvaluatedProperty> const *_pExtraProperties
			) const
		;
		CEntity *fp_ExpandEntity(CEntity &_Entity, CEntity &_ParentEntity, NContainer::TCVector<CEntity *> *o_pCreated) const;
		void fp_ExpandImport(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const;
		CBuildSystemData::CImportData fp_ExpandImportCMake(CEntity &_Entity, CEntity &_ParentEntity, CBuildSystemData &_BuildSystemData) const;
		CBuildSystemData::CImportData fp_ExpandImportCMake_FromGeneratedDirectory
			(
				CEntity &_Entity
				, CBuildSystemData &_BuildSystemData
				, NStr::CStr const &_Directory
			) const
		;
		void fp_TracePropertyEval(bool _bSuccess, CEntity const &_Entity, CProperty const &_Property, NEncoding::CEJSON const &_Value) const;

		ERetry fp_HandleRepositories(NContainer::TCMap<CPropertyKey, NEncoding::CEJSON> const &_Values);

		void fp_SaveEnvironment();

		void fp_RegisterBuiltinFunctions();
		void fp_RegisterBuiltinFunctions_Execute();
		void fp_RegisterBuiltinFunctions_List();
		void fp_RegisterBuiltinFunctions_Windows();
		void fp_RegisterBuiltinFunctions_File();
		void fp_RegisterBuiltinFunctions_Misc();
		void fp_RegisterBuiltinFunctions_Compare();
		void fp_RegisterBuiltinFunctions_Path();
		void fp_RegisterBuiltinFunctions_Property();
		void fp_RegisterBuiltinFunctions_String();

		void fp_RegisterBuiltinVariables();

		NEncoding::CEJSON fp_BuiltinFunction_FindFiles(CEvalPropertyValueContext &_Context, NContainer::TCVector<NEncoding::CEJSON> &&_Params, EFindFilesFunctionType _Function) const;
		NEncoding::CEJSON fp_BuiltinFunction_GetProperty
			(
				CEvalPropertyValueContext &_Context
				, NContainer::TCVector<NEncoding::CEJSON> &&_Params
				, EBuiltinFunctionGetProperty _Function
				, CEvalPropertyValueContext const *_pParentContext
			) const
		;

		NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> mp_fOutputConsole;

		NContainer::TCMap<NStr::CStr, CBuiltinFunction> mp_BuiltinFunctions;
		mutable NContainer::TCMap<CPropertyKey, CTypeWithPosition> mp_BuiltinVariablesDefinitions;
		CTypeWithPosition mp_TypeForPropertyAny;

		CGenerateOptions mp_GenerateOptions;
		bool mp_bNoReconcileOptions = false;

		align_cacheline mutable NThread::CMutualManyRead mp_SourceFilesLock;
		mutable NContainer::TCSet<NStr::CStr> mp_SourceFiles;
		CFindCache mp_FindCache;
		CBuildSystemRegistry mp_Registry;

		CBuildSystemData mp_Data;
		CProperty mp_ExternalProperty[EPropertyType_Max];

		mutable NStorage::TCPointer<ICGeneratorInterface> mp_GeneratorInterface;

		mutable CUserSettingsState mp_UserSettingsLocal;
		mutable CUserSettingsState mp_UserSettingsGlobal;

		align_cacheline mutable NThread::CMutualManyRead mp_UsedExternalsLock;
		mutable NContainer::TCSet<NStr::CStr> mp_UsedExternals;

		NStr::CStr mp_FileLocation;
		NStr::CStr mp_BaseDir;
		NStr::CStr mp_OutputDir;

		NTime::CTime mp_Now;
		NTime::CTime mp_NowUTC;

		NStr::CStr mp_UserSettingsFileLocal;
		NStr::CStr mp_UserSettingsFileGlobal;
		NStr::CStr mp_FileLocationFile;
		NStr::CStr mp_GeneratorStateFileName;
		NContainer::TCMap<NStr::CStr, NStr::CStr> mp_SaveEnvironment;
		NContainer::TCMap<NStr::CStr, NStr::CStr> mp_Environment;
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

		NFile::EFileAttrib mp_SupportedAttributes = NFile::CFile::fs_GetSupportedAttributes();
		NFile::EFileAttrib mp_ValidAttributes = NFile::CFile::fs_GetValidAttributes();
		bool mp_bDebugFileLocks = fg_GetSys()->f_GetEnvironmentVariable("MalterlibBuildSystemDebugFileLocks", "false") == "true";
		NCommandLine::EAnsiEncodingFlag mp_AnsiFlags = NCommandLine::EAnsiEncodingFlag_None;
	};
}

#include "Malterlib_BuildSystem.hpp"
#include "Malterlib_BuildSystem_Property.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NBuildSystem;
#endif

#endif
