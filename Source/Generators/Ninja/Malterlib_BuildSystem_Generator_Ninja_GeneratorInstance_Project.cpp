// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "../../Malterlib_BuildSystem_Helpers.h"
#include "Malterlib_BuildSystem_Generator_Ninja.h"
#include <Mib/Process/ProcessLaunch>

namespace NMib::NBuildSystem::NNinja
{
	TCUnsafeFuture<void> CGeneratorInstance::f_GenerateProjectFile(CProject &_Project) const
	{
		co_await ECoroutineFlag_CaptureExceptions;

		co_await fp_EvaluateTargetSettings(_Project);

		co_await g_Yield;

		auto CompileTypes = co_await fp_GenerateProjectFile_File(_Project);

		co_await g_Yield;

		co_await fp_GenerateProjectFile_FileTypes(_Project, CompileTypes);

		co_await g_Yield;

		co_return {};
	}
}
