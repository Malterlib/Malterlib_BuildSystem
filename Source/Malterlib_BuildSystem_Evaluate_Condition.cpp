// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_BuildSystem.h"

namespace NMib::NBuildSystem
{
	bool CBuildSystem::f_EvalCondition(CEntity &_Context, CCondition const &_Condition, bool _bTrace) const
	{
		DMibRequire(_Condition.m_Type == EConditionType_Root);
		CEvaluationContext EvalContext(&_Context.m_EvaluatedProperties);
		return fpr_EvalCondition(_Context, _Context, _Condition, EvalContext, _bTrace, nullptr);
	}

	ch8 const *CCondition::fs_ConditionTypeToStr(EConditionType _Type)
	{
		switch (_Type)
		{
		case EConditionType_Root: return "";
		case EConditionType_MatchEqual: return "<==>";
		case EConditionType_MatchNotEqual: return "<!=>";
		case EConditionType_CompareEqual: return "==";
		case EConditionType_CompareNotEqual: return "!=";
		case EConditionType_CompareLessThan: return "<";
		case EConditionType_CompareLessThanEqual: return "<=";
		case EConditionType_CompareGreaterThan: return ">";
		case EConditionType_CompareGreaterThanEqual: return ">=";
		case EConditionType_Or: return "|";
		case EConditionType_And: return "&";
		case EConditionType_Not: return "!";
		}
		return "";
	}

	bool CBuildSystem::fsp_CompareValueRecursive
		(
			CEJSON const &_Left
			, CEJSON const &_Right
			, EConditionType _ConditionType
			, NFunction::TCFunctionNoAlloc<void (NStr::CStr const &_Error)> const &_fOnError
		) const
	{
		if (_Left.f_IsUserType())
			_fOnError("Left hand side cannot contain user type ({})"_f << _Left.f_UserType().m_Type);

		if (_Right.f_IsUserType())
		{
			auto &UserType = _Right.f_UserType();
			if (UserType.m_Type != "Wildcard")
				_fOnError("Unknow user type in value");

			if (_ConditionType != EConditionType_MatchEqual && _ConditionType != EConditionType_MatchNotEqual)
				_fOnError("Only match or not equal match can be used with a wildcard match");

			if (!UserType.m_Value.f_IsString())
				_fOnError("Expected string in wildcard type");

			if (!_Left.f_IsString())
				_fOnError("Expected string for wilcard comparison, got: {}"_f << _Left);

			bool bRet = NStr::fg_StrMatchWildcard
				(
					_Left.f_String().f_GetStr()
					, UserType.m_Value.f_String().f_GetStr()
				)
				== NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted
			;

			if (_ConditionType == EConditionType_MatchEqual)
				return bRet;
			else
				return !bRet;
		}

		if (_Left.f_EType() != _Right.f_EType())
		{
			if (_ConditionType == EConditionType_MatchEqual)
				return false;
			else if (_ConditionType == EConditionType_MatchNotEqual)
				return true;

			_fOnError("Trying to compare values of different types:\n{}\n{}"_f << _Left << _Right);
		}

		if (_Left.f_IsObject())
		{
			switch (_ConditionType)
			{
			case EConditionType_MatchEqual:
			case EConditionType_MatchNotEqual:
			case EConditionType_CompareEqual:
			case EConditionType_CompareNotEqual:
				{
					bool bReturnTrue = _ConditionType == EConditionType_CompareEqual || _ConditionType == EConditionType_MatchEqual;
					bool bReturnFalse = !bReturnTrue;

					auto iLeft = _Left.f_Object().f_SortedIterator();
					auto iRight = _Right.f_Object().f_SortedIterator();
					for (; iLeft && iRight; ++iLeft, ++iRight)
					{
						if (iLeft->f_Name() != iRight->f_Name())
							return bReturnFalse;
						if (!fsp_CompareValueRecursive(iLeft->f_Value(), iRight->f_Value(), _ConditionType, _fOnError))
							return bReturnFalse;
					}

					if (!!iLeft != !!iRight)
						return bReturnFalse;

					return bReturnTrue;
				}
			case EConditionType_CompareLessThan:
			case EConditionType_CompareGreaterThan:
			case EConditionType_CompareLessThanEqual:
			case EConditionType_CompareGreaterThanEqual:
				{
					bool bResult = false;
					{
						decltype(_Left.f_Object().f_SortedIterator()) iLeft;
						decltype(_Left.f_Object().f_SortedIterator()) iRight;

						if (_ConditionType == EConditionType_CompareLessThanEqual || _ConditionType == EConditionType_CompareGreaterThan)
						{
							iLeft = _Right.f_Object().f_SortedIterator();
							iRight = _Left.f_Object().f_SortedIterator();
						}
						else
						{
							iLeft = _Left.f_Object().f_SortedIterator();
							iRight = _Right.f_Object().f_SortedIterator();
						}

						for (; iLeft && iRight; ++iLeft, ++iRight)
						{
							if (iLeft->f_Name() < iRight->f_Name())
							{
								bResult = true;
								break;
							}
							else if (iRight->f_Name() < iLeft->f_Name())
								break;

							if (fsp_CompareValueRecursive(iLeft->f_Value(), iRight->f_Value(), EConditionType_CompareLessThan, _fOnError))
							{
								bResult = true;
								break;
							}
							else if (fsp_CompareValueRecursive(iRight->f_Value(), iLeft->f_Value(), EConditionType_CompareLessThan, _fOnError))
								break;
						}
						if (!iLeft && !!iRight)
							bResult = true;
					}

					if (_ConditionType == EConditionType_CompareLessThanEqual || _ConditionType == EConditionType_CompareGreaterThanEqual)
						return !bResult;
					return bResult;
				}
			default:
				DMibNeverGetHere;
				break;
			}
		}
		else if (_Left.f_IsArray())
		{
			switch (_ConditionType)
			{
			case EConditionType_MatchEqual:
			case EConditionType_MatchNotEqual:
			case EConditionType_CompareEqual:
			case EConditionType_CompareNotEqual:
				{
					bool bReturnTrue = _ConditionType == EConditionType_CompareEqual || _ConditionType == EConditionType_MatchEqual;
					bool bReturnFalse = !bReturnTrue;

					auto iLeft = _Left.f_Array().f_GetIterator();
					auto iRight = _Right.f_Array().f_GetIterator();
					for (; iLeft && iRight; ++iLeft, ++iRight)
					{
						if (!fsp_CompareValueRecursive(*iLeft, *iRight, _ConditionType, _fOnError))
							return bReturnFalse;
					}
					if (!!iLeft != !!iRight)
						return bReturnFalse;
					return bReturnTrue;
				}
			case EConditionType_CompareLessThan:
			case EConditionType_CompareGreaterThan:
			case EConditionType_CompareLessThanEqual:
			case EConditionType_CompareGreaterThanEqual:
				{
					bool bResult = false;
					{
						decltype(_Left.f_Array().f_GetIterator()) iLeft;
						decltype(_Left.f_Array().f_GetIterator()) iRight;

						if (_ConditionType == EConditionType_CompareLessThanEqual || _ConditionType == EConditionType_CompareGreaterThan)
						{
							iLeft = _Right.f_Array().f_GetIterator();
							iRight = _Left.f_Array().f_GetIterator();
						}
						else
						{
							iLeft = _Left.f_Array().f_GetIterator();
							iRight = _Right.f_Array().f_GetIterator();
						}

						for (; iLeft && iRight; ++iLeft, ++iRight)
						{
							if (fsp_CompareValueRecursive(*iLeft, *iRight, EConditionType_CompareLessThan, _fOnError))
							{
								bResult = true;
								break;
							}
							else if (fsp_CompareValueRecursive(*iRight, *iLeft, EConditionType_CompareLessThan, _fOnError))
								break;
						}
						if (!iLeft && !!iRight)
							bResult = true;
					}

					if (_ConditionType == EConditionType_CompareLessThanEqual || _ConditionType == EConditionType_CompareGreaterThanEqual)
						return !bResult;
					return bResult;
				}
			default:
				DMibNeverGetHere;
				break;
			}
		}

		switch (_ConditionType)
		{
		case EConditionType_MatchEqual:
		case EConditionType_CompareEqual:
			return _Left == _Right;
		case EConditionType_MatchNotEqual:
		case EConditionType_CompareNotEqual:
			return _Left != _Right;
		case EConditionType_CompareLessThan: return _Left < _Right;
		case EConditionType_CompareLessThanEqual: return _Left <= _Right;
		case EConditionType_CompareGreaterThan: return _Left > _Right;
		case EConditionType_CompareGreaterThanEqual: return _Left >= _Right;
		default:
			DMibNeverGetHere;
			break;
		}

		return true;
	}

	bool CBuildSystem::fp_EvalConditionSubject
		(
			CEntity &_Context
			, CEntity &_OriginalContext
			, CCondition const &_Condition
			, CEvaluationContext &_EvalContext
			, mint _TraceDepth
			, EConditionType _ConditionType
			, CEvalPropertyValueContext const *_pParentContext
		) const
	{
		CEvalPropertyValueContext Context{_Context, _OriginalContext, _Condition.m_Position, _EvalContext, _pParentContext};
		CEJSON LeftValue;
		CEJSON RightValue;

		LeftValue = fp_EvaluatePropertyValue(Context, _Condition.m_Left, nullptr);
		RightValue = fp_EvaluatePropertyValue(Context, _Condition.m_Right, nullptr);
		bool bRet = fsp_CompareValueRecursive
			(
				LeftValue
				, RightValue
				, _ConditionType
				, [&](CStr const &_Error)
				{
					CBuildSystem::fs_ThrowError(_Condition.m_Position, _Error);
				}
			)
		;

		if (_TraceDepth)
		{
			DConOut("{sj*}         {}" DNewLine, "" << _TraceDepth*3 << _Context.f_GetPath());
			DConOut("{sj*}         {}" DNewLine, "" << _TraceDepth*3 << _OriginalContext.f_GetPath());

			if (bRet)
			{
				DConOut
					(
						"{sj*}         {} {} {}" DNewLine
						, ""
						<< _TraceDepth * 3
						<< LeftValue.f_ToString(nullptr, EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat)
						<< CCondition::fs_ConditionTypeToStr(_ConditionType)
						<< RightValue.f_ToString(nullptr, EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat)
					)
				;
			}
			else
			{
				DConOut
					(
						"{sj*}       ! {} {} {}" DNewLine
						, ""
						<< _TraceDepth * 3
						<< LeftValue.f_ToString(nullptr, EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat)
						<< CCondition::fs_ConditionTypeToStr(_ConditionType)
						<< RightValue.f_ToString(nullptr, EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat)
					)
				;
			}
		}

		return bRet;
	}

	bool CBuildSystem::fpr_EvalCondition
		(
			CEntity &_Context
			, CEntity &_OriginalContext
			, CCondition const &_Condition
			, CEvaluationContext &_EvalContext
			, mint _TraceDepth
			, CEvalPropertyValueContext const *_pParentContext
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
					if (fpr_EvalCondition(_Context, _OriginalContext, Condition, _EvalContext, _TraceDepth ? _TraceDepth + 1 : 0, _pParentContext))
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
					if (!fpr_EvalCondition(_Context, _OriginalContext, Condition, _EvalContext, _TraceDepth ? _TraceDepth + 1 : 0, _pParentContext))
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
					bool bRet = !fpr_EvalCondition(_Context, _OriginalContext, Condition, _EvalContext, _TraceDepth ? _TraceDepth + 1 : 0, _pParentContext);
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
		case EConditionType_MatchEqual:
		case EConditionType_MatchNotEqual:
		case EConditionType_CompareEqual:
		case EConditionType_CompareNotEqual:
		case EConditionType_CompareLessThan:
		case EConditionType_CompareLessThanEqual:
		case EConditionType_CompareGreaterThan:
		case EConditionType_CompareGreaterThanEqual:
			{
				bool bRet = fp_EvalConditionSubject(_Context, _OriginalContext, _Condition, _EvalContext, _TraceDepth, _Condition.m_Type, _pParentContext);
				if (_TraceDepth)
				{
					if (bRet)
						DConOut("{sj*}Success: {} {} {}" DNewLine, "" << _TraceDepth*3 << _Condition.m_Left << CCondition::fs_ConditionTypeToStr(_Condition.m_Type) << _Condition.m_Right);
					else
						DConOut("{sj*}Failure: {} {} {}" DNewLine, "" << _TraceDepth*3 << _Condition.m_Left << CCondition::fs_ConditionTypeToStr(_Condition.m_Type) << _Condition.m_Right);
				}
				return bRet;
			}
		}
		DNeverGetHere;
		return false;
	}
}
