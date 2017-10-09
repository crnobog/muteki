#pragma once

#include <type_traits>

namespace mu {
	namespace details {
		// Helpers for calling members in variadic template expansion
		template<typename RANGE>
		struct RangeIsEmpty { bool operator()(const RANGE& r) { return r.IsEmpty(); } };

		template<typename RANGE>
		struct RangeAdvance { void operator()(RANGE& r) { r.Advance(); } };

		template<typename RANGE>
		struct RangeFront {
			auto operator()(RANGE& r) -> decltype(r.Front()) {
				return r.Front();
			}
		};

		template<typename RANGE>
		using RangeFrontType = decltype(std::declval<RANGE>().Front());

		// Adaptor for using ranges in begin-end based range-based for loops
		struct RangeSentinel {};

		template<typename RANGE>
		struct RangeIterator {
			RANGE m_range;

			RangeIterator(RANGE r) : m_range(std::move(r)) {}

			void operator++() { m_range.Advance(); }
			RangeFrontType<RANGE> operator*() { return m_range.Front(); }
			bool operator!=(const RangeSentinel&) { return !m_range.IsEmpty(); }
		};

		// TODO: Should be able to make this use a sentinal type for end() with VS2017
		template<typename RANGE>
		struct WithBeginEnd {
			auto begin() const { return RangeIterator<RANGE>{ *static_cast<const RANGE*>(this) }; }
			auto end() const { return RangeSentinel{}; }
		};
	}
}

#define MU_RANGE_ITERATION_SUPPORT \
	auto begin() { return *this; } \
	auto end() { return mu::details::RangeSentinel{}; } \
	void operator++() { Advance(); } \
	bool operator!=(mu::details::RangeSentinel) { return !IsEmpty(); } \
	auto operator*() { return Front(); } 