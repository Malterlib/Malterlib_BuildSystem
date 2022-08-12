// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem_ConstantKeys.h"
#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CStringCache::f_AddConstantStringWithoutHash(CStr const &_String)
	{
		fp_AddConstantString(_String, _String.f_Hash());
	}

	void CStringCache::f_AddConstantString(CStringAndHash const &_String)
	{
		fp_AddConstantString(_String.m_String, _String.m_Hash);
	}

	void CStringCache::fp_AddConstantString(CStr const &_String, uint32 _Hash)
	{
		DMibFastCheck(_String.f_IsConstant());
		DMibFastCheck(m_Lock.f_OwnsLock());

		m_Strings(_Hash, _String);

		DMibFastCheck(m_Strings[_Hash].f_IsSameWeak(_String));
	}

	NStr::CStr const &CStringCache::f_AddString(CStr const &_String, uint32 _Hash)
	{
		DMibFastCheck(!m_Lock.f_OwnsLock());
		{
			DMibLockRead(m_Lock);
			auto *pStr = m_Strings.f_FindEqual(_Hash);
			if (pStr)
			{
				if (*pStr != _String)
					CBuildSystem::fs_ThrowError(CFilePosition(), "'{}' and '{}' resolves to the same string hash, please rename one of them"_f << _String << *pStr);

				return *pStr;
			}
		}

		DMibLock(m_Lock);
		auto &Return = *m_Strings(_Hash, _String);
		if (Return != _String)
			CBuildSystem::fs_ThrowError(CFilePosition(), "'{}' and '{}' resolves to the same string hash, please rename one of them"_f << _String << Return);

		return Return;
	}

	void CStringCache::f_AddConstantString(CPropertyKeyReference const &_Key)
	{
		fp_AddConstantString(_Key.m_Name, _Key.f_GetNameHash());
	}
}
