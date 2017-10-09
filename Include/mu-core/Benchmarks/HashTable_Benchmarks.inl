static void BM_HashTable_Add1_Dense(benchmark::State& st) {
	mu::HashTable<i32, i32> def_h;
	i32 num_existing = st.range(0);
	for (i32 i = 0; i < num_existing; ++i) {
		def_h.Add(i + num_existing, i + num_existing);
	}
	while (st.KeepRunning()) {
		st.PauseTiming();
		mu::HashTable<i32, i32> h = def_h;
		st.ResumeTiming();
		h.Add(1, 1);
	}
}
BENCHMARK(BM_HashTable_Add1_Dense)->Range(32, 1024);

#include <unordered_map>

static void BM_unorderedmap_Add1_Dense(benchmark::State& st) {
	while (st.KeepRunning()) {
		std::unordered_map<i32, i32> m;
		st.PauseTiming();
		i32 num_existing = st.range(0);
		for (i32 i = 0; i < num_existing; ++i) {
			m.insert({ i + num_existing, i + num_existing });
		}
		st.ResumeTiming();
		m.insert({ 2, 2 });
	}
}
BENCHMARK(BM_unorderedmap_Add1_Dense)->Range(32, 1024);