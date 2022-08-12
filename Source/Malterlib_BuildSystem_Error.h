// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CBuildSystemError
	{
		CBuildSystemUniquePositions m_Positions;
		NStr::CStr m_Error;
	};
}
