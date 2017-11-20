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

		bool operator==(const Element& other) const { return Data == other.Data; }
		bool operator!=(const Element& other) const { return Data != other.Data; }

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

	TEST_CASE("Create from initializer list") {
		FixedArray<Element, 10> arr{ {1}, {2}, {3}, {4} };

		CHECK_EQ(arr.Num(), 4);
		CHECK_EQ(arr[0], 1);
		CHECK_EQ(arr[1], 2);
		CHECK_EQ(arr[2], 3);
		CHECK_EQ(arr[3], 4);
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

	TEST_CASE("Copy FixedArray") {
		FixedArray<Element, 10> a;
		a.Add({ 5 });
		a.Add({ 25 });
		a.Add({ 125 });

		FixedArray<Element, 10> b = a;
		CHECK_EQ(b.Num(), a.Num());
		CHECK_EQ(b[0], a[0]);
		CHECK_EQ(b[1], a[1]);
		CHECK_EQ(b[2], a[2]);
	}

	TEST_CASE("Move FixedArray") {
		FixedArray<Element, 10> a;
		a.Add({ 5 });
		a.Add({ 25 });
		a.Add({ 125 });
		Element::Reset();

		FixedArray<Element, 10> b(std::move(a));;
		CHECK_EQ(b.Num(), 3);
		CHECK_EQ(Element::MoveCount(), 3);
	}
}
