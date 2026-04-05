// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/Git/Helpers/Credentials>
#include <Mib/Git/HostingProvider>
#include <Mib/Git/Policy>

namespace NMib::NBuildSystem::NRepository
{
	struct CApplyPolicyOptions
	{
		CStr f_PretendDescription() const
		{
			if (fg_IsSet(m_Flags, EApplyPolicyFlag::mc_Pretend))
				return "Would have: ";
			else
				return "";
		}

		CStr m_Repository;
		CStr m_Url;
		NConcurrency::TCActor<CGitHostingProvider> m_HostingProvider;
		TCFunction<void (EOutputType _OutputType, CStr const &_String)> m_fOutputInfo;
		EApplyPolicyFlag m_Flags = EApplyPolicyFlag::mc_None;
	};

	TCFuture<bool> fg_ApplyPolicies_RepositorySettings(CApplyPolicyOptions _Options, CEJsonSorted _Policy)
	{
		TCActor<CGitPolicyActor> PolicyActor = fg_Construct();
		auto DestroyPolicyActor = co_await fg_AsyncDestroy(PolicyActor);

		CGitPolicyActor::CApplyPolicyContext Context
			{
				.m_Repository = _Options.m_Repository
				, .m_HostingProvider = _Options.m_HostingProvider
				, .m_fOnCreate = g_ActorFunctor / [=](CStr _Name, CStr _CreatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Warning
							, "{}Appying policy resulted in created repository '{}'"_f
							<< _Options.f_PretendDescription()
							<< _Options.m_Url
						)
					;

					co_return {};
				}
				, .m_fOnUpdate = g_ActorFunctor / [=](CStr _Name, CStr _UpdatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Warning
							, "{}Appying policy resulted in updated repository properties on repository '{}'. Updated values:\n{}"_f
							<< _Options.f_PretendDescription()
							<< _Options.m_Url
							<< _UpdatedValues.f_Indent("    ", true)
						)
					;

					co_return {};
				}
				, .m_bPretend = fg_IsSet(_Options.m_Flags, EApplyPolicyFlag::mc_Pretend)
				, .m_bCreateMissing = fg_IsSet(_Options.m_Flags, EApplyPolicyFlag::mc_CreateMissing)
			}
		;

		co_return co_await PolicyActor(&CGitPolicyActor::f_ApplyPolicy_Repository, fg_Move(Context), fg_Move(_Policy));
	}

	TCFuture<void> fg_ApplyPolicies_Permissions(CApplyPolicyOptions _Options, CEJsonSorted _Policy)
	{
		co_await ECoroutineFlag_CaptureExceptions;

		TCActor<CGitPolicyActor> PolicyActor = fg_Construct();
		auto DestroyPolicyActor = co_await fg_AsyncDestroy(PolicyActor);

		CGitPolicyActor::CApplyPolicyContext Context
			{
				.m_Repository = _Options.m_Repository
				, .m_HostingProvider = _Options.m_HostingProvider
				, .m_fOnCreate = g_ActorFunctor / [=](CStr _Name, CStr _CreatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Normal
							, "{}Appying policy resulted in add permissions to repository '{}': {}"_f
							<< _Options.f_PretendDescription()
 							<< _Options.m_Url
							<< _CreatedValues
						)
					;

					co_return {};
				}
				, .m_fOnUpdate = g_ActorFunctor / [=](CStr _Name, CStr _UpdatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Warning
							, "{}Appying policy resulted in updated permissions on repository '{}': {}"_f
							<< _Options.f_PretendDescription()
							<< _Options.m_Url
							<< _UpdatedValues
						)
					;

					co_return {};
				}
				, .m_fOnDelete = g_ActorFunctor / [=](CStr _Name, CStr _DeletedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Warning
							, "{}Appying policy resulted in removed permissions on repository '{}': {}"_f
							<< _Options.f_PretendDescription()
							<< _Options.m_Url
							<< _DeletedValues
						)
					;

					co_return {};
				}
				, .m_bPretend = fg_IsSet(_Options.m_Flags, EApplyPolicyFlag::mc_Pretend)
			}
		;

		co_await PolicyActor(&CGitPolicyActor::f_ApplyPolicy_Permissions, fg_Move(Context), fg_Move(_Policy));

		co_return {};
	}

	TCFuture<void> fg_ApplyPolicies_BranchProtection(CApplyPolicyOptions _Options, CEJsonSorted _Policy)
	{
		TCActor<CGitPolicyActor> PolicyActor = fg_Construct();
		auto DestroyPolicyActor = co_await fg_AsyncDestroy(PolicyActor);

		CGitPolicyActor::CApplyPolicyContext Context
			{
				.m_Repository = _Options.m_Repository
				, .m_HostingProvider = _Options.m_HostingProvider
				, .m_fOnCreate = g_ActorFunctor / [=](CStr _Name, CStr _CreatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Normal
							, "{}Appying policy resulted in created branch protection rule for branch pattern '{}' on '{}'"_f
							<< _Options.f_PretendDescription()
							<< _Name
							<< _Options.m_Url
						)
					;

					co_return {};
				}
				, .m_fOnUpdate = g_ActorFunctor / [=](CStr _Name, CStr _UpdatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Warning
							, "{}Appying policy resulted in updated branch protection rule for branch pattern '{}' on '{}'. Updated values:\n{}"_f
							<< _Options.f_PretendDescription()
							<< _Name
							<< _Options.m_Url
							<< _UpdatedValues.f_Indent("    ", true)
						)
					;

					co_return {};
				}
				, .m_bPretend = fg_IsSet(_Options.m_Flags, EApplyPolicyFlag::mc_Pretend)
			}
		;

		co_await PolicyActor(&CGitPolicyActor::f_ApplyPolicy_BranchProtection, fg_Move(Context), fg_Move(_Policy));

		co_return {};
	}

	TCFuture<void> fg_ApplyPolicies_GenericRules(CApplyPolicyOptions _Options, CEJsonSorted _Policy)
	{
		TCActor<CGitPolicyActor> PolicyActor = fg_Construct();
		auto DestroyPolicyActor = co_await fg_AsyncDestroy(PolicyActor);

		CGitPolicyActor::CApplyPolicyContext Context
			{
				.m_Repository = _Options.m_Repository
				, .m_HostingProvider = _Options.m_HostingProvider
				, .m_fOnCreate = g_ActorFunctor / [=](CStr _Name, CStr _CreatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Normal
							, "{}Appying policy resulted in created generic ruleset for branch pattern '{}' on '{}'"_f
							<< _Options.f_PretendDescription()
							<< _Name
							<< _Options.m_Url
						)
					;

					co_return {};
				}
				, .m_fOnUpdate = g_ActorFunctor / [=](CStr _Name, CStr _UpdatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
 							EOutputType_Warning
							, "{}Appying policy resulted in updated generic ruleset for branch pattern '{}' on '{}'. Updated values:\n{}"_f
							<< _Options.f_PretendDescription()
							<< _Name
							<< _Options.m_Url
							<< _UpdatedValues.f_Indent("    ", true)
						)
					;

					co_return {};
				}
				, .m_bPretend = fg_IsSet(_Options.m_Flags, EApplyPolicyFlag::mc_Pretend)
			}
		;

		co_await PolicyActor(&CGitPolicyActor::f_ApplyPolicy_GenericRules, fg_Move(Context), fg_Move(_Policy));

		co_return {};
	}

	TCFuture<void> fg_ApplyPolicies_ActionsSettings(CApplyPolicyOptions _Options, CEJsonSorted _Policy)
	{
		TCActor<CGitPolicyActor> PolicyActor = fg_Construct();
		auto DestroyPolicyActor = co_await fg_AsyncDestroy(PolicyActor);

		CGitPolicyActor::CApplyPolicyContext Context
			{
				.m_Repository = _Options.m_Repository
				, .m_HostingProvider = _Options.m_HostingProvider
				, .m_fOnUpdate = g_ActorFunctor / [=](CStr _Name, CStr _UpdatedValues) -> TCFuture<void>
				{
					_Options.m_fOutputInfo
						(
							EOutputType_Warning
							, "{}Appying policy resulted in updated actions settings on repository '{}'. Updated values:\n{}"_f
							<< _Options.f_PretendDescription()
							<< _Options.m_Url
							<< _UpdatedValues.f_Indent("    ", true)
						)
					;

					co_return {};
				}
				, .m_bPretend = fg_IsSet(_Options.m_Flags, EApplyPolicyFlag::mc_Pretend)
			}
		;

		co_await PolicyActor(&CGitPolicyActor::f_ApplyPolicy_ActionsSettings, fg_Move(Context), fg_Move(_Policy));

		co_return {};
	}

	TCFuture<void> fg_ApplyPolicies(CStr _Url, CStr _RepoDir, CEJsonSorted _Policy, EApplyPolicyFlag _Flags, TCFunction<void (EOutputType _OutputType, CStr const &_String)> _fOutputInfo)
	{
		co_await ECoroutineFlag_CaptureExceptions;

		NWeb::NHTTP::CURL Url(_Url);
		CStr Provider;
		auto &HostName = Url.f_GetHost();
		if (HostName == "github.com")
			Provider = "CGitHostingProviderFactory_CGitHostingProvider_GitHub";
		else
			co_return DMibErrorInstance("Unsupported hosting provider for '{}'"_f << HostName);

		CStr HostingProviderToken = co_await fg_GetGitCredentials(Url, _RepoDir);

		auto HostingProvider = CGitHostingProvider::fs_CreateHostingProvider(Provider);
		auto DestroyHostingProvider = co_await fg_AsyncDestroy(HostingProvider);

		if (HostingProviderToken)
			co_await HostingProvider(&CGitHostingProvider::f_Login, CEJsonSorted{"Token"_= HostingProviderToken});

		CApplyPolicyOptions Options
			{
				.m_Repository = CStr::fs_Join(Url.f_GetPath(), "/").f_RemoveSuffix(".git")
				, .m_Url = _Url
				, .m_HostingProvider = HostingProvider
				, .m_fOutputInfo = _fOutputInfo
				, .m_Flags = _Flags
			}
		;

		if (auto pRepositorySettings = _Policy.f_GetMember("RepositorySettings"))
		{
			bool bCreated = co_await fg_ApplyPolicies_RepositorySettings(Options, *pRepositorySettings);
			if (bCreated && fg_IsSet(_Flags, EApplyPolicyFlag::mc_Pretend))
				co_return {};
		}

		if (auto pPermissions = _Policy.f_GetMember("Permissions"))
			co_await fg_ApplyPolicies_Permissions(Options, *pPermissions);

		if (auto pBranchProtection = _Policy.f_GetMember("BranchProtection"))
			co_await fg_ApplyPolicies_BranchProtection(Options, *pBranchProtection);

		if (auto pGenericRules = _Policy.f_GetMember("GenericRules"))
			co_await fg_ApplyPolicies_GenericRules(Options, *pGenericRules);

		if (auto pActions = _Policy.f_GetMember("ActionsSettings"))
			co_await fg_ApplyPolicies_ActionsSettings(Options, *pActions);

		co_return {};
	}
}
