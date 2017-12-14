TEST_SUITE("PointerRange") {
	using namespace mu;

	TEST_CASE("Size") {
		const size_t size = 100;
		int ptr[size] = {};
		{
			auto r = Range(ptr, size);

			CHECK(r.HasSize);
			CHECK_EQ(size, r.Size());
		}
		{
			auto r = Range(ptr, ptr + size);

			CHECK(r.HasSize);
			CHECK_EQ(size, r.Size());
		}
		{
			auto r = Range(ptr);

			CHECK(r.HasSize);
			CHECK_EQ(size, r.Size());
		}
	}

	TEST_CASE("Reading") {
		int arr[] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18 };
		auto r = Range(arr);

		CHECK(std::is_same<int&, decltype(r.Front())>::value);
		int i = 0;
		for (; !r.IsEmpty(); r.Advance(), ++i) {
			CHECK_EQ(i * 2, r.Front());
		}
	}

	TEST_CASE("Writing") {
		int arr[] = { 0, 1, 2, 3 };
		auto r = Range(arr);

		for (; !r.IsEmpty(); r.Advance()) {
			int old = r.Front();
			r.Front() = r.Front() * 2;
			CHECK_EQ(old * 2, r.Front());
		}
	}

	TEST_CASE("ConstRange") {
		const int arr[] = { 0, 1, 2, 3, 4 };
		auto r = Range(arr);

		CHECK(std::is_same<const int&, decltype(r.Front())>::value);
	}

	TEST_CASE("IterateRangeBased") {
		int arr[] = { 1, 2, 3, 4 };
		auto r = Range(arr);
		const auto cr = Range(arr);

		int i = 0;
		for (auto item : r) {
			CHECK_EQ(arr[i], item);
			++i;
		}

		i = 0;
		for (auto item : cr) {
			CHECK_EQ(arr[i], item);
			++i;
		}
	}

	TEST_CASE("ByteRanges") {
		struct Foo { i32 value1; i32 value2; };
		Foo f;
		Foo fs[2];

		auto ref_range = ByteRange(f);
		CHECK_EQ(&f, (Foo*)&ref_range.Front());
		CHECK_EQ(sizeof(Foo), ref_range.Size());

		auto array_ref_range = ByteRange(fs);
		CHECK_EQ(&fs[0], (Foo*)&array_ref_range.Front());
		CHECK_EQ(sizeof(Foo) * 2, array_ref_range.Size());

		auto ptr_range = ByteRange(fs, 2);
		CHECK_EQ(&fs[0], (Foo*)&ptr_range.Front());
		CHECK_EQ(sizeof(Foo) * 2, ptr_range.Size());
	}

	TEST_CASE("RangeToBytes") {
		i32 is[] = { -1, -1, -1, -1 };

		PointerRange<i32> ir = Range(is);
		static_assert(std::is_same_v<decltype(ir.Bytes()), PointerRange<u8>>, "Type of PointerRange<i32>::Bytes");
		PointerRange<u8> ur = ir.Bytes();
		CHECK_EQ((void*)&ir.Front(), (void*)&ur.Front());
		CHECK_EQ(ir.Size() * sizeof(i32), ur.Size());

		PointerRange<const i32> cir = Range(is);
		static_assert(std::is_same_v<decltype(cir.Bytes()), PointerRange<const u8>>, "Type of PointerRange<const i32>::Bytes");
		PointerRange<const u8> cur = cir.Bytes();
		CHECK_EQ((void*)&cir.Front(), (void*)&cur.Front());
		CHECK_EQ(cir.Size() * sizeof(i32), cur.Size());
	}
}
