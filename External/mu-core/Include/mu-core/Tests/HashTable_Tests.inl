#include <random>

// TODO: Test destructors are called
TEST_SUITE("Hashtable") {
	using namespace mu;

	struct Key {
		u64 Data;
	};
	struct Value {
		u64 Data;
	};

	bool operator==(const Key& a, const Key& b) {
		return a.Data == b.Data;
	}
	bool operator==(const Value& a, const Value& b) {
		return a.Data == b.Data;
	}

	TEST_CASE("Empty hash") {
		HashTable<Key, Value> hash;
		CHECK_EQ(hash.GetNum(), 0);
		CHECK_FALSE(hash.Contains(Key{ 12 }));

		auto r = Range(hash);
		CHECK(r.IsEmpty());
	}

	TEST_CASE("Add one key and value") {
		HashTable<Key, Value> hash;

		Key k{ 17 };
		Value v{ 170 };
		hash.Add(k, v);

		CHECK_EQ(hash.GetNum(), 1);
		CHECK(hash.Contains(k));
		CHECK_EQ(hash.Find(k), v);

		hash.Remove(k);

		CHECK_EQ(hash.GetNum(), 0);
		CHECK_FALSE(hash.Contains(k));
	}

	TEST_CASE("Add many keys and value") {
		Array<Key> keys;
		Array<Value> values;
		HashTable<Key, Value> hash;
		std::mt19937 gen(23526234);
		for (i32 i = 0; keys.Num() < 1000; ) {
			Key k{ gen() };
			if (!keys.Contains(k)) {
				++i;
				keys.Emplace(k);
				values.Emplace(gen());
			}
		}
		for (i32 i = 0; i < keys.Num(); ++i) {
			hash.Add(keys[i], values[i]);
		}

		CHECK_EQ(keys.Num(), hash.GetNum());

		SUBCASE("Contains") {
			for (Key k : keys) {
				bool hash_contains = hash.Contains(k);
				CHECK(hash_contains);
			}

			for (auto[key, value] : Zip(keys, values)) {
				CHECK(hash.Find(key) == value);
			}
		}

		SUBCASE("Iterate") {
			u64 expected_pairs = keys.Num();
			u64 num = 0;
			for (auto[k, v] : Range(hash)) {
				++num;
				auto found_key = Find(keys, [k](const Key& arr_k) { return k == arr_k; });
				CHECK_FALSE(found_key.IsEmpty());
				size_t idx = &found_key.Front() - keys.Data();
				CHECK_EQ(values[idx].Data, v.Data);
				values.RemoveAt(idx);
				keys.RemoveAt(idx);
			}

			CHECK(keys.IsEmpty());
			CHECK(values.IsEmpty());
			CHECK_EQ(expected_pairs, num);
		}

		SUBCASE("Remove") {
			CHECK_EQ(keys.Num(), hash.GetNum());
			Array<Key> shuffled_keys;
			while (keys.Num()) {
				size_t i = gen() % keys.Num();
				shuffled_keys.Add(keys[i]);
				keys.RemoveAt(i);
			}

			for (Key k : shuffled_keys) {
				hash.Remove(k);
			}
			CHECK_EQ(0ull, hash.GetNum());
		}
	}
}
