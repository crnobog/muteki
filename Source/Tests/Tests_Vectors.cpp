#include "mu-core/mu-core-tests.inl"
#include "../Vectors.h"


namespace Microsoft {
	namespace VisualStudio {
		namespace CppUnitTestFramework {
			template<typename T, size_t N>
			std::wstring ToString(const Vector<T, N>& v) {
				mu::String s;
				for (size_t i = 0; i < N; ++i) {
					if (i != 0) {
						s.Append(mu::Format(" ", v[i]));
					}
					else {
						s.Append(mu::Format(v[i]));
					}
				}
				return ToString(s);
			}
			template<typename T, size_t ROWS, size_t COLUMNS>
			std::wstring ToString(const Matrix<T, ROWS, COLUMNS>& m) {
				mu::String s;
				//for (size_t i = 0; i < N; ++i) {
				//	if (i != 0) {
				//		s.Append(mu::Format(" ", v[i]));
				//	}
				//	else {
				//		s.Append(mu::Format(v[i]));
				//	}
				//}
				return ToString(s);
			}
		}
	}
}

// These tests mostly exist to ensure all the templates are expanded and compiled
namespace mu_core_tests_vectors {
	using namespace mu;

	TEST_CLASS(VectorConstructTests) {
		TEST_METHOD(Vec2Construct) {
			Vec2 def;
			Assert::AreEqual(0.0f, def.X);
			Assert::AreEqual(0.0f, def.Y);

			Vec2 a{ 1.0f, 2.0f };
			Assert::AreEqual(1.0f, a.X);
			Assert::AreEqual(2.0f, a.Y);

			Vec2 b = { 1.0f, 2.0f };
			Assert::AreEqual(1.0f, b.X);
			Assert::AreEqual(2.0f, b.Y);

			Vec2 c(1.0f, 2.0f);
			Assert::AreEqual(1.0f, c.X);
			Assert::AreEqual(2.0f, c.Y);

			Vec2 d = Vec2(1.0f, 2.0f);
			Assert::AreEqual(1.0f, d.X);
			Assert::AreEqual(2.0f, d.Y);
		}

		TEST_METHOD(Vec3Construct) {
			Vec3 def;
			Assert::AreEqual(0.0f, def.X);
			Assert::AreEqual(0.0f, def.Y);
			Assert::AreEqual(0.0f, def.Z);

			Vec3 a{ 1.0f, 2.0f, 3.0f };
			Assert::AreEqual(1.0f, a.X);
			Assert::AreEqual(2.0f, a.Y);
			Assert::AreEqual(3.0f, a.Z);

			Vec3 b = { 1.0f, 2.0f, 3.0f };
			Assert::AreEqual(1.0f, b.X);
			Assert::AreEqual(2.0f, b.Y);
			Assert::AreEqual(3.0f, b.Z);

			Vec3 c(1.0f, 2.0f, 3.0f);
			Assert::AreEqual(1.0f, c.X);
			Assert::AreEqual(2.0f, c.Y);
			Assert::AreEqual(3.0f, c.Z);

			Vec3 d = Vec3(1.0f, 2.0f, 3.0f);
			Assert::AreEqual(1.0f, d.X);
			Assert::AreEqual(2.0f, d.Y);
			Assert::AreEqual(3.0f, d.Z);
		}
	};
	TEST_CLASS(VectorMemberAliasTests) {
		TEST_METHOD(Vec2MembersAlias) {
			Vec2 v = { 10, 20 };
			Assert::AreEqual(10.0f, v.X);
			Assert::AreEqual(10.0f, v[0]);
			Assert::AreEqual(10.0f, v.Data[0]);
			Assert::AreEqual(20.0f, v.Y);
			Assert::AreEqual(20.0f, v[1]);
			Assert::AreEqual(20.0f, v.Data[1]);
		}
		TEST_METHOD(Vec3MembersAlias) {
			Vec3 v{ 2, 4, 9 };
			Assert::AreEqual(2.0f, v.X);
			Assert::AreEqual(2.0f, v.R);
			Assert::AreEqual(2.0f, v[0]);
			Assert::AreEqual(2.0f, v.Data[0]);

			Assert::AreEqual(4.0f, v.Y);
			Assert::AreEqual(4.0f, v.G);
			Assert::AreEqual(4.0f, v[1]);
			Assert::AreEqual(4.0f, v.Data[1]);

			Assert::AreEqual(9.0f, v.Z);
			Assert::AreEqual(9.0f, v.B);
			Assert::AreEqual(9.0f, v[2]);
			Assert::AreEqual(9.0f, v.Data[2]);

			Assert::AreEqual(Vec2{ 2, 4 }, v.XY);
		}
		TEST_METHOD(Vec4MembersAlias) {
			Vec4 v{ 8, 3, 5, 2 };
			Assert::AreEqual(8.0f, v.X);
			Assert::AreEqual(8.0f, v.R);
			Assert::AreEqual(8.0f, v[0]);
			Assert::AreEqual(8.0f, v.Data[0]);

			Assert::AreEqual(3.0f, v.Y);
			Assert::AreEqual(3.0f, v.G);
			Assert::AreEqual(3.0f, v[1]);
			Assert::AreEqual(3.0f, v.Data[1]);

			Assert::AreEqual(5.0f, v.Z);
			Assert::AreEqual(5.0f, v.B);
			Assert::AreEqual(5.0f, v[2]);
			Assert::AreEqual(5.0f, v.Data[2]);

			Assert::AreEqual(2.0f, v.W);
			Assert::AreEqual(2.0f, v.A);
			Assert::AreEqual(2.0f, v[3]);
			Assert::AreEqual(2.0f, v.Data[3]);

			Assert::AreEqual(Vec2{ 8, 3 }, v.XY);
			Assert::AreEqual(Vec3{ 8, 3, 5 }, v.XYZ);
			Assert::AreEqual(Vec3{ 8, 3, 5 }, v.RGB);

		}
	};

	TEST_CLASS(DotProductTests) {
		TEST_METHOD(Dot2) {
			Vec2 a{ 1, 2 };
			Vec2 b{ 2, 3 };
			float d = Dot(a, b);
			Assert::AreEqual(8.0f, d);
		}
		TEST_METHOD(Dot3) {
			Vec3 a{ 1, 2, 4 };
			Vec3 b{ 2, 3, 5 };
			float d = Dot(a, b);
			Assert::AreEqual(2.0f + 6.0f + 20.0f, d);
		}
		TEST_METHOD(Dot4) {
			Vec4 a{ 1, 2, 2, 2 };
			Vec4 b{ 2, 1, 3, 2 };
			float d = Dot(a, b);
			Assert::AreEqual(2 + 2 + 6 + 4.0f, d);
		}
	};

	TEST_CLASS(CrossProductTests) {
		TEST_METHOD(CrossBases) {
			Vec3 x{ 1, 0, 0 };
			Vec3 y{ 0, 1, 0 };
			Vec3 z{ 0, 0, 1 };

			Vec3 xy = Cross(x, y);
			Assert::AreEqual(z, xy);
			Vec3 yx = Cross(y, x);
			Assert::AreEqual(-z, yx);

			Vec3 yz = Cross(y, z);
			Assert::AreEqual(x, yz);
			Vec3 zy = Cross(z, y);
			Assert::AreEqual(-x, zy);

			Vec3 zx = Cross(z, x);
			Assert::AreEqual(y, zx);
			Vec3 xz = Cross(x, z);
			Assert::AreEqual(-y, xz);
		}
	};

	TEST_CLASS(ArithmeticTests) {
		TEST_METHOD(AddVectors) {
			Assert::AreEqual(Vec2{ 30, 50 }, Vec2{ 10, 10 }+Vec2{ 20, 40 });

			Assert::AreEqual(Vec3{ 3, 5, 10 }, Vec3{ 5, 5, 2 }+Vec3{ -2, 0, 8 });

			Assert::AreEqual(Vec4{ 23, 18, 10, 16 }, Vec4{ 13, 12, 24, 32 }+Vec4{ 10, 6, -14, -16 });
		}
		TEST_METHOD(AddEqVectors) {
			Vec2 a = { 10, 14 };
			a += Vec2{ 7, 12 };
			Assert::AreEqual(Vec2{ 17, 26 }, a);

			Vec3 b = { 1, 7, 19 };
			b += Vec3{ 8, 9, -10 };
			Assert::AreEqual(Vec3{ 9, 16, 9 }, b);

			Vec4 c = { 7, 11, 14, -6 };
			c += Vec4{ 7, 4, -9, 12 };
			Assert::AreEqual(Vec4{ 14, 15, 5, 6 }, c);
		}
		TEST_METHOD(SubVectors) {
			Assert::AreEqual(Vec2{ 5, 4 }, Vec2{ 10, 10 }-Vec2{ 5, 6 });

			Assert::AreEqual(Vec3{ 7, 5, -6 }, Vec3{ 5, 5, 2 }-Vec3{ -2, 0, 8 });

			Assert::AreEqual(Vec4{ 3, 6, 18, 18 }, Vec4{ 13, 12, 4, 2 }-Vec4{ 10, 6, -14, -16 });
		}
		TEST_METHOD(SubEqVectors) {
			Vec2 a = { 10, 14 };
			a -= Vec2{ 7, 12 };
			Assert::AreEqual(Vec2{ 3, 2 }, a);

			Vec3 b = { 1, 7, 19 };
			b -= Vec3{ 8, 9, -10 };
			Assert::AreEqual(Vec3{ -7, -2, 29 }, b);

			Vec4 c = { 7, 11, 14, -6 };
			c -= Vec4{ 7, 4, -9, 12 };
			Assert::AreEqual(Vec4{ 0, 7, 23, -18 }, c);
		}
		TEST_METHOD(MulVectors) {
			Assert::AreEqual(Vec2{ 50, 60 }, Vec2{ 10, 10 }*Vec2{ 5, 6 });

			Assert::AreEqual(Vec3{ -10, 0, 16 }, Vec3{ 5, 5, 2 }*Vec3{ -2, 0, 8 });

			Assert::AreEqual(Vec4{ 10, 24, 6, 16 }, Vec4{ 1, 4, 2, 2 }*Vec4{ 10, 6, 3, 8 });
		}
		TEST_METHOD(MulEqVectors) {
			Vec2 a = { 10, 4 };
			a *= Vec2{ 7, 12 };
			Assert::AreEqual(Vec2{ 70, 48 }, a);

			Vec3 b = { 1, 7, 2 };
			b *= Vec3{ 3, 3, -10 };
			Assert::AreEqual(Vec3{ 3, 21, -20 }, b);

			Vec4 c = { 7, 11, 14, -6 };
			c *= Vec4{ 0, -2, 3, -3 };
			Assert::AreEqual(Vec4{ 0, -22, 42, 18 }, c);
		}
		TEST_METHOD(DivVectors) {
			Assert::AreEqual(Vec2{ 1, 3 }, Vec2{ 2, 9 } / Vec2{ 2, 3 });

			Assert::AreEqual(Vec3{ 2, 3, 2 }, Vec3{ 14, 15, 22 } / Vec3{ 7, 5, 11 });

			Assert::AreEqual(Vec4{ 3, 1, 2, 5 }, Vec4{ 24, 8, 12, 10 } / Vec4{ 8, 8, 6, 2 });
		}
		TEST_METHOD(DivEqVectors) {
			Vec2 a = { 12, 14 };
			a /= Vec2{ 2, 7 };
			Assert::AreEqual(Vec2{ 6, 2 }, a);

			Vec3 b = { 100, 18, 9 };
			b /= Vec3{ 10, 9, 9 };
			Assert::AreEqual(Vec3{ 10, 2, 1 }, b);

			Vec4 c = { 10, 12, 22, 16 };
			c /= Vec4{ 5, 6, 2, 4 };
			Assert::AreEqual(Vec4{ 2, 2, 11, 4 }, c);
		}
		TEST_METHOD(MulScalarVectors) {
			Vec2 a{ 2, 3 };
			Assert::AreEqual(Vec2{ 4, 6 }, a * 2);

			Vec3 b{ 11, 4, 12 };
			Assert::AreEqual(Vec3{ 33, 12, 36 }, b * 3);

			Vec4 c{ 5, 1, 3, 6 };
			Assert::AreEqual(Vec4{ 15, 3, 9, 18 }, c * 3);
		}
		TEST_METHOD(MulEqScalarVectors) {
			Vec2 a{ 2, 3 };
			a *= 2;
			Assert::AreEqual(Vec2{ 4, 6 }, a);

			Vec3 b{ 11, 4, 12 };
			b *= 3;
			Assert::AreEqual(Vec3{ 33, 12, 36 }, b);

			Vec4 c{ 5, 1, 3, 6 };
			c *= 3;
			Assert::AreEqual(Vec4{ 15, 3, 9, 18 }, c);
		}
		TEST_METHOD(DivScalarVectors) {
			Vec2 a{ 4, 6 };
			Assert::AreEqual(Vec2{ 2, 3 }, a / 2);

			Vec3 b{ 9, 18, 36 };
			Assert::AreEqual(Vec3{ 3, 6, 12 }, b / 3);

			Vec4 c{ 15, 10, 5, 25 };
			Assert::AreEqual(Vec4{ 3, 2, 1, 5 }, c / 5);
		}
		TEST_METHOD(DivEqScalarVectors) {
			Vec2 a{ 4, 6 };
			a /= 2;
			Assert::AreEqual(Vec2{ 2, 3 }, a);

			Vec3 b{ 9, 18, 36 };
			b /= 3;
			Assert::AreEqual(Vec3{ 3, 6, 12 }, b);

			Vec4 c{ 15, 10, 5, 25 };
			c /= 5;
			Assert::AreEqual(Vec4{ 3, 2, 1, 5 }, c);
		}
	};

	TEST_CLASS(MatrixMulTests) {
		TEST_METHOD(Mat44Vec4Mul) {
			Vec4 v{ 1, 2, 3, 4 };
			Mat4x4 m{ 1, 0, 0, 0,
					 0, 1, 0, 0,
					 0, 0, 1, 0,
					 0, 0, 0, 1 };
			Vec4 rs = m * v;
			Assert::AreEqual(v, rs);
		}
		TEST_METHOD(Mat44Mat44Mul) {
			Mat4x4 m1{ 1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1 };
			Mat4x4 m2{ 1, 2, 3, 4,
				5, 6, 7, 8,
				9, 10, 11, 12,
				13, 14, 15, 16 };
			Assert::AreEqual(m2, m1 * m2);
			Assert::AreEqual(m2, m2 * m1);
		}
	};
}