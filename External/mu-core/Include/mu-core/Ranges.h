#pragma once

#include "mu-core/Functors.h"
#include "mu-core/Metaprogramming.h"

#include "mu-core/RangeIteration.h"

#include <tuple>

// Prototype of a forward range:
//	template<typename T>
//	class ForwardRange
//	{
//		enum {HasSize = ?};
//
//		void Advance();
//		bool IsEmpty();
//		T& Front();
//		size_t Size(); // if HasSize == 1
//	};

namespace mu {
	
	template<typename R>
	auto MakeRangeIterator(R&& r) {
		typedef std::decay<R>::type RANGE_TYPE;
		return details::RangeIterator<RANGE_TYPE>(std::forward<RANGE_TYPE>(r));
	}

	inline auto MakeRangeSentinel() {
		return details::RangeSentinel{};
	}
}