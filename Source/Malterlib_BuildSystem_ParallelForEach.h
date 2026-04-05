// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NBuildSystem
{
	template <typename tf_CContainer, typename tf_CFunctor>
	NConcurrency::TCUnsafeFuture<void> DMibWorkaroundUBSanSectionErrorsDisable fg_ParallelForEach(tf_CContainer &&_Container, tf_CFunctor &&_fFunctor, bool _bSingleThreaded)
	{
		co_await NConcurrency::ECoroutineFlag_CaptureMalterlibExceptions;

		if (_Container.f_IsEmpty())
			co_return {};

		umint nConcurrency = NConcurrency::fg_ConcurrencyManager().f_GetConcurrency();
		if (nConcurrency <= 1 || _bSingleThreaded)
		{
			NConcurrency::TCFutureVector<void> Results;
			for (auto &Value : _Container)
				_fFunctor(Value) > Results;

			auto Result = fg_UnwrapFirst(co_await fg_AllDoneWrapped(Results));
			if (!Result)
				co_return fg_Move(Result).f_GetException();

			co_return {};
		}

		umint iValue = 0;

		NConcurrency::TCFutureVector<void> Results;

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
				> Results;
			;
		}

		iValue = 0;
		for (auto &Value : _Container)
		{
			if (iValue == 0)
				_fFunctor(Value) > Results;

			if (++iValue == nConcurrency)
				iValue = 0;
		}

		auto Result = fg_UnwrapFirst(co_await fg_AllDoneWrapped(Results));
		if (!Result)
			co_return fg_Move(Result).f_GetException();

		co_return {};
	}
}
