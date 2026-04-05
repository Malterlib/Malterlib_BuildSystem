// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_BuildSystem_Repository.h"

#include <Mib/CommandLine/AnsiEncodingParse>
#include <Mib/CommandLine/TableRenderer>
#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/Git/Helpers/Credentials>
#include <Mib/Git/HostingProvider>
#include <Mib/Git/Policy>

namespace NMib::NBuildSystem
{
	namespace
	{
		TCFuture<CStr> fg_LaunchBsdTar(TCVector<CStr> _Params, CStr _WorkingDirectory)
		{
			TCActor<CProcessLaunchActor> ProcessLaunch = fg_Construct();
			auto Destroy = co_await fg_AsyncDestroy(ProcessLaunch);

			CProcessLaunchActor::CSimpleLaunch SimpleLaunch
				(
					CFile::fs_GetProgramDirectory() / ("bsdtar" + CFile::mc_ExecutableExtension)
					, _Params
					, _WorkingDirectory
					, CProcessLaunchActor::ESimpleLaunchFlag_GenerateExceptionOnNonZeroExitCode
				)
			;

			co_return (co_await ProcessLaunch(&CProcessLaunchActor::f_LaunchSimple, fg_Move(SimpleLaunch))).f_GetStdOut();
		}

		struct CGetOrCreateRelease
		{
			TCActor<CGitHostingProvider> m_HostingProvider;
			CStr m_Repository;
			CStr m_TagName;
			CStr m_SourceReference;
			CStr m_Description;
			bool m_bMakeLatest = true;
		};

		TCFuture<CGitHostingProvider::CRelease> fg_GetOrCreateRelease(CGetOrCreateRelease _Options)
		{
			CGitHostingProvider::CCreateRelease CreateRelease;
			CreateRelease.m_TagName = _Options.m_TagName;
			CreateRelease.m_Name = CreateRelease.m_TagName;
			CreateRelease.m_Description = _Options.m_Description;
			CreateRelease.m_GenerateReleaseNotes = false;
			CreateRelease.m_Published = true;
			CreateRelease.m_PreRelease = false;
			CreateRelease.m_MakeLatest = _Options.m_bMakeLatest;
			CreateRelease.m_TargetReference = _Options.m_SourceReference;

			CGitHostingProvider::CRelease Release;

			for (umint iRetry = 0; ; ++iRetry)
			{
				if (auto ExistingRelease = co_await _Options.m_HostingProvider(&CGitHostingProvider::f_GetRelease, _Options.m_Repository, CreateRelease.m_TagName))
				{
					Release = fg_Move(*ExistingRelease);
					break;
				}

				auto CreateReleaseResult = co_await _Options.m_HostingProvider(&CGitHostingProvider::f_CreateRelease, _Options.m_Repository, CreateRelease).f_Wrap();
				if (CreateReleaseResult)
				{
					Release = *CreateReleaseResult;
					break;
				}

				bool bAlreadyExists = false;
				NException::fg_VisitException<CGitHostingProviderException>
					(
						CreateReleaseResult.f_GetException()
						, [&](CGitHostingProviderException const &_Exception)
						{
							auto &Specific = _Exception.f_GetSpecific();
							if (Specific.f_HasError("TagName", EGitHostingProviderErrorCode::mc_ResourceAlreadyExists))
								bAlreadyExists = true;
							else if (Specific.f_HasError({}, EGitHostingProviderErrorCode::mc_Custom, "Release", "Published releases must have a valid tag"))
								bAlreadyExists = true;
						}
					)
				;

				if (bAlreadyExists && iRetry < 5)
					;
				else
					co_return CreateReleaseResult.f_GetException();
			}

			co_return fg_Move(Release);
		}

		TCFuture<bool> fg_UploadReleaseAsset
			(
				TCActor<CGitHostingProvider> _HostingProvider
				, CStr _Repository
				, CGitHostingProvider::CRelease _Release
				, CStr _AssetName
				, CStr _SourceFile
			)
		{
			struct CFileReadState
			{
				CFile m_File;
			};

			TCSharedPointer<CFileReadState> pFileReadState = fg_Construct();

			auto CleanupFileState = g_BlockingActorSubscription / [pFileReadState]
				{
					pFileReadState->m_File.f_IsValid();
					pFileReadState->m_File.f_Close();
				}
			;

			uint64 AssetSize;
			{
				auto BlockingActorCheckout = fg_BlockingActor();
				AssetSize = co_await
					(
						g_Dispatch(BlockingActorCheckout) / [_SourceFile, pFileReadState]() -> uint64
						{
							pFileReadState->m_File.f_Open(_SourceFile, EFileOpen_Read | EFileOpen_ShareAll);
							return pFileReadState->m_File.f_GetLength();
						}
					)
				;
			}

			bool bDeletedOld = false;
			for (auto &Asset : _Release.m_Assets)
			{
				if (Asset.m_Name == _AssetName)
				{
					co_await _HostingProvider(&CGitHostingProvider::f_DeleteReleaseAsset, _Repository, Asset.m_Identifier);
					bDeletedOld = true;
					break;
				}
			}

			co_await _HostingProvider
				(
					&CGitHostingProvider::f_UploadReleaseAsset
					, _Repository
					, _Release.m_Identifier
					, CGitHostingProvider::CUploadReleaseAsset
					{
						.m_Name = _AssetName
						, .m_AssetSize = AssetSize
						, .m_fReadData = g_ActorFunctor
						/ [pFileReadState]
						(umint _nBytes) mutable -> TCFuture<CByteVector>
						{
							CByteVector Result;
							{
								auto BlockingActorCheckout = fg_BlockingActor();
								Result = co_await
									(
										g_Dispatch(BlockingActorCheckout) / [pFileReadState, _nBytes]
										{
											auto BytesLeftInFile = pFileReadState->m_File.f_GetLength() - pFileReadState->m_File.f_GetPosition();

											umint BytesToRead = fg_Min(umint(BytesLeftInFile), _nBytes);

											CByteVector Return;
											Return.f_SetLen(BytesToRead);

											pFileReadState->m_File.f_Read(Return.f_GetArray(), BytesToRead);

											return Return;
										}
									)
								;
							}

							if (Result.f_GetLen() != _nBytes)
								co_return DMibErrorInstance("Could not read all bytes from file. {} != {}"_f << Result.f_GetLen() << _nBytes);

							co_return fg_Move(Result);
						}
					}
				)
			;

			co_await CleanupFileState->f_Destroy();

			co_return bDeletedOld;
		}

		TCFuture<void> fg_ReleasePackage
			(
				CStr _RepoDir
				, NRepository::CRemoteProperties _RemoteProperties
				, NFunction::TCFunction<void (NStr::CStr const &_Output, bool _bError)> _fOutput
			)
		{
			DMibRequire(_RemoteProperties.m_ReleasePackage);

			NWeb::NHTTP::CURL Url(_RemoteProperties.m_URL);
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

			CStr GitRepository = CStr::fs_Join(Url.f_GetPath(), "/").f_RemoveSuffix(".git");

			auto &ReleasePackage = *_RemoteProperties.m_ReleasePackage;
			[[maybe_unused]] auto Release = co_await fg_GetOrCreateRelease
				(
					CGetOrCreateRelease
					{
						.m_HostingProvider = HostingProvider
						, .m_Repository = GitRepository
						, .m_TagName = ReleasePackage.m_ReleaseName
						, .m_SourceReference = ReleasePackage.m_SourceReference
						, .m_Description = ReleasePackage.m_Description
						, .m_bMakeLatest = ReleasePackage.m_bMakeLatest
					}
				)
			;

			for (auto &Package : ReleasePackage.m_Packages)
			{
				CStr SourceFile;
				CStr CompressedFileName;
				if (Package.m_bCompress)
					CompressedFileName = CFile::fs_GetUserHomeDirectory() / ".Malterlib/temp" / fg_FastRandomID() / Package.m_PackageName;

				auto CleanupFile = g_BlockingActorSubscription / [CompressedFileName]
					{
						if (CompressedFileName)
						{
							auto CompressedDirectory = CFile::fs_GetPath(CompressedFileName);
							if (CFile::fs_FileExists(CompressedDirectory))
								CFile::fs_DeleteDirectoryRecursive(CompressedDirectory);
						}
					}
				;

				if (Package.m_bCompress)
				{
					CFile::fs_CreateDirectory(CFile::fs_GetPath(CompressedFileName));

					TCVector<CStr> Params{"-cf", CompressedFileName};
					Params.f_Insert(Package.m_CompressArguments);
					Params.f_Insert("--");
					Params.f_Insert(Package.m_Files);

					co_await fg_LaunchBsdTar(Params, _RepoDir);

					SourceFile = CompressedFileName;
				}
				else
				{
					if (Package.m_Files.f_GetLen() != 1)
						co_return DMibErrorInstance("When package is not compressed you need to specify exatly one file, not: {vs}"_f << Package.m_Files);

					SourceFile = Package.m_Files[0];
				}

				bool bDeletedOld = co_await fg_UploadReleaseAsset(HostingProvider, GitRepository, Release, Package.m_PackageName, SourceFile);

				if (bDeletedOld)
					_fOutput("{} - {}: Updated asset {}\n"_f << _RepoDir << ReleasePackage.m_ReleaseName << Package.m_PackageName, false);
				else
					_fOutput("{} - {}: Created asset {}\n"_f << _RepoDir << ReleasePackage.m_ReleaseName << Package.m_PackageName, false);

				co_await CleanupFile->f_Destroy();
			}

			co_return {};
		}
	}

	using namespace NRepository;
	TCUnsafeFuture<CBuildSystem::ERetry> CBuildSystem::f_Action_Repository_ReleasePackage
		(
			CGenerateOptions const &_GenerateOptions
			, CRepoFilter const &_Filter
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		co_await ECoroutineFlag_CaptureMalterlibExceptions;

		auto fOutput = this->f_OutputConsoleFunctor();

		CGenerateEphemeralState GenerateState;
		if (ERetry Retry = co_await fp_GeneratePrepare(_GenerateOptions, GenerateState, nullptr); Retry != ERetry_None)
			co_return Retry;

		TCSharedPointer<CFilteredRepos> pFilteredRepositories = fg_Construct(co_await fg_GetFilteredRepos(_Filter, *this, mp_Data, EGetRepoFlag::mc_IncludeReleasePackage));
		[[maybe_unused]] auto &FilteredRepositories = *pFilteredRepositories;

		for (auto &Repos : FilteredRepositories.m_FilteredRepositories)
		{
			TCFutureVector<void> Results;
			for (auto *pRepo : Repos)
			{
				auto &Repo = *pRepo;

				if (Repo.m_OriginProperties.m_ReleasePackage)
					co_await fg_ReleasePackage(Repo.m_Location, Repo.m_OriginProperties, fOutput);

				for (auto &Remote : Repo.m_Remotes)
				{
					if (Remote.m_Properties.m_ReleasePackage)
						co_await fg_ReleasePackage(Repo.m_Location, Remote.m_Properties, fOutput);
				}
			}
		}

		co_return ERetry_None;
	}
}
