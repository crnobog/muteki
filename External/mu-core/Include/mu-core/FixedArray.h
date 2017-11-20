#pragma once

#include "mu-core/Algorithms.h"
#include "mu-core/Debug.h"
#include "mu-core/Ranges.h"

namespace mu {
	template<typename T, size_t MAX>
	class FixedArrayBase {
	protected:
		static constexpr size_t DataSize = sizeof(T) * MAX;
		u8 m_data[DataSize];
		size_t m_num = 0;
	};

	template<typename T, size_t MAX, bool TRIVIAL_DESTRUCTOR>
	class FixedArrayDestructorMixin : public FixedArrayBase<T, MAX> {};

	template<typename T, size_t MAX>
	class FixedArrayDestructorMixin<T, MAX, false> : public FixedArrayBase<T, MAX> {
	protected:
		~FixedArrayDestructorMixin() {
			T* start = (T*)m_data;
			T* end = start + m_num;
			Destroy(Range(start, end));
		}
	};

	template<typename T, size_t MAX>
	class FixedArray : protected FixedArrayDestructorMixin<T, MAX, std::is_trivially_destructible_v<T>> {

	public:
		FixedArray() {}
		FixedArray(std::initializer_list<T> init) {
			Add(init);
		}
		FixedArray(const FixedArray& other) {
			AddRange(Range(other));
		}
		FixedArray(FixedArray&& other) {
			for (T& t : other) {
				Emplace(std::move(t));
			}
		}

		FixedArray& operator=(const FixedArray& other) {
			Empty();
			AddRange(Range(other));
			return *this;
		}
		FixedArray& operator=(FixedArray&& other) {
			Empty();
			for (T& t : other) {
				Emplace(std::move(t));
			}
			return *this;
		}

		size_t Num() const { return m_num; }
		T* Data() { return (T*)m_data; }
		const T* Data() const { return (T*)m_data; }
		T& operator[](size_t index) {
			Assert(index < m_num);
			return ((T*)m_data)[index];
		}
		const T& operator[](size_t index) const {
			Assert(index < m_num);
			return ((T*)m_data)[index];
		}

		PointerRange<T> AddZeroed(size_t count) {
			Assert(GetSlack() >= count);
			T* start = Data() + m_num;
			m_num += count;
			memset(start, 0, sizeof(T)*count);
			return { start, start + count };
		}

		PointerRange<T> AddDefaulted(size_t count) {
			Assert(GetSlack() >= count);
			T* start = Data() + m_num;
			m_num += count;
			PointerRange<T> r{ start, start + count };
			for (T& t : r) {
				new(&t) T{};
			}
			return r;
		}

		PointerRange<T> AddUninitialized(size_t count) {
			Assert(GetSlack() >= count);
			T* start = Data() + m_num;
			m_num += count;
			return { start, start + count };
		}

		template<typename RANGE>
		void AddRange(RANGE r) {
			for (; !r.IsEmpty(); r.Advance()) {
				Add(r.Front());
			}
		}
		void Add(std::initializer_list<T> elems) {
			for (const T& elem : elems) {
				Add(elem);
			}
		}
		template<typename... TS>
		void Emplace(TS&&... ts) {
			new(AddInternal()) T(std::forward<TS>(ts)...);
		}

		size_t Add(const T& element) {
			Assert(m_num < MAX);
			new(AddInternal()) T(element);
			return m_num - 1;
		}
		PointerRange<T> AddMany(size_t count, const T& element) {
			size_t start = m_num;
			for (size_t i = 0; i < count; ++i) {
				Add(element);
			}
			T* d = (T*)m_data;
			return PointerRange<T>{ d + start, d + m_num };
		}
		void Empty() {
			for (size_t i = 0; i < m_num; ++i) {
				(*this)[i].~T();
			}
			m_num = 0;
		}

		PointerRange<u8> Bytes() { return ByteRange((u8*)m_data, m_num * sizeof(T)); }
		PointerRange<const u8> Bytes() const { return ByteRange((u8*)m_data, m_num * sizeof(T)); }

		size_t GetSlack() const { return MAX - m_num; }

		auto begin() { return mu::MakeRangeIterator(mu::Range((T*)m_data, m_num)); }
		auto end() { return mu::MakeRangeSentinel(); }

		auto begin() const { return mu::MakeRangeIterator(mu::Range((T*)m_data, m_num)); }
		auto end() const { return mu::MakeRangeSentinel(); }

	private:
		T* AddInternal() {
			T* t = ((T*)m_data) + m_num;
			++m_num;
			return t;
		}
	};
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/FixedArray_Tests.inl"
#endif
