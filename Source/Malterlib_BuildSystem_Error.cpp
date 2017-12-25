// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fs_ThrowError(CFilePosition const &_Position, CStr const &_Error)
	{
		fsp_ThrowError(_Position, _Error);
	}

	void CBuildSystem::fs_ThrowError(CFilePosition const &_Position, CStr const &_Error, TCVector<CBuildSystemError> const &_Errors)
	{
		fsp_ThrowError(_Position, _Error, _Errors);
	}

	void CBuildSystem::fs_ThrowError(CRegistryPreserveAndOrder_CStr const &_Registry, CStr const &_Error)
	{
		fsp_ThrowError(_Registry, _Error);
	}

	void CBuildSystem::fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, CStr const &_Error)
	{
		CStr Error = NMib::NStr::CStr::CFormat(DMibPFileLineFormat " error: {}") << _Position.m_FileName << _Position.m_Line << _Error;
		for (auto pEntity = &_Entity; pEntity && pEntity->m_Key.m_Type != EEntityType_Root; pEntity = pEntity->m_pParent)
			Error += NMib::NStr::CStr::CFormat(DNewLine DMibPFileLineFormatIndent DMibPFileLineFormat " Path={}") << pEntity->m_Position.m_FileName << pEntity->m_Position.m_Line << pEntity->f_GetPath();
		DMibError(Error);
	}

	void CBuildSystem::fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error, TCVector<CBuildSystemError> const &_Errors)
	{
		CStr Error = NMib::NStr::CStr::CFormat(DMibPFileLineFormat " {}") << _Position.m_FileName << _Position.m_Line << _Error;
		for (auto iError = _Errors.f_GetIterator(); iError; ++iError)
			Error += NMib::NStr::CStr::CFormat(DNewLine DMibPFileLineFormatIndent DMibPFileLineFormat " {}") << iError->m_Position.m_FileName << iError->m_Position.m_Line << iError->m_Error;
		DMibError(Error);
	}
	
	void CBuildSystem::fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error)
	{
		DMibError((NMib::NStr::CStr::CFormat(DMibPFileLineFormat " {}") << _Position.m_FileName << _Position.m_Line << _Error).f_GetStr());
	}

	void CBuildSystem::fsp_ThrowError(CRegistryPreserveAndOrder_CStr const &_Registry, CStr const &_Error)
	{
		DMibError((NMib::NStr::CStr::CFormat(DMibPFileLineFormat " {}") << _Registry.f_GetFile() << _Registry.f_GetLine() << _Error).f_GetStr());
	}
}
