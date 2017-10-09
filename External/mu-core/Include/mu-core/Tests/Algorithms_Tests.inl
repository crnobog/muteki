#include "PointerRange.h"

TEST_SUITE("Algorithms") {
	using namespace mu;

	struct Element {
		i32 data;

		Element() : data(0) {
			++ConstructCount();
		}
		Element(i32 d) : data(d) {
			++ConstructCount();
		}
		Element(const Element& other) : data(other.data) {
			++CopyCount();
		}
		Element(Element&& other) : data(other.data) {
			++MoveCount();
		}
		Element& operator=(const Element& other) {
			data = other.data;
			++CopyAssignCount();
			return *this;
		}
		Element& operator=(Element&& other) {
			data = other.data;
			++MoveAssignCount();
			return *this;
		}
		~Element() {
			++DestructCount();
		}

		static i32& ConstructCount() { static i32 local = 0; return local; }
		static i32& CopyCount() { static i32 local = 0; return local; }
		static i32& MoveCount() { static i32 local = 0; return local; }
		static i32& CopyAssignCount() { static i32 local = 0; return local; }
		static i32& MoveAssignCount() { static i32 local = 0; return local; }
		static i32& DestructCount() { static i32 local = 0; return local; }

		static void Reset() {
			ConstructCount() = CopyCount() = MoveCount() = CopyAssignCount() = MoveAssignCount() = DestructCount() = 0;
		}
	};

	TEST_CASE("Move Primitive") {
		int from[] = { 0,1,2,3,4,5,6,7,8,9 };
		int to[20] = {};

		auto source = Range(from);
		auto dest = Range(to);
		size_t source_size_start = source.Size(), dest_size_start = dest.Size();
		CHECK_EQ(10, source.Size());
		CHECK_EQ(20, dest.Size());

		auto dest2 = Move(dest, source);
		CHECK_EQ(dest_size_start, dest.Size());
		CHECK_EQ(source_size_start, source.Size());
		CHECK_EQ(dest.Size() - source.Size(), dest2.Size());
		CHECK_EQ(size_t(10), source.Size());

		auto dest3 = Move(dest2, source);
		CHECK_EQ(dest.Size() - source.Size() * 2, dest3.Size());
		CHECK(dest3.IsEmpty());

		i32 index = 0;
		for (auto r = Range(to); !r.IsEmpty(); r.Advance(), ++index) {
			CHECK_EQ(from[index % 10], r.Front());
		}
	}

	TEST_CASE("MoveConstruct Primitive") {
		int from[] = { 0,1,2,3,4,5,6,7,8,9 };
		int to[20] = {};

		auto source = Range(from);
		auto dest = Range(to);
		CHECK_EQ(size_t(10), source.Size());
		CHECK_EQ(size_t(20), dest.Size());

		auto dest2 = MoveConstruct(dest, source);
		CHECK_EQ(dest.Size() - source.Size(), dest2.Size());
		CHECK_EQ(size_t(10), source.Size());

		auto dest3 = MoveConstruct(dest2, source);
		CHECK_EQ(dest.Size() - source.Size() * 2, dest3.Size());
		CHECK(dest3.IsEmpty());

		i32 index = 0;
		for (auto r = Range(to); !r.IsEmpty(); r.Advance(), ++index) {
			CHECK_EQ(from[index % 10], r.Front());
		}
	}

	TEST_CASE("Move Object") {
		Element es[10] = {};
		Element dest[10] = {};
		Element::Reset();

		Move(dest, es);
		CHECK_EQ(0, Element::ConstructCount());
		CHECK_EQ(10, Element::MoveAssignCount());
		CHECK_EQ(0, Element::MoveCount());
		CHECK_EQ(0, Element::DestructCount());
	}

	TEST_CASE("MoveConstruct Object") {
		Element es[10] = {};
		Element dest[10] = {};
		Element::Reset();

		MoveConstruct(dest, es);
		CHECK_EQ(0, Element::ConstructCount());
		CHECK_EQ(0, Element::MoveAssignCount());
		CHECK_EQ(10, Element::MoveCount());
		CHECK_EQ(0, Element::DestructCount());
	}

	TEST_CASE("Map Lambda") {
		int arr[] = { 1,2,3,4 };
		int expected[] = { 2,4,6,8 };
		Map(arr, [](int& a) { a *= 2; });

		for (auto r = Range(arr), x = Range(expected); !r.IsEmpty(); r.Advance(), x.Advance()) {
			CHECK_EQ(x.Front(), r.Front());
		}
	}

	TEST_CASE("Map Lambda Const") {
		int sum = 0;
		int arr[] = { 5, 10, 20 };
		Map(arr, [&](const int&a) {sum += a; });

		CHECK_EQ(35, sum);
	}

	TEST_CASE("Map Lambda - original range not advanced") {
		int arr[] = { 1,2,3,4 };
		auto initial = Range(arr);

		Map(initial, [](int& a) { a++; });

		CHECK(Range(arr) == initial);
	}
	
	TEST_CASE("Find returns non-empty iterator") {
		int arr[] = { 10, 20, 100, 50, 40, 6, 100, 120, 50 };
		auto r = Find(arr, [](int a) { return a < 10; });

		CHECK_FALSE(r.IsEmpty());
	}

	TEST_CASE("Find returns empty iterator") {
		int arr[] = { 10, 20, 100, 50, 40, 100, 120, 50 };
		auto r = Find(arr, [](int a) { return a < 10; });

		CHECK(r.IsEmpty());
	}

	TEST_CASE("Range matches manual advance") {
		int arr[] = { 10, 20, 100, 50, 40, 100, 120, 50 };
		auto found = Find(arr, [](int a) { return a == 100; });

		auto r = Range(arr);
		r.Advance(); r.Advance();
		CHECK(r == found);
	}

	TEST_CASE("Successive finds") {
		int arr[] = { 10, 20, 100, 50, 40, 100, 120, 50 };
		auto found = Find(arr, [](int a) { return a == 100; });

		auto first = found;
		found.Advance();
		auto second = Find(found, [](int a) { return a == 100;  });

		CHECK(first != second);

		auto also_second = FindNext(first, [](int a) { return a == 100;  });
		CHECK(second == also_second);
	}
};