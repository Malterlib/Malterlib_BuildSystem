// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	void CBuildSystem::f_EvaluateAllGeneratorSettings(CEntity &_Entity) const
	{
		fpr_EvaluateAllGeneratorSettings(_Entity);
	}

	void CBuildSystem::fpr_EvaluateAllGeneratorSettings(CEntity &_Entity) const
	{
		if (_Entity.m_Key.m_Type == EEntityType_GeneratorSetting)
			fp_EvaluateAllProperties(_Entity);
		for (auto iChild = _Entity.m_ChildEntitiesOrdered.f_GetIterator(); iChild; ++iChild)
		{
			if (iChild->m_Key.m_Type == EEntityType_GeneratorSetting)
				fpr_EvaluateAllGeneratorSettings(*iChild);
		}
	}
}
