// Copyright © 2024 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

#include <Mib/Process/Platform>

namespace NMib::NBuildSystem
{
	namespace
	{
		void fg_InstallBinaries(CStr const &_DestinationDirectory)
		{
			CStr SourceDirectory = CFile::fs_GetProgramDirectory();

			CStr MainSourceExecutable = CFile::fs_GetProgramPath();
			CStr MainDestinationExecutable = _DestinationDirectory / CFile::fs_GetFile(MainSourceExecutable);

			if (CFile::fs_FileExists(MainDestinationExecutable))
			{
				auto SourceWriteTime = CFile::fs_GetWriteTime(MainSourceExecutable);
				if (CFile::fs_GetWriteTime(MainDestinationExecutable) >= SourceWriteTime)
					return;

				NMib::NProcess::CVersionInfo SourceVersionInfo;
				NProcess::NPlatform::fg_Process_GetVersionInfo(MainSourceExecutable, SourceVersionInfo);

				NMib::NProcess::CVersionInfo DestinationVersionInfo;
				NProcess::NPlatform::fg_Process_GetVersionInfo(MainDestinationExecutable, DestinationVersionInfo);

				if (DestinationVersionInfo.f_GetFullVersion() > SourceVersionInfo.f_GetFullVersion())
				{
					CFile::fs_SetWriteTime(MainDestinationExecutable, SourceWriteTime);
					return;
				}
			}

			CFile::fs_CreateDirectory(_DestinationDirectory);

			auto fTryCopy = [&](CStr const &_FileName)
				{
					auto SourceFile = SourceDirectory / _FileName;

					if (!CFile::fs_FileExists(SourceFile))
						return;

					CFile::fs_DiffCopyFileOrDirectory
						(
							SourceFile
							, _DestinationDirectory / _FileName
							, [&](CFile::EDiffCopyChange _Change, CStr const &_Source, CStr const &_Destination, CStr const &_Link)
							{
								return CFile::EDiffCopyChangeAction_Perform;
							}
						)
					;
				}
			;

			fTryCopy("MTool" + CFile::mc_ExecutableExtension);
			fTryCopy("MalterlibHelper" + CFile::mc_ExecutableExtension);
#if defined(DPlatformFamily_macOS)
			fTryCopy("MalterlibOverrideMalloc.dylib");
#endif
			fTryCopy("mib" + CFile::mc_ExecutableExtension);
			fTryCopy("bsdtar" + CFile::mc_ExecutableExtension);
			fTryCopy("Bootstrap.version");
		}
	}

	TCFuture<void> CBuildSystem::f_SetupGlobalMTool() const
	{
		if (mp_bGlobalMToolAlreadySetup.f_Load())
			co_return {};

		auto Subscription = co_await mp_SetupGlobalMToolSequencer.f_Sequence();

		if (mp_bGlobalMToolAlreadySetup.f_Load())
			co_return {};

		auto BlockingActorCheckout = fg_BlockingActor();
		co_await
			(
				g_Dispatch(BlockingActorCheckout) / []
				{
					fg_InstallBinaries(CFile::fs_GetUserHomeDirectory() / ".Malterlib/bin");
				}
			)
		;

		mp_bGlobalMToolAlreadySetup.f_Exchange(true);

		co_return {};
	}
}
