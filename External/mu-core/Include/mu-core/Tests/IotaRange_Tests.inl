TEST_SUITE("IotaRange") {
	using namespace mu;

	TEST_CASE("Iota from 0") {
		size_t i = 0;
		auto r = IotaRange<size_t>();
		CHECK_FALSE(r.HasSize);
		for (; !r.IsEmpty() && i < 10; ++i, r.Advance()) {
			CHECK_EQ(i, r.Front());
		}
	}
}