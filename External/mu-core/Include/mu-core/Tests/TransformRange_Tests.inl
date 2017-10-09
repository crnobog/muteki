TEST_SUITE("TransformRange") {
	using namespace mu;

	TEST_CASE("Transform range with lambda") {
		int arr[] = { 1, 2, 3, 4, 5 };
		auto r = Transform(Range(arr), [](int a) { return a * 5; });

		CHECK(r.HasSize);
		CHECK_EQ(size_t(5), r.Size());

		for (int i = 0; !r.IsEmpty(); ++i, r.Advance()) {
			CHECK_EQ(arr[i] * 5, r.Front());
		}
	}

	TEST_CASE("Transform infinite range") {
		auto r = Transform(Iota<int>(), [](int a) { return a * 5; });

		CHECK_FALSE(r.HasSize);

		for (int i = 0; i < 10; ++i, r.Advance()) {
			CHECK_EQ(i * 5, r.Front());
		}
	}

	TEST_CASE("Transform const range with lambda") {
		const int arr[] = { 1, 2, 3, 4, 5 };
		auto r = Transform(Range(arr), [](int a) { return a * 5; });

		CHECK(r.HasSize);
		CHECK_EQ(size_t(5), r.Size());

		for (int i = 0; !r.IsEmpty(); ++i, r.Advance()) {
			CHECK_EQ(arr[i] * 5, r.Front());
		}
	}
}