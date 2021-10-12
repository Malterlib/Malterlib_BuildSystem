// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include "Malterlib_BuildSystem_Registry.h"

namespace NMib::NBuildSystem
{
	struct CFilePosition : public NStr::CParseLocation
	{
		inline CFilePosition(CBuildSystemRegistry const &_Position);
		inline CFilePosition(NStr::CParseLocation const &_Position);
		inline CFilePosition();
		inline CFilePosition &operator = (CBuildSystemRegistry const &_Position);
		inline CFilePosition &operator = (NStr::CParseLocation const &_Position);
		inline NStr::CParseLocation const &f_Location() const;
	};
}

#include "Malterlib_BuildSystem_FilePosition.hpp"
