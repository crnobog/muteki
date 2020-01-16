#include "ZipRange.h"

TEST_SUITE("BitArray") {
	using namespace mu;

	using std::get;

	TEST_CASE("InitBitsClear") {
		BitArray b;
		b.Init(256, false);

		for (size_t i = 0; i < 256; i += 7) {
			CHECK_EQ(false, b.GetBit(i));
		}
	}
	TEST_CASE("InitBitsSet") {
		BitArray b;
		b.Init(256, true);

		for (size_t i = 0; i < 256; i += 7) {
			CHECK_EQ(true, b.GetBit(i));
		}
	}

	TEST_CASE("SetBits") {
		BitArray b;
		b.Init(256, false);

		b.SetBit(12);
		b.SetBit(32);
		b.SetBit(64);
		b.SetBit(65);
		b.SetBit(100);

		CHECK_EQ(true, b.GetBit(12));
		CHECK_EQ(true, b.GetBit(32));
		CHECK_EQ(true, b.GetBit(64));
		CHECK_EQ(true, b.GetBit(65));
		CHECK_EQ(true, b.GetBit(100));
	}

	TEST_CASE("ClearBits") {
		BitArray b;
		b.Init(256, true);

		b.ClearBit(12);
		b.ClearBit(32);
		b.ClearBit(64);
		b.ClearBit(65);
		b.ClearBit(100);

		CHECK_EQ(false, b.GetBit(12));
		CHECK_EQ(false, b.GetBit(32));
		CHECK_EQ(false, b.GetBit(64));
		CHECK_EQ(false, b.GetBit(65));
		CHECK_EQ(false, b.GetBit(100));
	}

	TEST_CASE("IterateEmpty") {
		{
			BitArray b;
			auto r1 = b.GetSetBits();
			auto r2 = b.GetClearBits();
			CHECK(r1.IsEmpty());
			CHECK(r2.IsEmpty());
		}
		{
			BitArray b;
			b.Init(256, false);
			b.Empty();

			auto r1 = b.GetSetBits();
			auto r2 = b.GetClearBits();
			CHECK(r1.IsEmpty());
			CHECK(r2.IsEmpty());
		}
	}
	TEST_CASE("IterateSetBits") {
		size_t indices[] = { 12, 32, 64, 65, 100 };

		BitArray bits;
		bits.Init(256, false);

		for (size_t i : indices) {
			bits.SetBit(i);
		}

		for (auto[a, b] : Zip(Range(indices), bits.GetSetBits())) {
			CHECK_EQ(a, b);
		}
	}
}
