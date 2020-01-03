#include "mu-core/Math.h"

TEST_SUITE("Math") {
	TEST_CASE("AlignPow2") {
		SUBCASE("Align16") {
			CHECK_EQ(AlignPow2(16, 16), 16);
			CHECK_EQ(AlignPow2(15, 16), 16);

			CHECK_EQ(AlignPow2(31, 16), 32);
			CHECK_EQ(AlignPow2(32, 16), 32);
		}
	}

	TEST_CASE("AlignUp") {
		SUBCASE("AlignUp16") {
			CHECK_EQ(AlignUp(16, 16), 16);
			CHECK_EQ(AlignUp(15, 16), 16);

			CHECK_EQ(AlignUp(31, 16), 32);
			CHECK_EQ(AlignUp(32, 16), 32);
		}

		SUBCASE("AlignUp40") {
			CHECK_EQ(AlignUp(9, 40), 40);
			CHECK_EQ(AlignUp(39, 40), 40);
			CHECK_EQ(AlignUp(40, 40), 40);

			CHECK_EQ(AlignUp(100, 40), 120);
			CHECK_EQ(AlignUp(119, 40), 120);
			CHECK_EQ(AlignUp(120, 40), 120);
		}
	}
}