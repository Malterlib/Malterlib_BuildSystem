// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NBuildSystem
{
	template <typename tf_CContainer, typename tf_CFunctor>
	NConcurrency::TCFuture<void> DMibWorkaroundUBSanSectionErrorsDisable fg_ParallelForEach(tf_CContainer &&_Container, tf_CFunctor &&_fFunctor, bool _bSingleThreaded)
	{
		co_await (NConcurrency::ECoroutineFlag_AllowReferences | NConcurrency::ECoroutineFlag_CaptureExceptions);

		if (_Container.f_IsEmpty())
			co_return {};

		mint nConcurrency = NConcurrency::fg_ConcurrencyManager().f_GetConcurrency();
		if (nConcurrency <= 1 || _bSingleThreaded)
		{
			NConcurrency::TCActorResultVector<void> Results;
			for (auto &Value : _Container)
				_fFunctor(Value) > Results.f_AddResult();

			fg_UnwrapFirst(co_await Results.f_GetResults());

			co_return {};
		}

		mint iValue = 0;

		NConcurrency::TCActorResultVector<void> Results;

		for (auto &Value : _Container)
		{
			bool bSkip = false;
			if (iValue == 0)
				bSkip = true;

			if (++iValue == nConcurrency)
				iValue = 0;

			if (bSkip)
				continue;

			NConcurrency::g_Dispatch(NConcurrency::fg_OtherConcurrentActorLowPrio()) / [&, pValue = &Value]() -> NConcurrency::TCFuture<void>
				{
					return _fFunctor(*pValue);
				}
				> Results.f_AddResult();
			;
		}

		iValue = 0;
		for (auto &Value : _Container)
		{
			if (iValue == 0)
				_fFunctor(Value) > Results.f_AddResult();;

			if (++iValue == nConcurrency)
				iValue = 0;
		}

		fg_UnwrapFirst(co_await Results.f_GetResults());

		co_return {};
	}
}
