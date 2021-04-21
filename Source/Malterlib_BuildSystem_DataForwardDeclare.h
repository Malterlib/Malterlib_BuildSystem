// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
	typedef NStorage::TCSharedPointer<CEntity const> CEntityPointer;
	typedef NStorage::TCSharedPointer<CEntity> CEntityMutablePointer;
#else
	typedef NStorage::TCPointer<CEntity const> CEntityPointer;
	typedef NStorage::TCPointer<CEntity> CEntityMutablePointer;
#endif
}
