// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem
{
	template <typename tf_CContainer, typename tf_FFunctor>
	DMibRunsToCompletion NConcurrency::TCUnsafeFuture<void> DMibWorkaroundUBSanSectionErrorsDisable fg_ParallelForEach
		(
			tf_CContainer &&_Container
			, tf_FFunctor &&_fFunctor
			, bool _bSingleThreaded
		)
	;
}

#include "Malterlib_BuildSystem_ParallelForEach.hpp"
