TEST_SUITE("String") {
	using namespace mu;

	TEST_CASE("Construct from cstring") {
		String s{ "test" };
		CHECK_EQ(4, s.GetLength());
	}
	TEST_CASE("Construct from tuple") {
		const char t[] = "test";
		std::tuple<const char*, size_t> tup{ t, sizeof(t) };
		String s{ tup };
		CHECK_EQ(4, s.GetLength());
	}
	TEST_CASE("Construct from char array") {
		Array<char> a;
		const char t[] = "test";
		for (const char* c = t; ; ++c) {
			a.Add(*c);
			if (*c == '\0')
				break;
		}
		String s{ a };
		CHECK_EQ(4, s.GetLength());
	}
	TEST_CASE("Construct from range of chars") {
		const char abc[] = "abc";
		String s{ Range(abc) };
		CHECK_EQ(String{ abc }, s);
	}
	TEST_CASE("Construct from multiple ranges") {
		const char abc[] = "abc";
		const char def[] = "def";
		String s{ Range(abc), Range(def) };
		CHECK_EQ(String{ "abcdef" }, s);
	}

	TEST_CASE("Format from strings") {
		String a{ "testA" };
		String b{ "testB" };

		String c = String::Format("{}{}", a, b);
		String expected = "testAtestB";
		CHECK_EQ(expected, c);
	}
	TEST_CASE("Format: int") {
		String s = String::Format("{}", 100);
		String expected = "100";
		CHECK_EQ(expected, s);
	}
}
