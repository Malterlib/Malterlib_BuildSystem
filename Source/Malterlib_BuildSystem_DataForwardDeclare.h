// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	struct CEntity;
}

#ifdef DMibBuildSystem_DebugReferences
	DMibDefineSharedPointerType(NMib::NBuildSystem::CEntity, true, false);
#endif

namespace NMib::NBuildSystem
{
#ifdef DMibBuildSystem_DebugReferences
	using CEntityPointer = NStorage::TCSharedPointer<CEntity const>;
	using CEntityMutablePointer = NStorage::TCSharedPointer<CEntity>;
#else
	using CEntityPointer = NStorage::TCPointer<CEntity const>;
	using CEntityMutablePointer = NStorage::TCPointer<CEntity>;
#endif
}
