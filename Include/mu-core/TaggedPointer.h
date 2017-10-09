#pragma once

#include "Debug.h"

namespace mu {

	constexpr size_t ConstLog2(size_t n) {
		if (n < 2) { return 1; }
		else { return 1 + ConstLog2(n / 2); }
	}

	template<typename T>
	struct TaggablePointerTraits;

	template<>
	struct TaggablePointerTraits<void*> {
		// All memory allocated should be at least 4 byte aligned on x86 systems, TaggedPointer should assert on this.
		static constexpr size_t NumFreeLowBits = 2;
	};

	template<typename T>
	struct TaggablePointerTraits<T*> {
		static constexpr size_t NumFreeLowBits = ConstLog2(alignof(std::remove_pointer_t<T>));
	};

	// TODO: Consider if we need to invoke this template recursively like LLVM do
	template<typename PointerType, typename IntType, int NumIntBits, typename Traits = TaggablePointerTraits<PointerType>>
	class TaggedPointer {
		static_assert(Traits::NumFreeLowBits >= NumIntBits, "No free space in pointer for int bits");

		//static constexpr iptr IntMask = (((iptr)1 << Traits::NumFreeLowBits) - 1);
		//static constexpr iptr PointerMask = ~(((iptr)1 << Traits::NumFreeLowBits) - 1);

		enum : iptr {
			IntMask = (((iptr)1 << Traits::NumFreeLowBits) - 1),
			PointerMask = ~(((iptr)1 << Traits::NumFreeLowBits) - 1),
		};
		iptr m_value;
	public:
		constexpr TaggedPointer() : m_value(0) {}
		TaggedPointer(PointerType p, IntType i) {
			Assert(((iptr)p & IntMask) == 0);
			m_value = (iptr)p;
			m_value |= (iptr)i;
		}
		TaggedPointer(const TaggedPointer&) = default;
		TaggedPointer& operator=(const TaggedPointer&) = default;

		PointerType GetPointer() const {
			return(PointerType)(m_value & PointerMask);
		}
		IntType GetInt() const {
			return m_value & IntMask;
		}

		iptr GetRaw() const { return m_value; }

		bool operator==(TaggedPointer o) { return m_value == o.m_value; }
		bool operator!=(TaggedPointer o) { return m_value != o.m_value; }
	};
}
