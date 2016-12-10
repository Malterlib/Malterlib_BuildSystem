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
	typedef TCSharedPointer<CEntity const> CEntityPointer;
	typedef TCSharedPointer<CEntity> CEntityMutablePointer;
#else
	typedef TCPointer<CEntity const> CEntityPointer;
	typedef TCPointer<CEntity> CEntityMutablePointer;
#endif
}
