// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	struct CBuildSystemError
	{
		CFilePosition m_Position;
		NStr::CStr m_Error;
	};
}
