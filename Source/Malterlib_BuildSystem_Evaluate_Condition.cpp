// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	bool CBuildSystem::f_EvalCondition(CEntity const &_Context, CCondition const &_Condition) const
	{
		DMibRequire(_Condition.m_Type == EConditionType_Root);
		DMibLock(_Context.m_Lock);
		CEvaluationContext EvalContext(&_Context.m_EvaluatedProperties);
		return fpr_EvalCondition(_Context, _Context, _Condition, EvalContext, false);
	}

	bool CBuildSystem::fp_EvalConditionSubject
		(
			CEntity const &_Context
			, CEntity const &_OriginalContext
			, CCondition const &_Condition
			, CEvaluationContext &_EvalContext
			, mint _TraceDepth
		) const
	{
		{
			CStr Value = fp_EvaluatePropertyValue(_Context, _OriginalContext, _Condition.m_Value, _Condition.m_Position, _EvalContext);
			CStr CompareToValue = fp_GetPropertyValue(_Context, _OriginalContext, _Condition.m_Subject, _Condition.m_Position, _EvalContext);

			ch8 const *pParse = Value;

			bool bRet;
			if (*pParse == '~')
			{
				// Wildcard search
				CStr WildCard = Value.f_Extract(1);

				bRet = NStr::fg_StrMatchWildcard(CompareToValue.f_GetStr(), WildCard.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted;
			}
			else 
				bRet = CompareToValue == Value;

			if (_TraceDepth)
			{
				DConOut("{sj*}         {}" DNewLine, "" << _TraceDepth*3 << _Context.f_GetPath());
				DConOut("{sj*}         {}" DNewLine, "" << _TraceDepth*3 << _OriginalContext.f_GetPath());
				if (bRet)
				{
					DConOut("{sj*}         {}" DNewLine, "" << _TraceDepth*3 << Value.f_EscapeStr());
					DConOut("{sj*}         ==" DNewLine, "" << _TraceDepth*3);
					DConOut("{sj*}         {}" DNewLine, "" << _TraceDepth*3 << CompareToValue.f_EscapeStr());
				}
				else
				{
					DConOut("{sj*}         {}" DNewLine, "" << _TraceDepth*3 << Value.f_EscapeStr());
					DConOut("{sj*}         !=" DNewLine, "" << _TraceDepth*3);
					DConOut("{sj*}         {}" DNewLine, "" << _TraceDepth*3 << CompareToValue.f_EscapeStr());
				}
			}

			return bRet;				
		}
	}

	bool CBuildSystem::fpr_EvalCondition
		(
			CEntity const &_Context
			, CEntity const &_OriginalContext
			, CCondition const &_Condition
			, CEvaluationContext &_EvalContext
			, mint _TraceDepth
		) const
	{
		switch (_Condition.m_Type)
		{
		case EConditionType_Root:
		case EConditionType_Or:
			{
				if (_TraceDepth)
				{
					if (_Condition.m_Type == EConditionType_Root)
						DConOut("{sj*}Root" DNewLine, "" << _TraceDepth*3);
					else
						DConOut("{sj*}Or" DNewLine, "" << _TraceDepth*3);
				}
				for (auto iCondition = _Condition.m_Children.f_GetIterator(); iCondition; ++iCondition)
				{
					auto &Condition = *iCondition;
					if (fpr_EvalCondition(_Context, _OriginalContext, Condition, _EvalContext, _TraceDepth ? _TraceDepth + 1 : 0))
					{
						if (_TraceDepth)
						{
							if (_Condition.m_Type == EConditionType_Root)
								DConOut("{sj*}Success" DNewLine, "" << _TraceDepth*3);
							else
								DConOut("{sj*}Success" DNewLine, "" << _TraceDepth*3);
						}
						return true;
					}
				}
				if (_Condition.m_Type == EConditionType_Root)
				{
					if (_Condition.m_Children.f_IsEmpty())
					{
						if (_TraceDepth)
						{
							if (_Condition.m_Type == EConditionType_Root)
								DConOut("{sj*}Success" DNewLine, "" << _TraceDepth*3);
							else
								DConOut("{sj*}Success" DNewLine, "" << _TraceDepth*3);
						}
						return true;
					}
				}
				if (_TraceDepth)
				{
					if (_Condition.m_Type == EConditionType_Root)
						DConOut("{sj*}Failure" DNewLine, "" << _TraceDepth*3);
					else
						DConOut("{sj*}Failure" DNewLine, "" << _TraceDepth*3);
				}

				return false;
			}
			break;
		case EConditionType_And:
			{
				if (_TraceDepth)
					DConOut("{sj*}And" DNewLine, "" << _TraceDepth*3);
				for (auto iCondition = _Condition.m_Children.f_GetIterator(); iCondition; ++iCondition)
				{
					auto &Condition = (*iCondition);
					if (!fpr_EvalCondition(_Context, _OriginalContext, Condition, _EvalContext, _TraceDepth ? _TraceDepth + 1 : 0))
					{
						if (_TraceDepth)
							DConOut("{sj*}Failure" DNewLine, "" << _TraceDepth*3);
						return false;
					}
				}
				if (_TraceDepth)
					DConOut("{sj*}Success" DNewLine, "" << _TraceDepth*3);
				return true;
			}
			break;
		case EConditionType_Not:
			{
				if (_TraceDepth)
					DConOut("{sj*}Not" DNewLine, "" << _TraceDepth*3);

				for (auto iCondition = _Condition.m_Children.f_GetIterator(); iCondition; ++iCondition)
				{
					auto &Condition = (*iCondition);
					bool bRet = !fpr_EvalCondition(_Context, _OriginalContext, Condition, _EvalContext, _TraceDepth ? _TraceDepth + 1 : 0);
					if (_TraceDepth)
					{
						if (bRet)
							DConOut("{sj*}Success" DNewLine, "" << _TraceDepth*3);
						else
							DConOut("{sj*}Failure" DNewLine, "" << _TraceDepth*3);
					}
					

					return bRet;
				}
				DNeverGetHere;
			}
			break;
		case EConditionType_Compare:
			{
				bool bRet = fp_EvalConditionSubject(_Context, _OriginalContext, _Condition, _EvalContext, _TraceDepth);
				if (_TraceDepth)
				{
					if (bRet)
						DConOut("{sj*}Success: {} == {}" DNewLine, "" << _TraceDepth*3 << _Condition.m_Subject.f_EscapeStr() << _Condition.m_Value.f_EscapeStr());
					else
						DConOut("{sj*}Failure: {} != {}" DNewLine, "" << _TraceDepth*3 << _Condition.m_Subject.f_EscapeStr() << _Condition.m_Value.f_EscapeStr());
				}
				return bRet;
			}
		case EConditionType_CompareNot:
			{
				bool bRet = !fp_EvalConditionSubject(_Context, _OriginalContext, _Condition, _EvalContext, _TraceDepth);
				if (_TraceDepth)
				{
					if (bRet)
						DConOut("{sj*}Success: {} != {}" DNewLine, "" << _TraceDepth*3 << _Condition.m_Subject.f_EscapeStr() << _Condition.m_Value.f_EscapeStr());
					else
						DConOut("{sj*}Failure: {} == {}" DNewLine, "" << _TraceDepth*3 << _Condition.m_Subject.f_EscapeStr() << _Condition.m_Value.f_EscapeStr());
				}
				return bRet;
			}
		default:
			{
				DNeverGetHere;
			}
			break;
		}
		return false;
	}
}
