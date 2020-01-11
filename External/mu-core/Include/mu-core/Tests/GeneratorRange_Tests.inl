#include "mu-core/UnitTesting.h"

#include "mu-core/Global.h"
#include "mu-core/PrimitiveTypes.h"
#include "mu-core/GeneratorRange.h"

namespace GeneratorRangeTests {
	i32 Numbers[] = {
		100,
		4,
		65535,
		4 << 17,
	};

	mu::GeneratorRange<i32> GetNumbers() {
		for (i32 i : Numbers) {
			co_yield i;
		}
		co_return;
	}
}

TEST_SUITE("GeneratorRange") {
	TEST_CASE("NumbersCorrect") {
		mu::GeneratorRange<i32> r = GeneratorRangeTests::GetNumbers();
		for (i32 i : GeneratorRangeTests::Numbers) {
			CHECK_FALSE(r.IsEmpty());
			CHECK_EQ(i, r.Front());
			r.Advance();
		}
	}

	TEST_CASE("MoveRange") {
		mu::GeneratorRange<i32> old_r = GeneratorRangeTests::GetNumbers();
		mu::GeneratorRange<i32> r = std::move(old_r);

		CHECK(old_r.IsEmpty());

		for (i32 i : GeneratorRangeTests::Numbers) {
			CHECK_FALSE(r.IsEmpty());
			CHECK_EQ(i, r.Front());
			r.Advance();
		}
	}
}