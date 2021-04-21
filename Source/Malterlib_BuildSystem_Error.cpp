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

	void CBuildSystem::fs_ThrowError(CBuildSystemRegistry const &_Registry, CStr const &_Error)
	{
		fsp_ThrowError(_Registry, _Error);
	}

	void CBuildSystem::fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, NStr::CStr const &_Error, NContainer::TCVector<CBuildSystemError> const &_Errors)
	{
		CStr Error = "{} error: {}"_f << _Position.f_Location() << _Error;
		for (auto iError = _Errors.f_GetIterator(); iError; ++iError)
			Error += DNewLine DMibPFileLineFormatIndent "{} {}"_f << iError->m_Position << iError->m_Error;

		for (auto pEntity = &_Entity; pEntity && pEntity->f_GetKey().m_Type != EEntityType_Root; pEntity = pEntity->m_pParent)
			Error += DNewLine DMibPFileLineFormatIndent "{} Path={}"_f << pEntity->f_Data().m_Position << pEntity->f_GetPath();

		DMibError(Error);
	}

	void CBuildSystem::fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, CStr const &_Error)
	{
		CStr Error = "{} error: {}"_f  << _Position.f_Location() << _Error;
		for (auto pEntity = &_Entity; pEntity && pEntity->f_GetKey().m_Type != EEntityType_Root; pEntity = pEntity->m_pParent)
			Error += DNewLine DMibPFileLineFormatIndent "{} Path={}"_f << pEntity->f_Data().m_Position << pEntity->f_GetPath();
		DMibError(Error);
	}

	void CBuildSystem::fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error, TCVector<CBuildSystemError> const &_Errors)
	{
		CStr Error = "{} error: {}"_f << _Position.f_Location() << _Error;
		for (auto iError = _Errors.f_GetIterator(); iError; ++iError)
			Error += DNewLine DMibPFileLineFormatIndent "{} {}"_f << iError->m_Position << iError->m_Error;
		DMibError(Error);
	}

	void CBuildSystem::fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error)
	{
		DMibError((NStr::CStr::CFormat("{} error: {}") << _Position.f_Location() << _Error).f_GetStr());
	}

	void CBuildSystem::fsp_ThrowError(CBuildSystemRegistry const &_Registry, CStr const &_Error)
	{
		DMibError((NStr::CStr::CFormat("{} error: {}") << _Registry.f_GetLocation() << _Error).f_GetStr());
	}

	void CBuildSystem::fsp_ThrowError
		(
			CEvalPropertyValueContext &_Context
			, CFilePosition const &_Position
			, NStr::CStr const &_Error
			, NContainer::TCVector<CBuildSystemError> const &_Errors
		)
	{
		CStr Error = "{} error: {}"_f << _Position.f_Location() << _Error;
		for (auto iError = _Errors.f_GetIterator(); iError; ++iError)
			Error += DNewLine DMibPFileLineFormatIndent "{} {}"_f << iError->m_Position << iError->m_Error;

		if (_Context.m_Position != _Position)
			Error += DNewLine DMibPFileLineFormatIndent "{} Context Position"_f << _Context.m_Position;

		for (auto pParentContext = _Context.m_pParentContext; pParentContext; pParentContext = pParentContext->m_pParentContext)
		{
			Error += DNewLine DMibPFileLineFormatIndent "{} --- {}"_f
				<< pParentContext->m_Position
				<< (pParentContext->m_Context.m_pParent ? CStr("{}"_f << pParentContext->m_Context.f_GetKey().m_Name) : CStr("Root"))
			;
		}

		DMibError(Error);
	}

	void CBuildSystem::fsp_ThrowError
		(
			CEvalPropertyValueContext &_Context
			, NStr::CStr const &_Error
			, NContainer::TCVector<CBuildSystemError> const &_Errors
		)
	{
		fsp_ThrowError(_Context, _Context.m_Position, _Error, _Errors);
	}
}
