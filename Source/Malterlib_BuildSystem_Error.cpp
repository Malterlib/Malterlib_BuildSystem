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
		TCVector<CParseError> ParseErrors;

		ParseErrors.f_Insert({_Error, _Position});

		CStr Error = "{} error: {}"_f << _Position.f_Location() << _Error;
		for (auto iError = _Errors.f_GetIterator(); iError; ++iError)
		{
			ParseErrors.f_Insert({iError->m_Error, iError->m_Position});
			Error += DNewLine DMibPFileLineFormatIndent "{} {}"_f << iError->m_Position << iError->m_Error;
		}

		for (auto pEntity = &_Entity; pEntity && pEntity->f_GetKey().m_Type != EEntityType_Root; pEntity = pEntity->m_pParent)
		{
			ParseErrors.f_Insert({"Path={}"_f << pEntity->f_GetPath(), pEntity->f_Data().m_Position});
			Error += DNewLine DMibPFileLineFormatIndent "{} Path={}"_f << pEntity->f_Data().m_Position << pEntity->f_GetPath();
		}

		DMibErrorParse(Error, ParseErrors);
	}

	void CBuildSystem::fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, CStr const &_Error)
	{
		TCVector<CParseError> ParseErrors;
		ParseErrors.f_Insert({_Error, _Position});

		CStr Error = "{} error: {}"_f  << _Position.f_Location() << _Error;
		for (auto pEntity = &_Entity; pEntity && pEntity->f_GetKey().m_Type != EEntityType_Root; pEntity = pEntity->m_pParent)
		{
			ParseErrors.f_Insert({"Path={}"_f << pEntity->f_GetPath(), pEntity->f_Data().m_Position});

			Error += DNewLine DMibPFileLineFormatIndent "{} Path={}"_f << pEntity->f_Data().m_Position << pEntity->f_GetPath();
		}

		DMibErrorParse(Error, ParseErrors);
	}

	void CBuildSystem::fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error, TCVector<CBuildSystemError> const &_Errors)
	{
		TCVector<CParseError> ParseErrors;
		ParseErrors.f_Insert({_Error, _Position});

		CStr Error = "{} error: {}"_f << _Position.f_Location() << _Error;
		for (auto iError = _Errors.f_GetIterator(); iError; ++iError)
		{
			ParseErrors.f_Insert({iError->m_Error, iError->m_Position});
			Error += DNewLine DMibPFileLineFormatIndent "{} {}"_f << iError->m_Position << iError->m_Error;
		}

		DMibErrorParse(Error, ParseErrors);
	}

	void CBuildSystem::fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error)
	{
		CParseError ParseError{_Error, _Position};
		DMibErrorParse((NStr::CStr::CFormat("{} error: {}") << _Position.f_Location() << _Error).f_GetStr(), {ParseError});
	}

	void CBuildSystem::fsp_ThrowError(CBuildSystemRegistry const &_Registry, CStr const &_Error)
	{
		CParseError ParseError{_Error, _Registry.f_GetLocation()};
		DMibErrorParse((NStr::CStr::CFormat("{} error: {}") << _Registry.f_GetLocation() << _Error).f_GetStr(), {ParseError});
	}

	void CBuildSystem::fs_ThrowError
		(
			CEvalPropertyValueContext &_Context
			, CFilePosition const &_Position
			, NStr::CStr const &_Error
			, NContainer::TCVector<CBuildSystemError> const &_Errors
		)
	{
		TCVector<CParseError> ParseErrors;

		ParseErrors.f_Insert({_Error, _Position});
		CStr Error = "{} error: {}"_f << _Position.f_Location() << _Error;

		for (auto iError = _Errors.f_GetIterator(); iError; ++iError)
		{
			ParseErrors.f_Insert({iError->m_Error, iError->m_Position});
			Error += DNewLine DMibPFileLineFormatIndent "{} {}"_f << iError->m_Position << iError->m_Error;
		}

		if (_Context.m_Position != _Position)
		{
			ParseErrors.f_Insert({"Context Position", _Context.m_Position});
			Error += DNewLine DMibPFileLineFormatIndent "{} Context Position"_f << _Context.m_Position;
		}

		for (auto pParentContext = _Context.m_pParentContext; pParentContext; pParentContext = pParentContext->m_pParentContext)
		{
			auto ErrorString = (pParentContext->m_Context.m_pParent ? CStr("{}"_f << pParentContext->m_Context.f_GetKey().m_Name) : CStr("Root"));
			ParseErrors.f_Insert({ErrorString, pParentContext->m_Position});
			Error += DNewLine DMibPFileLineFormatIndent "{} --- {}"_f
				<< pParentContext->m_Position
				<< ErrorString
			;
		}

		DMibErrorParse(Error, ParseErrors);
	}

	void CBuildSystem::fs_ThrowError
		(
			CEvalPropertyValueContext &_Context
			, NStr::CStr const &_Error
			, NContainer::TCVector<CBuildSystemError> const &_Errors
		)
	{
		fs_ThrowError(_Context, _Context.m_Position, _Error, _Errors);
	}
}
