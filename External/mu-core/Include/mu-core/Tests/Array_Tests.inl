// TODO: Should these tests be written in terms of "valid elements" rather than method call counts? 
//	e.g. To account for differences in whether elements are destroyed or have other elements moved on top of them, achieving the same result

#include "PointerRange.h"
#include "Utils.h"

TEST_SUITE("Array") {
	struct Element {
		i32 Payload;

		Element() : Payload(0) { ++ConstructorCalls(); }
		Element(i32 i) : Payload(i) { ++ConstructorCalls(); }
		Element(const Element& e) : Payload(e.Payload) { ++CopyCalls(); }
		Element(Element&& e) : Payload(e.Payload) { ++MoveCalls(); }
		~Element() { ++DestructorCalls(); }

		Element& operator=(const Element& e) { Payload = e.Payload; ++CopyCalls(); return *this; }
		Element& operator=(Element&& e) { Payload = e.Payload; ++MoveCalls(); return *this; }

		static i32& ConstructorCalls() {
			static i32 local = 0; return local;
		}
		static i32& DestructorCalls() {
			static i32 local = 0; return local;
		}
		static i32& CopyCalls() {
			static i32 local = 0; return local;
		}
		static i32& MoveCalls() {
			static i32 local = 0; return local;
		}

		static void Reset() {
			ConstructorCalls() = DestructorCalls() = CopyCalls() = MoveCalls() = 0;
		}
	};
	TEST_CASE("Default constructed array") {
		Element::Reset();
		mu::Array<Element> arr;

		CHECK_EQ(arr.Num(), 0);
		CHECK_EQ(arr.Data(), nullptr);
		CHECK_EQ(Element::ConstructorCalls(), 0);
		CHECK_EQ(Element::DestructorCalls(), 0);
	}

	TEST_CASE("Array from initializer list") {
		Element::Reset();

		mu::Array<Element> arr{ Element(1), Element(12), Element(33) };

		CHECK_EQ(arr.Num(), 3);
		CHECK_GE(arr.Max(), 3);
		CHECK_EQ(Element::ConstructorCalls(), 3);
		CHECK_EQ(Element::CopyCalls(), 3);
		CHECK_EQ(Element::DestructorCalls(), 3);

		CHECK_EQ(arr[0].Payload, 1);
		CHECK_EQ(arr[1].Payload, 12);
		CHECK_EQ(arr[2].Payload, 33);
	}

	TEST_CASE("Array Add") {
		Element::Reset();
		mu::Array<Element> arr;

		SUBCASE("By copy") {
			Element elem{ 12 };
			arr.Add(elem);			// copied in

			CHECK_EQ(Element::ConstructorCalls(), 1);
			CHECK_EQ(Element::CopyCalls(), 1);
			CHECK_EQ(Element::MoveCalls(), 0);
			CHECK_EQ(Element::DestructorCalls(), 0);
		}

		SUBCASE("By move") {
			arr.Add(Element(12)); // temporary will be moved in

			CHECK_EQ(Element::ConstructorCalls(), 1);
			CHECK_EQ(Element::CopyCalls(), 0);
			CHECK_EQ(Element::MoveCalls(), 1);
			CHECK_EQ(Element::DestructorCalls(), 1); // temporary was destroyed
		}

		CHECK_EQ(arr.Num(), 1);
		CHECK_GE(arr.Max(), 1);
	}

	TEST_CASE("Array leaves scope") {
		{
			mu::Array<Element> arr;
			arr.Add(Element(12));
			arr.Add(Element(22));
			Element::Reset();
		}

		CHECK_EQ(Element::DestructorCalls(), 2);
	}

	TEST_CASE("Array exceeds capacity") {
		mu::Array<Element> arr;
		arr.Add(Element(-1));

		Element::Reset();
		CHECK_LT(arr.Max(), 32); // Capacity should be reasonable after one element

		size_t initial_max = arr.Max();
		for (i32 i = 0; i < initial_max - 1; ++i) {
			arr.Add(Element(i));
		}
		CHECK_EQ(arr.Max(), initial_max); // Should not have grown yet
		Element::Reset();

		arr.Add(Element(100));
		CHECK_GT(arr.Max(), initial_max); // Should have grown
		CHECK_EQ(Element::ConstructorCalls(), 1); // Temporary only
		CHECK_EQ(Element::MoveCalls(), initial_max + 1);
		CHECK_EQ(Element::CopyCalls(), 0);
		CHECK_EQ(Element::DestructorCalls(), 1); // Temporary should have been destroyed
	}

	TEST_CASE("Array remove") {
		mu::Array<Element> arr;
		arr.Add(Element(12));
		arr.Add(Element(22));

		SUBCASE("Remove last element") {
			Element::Reset();
			arr.RemoveAt(1);

			CHECK_EQ(arr.Num(), 1);
			CHECK_EQ(Element::DestructorCalls(), 1);
		}

		SUBCASE("Remove non-last element") {
			Element::Reset();
			arr.RemoveAt(0);

			CHECK_EQ(arr.Num(), 1);
			CHECK_EQ(Element::MoveCalls(), 1);
		}
	}

	TEST_CASE("Array AddZeroed") {
		Element::Reset();
		mu::Array<Element> arr;

		auto r = arr.AddZeroed(3);
		CHECK_EQ(r.Size(), 3);
		i32 num = 0;
		for (Element& e : r) {
			e = num++;
		}
		CHECK_EQ(arr[0].Payload, 0);
		CHECK_EQ(arr[1].Payload, 1);
		CHECK_EQ(arr[2].Payload, 2);
	}

	TEST_CASE("Array append") {
		Element src[] = { 1, 2, 3, 4, 5 };

		Element::Reset();
		mu::Array<Element> arr;
		auto r = mu::Range(src);
		auto old_size = r.Size();
		arr.Append(r);

		CHECK_EQ(r.Size(), old_size);
		CHECK_EQ(arr.Num(), ArraySize(src));
		for (i32 i = 0; i < arr.Num(); ++i) {
			CHECK_EQ(arr[i].Payload, src[i].Payload);
		}
	}
}
