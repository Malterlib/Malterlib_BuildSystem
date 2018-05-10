// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	namespace NBuild
	{
		class CMalterlibDependencyTracker
		{
		public:
			
			void f_AddInputFile(CStr const &_File);
			void f_AddOutputFile(CStr const &_File);
			void f_AddFind
				(
					CStr const &_SearchPattern
					, bool _bRecurse
					, bool _bFollowLinks
					, EFileAttrib _Attributes
					, TCVector<CStr> const &_Results
					, TCVector<CStr> const &_Excluded = fg_Default()
				)
			;
			
			void f_WriteDependencyFile(CStr const &_File);
			
		private:
			struct CDependencyFile
			{
				CStr m_Path;
				CTime m_Time;
			};
			struct CFind : public CDependencyFile
			{
				CStr m_Pattern;
				EFileAttrib m_Attributes;
				uint32 m_bRecurse;
				uint32 m_bFollowLinks;
				TCVector<CStr> m_Excluded;
				TCVector<CStr> m_Results;
			};
		private:
			TCVector<CStr> mp_Outputs;
			TCVector<CDependencyFile> mp_Inputs;
			TCVector<CFind> mp_Finds;
		};
	}
}


#ifndef DMibPNoShortCuts
using namespace NMib::NBuild;
#endif

