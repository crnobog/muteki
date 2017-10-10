#pragma once

#include "mu-core/Global.h"
#include "mu-core/RangeIteration.h"

namespace mu {

	// A linear range over raw memory of a certain type
	//	- Forward range
	//	- Indexable range
	template<typename T>
	class PointerRange : public details::WithBeginEnd<PointerRange<T>> {
	public:
		T* m_start, *m_end;

		static constexpr bool HasSize = true;
		static constexpr bool IsContiguous = true;
		static constexpr bool IsIndexable = true;

		template<class U>
		friend class PointerRange;

		FORCEINLINE PointerRange()
			: m_start(nullptr)
			, m_end(nullptr) {}

		FORCEINLINE PointerRange(T* start, T* end)
			: m_start(start)
			, m_end(end) {}

		template<typename U>
		FORCEINLINE PointerRange(PointerRange<U> other) {
			m_start = other.m_start;
			m_end = other.m_end;
		}

		FORCEINLINE void Advance() { ++m_start; }
		FORCEINLINE void AdvanceBy(size_t num) { m_start += num; }

		FORCEINLINE bool IsEmpty() const { return m_start >= m_end; }
		FORCEINLINE T& Front() { return *m_start; }
		FORCEINLINE const T& Front() const { return *m_start; }
		FORCEINLINE size_t Size() const { return m_end - m_start; }

		FORCEINLINE T& operator[](size_t index) { return m_start[index]; }
		FORCEINLINE const T& operator[](size_t index) const { return m_start[index]; }

		template<typename U>
		bool operator==(const PointerRange<U>& other) const {
			return m_start == other.m_start
				&& m_end == other.m_end;
		}

		template<typename U>
		bool operator!=(const PointerRange<U>& other) const {
			return m_start != other.m_start
				|| m_end != other.m_end;
		}
	};

	// A PointerRange can be used by algorithms with no further conversion
	template<typename T>
	FORCEINLINE auto Range(PointerRange<T> r) { return r; }

	template<typename T>
	class Array;

	template<typename T, size_t MAX>
	class FixedArray;

	template<typename T>
	FORCEINLINE auto Range(T* ptr, size_t num) { return Range(ptr, ptr + num); }

	template<typename T>
	FORCEINLINE auto Range(T* start, T* end) { return PointerRange<T>(start, end); }

	template<typename T, size_t SIZE>
	FORCEINLINE auto Range(T(&arr)[SIZE]) { return Range(arr, arr + SIZE); }

	template<typename T>
	FORCEINLINE auto Range(Array<T>& arr) { return Range(arr.Data(), arr.Num()); }

	template<typename T>
	FORCEINLINE auto Range(const Array<T>& arr) { return Range(arr.Data(), arr.Num()); }

	template<typename T, size_t MAX>
	FORCEINLINE auto Range(FixedArray<T, MAX>& arr) { return Range(arr.Data(), arr.Num()); }

	template<typename T, size_t MAX>
	FORCEINLINE auto Range(const FixedArray<T, MAX>& arr) { return Range(arr.Data(), arr.Num()); }

	template<typename T>
	FORCEINLINE auto ByteRange(const T& t) { return Range((const u8*)&t, sizeof(T)); }

	template<typename T>
	FORCEINLINE auto ByteRange(T* t, size_t num) { return Range((u8*)t, num * sizeof(T)); }

	template<typename T>
	FORCEINLINE auto ByteRange(const T* t, size_t num) { return Range((const u8*)t, num * sizeof(T)); }

	template<typename T>
	FORCEINLINE auto ByteRange(T* t) { return Range((u8*)t, sizeof(T)); }
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/PointerRange_Tests.inl"
#endif
