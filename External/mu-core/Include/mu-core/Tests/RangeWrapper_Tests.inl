TEST_SUITE("Range wrappers") {
	using namespace mu;

	TEST_CASE("Wrap PointerRange") {
		int is[] = { 1, 2, 3, 4, 5 };
		mu::ForwardRange<int> wrapped = WrapRange(Range(is));

		int index = 0;
		for (; !wrapped.IsEmpty(); wrapped.Advance(), ++index) {
			CHECK_EQ(is[index], wrapped.Front());
		}
	}
}
