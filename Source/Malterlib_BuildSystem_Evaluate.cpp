// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"
#include "Malterlib_BuildSystem_Preprocessor.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::fp_UsedExternal(CStr const &_Name) const
	{
		bool bRecorded;
		{
			DLockReadLocked(mp_UsedExternalsLock);
			bRecorded = mp_UsedExternals.f_FindEqual(_Name);
		}
			
		if (!bRecorded)
		{
			DMibLock(mp_UsedExternalsLock);
			mp_UsedExternals[_Name];
		}
	}
	
	void CBuildSystem::fp_TracePropertyEval(bool _bSuccess, CEntity const &_Entity, CProperty const &_Property, CStr const &_Value) const
	{
		if (_Property.m_Debug.f_Find("TraceEval") >= 0)
		{
			if (!_bSuccess)
			{
				if (_Property.m_Debug.f_Find("TraceEvalSuccess") < 0)
				{
					DConOut
						(
							DMibPFileLineFormat " !!!!!! {} {}:{} = {}" DNewLine
							, _Property.m_Position.m_FileName 
							<< _Property.m_Position.m_Line 
							<< _Entity.f_GetPath() 
							<< fg_PropertyTypeToStr(_Property.m_Key.m_Type) 
							<< _Property.m_Key.m_Name
							<< _Value
						)
					;
				}
			}
			else
			{
				DConOut
					(
						DMibPFileLineFormat "        {} {}:{} = {}" DNewLine
						, _Property.m_Position.m_FileName 
						<< _Property.m_Position.m_Line 
						<< _Entity.f_GetPath() 
						<< fg_PropertyTypeToStr(_Property.m_Key.m_Type) 
						<< _Property.m_Key.m_Name 
						<< _Value
					)
				;
			}
		}
	}
}
