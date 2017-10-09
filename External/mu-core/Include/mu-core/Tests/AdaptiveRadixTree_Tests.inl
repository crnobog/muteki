
TEST_SUITE("AdaptiveRadixTree") {
	using mu::Range;

	TEST_CASE("Empty tree, add single key") {
		mu::AdaptiveRadixTree art;

		static const u8 key[] = "abcdef";
		auto r = Range(key);
		CHECK_EQ(art.Find(r), nullptr);

		i32 value = 20;
		void* old = art.Add(r, &value);
		CHECK_EQ(old, nullptr);

		CHECK_EQ(art.Find(r), &value);

		i32 new_value = 40;
		CHECK_EQ(art.Add(r, &new_value), &value);

		CHECK_EQ(art.Find(r), &new_value);
	}

	TEST_CASE("Add common prefix keys") {
		mu::AdaptiveRadixTree art;

		static const u8 key1[] = "wanderlust";
		static const u8 key2[] = "wandering";
		static const u8 key3[] = "wandsworth";
		i32 values[3] = { 1, 2, 3 };

		CHECK_EQ(art.Add(Range(key1), values + 0), nullptr);
		CHECK_EQ(art.Find(Range(key1)), values + 0);

		CHECK_EQ(art.Add(Range(key2), values + 1), nullptr);

		CHECK_EQ(art.Find(Range(key1)), values + 0);
		CHECK_EQ(art.Find(Range(key2)), values + 1);

		CHECK_EQ(art.Add(Range(key3), values + 2), nullptr);

		CHECK_EQ(art.Find(Range(key1)), values + 0);
		CHECK_EQ(art.Find(Range(key2)), values + 1);
		CHECK_EQ(art.Find(Range(key3)), values + 2);
	}

	TEST_CASE("Remove keys") {
		mu::AdaptiveRadixTree art;

		static const u8 key1[] = "wanderlust";
		static const u8 key2[] = "wandering";
		static const u8 key3[] = "wandsworth";
		i32 values[3] = { 1, 2, 3 };

		art.Add(Range(key1), values + 0);
		art.Add(Range(key2), values + 1);
		art.Add(Range(key3), values + 2);

		SUBCASE("Remove 1") {
			CHECK_EQ(art.Remove(Range(key1)), values + 0);
			CHECK_EQ(art.Find(Range(key1)), nullptr);

			CHECK_EQ(art.Find(Range(key2)), values + 1);
			CHECK_EQ(art.Find(Range(key3)), values + 2);
		}

		SUBCASE("Remove 2") {
			CHECK_EQ(art.Remove(Range(key2)), values + 1);
			CHECK_EQ(art.Find(Range(key2)), nullptr);

			CHECK_EQ(art.Find(Range(key1)), values + 0);
			CHECK_EQ(art.Find(Range(key3)), values + 2);
		}

		SUBCASE("Remove 3") {
			CHECK_EQ(art.Remove(Range(key3)), values + 2);
			CHECK_EQ(art.Find(Range(key3)), nullptr);

			CHECK_EQ(art.Find(Range(key2)), values + 1);
			CHECK_EQ(art.Find(Range(key1)), values + 0);
		}

		SUBCASE("Remove 123") {
			CHECK_EQ(art.Remove(Range(key1)), values + 0);
			CHECK_EQ(art.Remove(Range(key2)), values + 1);
			CHECK_EQ(art.Remove(Range(key3)), values + 2);

			CHECK_EQ(art.Find(Range(key1)), nullptr);
			CHECK_EQ(art.Find(Range(key2)), nullptr);
			CHECK_EQ(art.Find(Range(key3)), nullptr);
		}

		SUBCASE("Remove 321") {
			CHECK_EQ(art.Remove(Range(key3)), values + 2);
			CHECK_EQ(art.Remove(Range(key2)), values + 1);
			CHECK_EQ(art.Remove(Range(key1)), values + 0);

			CHECK_EQ(art.Find(Range(key1)), nullptr);
			CHECK_EQ(art.Find(Range(key2)), nullptr);
			CHECK_EQ(art.Find(Range(key3)), nullptr);
		}

		SUBCASE("Remove 213") {
			CHECK_EQ(art.Remove(Range(key2)), values + 1);
			CHECK_EQ(art.Remove(Range(key1)), values + 0);
			CHECK_EQ(art.Remove(Range(key3)), values + 2);

			CHECK_EQ(art.Find(Range(key1)), nullptr);
			CHECK_EQ(art.Find(Range(key2)), nullptr);
			CHECK_EQ(art.Find(Range(key3)), nullptr);
		}
	}
}
