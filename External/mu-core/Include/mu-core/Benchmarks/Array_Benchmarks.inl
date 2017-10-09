static void BM_Array_Fill(benchmark::State& st) {
	while (st.KeepRunning()) {
		i32 num = st.range(0);
		mu::Array<i32> a;
		for (i32 i = 0; i < num; ++i) {
			a.Add(i);
		}
	}
}

BENCHMARK(BM_Array_Fill)->Range(8, 8 <<10);

static void BM_Array_Fill_Reserved(benchmark::State& st) {
	while (st.KeepRunning()) {
		i32 num = st.range(0);
		mu::Array<i32> a;
		a.Reserve(num);
		for (i32 i = 0; i < num; ++i) {
			a.Add(i);
		}
	}
}

BENCHMARK(BM_Array_Fill_Reserved)->Range(8, 8 << 10);

static void BM_stdvector_Fill(benchmark::State& st) {
	while (st.KeepRunning()) {
		i32 num = st.range(0);
		std::vector<i32> v;
		for (i32 i = 0; i < num; ++i) {
			v.push_back(i);
		}
	}
}

BENCHMARK(BM_stdvector_Fill)->Range(8, 8 << 10);

static void BM_stdvector_Fill_Reserved(benchmark::State& st) {
	while (st.KeepRunning()) {
		i32 num = st.range(0);
		std::vector<i32> v;
		v.reserve(num);
		for (i32 i = 0; i < num; ++i) {
			v.push_back(i);
		}
	}
}

BENCHMARK(BM_stdvector_Fill_Reserved)->Range(8, 8 << 10);