TEST_SUITE("Pool") {
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
			++CopyCount();
			return *this;
		}
		Element& operator=(Element&& other) {
			data = other.data;
			++MoveCount();
			return *this;
		}
		~Element() {
			++DestructCount();
		}

		static i32& ConstructCount() { static i32 local = 0; return local; }
		static i32& CopyCount() { static i32 local = 0; return local; }
		static i32& MoveCount() { static i32 local = 0; return local; }
		static i32& DestructCount() { static i32 local = 0; return local; }

		static void Reset() {
			ConstructCount() = CopyCount() = MoveCount() = DestructCount() = 0;
		}
	};

	TEST_CASE("AddToLimit") {
		Element::Reset();
		Pool<Element> p{ 4 };
		size_t a = p.AddDefaulted();
		size_t b = p.AddDefaulted();
		size_t c = p.AddDefaulted();
		size_t d = p.AddDefaulted();
		CHECK_EQ(0, (i32)p.GetFreeCount());

		CHECK_NE(a, b);
		CHECK_NE(a, c);
		CHECK_NE(a, d);

		CHECK_NE(b, c);
		CHECK_NE(b, d);

		CHECK_NE(c, d);
	}

	TEST_CASE("AddToLimitReleaseAddMore") {
		Element::Reset();
		Pool<Element> p{ 4 };
		{
			size_t a = p.AddDefaulted();
			size_t b = p.AddDefaulted();
			size_t c = p.AddDefaulted();
			size_t d = p.AddDefaulted();
			CHECK_EQ(0, (i32)p.GetFreeCount());

			p.Return(a);
			p.Return(b);
			p.Return(c);
			p.Return(d);

			CHECK_EQ(4, (i32)p.GetFreeCount());
		}

		size_t a = p.AddDefaulted();
		size_t b = p.AddDefaulted();
		size_t c = p.AddDefaulted();
		size_t d = p.AddDefaulted();
		CHECK_EQ(0, (i32)p.GetFreeCount());

		CHECK_NE(a, b);
		CHECK_NE(a, c);
		CHECK_NE(a, d);

		CHECK_NE(b, c);
		CHECK_NE(b, d);

		CHECK_NE(c, d);
	}

	TEST_CASE("OnlyAddedObjectsAreDestroyed") {
		Element::Reset();
		{
			Pool<Element> p{ 128 };
		}

		CHECK_EQ(0, Element::ConstructCount());
		CHECK_EQ(0, Element::DestructCount());

		{
			Pool<Element> p{ 128 };

			p.AddDefaulted();
			p.AddDefaulted();
			p.AddDefaulted();
			Element::Reset();
		}

		CHECK_EQ(3, Element::DestructCount());
	}

	TEST_CASE("RemovedObjectsAreDestroyedImmediately") {
		Element::Reset();
		{
			Pool<Element> p{ 128 };

			size_t a = p.AddDefaulted();
			size_t b = p.AddDefaulted();
			p.AddDefaulted();
			Element::Reset();
			p.Return(a);
			p.Return(b);

			CHECK_EQ(2, Element::DestructCount());
		}
	}

	TEST_CASE("RemovedObjectsAreNotDestroyedOnPoolDestruction") {
		Element::Reset();
		{
			Pool<Element> p{ 128 };

			size_t a = p.AddDefaulted();
			size_t b = p.AddDefaulted();
			p.AddDefaulted();
			p.Return(a);
			p.Return(b);

			Element::Reset();
		}

		CHECK_EQ(1, Element::DestructCount());
	}

}