// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Container/Registry>

namespace NMib::NBuildSystem
{
	struct CFilePosition
	{
		inline CFilePosition(CRegistryPreserveAll const &_Position);
		inline CFilePosition();
		inline CFilePosition &operator = (CRegistryPreserveAll const &_Position);

		CStr m_FileName;
		int32 m_Line;
	};
}

#include "Malterlib_BuildSystem_FilePosition.hpp"
