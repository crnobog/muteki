TEST_SUITE("FixedArray") {
	using namespace mu;

	struct EmptyElement {};

	static_assert(std::is_trivially_destructible_v<EmptyElement> == true, "EmptyElement is not trivially destructible");
	static_assert(std::is_trivially_destructible_v<FixedArray<EmptyElement, 10>> == true, "FixedArray<EmptyElement> is not trivially destructible");

	struct Element {
		i32 Data;

		Element() { ConstructCount()++; }
		Element(i32 i) { ConstructCount()++; Data = i; }
		Element(const Element& e) { CopyCount()++; Data = e.Data; }
		Element(Element&& e) { MoveCount()++; Data = e.Data; }
		Element& operator=(const Element& e) { CopyCount()++; Data = e.Data; }
		Element& operator=(Element&& e) { MoveCount()++; Data = e.Data; }
		~Element() { DestructCount()++; }

		static i32& ConstructCount() { static i32 local = 0; return local; }
		static i32& CopyCount() { static i32 local = 0; return local; }
		static i32& MoveCount() { static i32 local = 0; return local; }
		static i32& DestructCount() { static i32 local = 0; return local; }

		static void Reset() {
			ConstructCount() = CopyCount() = MoveCount() = DestructCount() = 0;
		}
	};

	static_assert(std::is_trivially_destructible_v<FixedArray<Element, 10>> == false, "FixedArray<Element> is trivially destructible");

	TEST_CASE("Empty FixedArray") {
		Element::Reset();
		{
			FixedArray<Element, 10> arr;

			CHECK_EQ(arr.Num(), 0);
			CHECK_NE(arr.Data(), nullptr);
			CHECK_EQ(Element::ConstructCount(), 0);
		}
		CHECK_EQ(Element::DestructCount(), 0);
	}

	TEST_CASE("Add elements") {
		Element::Reset();
		FixedArray<Element, 10> arr;

		for (i32 i = 0; i < 5; ++i) {
			arr.Add(Element{ i });
		}

		CHECK_EQ(Element::ConstructCount(), 5);
		CHECK_EQ(Element::CopyCount(), 5);

		SUBCASE("Elements index correctly") {
			for (i32 i = 0; i < 5; ++i) {
				CHECK_EQ(i, arr[i].Data);
			}
		}

		SUBCASE("Elements iterate correctly") {
			i32 i = 0;
			for (Element& e : arr) {
				CHECK_EQ(i, e.Data);
				++i;
			}
		}
	}

	TEST_CASE("Elements destroyed") {
		{
			FixedArray<Element, 10> arr;
			arr.Add({ 5 });
			Element::Reset();
		}

		CHECK_EQ(1, Element::DestructCount());
	}
}