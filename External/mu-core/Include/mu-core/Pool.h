#pragma once

#include "Array.h"
#include "BitArray.h"
#include "Debug.h"
#include "PrimitiveTypes.h"
#include "mu-core/RangeIteration.h"

#include <intrin.h>

namespace mu {

	// Externally synchronized fixed-size pool
	// TODO: Think about name, use of destructors, should be moved out of core?
	template<typename ElementType, typename IndexType=size_t>
	class Pool {
		ElementType* m_elements = nullptr;
		BitArray m_flags;
		size_t m_max = 0;
		size_t m_free = 0;

	public:
		Pool() {}

		Pool(size_t max_elements) : m_max(max_elements) {
			m_elements = (ElementType*)malloc(max_elements * sizeof(ElementType));
			m_flags.Init(max_elements, false);
			m_free = max_elements;
		}

		~Pool() {
			Empty();
		}

		Pool(const Pool& other) = delete;
		Pool(Pool&& other) = delete;
		Pool& operator=(const Pool& other) = delete;
		Pool& operator=(Pool&& other) = delete;

		size_t GetMax() const { return m_max; }
		size_t GetFreeCount() const { return m_free; }

		IndexType AddDefaulted() {
			size_t index = AllocateIndex();
			new(&m_elements[index]) ElementType;
			return IndexType{ index };
		}

		template<typename... TS>
		IndexType Emplace(TS&&... ts) {
			size_t index = AllocateIndex();
			new(&m_elements[index]) ElementType(std::forward<TS>(ts)...); // TODO: Replace with uniform initialization?
			return IndexType{ index };
		}

		void Return(IndexType i) {
			size_t index = (size_t)i;
			Assert(m_flags.GetBit(index));
			Assert(index < m_max);
			++m_free;
			m_flags.ClearBit(index);
			m_elements[index].~ElementType();
		}

		void Empty() {
			for (size_t index : m_flags.GetSetBits()) {
				m_elements[index].~ElementType();
			}

			if (m_elements) { free(m_elements); }
			m_elements = nullptr;
			m_flags.Empty();
			m_max = 0;
			m_free = 0;
		}

		ElementType& operator[](IndexType index) {
			return m_elements[(size_t)index];
		}
		const ElementType& operator[](IndexType index) const {
			return m_elements[(size_t)index];
		}
		
	protected:
		size_t AllocateIndex() {
			Assert(m_free > 0);
			--m_free;
			auto r = m_flags.GetClearBits();
			Assert(!r.IsEmpty());
			
			size_t index = r.Front();
			m_flags.SetBit(index);
			return index;
		}

		template<bool IsConst>
		struct RangeTypes {
			typedef Pool& PoolType;
			typedef ElementType& ElementRef;
			typedef ElementType* ElementPtr;
		};

		template<>
		struct RangeTypes<true> {
			typedef const Pool& PoolType;
			typedef const ElementType& ElementRef;
			typedef const ElementType* ElementPtr;
		};

		template<bool IsConst>
		struct PoolRangeBase : public details::WithBeginEnd<PoolRangeBase<IsConst>> {
			BitRange<true> m_flags;
			typename RangeTypes<IsConst>::ElementPtr m_elements = nullptr;

			PoolRangeBase(typename RangeTypes<IsConst>::PoolType p)
				: m_flags(p.m_flags.GetSetBits())
				, m_elements(p.m_elements) {}
			bool IsEmpty() { return m_flags.IsEmpty(); }
			void Advance() { m_flags.Advance(); }
			std::tuple<IndexType, typename RangeTypes<IsConst>::ElementRef> Front() {
				size_t index = m_flags.Front();
				return { IndexType(index), m_elements[index] };
			};
		};
		typedef PoolRangeBase<false> PoolRange;
		typedef PoolRangeBase<true> ConstPoolRange;
	public:
		PoolRange Range() { return PoolRange(*this); }
		ConstPoolRange Range() const { return ConstPoolRange(*this); }
	};

	template<typename T, typename IndexType>
	auto Range(Pool<T, IndexType>& pool) {
		return pool.Range();
	}

	template<typename T, typename IndexType>
	auto Range(const Pool<T, IndexType>& pool) {
		return pool.Range();
	}
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/Pool_Tests.inl"
#endif