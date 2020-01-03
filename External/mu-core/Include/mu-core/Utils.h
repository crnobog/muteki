#pragma once

#include "mu-core/PrimitiveTypes.h"
#include "mu-core/Math.h"

// TODO: rename to ArrayNum or ArrayCount 
template<typename T, size_t N> inline constexpr size_t ArraySize(const T(&)[N]) { return N; }

namespace mu {
	struct IndexIterator {
		size_t m_current;
		size_t m_max;

		size_t operator*() const {
			return m_current;
		}

		IndexIterator& operator++() {
			++m_current;
			return *this;
		}

		bool operator==(const IndexIterator& other) {
			return m_current == other.m_current;
		}

		bool operator!=(const IndexIterator& other) {
			return m_current != other.m_current;
		}
	};

	struct IndexRange {
		size_t m_max;

		IndexIterator begin() { return IndexIterator{ 0, m_max }; }
		IndexIterator end() { return IndexIterator{ m_max, m_max }; }
	};

	inline IndexRange Indices(size_t max) {
		return IndexRange{ max };
	}
}
