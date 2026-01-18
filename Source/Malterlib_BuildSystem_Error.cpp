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

	namespace
	{
		struct CReportedMessage
		{
			auto operator <=> (CReportedMessage const &) const = default;

			CFilePosition m_Position;
			CStr m_Message;
		};

		struct CParseErrors;
		struct CSeparator
		{
			CSeparator(CParseErrors &_ParseErrors, CStr const &_Name);
			~CSeparator();

			CStr m_Name;
			bool m_bAdded = false;
			CParseErrors &m_ParseErrors;
		};

		struct CParseErrors
		{
			bool f_AddError(CSeparator *_pSeparator, CStr const &_Error, CFilePosition const &_Position = {})
			{
				if (!m_ReportedMessages(CReportedMessage{_Position, _Error}).f_WasCreated())
					return false;

				if (_pSeparator && !_pSeparator->m_bAdded && _pSeparator->m_Name)
				{
					_pSeparator->m_bAdded = true;
					m_ParseErrors.f_Insert().f_SetSeparator(_pSeparator->m_Name).m_IndentDepth = m_IndentDepth;
				}

				m_ParseErrors.f_Insert({_Error, _Position}).m_IndentDepth = m_IndentDepth + (_pSeparator ? 1 : 0);

				return true;
			}

			bool f_AddError(CStr const &_Error, CFilePosition const &_Position = {})
			{
				return f_AddError(nullptr, _Error, _Position);
			}

			CStr f_ToString() const
			{
				return CParseError::fs_ToString(m_ParseErrors);
			}

			TCSet<CReportedMessage> m_ReportedMessages;
			TCVector<CParseError> m_ParseErrors;
			uint32 m_IndentDepth = 0;
		};

		CSeparator::CSeparator(CParseErrors &_ParseErrors, CStr const &_Name)
			: m_ParseErrors(_ParseErrors)
			, m_Name(_Name)
		{
			++m_ParseErrors.m_IndentDepth;
		}

		CSeparator::~CSeparator()
		{
			--m_ParseErrors.m_IndentDepth;
		}

		void fg_AddOtherMessages(CParseErrors &o_ParseErrors, NContainer::TCVector<CBuildSystemError> const &_Errors)
		{
			CSeparator Separator{o_ParseErrors, "Other messages"};

			for (auto iError = _Errors.f_GetIterator(); iError; ++iError)
			{
				if (iError->m_Positions.f_IsEmpty())
					o_ParseErrors.f_AddError(&Separator, iError->m_Error);
				else
				{
					auto &FirstPosition = iError->m_Positions.m_Positions.f_GetFirst();

					o_ParseErrors.f_AddError(&Separator, iError->m_Error, FirstPosition.m_Key.m_Position);

					CSeparator ContributingSeparator{o_ParseErrors, "Other message contributing values"};

					for (auto &Position : iError->m_Positions.m_Positions)
						o_ParseErrors.f_AddError(&ContributingSeparator, Position.f_GetMessage(), Position.m_Key.m_Position);
				}
			}
		}

		void fg_AddMainError(CParseErrors &o_ParseErrors, CFilePosition const &_Position, NStr::CStr const &_Error)
		{
			o_ParseErrors.f_AddError("error: {}"_f << _Error, _Position);
		}

		CFilePosition fg_AddMainError(CParseErrors &o_ParseErrors, CBuildSystemUniquePositions const &_Positions, NStr::CStr const &_Error)
		{
			CStr Error = "error: {}"_f << _Error;

			CFilePosition FirstPosition;
			if (!_Positions.m_Positions.f_IsEmpty())
			{
				FirstPosition = _Positions.m_Positions.f_GetFirst().m_Key.m_Position;
				o_ParseErrors.f_AddError(Error, FirstPosition);
			}
			else
				o_ParseErrors.f_AddError(Error);

			{
				CSeparator Separator{o_ParseErrors, "Contributing values"};

				for (auto &Position : _Positions.m_Positions)
					o_ParseErrors.f_AddError(&Separator, Position.f_GetMessage(), Position.m_Key.m_Position);
			}

			if (_Positions.m_pParentPositions)
			{
				CSeparator Separator{o_ParseErrors, "Parent positions"};

				for (auto *pPositions = _Positions.m_pParentPositions; pPositions; pPositions = pPositions->m_pParentPositions)
				{
					for (auto &Position : pPositions->m_Positions)
						o_ParseErrors.f_AddError(&Separator, Position.f_GetMessage(), Position.m_Key.m_Position);
				}
			}

			return FirstPosition;
		}

		void fg_AddEntities(CParseErrors &o_ParseErrors, CEntity const *_pEntity)
		{
			CSeparator Separator{o_ParseErrors, "Entity and parents"};

			for (auto pEntity = _pEntity; pEntity && pEntity->f_GetKey().m_Type != EEntityType_Root; pEntity = pEntity->m_pParent)
			{
				DMibFastCheck(pEntity->f_Data().m_Position.f_IsValid());
				o_ParseErrors.f_AddError(&Separator, "Path={}"_f << pEntity->f_GetPath(), pEntity->f_Data().m_Position);
			}
		}
	}

	void CBuildSystem::fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, NStr::CStr const &_Error, NContainer::TCVector<CBuildSystemError> const &_Errors)
	{
		CParseErrors ParseErrors;

		fg_AddMainError(ParseErrors, _Position, _Error);
		fg_AddOtherMessages(ParseErrors, _Errors);
		fg_AddEntities(ParseErrors, &_Entity);

		DMibErrorParse(ParseErrors.f_ToString(), ParseErrors.m_ParseErrors);
	}

	void CBuildSystem::fsp_ThrowError(CEntity const &_Entity, CFilePosition const &_Position, CStr const &_Error)
	{
		CParseErrors ParseErrors;

		fg_AddMainError(ParseErrors, _Position, _Error);
		fg_AddEntities(ParseErrors, &_Entity);

		DMibErrorParse(ParseErrors.f_ToString(), ParseErrors.m_ParseErrors);
	}

	void CBuildSystem::fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error, TCVector<CBuildSystemError> const &_Errors)
	{
		CParseErrors ParseErrors;

		fg_AddMainError(ParseErrors, _Position, _Error);
		fg_AddOtherMessages(ParseErrors, _Errors);

		DMibErrorParse(ParseErrors.f_ToString(), ParseErrors.m_ParseErrors);
	}

	void CBuildSystem::fsp_ThrowError(CFilePosition const &_Position, CStr const &_Error)
	{
		CParseErrors ParseErrors;

		fg_AddMainError(ParseErrors, _Position, _Error);

		DMibErrorParse(ParseErrors.f_ToString(), ParseErrors.m_ParseErrors);
	}

	void CBuildSystem::fsp_ThrowError(CBuildSystemRegistry const &_Registry, CStr const &_Error)
	{
		fsp_ThrowError(_Registry.f_GetLocation(), _Error);
	}

	void CBuildSystem::fs_ThrowError
		(
			CEvalPropertyValueContext &_Context
			, CFilePosition const &_Position
			, NStr::CStr const &_Error
			, NContainer::TCVector<CBuildSystemError> const &_Errors
		)
	{
		CBuildSystemUniquePositions Positions;
		Positions.f_AddPosition(_Position, "");
		fs_ThrowError(_Context, Positions, _Error, _Errors);
	}

	void CBuildSystem::fs_ThrowError
		(
			CEvalPropertyValueContext &_Context
			, CBuildSystemUniquePositions const &_Positions
			, NStr::CStr const &_Error
			, NContainer::TCVector<CBuildSystemError> const &_Errors
		)
	{
		CParseErrors ParseErrors;

		CFilePosition FirstPosition = fg_AddMainError(ParseErrors, _Positions, _Error);

		if (_Context.m_Position != FirstPosition)
		{
			CSeparator Separator{ParseErrors, "Context"};
			ParseErrors.f_AddError(&Separator, "Context", _Context.m_Position);
		}

		if (_Context.m_pStorePositions)
		{
			CSeparator Separator{ParseErrors, "Context contributing values"};

			for (auto *pStorePositions = _Context.m_pStorePositions; pStorePositions; pStorePositions = pStorePositions->m_pParentPositions)
			{
				for (auto &Position : pStorePositions->m_Positions)
					ParseErrors.f_AddError(&Separator, Position.f_GetMessage(), Position.m_Key.m_Position);
			}
		}

		fg_AddOtherMessages(ParseErrors, _Errors);

		{
			CSeparator Separator{ParseErrors, "Parent contexts"};
			for (auto pParentContext = _Context.m_pParentContext; pParentContext; pParentContext = pParentContext->m_pParentContext)
			{
				ParseErrors.f_AddError(&Separator, "Context", pParentContext->m_Position);

				if (pParentContext->m_pStorePositions)
				{
					CSeparator Separator{ParseErrors, "Parent context contributing values"};

					for (auto *pStorePositions = pParentContext->m_pStorePositions; pStorePositions; pStorePositions = pStorePositions->m_pParentPositions)
					{
						for (auto &Position : pStorePositions->m_Positions)
							ParseErrors.f_AddError(&Separator, Position.f_GetMessage(), Position.m_Key.m_Position);
					}
				}

				fg_AddEntities(ParseErrors, &pParentContext->m_Context);
			}
		}

		DMibErrorParse(ParseErrors.f_ToString(), ParseErrors.m_ParseErrors);
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

	void CBuildSystem::fs_ThrowError(CBuildSystemPropertyInfo const &_PropertyInfo, NStr::CStr const &_Error)
	{
		if (_PropertyInfo.m_pPositions && !_PropertyInfo.m_pPositions->f_IsEmpty())
			fs_ThrowError(*_PropertyInfo.m_pPositions, _Error);
		else if (_PropertyInfo.m_pProperty)
			fs_ThrowError(_PropertyInfo.m_pProperty->m_Position, _Error);
		else
			fs_ThrowError(CFilePosition(), _Error);
	}

	void CBuildSystem::fs_ThrowError(CBuildSystemUniquePositions const &_Positions, NStr::CStr const &_Error)
	{
		fs_ThrowError(_Positions, _Error, {});
	}

	void CBuildSystem::fs_ThrowError(CBuildSystemPropertyInfo const &_PropertyInfo, CBuildSystemUniquePositions const &_Positions, NStr::CStr const &_Error)
	{
		if (_PropertyInfo.m_pPositions && !_PropertyInfo.m_pPositions->f_IsEmpty())
		{
			CBuildSystemUniquePositions Positions;
			Positions.f_AddFirstFromPositions(*_PropertyInfo.m_pPositions);
			Positions.f_AddPositions(_Positions);
			Positions.f_AddPositions(*_PropertyInfo.m_pPositions);

			CBuildSystem::fs_ThrowError(Positions, _Error);
		}
		else
			CBuildSystem::fs_ThrowError(_Positions, _Error);
	}

	void CBuildSystem::fs_ThrowError(CBuildSystemPropertyInfo const &_PropertyInfo, CFilePosition const &_Position, NStr::CStr const &_Error)
	{
		if (_PropertyInfo.m_pPositions && !_PropertyInfo.m_pPositions->f_IsEmpty())
		{
			auto Positions = *_PropertyInfo.m_pPositions;
			if (_Position.f_IsValid())
				Positions.f_AddPosition(_Position, "");

			CBuildSystem::fs_ThrowError(Positions, _Error);
		}
		else
			CBuildSystem::fs_ThrowError(_Position, _Error);
	}

	void CBuildSystem::fs_ThrowError(CBuildSystemUniquePositions const &_Positions, NStr::CStr const &_Error, NContainer::TCVector<CBuildSystemError> const &_Errors)
	{
		CParseErrors ParseErrors;

		fg_AddMainError(ParseErrors, _Positions, _Error);
		fg_AddOtherMessages(ParseErrors, _Errors);

		DMibErrorParse(ParseErrors.f_ToString(), ParseErrors.m_ParseErrors);
	}
}
