// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_UsedExternal(CPropertyKeyReference const &_PropertyKey) const
	{
		bool bRecorded;
		{
			DLockReadLocked(mp_UsedExternalsLock);
			bRecorded = mp_UsedExternals.f_FindEqual(_PropertyKey);
		}

		if (!bRecorded)
		{
			DMibLock(mp_UsedExternalsLock);
			mp_UsedExternals[_PropertyKey];
		}
	}

	void CBuildSystem::fp_TracePropertyEval(bool _bSuccess, CEntity const &_Entity, CPropertyKey const &_PropertyKey, CProperty const &_Property, CEJSONSorted const &_Value) const
	{
		if (_Property.m_Flags & EPropertyFlag_TraceEval)
		{
			if (!_bSuccess)
			{
				if (_Property.m_Flags & EPropertyFlag_TraceEvalSuccess)
				{
					f_OutputConsole
						(
							"{} !!!!!! {} {} = {}{\n}"_f
							<< _Property.m_Position.f_Location()
							<< _Entity.f_GetPath()
							<< _PropertyKey
							<< _Property.m_Value
						)
					;
				}
			}
			else
			{
				f_OutputConsole
					(
						"{}        {} {} = {}{\n}"_f
						<< _Property.m_Position.f_Location()
						<< _Entity.f_GetPath()
						<< _PropertyKey
						<< _Value
					)
				;
			}
		}
	}
}
