#include "Ranges.h"
#include "PointerRange.h"
#include "IotaRange.h"

template<typename T>
struct TD;

TEST_SUITE("ZipRange") {
	using namespace mu;

	TEST_CASE("Zip ranges") {
		int as[] = { 0,1,2,3,4,5,6,7,8,9 };
		float bs[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

		int index = 0;
		auto r = Zip(Range(as), Range(bs));
		CHECK(r.HasSize);
		CHECK_FALSE(r.IsEmpty());
		CHECK_EQ(size_t(10), r.Size());

		for (; !r.IsEmpty(); r.Advance(), ++index) {
			std::tuple<int&, float&> front = r.Front();
			CHECK_EQ(as[index], std::get<0>(front));
			CHECK_EQ(bs[index], std::get<1>(front));
		}
	}

	TEST_CASE("Zip iotas") {
		size_t i = 0;
		auto r = Zip(Iota(), Iota(1));
		CHECK_FALSE(r.HasSize);
		for (; !r.IsEmpty() && i < 10; ++i, r.Advance()) {
			std::tuple<size_t, size_t> f = r.Front();
			CHECK_EQ(1 + std::get<0>(f), std::get<1>(f));
		}
	}

	TEST_CASE("Zip iota with finite") {
		float fs[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
		auto frange = Range(fs);
		auto r = Zip(Iota(), frange);

		CHECK(r.HasSize);
		CHECK_EQ(frange.Size(), r.Size());

		int i = 0;
		for (; !r.IsEmpty(); r.Advance(), ++i) {
			std::tuple<size_t, float&> f = r.Front();
			CHECK_EQ(size_t(i), std::get<0>(f));
			CHECK_EQ(fs[i], std::get<1>(f));
		}
	}

	TEST_CASE("Zip const with mutable") {
		int a[] = { 1,2,3,4 };
		const int b[] = { 5, 6, 7, 8 };

		auto r = Zip(Range(a), Range(b));
		CHECK(std::is_same<std::tuple<int&, const int&>, decltype(r.Front())>::value);
	}

}