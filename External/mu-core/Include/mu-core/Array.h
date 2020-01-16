#pragma once

#include <initializer_list>

#include "mu-core/PointerRange.h"
#include "mu-core/Algorithms.h"
#include "mu-core/PrimitiveTypes.h"

namespace mu {
	template<typename T>
	class Array {
		T* m_data = nullptr;
		size_t m_num = 0;
		size_t m_max = 0;

	public:
		Array() {}

		Array(std::initializer_list<T> init) {
			InitEmpty(init.size());
			for (auto&& item : init) {
				new(&AddSafe()) T(item);
			}
		}

		Array(const Array& other) {
			InitEmpty(other.m_num);
			for (auto&& item : other) {
				Add(item);
			}
		}

		Array(Array&& other) {
			*this = std::forward<Array>(other);
		}

		Array& operator=(const Array& other) {
			InitEmpty(other.Num());
			for (const auto& item : other) {
				Add(item);
			}
			return *this;
		}

		Array& operator=(Array&& other) {
			this->~Array();

			std::swap(m_data, other.m_data);
			std::swap(m_num, other.m_num);
			std::swap(m_max, other.m_max);
			return *this;
		}

		~Array() {
			Destruct(0, m_num);
			if (m_data) { free(m_data); }
		}

		void Reserve(size_t new_max) {
			if (new_max > m_max) {
				Grow(new_max);
			}
		}

		size_t Add(const T& item) {
			EnsureSpace(m_num + 1);
			new(&AddSafe()) T(item);
			return m_num - 1;
		}

		size_t Add(T&& item) {
			EnsureSpace(m_num + 1);
			return EmplaceSafe(std::forward<T>(item));
		}

		PointerRange<T> Add(std::initializer_list<T> elems) {
			EnsureSpace(m_num + elems.size());
			PointerRange<T> r{ m_data + m_num, m_data + elems.size() };
			for (const T& elem : elems) {
				new(&AddSafe()) T(elem);
			}
			return r;
		}

		// Returns the range of new items
		PointerRange<T> AddDefaulted(size_t count) {
			EnsureSpace(m_num + count);
			T* first = m_data + m_num; // After growing, pointer is valid
			for (size_t i = 0; i < count; ++i) { EmplaceSafe(T{}); }
			return PointerRange<T>{first, first + count};
		}

		PointerRange<T> AddZeroed(size_t count) {
			EnsureSpace(m_num + count);
			T* first = m_data + m_num; // After growing, pointer is valid
			m_num += count;
			memset(first, 0, sizeof(T)*count);
			return PointerRange<T>{first, first + count};
		}

		PointerRange<T> AddUninitialized(size_t count) {
			EnsureSpace(m_num + count);
			T* first = m_data + m_num; // After growing, pointer is valid
			m_num += count;
			return PointerRange<T>{first, first + count};
		}

		size_t AddUnique(const T& item) {
			size_t idx;
			if (FindIndex(item, idx)) { return idx; }
			else return Add(item);
		}

		size_t AddUnique(T&& item) {
			size_t idx;
			if (FindIndex(item, idx)) { return idx; }
			else return Add(std::move(item));
		}

		template<typename... US>
		size_t Emplace(US&&... us) {
			return Add(T{ std::forward<US>(us)... });
		}

		template<typename RANGE>
		void Append(RANGE&& r) {
			for (auto&& item : r) {
				Add(std::forward<decltype(item)>(item));
			}
		}

		void AppendRaw(const T* items, size_t count) {
			Append(mu::Range(items, count));
		}

		void RemoveAt(size_t index) {
			if (index == m_num - 1) {
				// Can't use below strategy as nothing will move on top of this element
				Destruct(index, 1);
			}
			else {
				auto from_range = mu::Range(m_data + index + 1, m_num - index - 1);
				auto to_range = mu::Range(m_data + index, m_num - index);
				// No destructor call necessary as moving element n+1 on top of n should free all necessary resources
				mu::Move(to_range, from_range);
			}
			m_num -= 1;
		}

		void RemoveAll() {
			Destruct(0, m_num);
			free(m_data);
			m_data = nullptr;
			m_num = m_max = 0;
		}

		T& operator[](size_t index) {
			return m_data[index];
		}

		const T& operator[](size_t index) const {
			return m_data[index];
		}

		T* Data() { return m_data; }
		const T* Data() const { return m_data; }

		PointerRange<u8> Bytes() { return ByteRange((u8*)m_data, m_num * sizeof(T)); }
		PointerRange<const u8> Bytes() const { return ByteRange((u8*)m_data, m_num * sizeof(T)); }

		size_t Num() const { return m_num; }
		size_t Max() const { return m_max; }
		bool IsEmpty() const { return m_num == 0; }

		bool Contains(const T& item) const {
			size_t i;
			return FindIndex(item, i);
		}

		auto begin() { return mu::MakeRangeIterator(mu::Range(m_data, m_num)); }
		auto end() { return mu::MakeRangeSentinel(); }

		auto begin() const { return mu::MakeRangeIterator(mu::Range(m_data, m_num)); }
		auto end() const { return mu::MakeRangeSentinel(); }

		auto Range() { return PointerRange<T>{ Data(), Data() + Num() }; }
		auto Range() const { return PointerRange<const T>{ Data(), Data() + Num() }; }
	private:
		bool FindIndex(const T& item, size_t& out_idx) const {
			for (size_t i = 0; i < m_num; ++i) {
				if (item == m_data[i]) {
					out_idx = i;
					return true;
				}
			}
			return false;
		}

		void InitEmpty(size_t num) {
			m_data = (T*)malloc(sizeof(T) * num);
			m_num = 0;
			m_max = num;
		}

		void EnsureSpace(size_t num) {
			if (num > m_max) {
				Grow(num > m_max * 2 ? num : m_max * 2);
			}
		}

		void Grow(size_t new_size) {
			T* new_data = (T*)malloc(sizeof(T) * new_size);
			auto from = mu::Range(m_data, m_num);
			auto to = mu::Range(new_data, m_num);
			mu::MoveConstruct(to, from);
			m_data = new_data;
			m_max = new_size;
		}

		T& AddSafe() {
			T* item = m_data + m_num++;
			return *item;
		}

		size_t EmplaceSafe(T&& item) {
			new(m_data + m_num) T(std::forward<T>(item));
			return m_num++;
		}

		void Destruct(size_t start, size_t num) {
			for (size_t i = start; i < start + num; ++i) {
				m_data[i].~T();
			}
		}
	};
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/Array_Tests.inl"
#endif

#ifdef BENCHMARK
#include "Benchmarks/Array_Benchmarks.inl"
#endif
