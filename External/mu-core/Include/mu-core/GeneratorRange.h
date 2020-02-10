#pragma once

#include "mu-core/Global.h"
#include "mu-core/RangeIteration.h"

#include <experimental/coroutine>

namespace mu {
	template<typename T>
	class GeneratorRange : public details::WithBeginEnd<GeneratorRange<T>> {
	public:
		struct promise_type {
			T* m_current = nullptr;

			auto& get_return_object() { return *this; }
			bool initial_suspend() { return true; }
			bool final_suspend() { return true; }
			void return_void() { }
			void yield_value(T& value) {
				m_current = std::addressof(value);
			}
			
			template<typename U>
			U&& await_transform(U&&) {
				static_assert(false, "co_await not supported in generator range");
			}
		};

		static constexpr bool HasSize		= false;
		static constexpr bool IsContiguous	= false;
		static constexpr bool IsIndexable	= false;

	private:
		using CoroutineHandle = std::experimental::coroutine_handle<promise_type>;
		CoroutineHandle m_coro = nullptr;

	public:
		GeneratorRange(promise_type& in_promise)
			: m_coro(CoroutineHandle::from_promise(in_promise))
		{
			Advance();
		}
		~GeneratorRange() {
			if (m_coro != nullptr) {
				m_coro.destroy();
			}
		}
		GeneratorRange(const GeneratorRange&) = delete;
		GeneratorRange& operator=(const GeneratorRange&) = delete;

		GeneratorRange(GeneratorRange&& other) 
		: m_coro(std::move(other.m_coro))
		{
			other.m_coro = nullptr;
		}

		void Advance() {
			Assert(m_coro != nullptr);
			if (!m_coro.done()) {
				m_coro.resume();
			}
		}
		void AdvanceBy(size_t num) {
			Assert(m_coro != nullptr);
			for (size_t i = 0; i < num; ++i) {
				Advance();
			}
		}

		bool IsEmpty() { return m_coro == nullptr || m_coro.done(); }
		T& Front() {
			return *m_coro.promise().m_current;
		}
		const T& Front() const {
			return *m_coro.promise().m_current;
		}
	};

	template<typename T> 
	FORCEINLINE auto Range(GeneratorRange<T> r) { return std::forward<GeneratorRange<T>>(r); }
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/GeneratorRange_Tests.inl"
#endif