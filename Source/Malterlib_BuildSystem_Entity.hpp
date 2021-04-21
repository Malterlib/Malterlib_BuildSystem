// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	bool CEntityKey::operator < (CEntityKey const &_Right) const
	{
		if (m_Type < _Right.m_Type)
			return true;
		else if (m_Type > _Right.m_Type)
			return false;
		return m_Name < _Right.m_Name;
	}

	bool CEntityKey::operator == (CEntityKey const &_Right) const
	{
		if (m_Type != _Right.m_Type)
			return false;
		if (m_Name != _Right.m_Name)
			return false;
		return true;
	}
	
	void CEntity::f_CheckParents() const
	{
		fpr_CheckParents();
	}
}
