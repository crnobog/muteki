#pragma once

#include <memory>

namespace mu {

	template<typename ELEMENT>
	class ForwardRange {
		struct WrapperBase {
			virtual bool IsEmpty() = 0;
			virtual void Advance() = 0;
			virtual ELEMENT Front() = 0;
		};

		template<typename RANGE>
		class Wrapper : public WrapperBase {
			RANGE m_range;
		public:
			Wrapper(RANGE in_r)
				: m_range(std::move(in_r)) {}

			virtual bool IsEmpty() { return m_range.IsEmpty(); }
			virtual void Advance() { m_range.Advance(); }
			virtual ELEMENT Front() { return m_range.Front(); }
		};

		std::unique_ptr<WrapperBase> m_wrapper;
	public:

		template<typename RANGE>
		ForwardRange(RANGE&& in_r) {
			m_wrapper = std::make_unique<Wrapper<RANGE>>(std::forward<RANGE>(in_r));
		}

		bool IsEmpty() { return m_wrapper->IsEmpty(); }
		void Advance() { m_wrapper->Advance(); }
		ELEMENT Front() { return m_wrapper->Front(); }
	};

	template<typename RANGE>
	auto WrapRange(RANGE&& in_r) {
		typedef decltype(Range(in_r).Front()) ElementType;
		return ForwardRange<ElementType>(Range(std::forward<RANGE>(in_r)));
	}
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/RangeWrapper_Tests.inl"
#endif