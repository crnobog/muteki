#pragma once

namespace mu {
	template<typename IN_RANGE, typename FUNC>
	class TransformRange : public details::WithBeginEnd<TransformRange<IN_RANGE, FUNC>> {
		IN_RANGE m_range;
		FUNC m_func;
	public:
		static constexpr bool HasSize = IN_RANGE::HasSize;

		TransformRange(IN_RANGE r, FUNC f)
			: m_range(std::move(r)), m_func(std::move(f)) {}

		bool IsEmpty() const { return m_range.IsEmpty(); }
		auto Front() { return m_func(m_range.Front()); }
		void Advance() { m_range.Advance(); }

		template<typename T = IN_RANGE, EnableIf<T::HasSize>...>
		size_t Size() const { return m_range.Size(); }
	};


	template<typename IN_RANGE, typename FUNC>
	auto Transform(IN_RANGE&& r, FUNC&& f) {
		return TransformRange<IN_RANGE, FUNC>(
			std::forward<std::decay<IN_RANGE>::type>(r),
			std::forward<std::decay<FUNC>::type>(f));
	}

}


#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/TransformRange_Tests.inl"
#endif