// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NBuildSystem
{
	template <typename tf_CStream>
	void CFindOptions::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_Path;
		_Stream << m_Attribs;
		_Stream << m_bRecursive;
		_Stream << m_bFollowLinks;
		_Stream << m_bExists;
		_Stream << m_Exclude;
	}

	template <typename tf_CStream>
	void CFindOptions::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_Path;
		_Stream >> m_Attribs;
		_Stream >> m_bRecursive;
		_Stream >> m_bFollowLinks;
		_Stream >> m_bExists;
		_Stream >> m_Exclude;
	}
}
