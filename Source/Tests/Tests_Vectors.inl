
// These tests mostly exist to ensure all the templates are expanded and compiled
TEST_SUITE("Vectors") {
	TEST_CASE("VectorConstruction") {
		SUBCASE("Vec2Construct") {
			Vec2 def;
			CHECK_EQ(0.0f, def.X);
			CHECK_EQ(0.0f, def.Y);

			Vec2 a{ 1.0f, 2.0f };
			CHECK_EQ(1.0f, a.X);
			CHECK_EQ(2.0f, a.Y);

			Vec2 b = { 1.0f, 2.0f };
			CHECK_EQ(1.0f, b.X);
			CHECK_EQ(2.0f, b.Y);

			Vec2 c(1.0f, 2.0f);
			CHECK_EQ(1.0f, c.X);
			CHECK_EQ(2.0f, c.Y);

			Vec2 d = Vec2(1.0f, 2.0f);
			CHECK_EQ(1.0f, d.X);
			CHECK_EQ(2.0f, d.Y);
		}

		SUBCASE("Vec3Construct") {
			Vec3 def;
			CHECK_EQ(0.0f, def.X);
			CHECK_EQ(0.0f, def.Y);
			CHECK_EQ(0.0f, def.Z);

			Vec3 a{ 1.0f, 2.0f, 3.0f };
			CHECK_EQ(1.0f, a.X);
			CHECK_EQ(2.0f, a.Y);
			CHECK_EQ(3.0f, a.Z);

			Vec3 b = { 1.0f, 2.0f, 3.0f };
			CHECK_EQ(1.0f, b.X);
			CHECK_EQ(2.0f, b.Y);
			CHECK_EQ(3.0f, b.Z);

			Vec3 c(1.0f, 2.0f, 3.0f);
			CHECK_EQ(1.0f, c.X);
			CHECK_EQ(2.0f, c.Y);
			CHECK_EQ(3.0f, c.Z);

			Vec3 d = Vec3(1.0f, 2.0f, 3.0f);
			CHECK_EQ(1.0f, d.X);
			CHECK_EQ(2.0f, d.Y);
			CHECK_EQ(3.0f, d.Z);
		}
	};

	TEST_CASE("VectorMemberAliasTests") {
		SUBCASE("Vec2MembersAlias") {
			Vec2 v = { 10, 20 };
			CHECK_EQ(10.0f, v.X);
			CHECK_EQ(10.0f, v[0]);
			CHECK_EQ(10.0f, v.Data[0]);
			CHECK_EQ(20.0f, v.Y);
			CHECK_EQ(20.0f, v[1]);
			CHECK_EQ(20.0f, v.Data[1]);
		}
		SUBCASE("Vec3MembersAlias") {
			Vec3 v{ 2, 4, 9 };
			CHECK_EQ(2.0f, v.X);
			CHECK_EQ(2.0f, v.R);
			CHECK_EQ(2.0f, v[0]);
			CHECK_EQ(2.0f, v.Data[0]);

			CHECK_EQ(4.0f, v.Y);
			CHECK_EQ(4.0f, v.G);
			CHECK_EQ(4.0f, v[1]);
			CHECK_EQ(4.0f, v.Data[1]);

			CHECK_EQ(9.0f, v.Z);
			CHECK_EQ(9.0f, v.B);
			CHECK_EQ(9.0f, v[2]);
			CHECK_EQ(9.0f, v.Data[2]);

			CHECK_EQ(Vec2{ 2, 4 }, v.XY);
		}
		SUBCASE("Vec4MembersAlias") {
			Vec4 v{ 8, 3, 5, 2 };
			CHECK_EQ(8.0f, v.X);
			CHECK_EQ(8.0f, v.R);
			CHECK_EQ(8.0f, v[0]);
			CHECK_EQ(8.0f, v.Data[0]);

			CHECK_EQ(3.0f, v.Y);
			CHECK_EQ(3.0f, v.G);
			CHECK_EQ(3.0f, v[1]);
			CHECK_EQ(3.0f, v.Data[1]);

			CHECK_EQ(5.0f, v.Z);
			CHECK_EQ(5.0f, v.B);
			CHECK_EQ(5.0f, v[2]);
			CHECK_EQ(5.0f, v.Data[2]);

			CHECK_EQ(2.0f, v.W);
			CHECK_EQ(2.0f, v.A);
			CHECK_EQ(2.0f, v[3]);
			CHECK_EQ(2.0f, v.Data[3]);

			CHECK_EQ(Vec2{ 8, 3 }, v.XY);
			CHECK_EQ(Vec3{ 8, 3, 5 }, v.XYZ);
			CHECK_EQ(Vec3{ 8, 3, 5 }, v.RGB);

		}
	};

	TEST_CASE("DotProductTests") {
		SUBCASE("Dot2") {
			Vec2 a{ 1, 2 };
			Vec2 b{ 2, 3 };
			float d = Dot(a, b);
			CHECK_EQ(8.0f, d);
		}
		SUBCASE("Dot3") {
			Vec3 a{ 1, 2, 4 };
			Vec3 b{ 2, 3, 5 };
			float d = Dot(a, b);
			CHECK_EQ(2.0f + 6.0f + 20.0f, d);
		}
		SUBCASE("Dot4") {
			Vec4 a{ 1, 2, 2, 2 };
			Vec4 b{ 2, 1, 3, 2 };
			float d = Dot(a, b);
			CHECK_EQ(2 + 2 + 6 + 4.0f, d);
		}
	};

	TEST_CASE("CrossProductTests") {
		SUBCASE("CrossBases") {
			Vec3 x{ 1, 0, 0 };
			Vec3 y{ 0, 1, 0 };
			Vec3 z{ 0, 0, 1 };

			Vec3 xy = Cross(x, y);
			CHECK_EQ(z, xy);
			Vec3 yx = Cross(y, x);
			CHECK_EQ(-z, yx);

			Vec3 yz = Cross(y, z);
			CHECK_EQ(x, yz);
			Vec3 zy = Cross(z, y);
			CHECK_EQ(-x, zy);

			Vec3 zx = Cross(z, x);
			CHECK_EQ(y, zx);
			Vec3 xz = Cross(x, z);
			CHECK_EQ(-y, xz);
		}
	};

	TEST_CASE("MagnitudeTests") {
		Vec3 x{ 1.0f, 0.0f, 0.0f };
		Vec3 y{ 0.0f, 1.0f, 0.0f };
		Vec3 z{ 0.0f, 0.0f, 1.0f };

		CHECK_EQ(1.0f, MagnitudeSq(x));
		CHECK_EQ(1.0f, MagnitudeSq(y));
		CHECK_EQ(1.0f, MagnitudeSq(z));

		CHECK_EQ(1.0f, Magnitude(x));
		CHECK_EQ(1.0f, Magnitude(y));
		CHECK_EQ(1.0f, Magnitude(z));

		Vec3 a{ 3.0f, 4.0f, 0.0f };
		CHECK_EQ(25.0f, MagnitudeSq(a));
		CHECK_EQ(5.0f, Magnitude(a));
	}

	TEST_CASE("ArithmeticTests") {
		SUBCASE("AddVectors") {
			CHECK_EQ(Vec2{ 30, 50 }, Vec2{ 10, 10 }+Vec2{ 20, 40 });

			CHECK_EQ(Vec3{ 3, 5, 10 }, Vec3{ 5, 5, 2 }+Vec3{ -2, 0, 8 });

			CHECK_EQ(Vec4{ 23, 18, 10, 16 }, Vec4{ 13, 12, 24, 32 }+Vec4{ 10, 6, -14, -16 });
		}
		SUBCASE("AddEqVectors") {
			Vec2 a = { 10, 14 };
			a += Vec2{ 7, 12 };
			CHECK_EQ(Vec2{ 17, 26 }, a);

			Vec3 b = { 1, 7, 19 };
			b += Vec3{ 8, 9, -10 };
			CHECK_EQ(Vec3{ 9, 16, 9 }, b);

			Vec4 c = { 7, 11, 14, -6 };
			c += Vec4{ 7, 4, -9, 12 };
			CHECK_EQ(Vec4{ 14, 15, 5, 6 }, c);
		}
		SUBCASE("SubVectors") {
			CHECK_EQ(Vec2{ 5, 4 }, Vec2{ 10, 10 }-Vec2{ 5, 6 });

			CHECK_EQ(Vec3{ 7, 5, -6 }, Vec3{ 5, 5, 2 }-Vec3{ -2, 0, 8 });

			CHECK_EQ(Vec4{ 3, 6, 18, 18 }, Vec4{ 13, 12, 4, 2 }-Vec4{ 10, 6, -14, -16 });
		}
		SUBCASE("SubEqVectors") {
			Vec2 a = { 10, 14 };
			a -= Vec2{ 7, 12 };
			CHECK_EQ(Vec2{ 3, 2 }, a);

			Vec3 b = { 1, 7, 19 };
			b -= Vec3{ 8, 9, -10 };
			CHECK_EQ(Vec3{ -7, -2, 29 }, b);

			Vec4 c = { 7, 11, 14, -6 };
			c -= Vec4{ 7, 4, -9, 12 };
			CHECK_EQ(Vec4{ 0, 7, 23, -18 }, c);
		}
		SUBCASE("MulVectors") {
			CHECK_EQ(Vec2{ 50, 60 }, Vec2{ 10, 10 }*Vec2{ 5, 6 });

			CHECK_EQ(Vec3{ -10, 0, 16 }, Vec3{ 5, 5, 2 }*Vec3{ -2, 0, 8 });

			CHECK_EQ(Vec4{ 10, 24, 6, 16 }, Vec4{ 1, 4, 2, 2 }*Vec4{ 10, 6, 3, 8 });
		}
		SUBCASE("MulEqVectors") {
			Vec2 a = { 10, 4 };
			a *= Vec2{ 7, 12 };
			CHECK_EQ(Vec2{ 70, 48 }, a);

			Vec3 b = { 1, 7, 2 };
			b *= Vec3{ 3, 3, -10 };
			CHECK_EQ(Vec3{ 3, 21, -20 }, b);

			Vec4 c = { 7, 11, 14, -6 };
			c *= Vec4{ 0, -2, 3, -3 };
			CHECK_EQ(Vec4{ 0, -22, 42, 18 }, c);
		}
		SUBCASE("DivVectors") {
			CHECK_EQ(Vec2{ 1, 3 }, Vec2{ 2, 9 } / Vec2{ 2, 3 });

			CHECK_EQ(Vec3{ 2, 3, 2 }, Vec3{ 14, 15, 22 } / Vec3{ 7, 5, 11 });

			CHECK_EQ(Vec4{ 3, 1, 2, 5 }, Vec4{ 24, 8, 12, 10 } / Vec4{ 8, 8, 6, 2 });
		}
		SUBCASE("DivEqVectors") {
			Vec2 a = { 12, 14 };
			a /= Vec2{ 2, 7 };
			CHECK_EQ(Vec2{ 6, 2 }, a);

			Vec3 b = { 100, 18, 9 };
			b /= Vec3{ 10, 9, 9 };
			CHECK_EQ(Vec3{ 10, 2, 1 }, b);

			Vec4 c = { 10, 12, 22, 16 };
			c /= Vec4{ 5, 6, 2, 4 };
			CHECK_EQ(Vec4{ 2, 2, 11, 4 }, c);
		}
		SUBCASE("MulScalarVectors") {
			Vec2 a{ 2, 3 };
			CHECK_EQ(Vec2{ 4, 6 }, a * 2);

			Vec3 b{ 11, 4, 12 };
			CHECK_EQ(Vec3{ 33, 12, 36 }, b * 3);

			Vec4 c{ 5, 1, 3, 6 };
			CHECK_EQ(Vec4{ 15, 3, 9, 18 }, c * 3);
		}
		SUBCASE("MulEqScalarVectors") {
			Vec2 a{ 2, 3 };
			a *= 2;
			CHECK_EQ(Vec2{ 4, 6 }, a);

			Vec3 b{ 11, 4, 12 };
			b *= 3;
			CHECK_EQ(Vec3{ 33, 12, 36 }, b);

			Vec4 c{ 5, 1, 3, 6 };
			c *= 3;
			CHECK_EQ(Vec4{ 15, 3, 9, 18 }, c);
		}
		SUBCASE("DivScalarVectors") {
			Vec2 a{ 4, 6 };
			CHECK_EQ(Vec2{ 2, 3 }, a / 2);

			Vec3 b{ 9, 18, 36 };
			CHECK_EQ(Vec3{ 3, 6, 12 }, b / 3);

			Vec4 c{ 15, 10, 5, 25 };
			CHECK_EQ(Vec4{ 3, 2, 1, 5 }, c / 5);
		}
		SUBCASE("DivEqScalarVectors") {
			Vec2 a{ 4, 6 };
			a /= 2;
			CHECK_EQ(Vec2{ 2, 3 }, a);

			Vec3 b{ 9, 18, 36 };
			b /= 3;
			CHECK_EQ(Vec3{ 3, 6, 12 }, b);

			Vec4 c{ 15, 10, 5, 25 };
			c /= 5;
			CHECK_EQ(Vec4{ 3, 2, 1, 5 }, c);
		}
	};

	TEST_CASE("MatrixMulTests") {
		SUBCASE("Mat44Vec4Mul") {
			Vec4 v{ 1, 2, 3, 4 };
			Mat4x4 m{ 1, 0, 0, 0,
					 0, 1, 0, 0,
					 0, 0, 1, 0,
					 0, 0, 0, 1 };
			Vec4 rs = m * v;
			CHECK_EQ(v, rs);
		}
		SUBCASE("Mat44Mat44Mul") {
			Mat4x4 m1{ 1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1 };
			Mat4x4 m2{ 1, 2, 3, 4,
				5, 6, 7, 8,
				9, 10, 11, 12,
				13, 14, 15, 16 };
			CHECK_EQ(m2, m1 * m2);
			CHECK_EQ(m2, m2 * m1);
		}
	};
}
