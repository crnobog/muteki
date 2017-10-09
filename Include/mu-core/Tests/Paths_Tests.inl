TEST_SUITE("Paths") {
	using namespace mu;

	TEST_CASE("Get extension") {
		SUBCASE("Filename") {
			String s{ "foo.txt" };
			auto ext = paths::GetExtension(Range(s));
			CHECK_FALSE(ext.IsEmpty());
			String ext_s{ ext };
			CHECK_EQ(String{"txt"}, ext_s);
		}
		SUBCASE("MultipleExtensions") {
			String s{ "foo.txt.zip" };
			auto ext = paths::GetExtension(Range(s));
			CHECK_FALSE(ext.IsEmpty());
			String ext_s{ ext };
			CHECK_EQ(String{"zip"}, ext_s);
		}
		SUBCASE("CurrentDir") {
			String s{ "." };
			auto ext = paths::GetExtension(Range(s));
			CHECK(ext.IsEmpty());
		}
		SUBCASE("ParentDir") {
			String s{ ".." };
			auto ext = paths::GetExtension(Range(s));
			CHECK(ext.IsEmpty());
		}

		SUBCASE("RelativePathPlusCurrentDir") {
			String s{ "../Foo/Bar/." };
			auto ext = paths::GetExtension(Range(s));
			CHECK(ext.IsEmpty());
		}
		SUBCASE("RelativePathPlusParentDir") {
			String s{ "/Foo/Bar/.." };
			auto ext = paths::GetExtension(Range(s));
			CHECK(ext.IsEmpty());
		}

		SUBCASE("RelativePathWithNoFilename") {
			String s{ "../Foo/Bar/Baz/" };
			auto ext = paths::GetExtension(Range(s));
			CHECK(ext.IsEmpty());
		}

		SUBCASE("RelativePathFilenameNoExtension") {
			String s{ "../Foo/Bar/Baz" };
			auto ext = paths::GetExtension(Range(s));
			CHECK(ext.IsEmpty());
		}

		SUBCASE("RelativePathFilenameWithExtension") {
			String s{ "../Foo/Bar/Baz.txt" };
			auto ext = paths::GetExtension(Range(s));
			CHECK_FALSE(ext.IsEmpty());
			String ext_s{ ext };
			CHECK_EQ(String{"txt"}, ext_s);
		}
	};

	TEST_CASE("Get filename") {
		SUBCASE("Filename") {
			String s{ "foo.txt" };
			auto fn = paths::GetFilename(Range(s));
			CHECK_FALSE(fn.IsEmpty());
			String fn_s{ fn };
			CHECK_EQ(s, fn_s);
		}
		SUBCASE("RelativePathAndFilename") {
			String s{ "../bar/baz/foo.txt" };
			auto fn = paths::GetFilename(Range(s));
			CHECK_FALSE(fn.IsEmpty());
			String fn_s{ fn };
			CHECK_EQ(String{ "foo.txt" }, fn_s);
		}
		SUBCASE("RootPathAndFilename") {
			String s{ "/foo.txt" };
			auto fn = paths::GetFilename(Range(s));
			CHECK_FALSE(fn.IsEmpty());
			String fn_s{ fn };
			CHECK_EQ(String{ "foo.txt" }, fn_s);
		}

		SUBCASE("MultipleExtensions") {
			String s{ "foo.txt.zip" };
			auto fn = paths::GetFilename(Range(s));
			CHECK_FALSE(fn.IsEmpty());
			String fn_s{ fn };
			CHECK_EQ(s, fn_s);
		}
	};
}