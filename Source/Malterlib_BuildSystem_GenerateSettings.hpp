// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	template <typename tf_CStream>
	void CGenerateSettings::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_SourceFile;
		_Stream << m_OutputDir;
		_Stream << m_Generator;
		_Stream << m_Workspace;
		_Stream << m_Action;
		_Stream << m_GenerationFlags;
		_Stream << m_ActionParams;
	}

	template <typename tf_CStream>
	void CGenerateSettings::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_SourceFile;
		_Stream >> m_OutputDir;
		_Stream >> m_Generator;
		_Stream >> m_Workspace;
		_Stream >> m_Action;
		_Stream >> m_GenerationFlags;
		_Stream >> m_ActionParams;
	}
}
