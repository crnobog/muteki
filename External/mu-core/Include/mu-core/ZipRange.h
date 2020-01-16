#pragma once

#include "mu-core/Functors.h"
#include "mu-core/Metaprogramming.h"

namespace mu {
	namespace details {
		// Functor for folding over ranges of finite/infinite size and picking the minimum size
		template<typename RANGE>
		struct RangeMinSizeFolder {
			size_t operator()(size_t s, const RANGE& r) const {
				if constexpr (RANGE::HasSize) {
					size_t rs = r.Size();
					return rs < s ? rs : s;
				}
				else
				{
					return s;
				}
			}
		};

	}

	// ZipRange combines multiple ranges and iterates them in lockstep
	template<typename... RANGES>
	class ZipRange : public details::WithBeginEnd<ZipRange<RANGES...>> {
		std::tuple<RANGES...> m_ranges;

	public:
		static constexpr bool HasSize = mu::functor::FoldOr(RANGES::HasSize...);
		static constexpr bool IsContiguous = false;
		static constexpr bool IsIndexable = false; // TODO: If all ranges are indexable

		ZipRange(RANGES... ranges) : m_ranges(ranges...) {}

		bool IsEmpty() const {
			return mu::functor::FoldOr(
				mu::functor::FMap<details::RangeIsEmpty>(m_ranges)
			);
		}

		void Advance() {
			mu::functor::FMapVoid<details::RangeAdvance>(m_ranges);
		}

		auto Front() {
			return mu::functor::FMap<details::RangeFront>(m_ranges);
		}

		template<typename T = ZipRange, EnableIf<T::HasSize>...>
		size_t Size() const {
			return mu::functor::Fold<details::RangeMinSizeFolder>(
				std::numeric_limits<size_t>::max(), m_ranges);
		}
	};

	template<typename... RANGES>
	auto Zip(RANGES&&... ranges) {
		return ZipRange<decltype(Range(std::forward<RANGES>(ranges)))...>(Range(std::forward<RANGES>(ranges))...);
	}
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/ZipRange_Tests.inl"
#endif