
#include "CoreMath.h"
#include "TransformMatrices.h"
#include "Quaternion.h"

template<typename T, size_t N>
std::ostream& operator<< (std::ostream& os, const Vector<T, N>& value) {
	os << "(";
	for (size_t i = 0; i < N; ++i) {
		if (i != 0) { os << ", "; }
		os << value[i];
	}
	os << ")";
	return os;
}

template<typename T, size_t ROWS, size_t COLUMNS>
std::ostream& operator<< (std::ostream& os, const Matrix<T, ROWS, COLUMNS>& value) {
	os << "[";
	for (size_t i = 0; i < ROWS; ++i) {
		for (size_t j = 0; j < COLUMNS; ++j) {
			if (j != 0) { os << ", "; }
			os << value(i, j);
		}
		if (i != ROWS - 1) { os << "\n"; }
	}
	os << "]";
	return os;
}


std::ostream& operator<< (std::ostream& os, Quat value) {
	os << "(" << value.X << "," << value.Y << "," << value.Z << "," << value.W << ")";
	return os;
}

#define CHECK_MATRIX_EQ(m1, m2) \
	for (size_t i = 0; i < 4; ++i) {\
		for (size_t j = 0; j < 4; ++j) {\
			CHECK_EQ(m1(i, j), m2(i, j));\
		}\
	}

#define CHECK_MATRIX_EQ_APPROX(m1, m2) \
	for (size_t i = 0; i < 4; ++i) {\
		for (size_t j = 0; j < 4; ++j) {\
			CHECK_EQ(m1(i, j), doctest::Approx(m2(i, j)));\
		}\
	}

#define CHECK_VEC3_EQ_APPROX(v1, v2) \
	CHECK_EQ(v1.X, doctest::Approx(v2.X)); \
	CHECK_EQ(v1.Y, doctest::Approx(v2.Y)); \
	CHECK_EQ(v1.Z, doctest::Approx(v2.Z)); \

#define CHECK_VEC4_EQ_APPROX(v1, v2) \
	CHECK_VEC3_EQ_APPROX(v1, v2); \
	CHECK_EQ(v1.W, doctest::Approx(v2.W)); \

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
			Vec3 x = Vec3::UnitX;
			Vec3 y = Vec3::UnitY;
			Vec3 z = Vec3::UnitZ;

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
		Vec3 x = Vec3::UnitX;
		Vec3 y = Vec3::UnitY;
		Vec3 z = Vec3::UnitZ;

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

	TEST_CASE("NormalizeTests") {
		Vec3 x = Vec3::UnitX;
		Vec3 y = Vec3::UnitY;
		Vec3 z = Vec3::UnitZ;

		CHECK_EQ(x, Normalize(x));
		CHECK_EQ(y, Normalize(y));
		CHECK_EQ(z, Normalize(z));

		Vec3 a{ 3.0f, 4.0f, 0.0f };
		CHECK_EQ(Vec3{ 0.6f, 0.8f, 0.0f }, Normalize(a));

		Vec3 small{ 3.0e-25f, 4.0e-25f, 0.0f };
		Vec3 large{ 3.0e25f, 4.0e25f, 0.0f };

		CHECK_EQ(Vec3{ 0.6f, 0.8f, 0.0f }, Normalize(small));
		CHECK_EQ(Vec3{ 0.6f, 0.8f, 0.0f }, Normalize(large));
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
}

TEST_SUITE("Matrices") {
	TEST_CASE("Construct") {
		Mat4x4 uninit;

		for (size_t i = 0; i < 16; ++i) {
			CHECK_EQ(0.0f, uninit.Data[i]);
		}

		// Storage order is the same order that init lists are used in
		Matrix<i32, 2, 2> init_list{ 0, 1, 2, 3 };
		CHECK_EQ(0, init_list.Data[0]);
		CHECK_EQ(1, init_list.Data[1]);
		CHECK_EQ(2, init_list.Data[2]);
		CHECK_EQ(3, init_list.Data[3]);

		// Storage order is row-major
		CHECK_EQ(0, init_list(0, 0));
		CHECK_EQ(1, init_list(0, 1));
		CHECK_EQ(2, init_list(1, 0));
		CHECK_EQ(3, init_list(1, 1));

		// Array-of-arrays and subscript operator index the same
		CHECK_EQ(init_list.M[0][0], init_list(0, 0));
		CHECK_EQ(init_list.M[0][1], init_list(0, 1));
		CHECK_EQ(init_list.M[1][0], init_list(1, 0));
		CHECK_EQ(init_list.M[1][1], init_list(1, 1));

		Mat4x4 identity4 = Mat4x4::Identity();
		for (size_t i = 0; i < 4; ++i) {
			for (size_t j = 0; j < 4; ++j) {
				if (i == j) { CHECK_EQ(identity4(i, j), 1.0f); }
				else { CHECK_EQ(identity4(i, j), 0.0f); }
			}
		}
	}
	TEST_CASE("Transpose") {
		Matrix<i32, 4, 2> m{
			0, 1,
			2, 3,
			4, 5,
			6, 7 };
		Matrix<i32, 2, 4> t = Transpose(m);

		CHECK_EQ(0, t(0, 0));
		CHECK_EQ(1, t(1, 0));
		CHECK_EQ(2, t(0, 1));
		CHECK_EQ(3, t(1, 1));
		CHECK_EQ(4, t(0, 2));
		CHECK_EQ(5, t(1, 2));
		CHECK_EQ(6, t(0, 3));
		CHECK_EQ(7, t(1, 3));

		Vector<i32, 3> v{ 4, 5, 6 };
		Matrix<i32, 1, 3> tv = Transpose(v);
		CHECK_EQ(4, tv(0, 0));
		CHECK_EQ(5, tv(0, 1));
		CHECK_EQ(6, tv(0, 2));
	}
	TEST_CASE("Multiply") {
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
	}
	TEST_CASE("MultiplyRotationX") {
		SUBCASE("Concat45") {
			Mat4x4 m45 = CreateRotationX(PiOver4);
			Mat4x4 m90 = CreateRotationX(PiOver2);
			Mat4x4 concat = m45 * m45;

			for (size_t i = 0; i < 16; ++i) {
				CHECK_EQ(concat.Data[i], doctest::Approx(m90.Data[i]));
			}
		}
	}
}

TEST_SUITE("TransformMatrices") {
	TEST_CASE("RotateX90") {
		Mat4x4 m = CreateRotationX(DegreesToRadians(90.0f));
		Vec4 v = { 0, 1, 0, 1 };
		Vec4 v_ = m * v;

		Vec4 expected = { 0, 0, 1, 1 };
		CHECK_VEC4_EQ_APPROX(v_, expected);
	}

	TEST_CASE("RotateY90") {
		Mat4x4 m = CreateRotationY(DegreesToRadians(90.0f));
		Vec4 v = { 0, 0, 1, 1 };
		Vec4 v_ = m * v;

		Vec4 expected = { 1, 0, 0, 1 };
		CHECK_VEC4_EQ_APPROX(v_, expected);
	}

	TEST_CASE("RotateZ90") {
		Mat4x4 m = CreateRotationZ(DegreesToRadians(90.0f));
		Vec4 v = { 1, 0, 0, 1 };
		Vec4 v_ = m * v;

		Vec4 expected = { 0, 1, 0, 1 };
		CHECK_VEC4_EQ_APPROX(v_, expected);
	}

	TEST_CASE("AxisAngle_Equivalent_X45") {
		f32 rads = DegreesToRadians(45.0f);
		Mat4x4 m1 = CreateRotationAxisAngle({ 1, 0, 0 }, rads);
		Mat4x4 m2 = CreateRotationX(rads);

		CHECK_MATRIX_EQ_APPROX(m1, m2);
	}
	TEST_CASE("AxisAngle_Equivalent_Y60") {
		f32 rads = DegreesToRadians(60.0f);
		Mat4x4 m1 = CreateRotationAxisAngle({ 0, 1, 0 }, rads);
		Mat4x4 m2 = CreateRotationY(rads);

		CHECK_MATRIX_EQ_APPROX(m1, m2);
	}
	TEST_CASE("AxisAngle_Equivalent_Z70") {
		f32 rads = DegreesToRadians(70.0f);
		Mat4x4 m1 = CreateRotationAxisAngle({ 0, 0, 1 }, rads);
		Mat4x4 m2 = CreateRotationZ(rads);

		CHECK_MATRIX_EQ_APPROX(m1, m2);
	}
}

TEST_SUITE("Quats") {
	TEST_CASE("Construct") {
		Quat q1;
		CHECK_EQ(q1.X, 0.0f);
		CHECK_EQ(q1.Y, 0.0f);
		CHECK_EQ(q1.Z, 0.0f);
		CHECK_EQ(q1.W, 1.0f);

		Quat q2 = Quat::Identity();
		CHECK_EQ(q2.X, 0.0f);
		CHECK_EQ(q2.Y, 0.0f);
		CHECK_EQ(q2.Z, 0.0f);
		CHECK_EQ(q2.W, 1.0f);

		CHECK_EQ(q1, q2);
	}

	TEST_CASE("Concat") {
		Quat qy45 = Quat::FromAxisAngle({ 0, 1, 0 }, DegreesToRadians(45.0f));
		Quat qy90 = Quat::FromAxisAngle({ 0, 1, 0 }, DegreesToRadians(90.0f));

		Quat qcomposed = qy45 * qy45;

		CHECK_EQ(qy90.X, doctest::Approx(qcomposed.X));
		CHECK_EQ(qy90.Y, doctest::Approx(qcomposed.Y));
		CHECK_EQ(qy90.Z, doctest::Approx(qcomposed.Z));
		CHECK_EQ(qy90.W, doctest::Approx(qcomposed.W));
	}

	TEST_CASE("RotateVectorX90") {
		Quat q = Quat::FromAxisAngle({ 1, 0, 0 }, DegreesToRadians(90.0f));
		Vec3 v = { 0, 1, 0 };
		Vec3 v_ = q.Rotate(v);

		Vec3 expected{ 0, 0, 1 };
		CHECK_VEC3_EQ_APPROX(v_, expected);
	}

	TEST_CASE("RotateVectorY90") {
		Quat q = Quat::FromAxisAngle({ 0, 1, 0 }, DegreesToRadians(90.0f));
		Vec3 v = { 0, 0, 1 };
		Vec3 v_ = q.Rotate(v);

		Vec3 expected{ 1, 0, 0 };
		CHECK_VEC3_EQ_APPROX(v_, expected);
	}

	TEST_CASE("RotateVectorZ90") {
		Quat q = Quat::FromAxisAngle({ 0, 0, 1 }, DegreesToRadians(90.0f));
		Vec3 v = { 1, 0, 0 };
		Vec3 v_ = q.Rotate(v);

		Vec3 expected{ 0, 1, 0 };
		CHECK_VEC3_EQ_APPROX(v_, expected);
	}

	TEST_CASE("ToMatrix_Equivalent_XAxis") {
		f32 rads = DegreesToRadians(32.0f);
		Quat q = Quat::FromAxisAngle({ 1, 0, 0 }, rads);
		Mat4x4 m = CreateRotationX(rads);
		Mat4x4 mq = q.ToMatrix4x4();

		CHECK_MATRIX_EQ_APPROX(m, mq);
	}

	TEST_CASE("ToMatrix_Equivalent_YAxis") {
		f32 rads = DegreesToRadians(74.0f);
		Quat q = Quat::FromAxisAngle({ 0, 1, 0 }, rads);
		Mat4x4 m = CreateRotationY(rads);
		Mat4x4 mq = q.ToMatrix4x4();

		CHECK_MATRIX_EQ_APPROX(m, mq);
	}

	TEST_CASE("ToMatrix_Equivalent_ZAxis") {
		f32 rads = DegreesToRadians(100.0f);
		Quat q = Quat::FromAxisAngle({ 0, 0, 1 }, rads);
		Mat4x4 m = CreateRotationZ(rads);
		Mat4x4 mq = q.ToMatrix4x4();

		CHECK_MATRIX_EQ_APPROX(m, mq);
	}
}
