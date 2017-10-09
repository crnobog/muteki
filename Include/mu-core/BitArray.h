#pragma once

#include <memory.h>
#include "mu-core/PrimitiveTypes.h"
#include "mu-core/RangeIteration.h"

namespace mu {

	// Iterates the set bits in the BitArray and returns their indices
	template<bool Set>
	class BitRange : public details::WithBeginEnd<BitRange<Set>> {
		u64* m_data = nullptr;
		size_t m_num_bits = 0;
		size_t m_num_qwords = 0;

		size_t m_qword_index = 0;
		size_t m_bit_index = 0;
	public:
		static constexpr bool HasSize = false; // Unknown how many bits are set without preprocessing
		static constexpr bool IsContiguous = false; // Cannot be treated as a C array

		BitRange(u64* data, size_t num_qwords, size_t num_bits)
			: m_data(data)
			, m_num_qwords(num_qwords)
			, m_num_bits(num_bits) {
			if( m_data && (m_data[0] & 0x1) == (Set ? 0 : 0x1) )
			{
				// Find first set bit
				Advance();
			}
		}

		bool IsEmpty() const {
			return !m_data || (m_qword_index * 64 + m_bit_index) >= m_num_bits;
		}
		void Advance() {
			do {
				++m_bit_index;
				if (m_bit_index == 64) {
					m_bit_index = 0;
					do
					{
						++m_qword_index;
						if (m_qword_index >= m_num_qwords) {
							return; // EOF
						}
					} while (Set ? m_data[m_qword_index] == 0 : m_data[m_qword_index] == 0xffffffffffffffff);
				}
			} while ((m_data[m_qword_index] & ((u64)1 << m_bit_index)) == (Set? 0 : (u64)1 << m_bit_index));
		}
		size_t Front() const {
			return m_qword_index * 64 + m_bit_index;
		}
	};

	template<bool Set>
	auto Range(BitRange<Set> r) { return r; }

	class BitArray {
		u64* m_data = nullptr;
		size_t m_num_bits = 0;
		size_t m_num_qwords = 0;

	public:
		BitArray() {}
		~BitArray() {
			delete[] m_data;
		}
		BitArray(const BitArray& other)  {
			*this = other;
		}
		BitArray(BitArray&& other){
			*this = std::forward<BitArray>(other);
		}
		BitArray& operator=(const BitArray& other) {
			m_num_bits = other.m_num_bits;
			m_num_qwords = other.m_num_qwords;
			m_data = new u64[m_num_qwords];
			memcpy(m_data, other.m_data, m_num_qwords * sizeof(u64));
		}
		BitArray& operator=(BitArray&& other) {
			m_data = other.m_data;
			m_num_bits = other.m_num_bits;
			m_num_qwords = other.m_num_qwords;
			other.m_data = nullptr;
			other.m_num_bits = 0;
			other.m_num_qwords = 0;
		}

		void Init(size_t num_bits, bool value) {
			if (m_data) {
				delete[] m_data;
			}
			m_num_bits = num_bits;
			m_num_qwords = (num_bits + 63) / 64;
			if (num_bits == 0) {
				return;
			}

			m_data = new u64[m_num_qwords];
			memset(m_data, value ? 0xff : 0x00, m_num_qwords * sizeof(u64));
		}
		void Empty() {
			delete[] m_data;
			m_data = nullptr;
			m_num_bits = 0;
			m_num_qwords = 0;
		}
		void SetBit(size_t index) {
			size_t qword_index = index / 64;
			size_t bit_index = index - (qword_index * 64);
			if (qword_index < m_num_qwords) {
				m_data[qword_index] |= ((u64)1 << bit_index);
			}
		}
		void ClearBit(size_t index) {
			size_t qword_index = index / 64;
			size_t bit_index = index - (qword_index * 64);
			if (qword_index < m_num_qwords) {
				m_data[qword_index] &= ~((u64)1 << bit_index);
			}
		}
		bool GetBit(size_t index) const {
			size_t qword_index = index / 64;
			size_t bit_index = index - (qword_index * 64);
			if (qword_index < m_num_qwords) {
				return bool(m_data[qword_index] & ((u64)1 << bit_index));
			}
			return false;
		}

		BitRange<true> GetSetBits() const { return BitRange<true>{ m_data, m_num_qwords, m_num_bits }; }
		BitRange<false> GetClearBits() const { return BitRange<false>{ m_data, m_num_qwords, m_num_bits }; }
	};
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/BitArray_Tests.inl"
#endif