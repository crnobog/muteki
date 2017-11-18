#pragma once

// Defines functions for points, vectors and matrices
// 
// Conventions
//	- Column vectors and pre-multiplication are used
//	 e.g. Translation * Rotation * v
//	- Row-major storage order is used
//	- All objects are default-initialized
//
// TODO
//	- Is there any reason to have element-wise addition and subtraction for Matrices?
//	- Separate notion of points and vectors. Only for 3d vectors and associated matrices?
//

#include "mu-core/PrimitiveTypes.h"
#include "mu-core/Utils.h"

#include <initializer_list>

#pragma warning (push)
#pragma warning (disable : 4201) // Disable warning about nonstandard anonymous structs 

// Unspecialized vector template with access only by index
template<typename T, size_t N>
struct Vector {
	T Data[N];

	Vector() {
		for (size_t i = 0; i < N; ++i) {
			Data[i] = T{};
		}
	}
	Vector(std::initializer_list<T> list) {
		size_t i = 0;
		for (auto it = list.begin(); i < ArraySize(vec.Data) && it != list.end(); ++i, ++it) {
			vec.Data[i] = *it;
		}
		for (; i < ArraySize(vec.Data); ++i) {
			vec.Data[i] = T{};
		}
	}

	T& operator[](size_t i) { return Data[i]; }
	const T& operator[](size_t i) const { return Data[i]; }
};

// 2D vector specialization with X and Y members
template<typename T>
struct Vector<T, 2> {
	union {
		T Data[2];
		struct {
			T X, Y;
		};
	};

	Vector() { X = Y = T{}; }
	Vector(T x, T y) { X = x; Y = y; }
	T& operator[](size_t i) { return Data[i]; }
	const T& operator[](size_t i) const { return Data[i]; }

	static const Vector UnitX;
	static const Vector UnitY;
};

template<typename T> const Vector<T, 2> Vector<T, 2>::UnitX{ 1, 0 };
template<typename T> const Vector<T, 2> Vector<T, 2>::UnitY{ 0, 1 };

// 3D vector specialization with X,Y,Z, R,G,B members and XY swizzle.
template<typename T>
struct Vector<T, 3> {
	union {
		T Data[3];
		struct {
			T X, Y, Z;
		};
		struct {
			T R, G, B;
		};
		Vector<T, 2> XY;
	};

	Vector() { X = Y = Z = T{}; }
	Vector(T x, T y, T z) { X = x; Y = y; Z = z; }
	T& operator[](size_t i) { return Data[i]; }
	const T& operator[](size_t i) const { return Data[i]; }

	static const Vector UnitX;
	static const Vector UnitY;
	static const Vector UnitZ;
};

template<typename T> const Vector<T, 3> Vector<T, 3>::UnitX{ 1, 0, 0 };
template<typename T> const Vector<T, 3> Vector<T, 3>::UnitY{ 0, 1, 0 };
template<typename T> const Vector<T, 3> Vector<T, 3>::UnitZ{ 0, 0, 1 };

// 4D vector specialization with X,Y,Z,W, R,G,B,A members, XY, XYZ, RGB swizzles.
template<typename T>
struct Vector<T, 4> {
	union {
		T Data[4];
		struct {
			T X, Y, Z, W;
		};
		struct {
			T R, G, B, A;
		};
		Vector<T, 2> XY;
		Vector<T, 3> XYZ;
		Vector<T, 3> RGB;
	};
	Vector() { X = Y = Z = W = T{}; }
	Vector(T x, T y, T z, T w) { X = x; Y = y; Z = z; W = w; }
	Vector(Vector<T, 3> xyz, T w) { XYZ = xyz; W = w; }
	T& operator[](size_t i) { return Data[i]; }
	const T& operator[](size_t i) const { return Data[i]; }

	static const Vector UnitX;
	static const Vector UnitY;
	static const Vector UnitZ;
	static const Vector UnitW;
};

template<typename T> const Vector<T, 4> Vector<T, 4>::UnitX{ 1, 0, 0, 0 };
template<typename T> const Vector<T, 4> Vector<T, 4>::UnitY{ 0, 1, 0, 0 };
template<typename T> const Vector<T, 4> Vector<T, 4>::UnitZ{ 0, 0, 1, 0 };
template<typename T> const Vector<T, 4> Vector<T, 4>::UnitW{ 0, 0, 0, 1 };

template<typename T, size_t ROWS, size_t COLUMNS>
struct Matrix {
	union {
		T M[ROWS][COLUMNS];
		T Data[ROWS * COLUMNS];
	};

	Matrix() {
		for (size_t i = 0; i < ArraySize(Data); ++i) {
			Data[i] = T{};
		}
	}
	Matrix(std::initializer_list<T> list) {
		size_t i = 0;
		for (T t : list) {
			if (i >= ROWS * COLUMNS) { break; }
			Data[i] = t;
			++i;
		}
		for (; i < ROWS * COLUMNS; ++i) {
			Data[i] = T{};
		}
	}

	Matrix(const Matrix&) = default;
	Matrix& operator=(const Matrix& other) = default;

	Vector<T, COLUMNS> Row(size_t row) const {
		Vector<T, COLUMNS> v;
		for (size_t i = 0; i < COLUMNS; ++i) {
			v[i] = M[row][i];
		}
		return v;
	}

	Vector<T, ROWS> Column(size_t col) const {
		Vector<T, ROWS> v;
		for (size_t i = 0; i < ROWS; ++i) {
			v[i] = M[i][col];
		}
		return v;
	}

	T& operator()(size_t i, size_t j) { return M[i][j]; }
	const T& operator()(size_t i, size_t j) const { return M[i][j]; }

	static Matrix Identity() {
		static_assert(ROWS == COLUMNS, "Identity is only valid for square matrices");
		Matrix m;
		for (size_t i = 0; i < ROWS; ++i) {
			m(i, i) = (T)1;
		}
		return m;
	}
};

using Vec2 = Vector<f32, 2>;
using Vec3 = Vector<f32, 3>;
using Vec4 = Vector<f32, 4>;

using Mat3x3 = Matrix<f32, 3, 3>;
using Mat3x4 = Matrix<f32, 3, 4>;
using Mat4x4 = Matrix<f32, 4, 4>;

template<typename T, size_t N>
inline Vector<T, N> operator-(Vector<T, N> v) {
	Vector<T, N> r;
	for (size_t i = 0; i < N; ++i) {
		r[i] = -v[i];
	}
	return r;
}

template<typename T, size_t N>
inline bool operator==(Vector<T, N> a, Vector<T, N> b) {
	for (size_t i = 0; i < N; ++i) {
		if (a[i] != b[i]) {
			return false;
		}
	}
	return true;
}
template<typename T, size_t N>
inline bool operator!=(Vector<T, N> a, Vector<T, N> b) { return !(a == b); }

template<typename T, size_t N>
inline Vector<T, N> operator+(Vector<T, N> a, Vector<T, N> b) {
	Vector<T, N> r;
	for (size_t i = 0; i < N; ++i) {
		r[i] = a[i] + b[i];
	}
	return r;
}

template<typename T, size_t N>
inline Vector<T, N>& operator+=(Vector<T, N>& a, Vector<T, N> b) { a = a + b; return a; }

template<typename T, size_t N>
inline Vector<T, N> operator-(Vector<T, N> a, Vector<T, N> b) {
	Vector<T, N> r;
	for (size_t i = 0; i < N; ++i) {
		r[i] = a[i] - b[i];
	}
	return r;
}
template<typename T, size_t N>
inline Vector<T, N>& operator-=(Vector<T, N>& a, Vector<T, N> b) { a = a - b; return a; }

template<typename T, size_t N>
inline Vector<T, N> operator*(Vector<T, N> a, Vector<T, N> b) {
	Vector<T, N> r;
	for (size_t i = 0; i < N; ++i) {
		r[i] = a[i] * b[i];
	}
	return r;
}
//template<typename T, size_t N>
//inline Vector<T, N>& operator*=(Vector<T, N>& a, Vector<T, N> b) { a = a * b; return a; }

template<typename T, size_t N>
inline Vector<T, N> operator/(Vector<T, N> a, Vector<T, N> b) {
	Vector<T, N> r;
	for (size_t i = 0; i < N; ++i) {
		r[i] = a[i] / b[i];
	}
	return r;
}
//template<typename T, size_t N>
//inline Vector<T, N>& operator/=(Vector<T, N>& a, Vector<T, N> b) { a = a / b; return a; }

template<typename T, size_t N, typename SCALAR>
inline Vector<T, N> operator*(Vector<T, N> a, SCALAR scalar) {
	Vector<T, N> r;
	for (size_t i = 0; i < N; ++i) {
		r[i] = a[i] * scalar;
	}
	return r;
}

template<typename T, size_t N, typename SCALAR>
inline Vector<T, N> operator*(SCALAR scalar, Vector<T, N> a) { return a * scalar; }
template<typename T, size_t N, typename OTHER>
inline Vector<T, N>& operator*=(Vector<T, N>& a, OTHER b) { a = a * b; return a; }

template<typename T, size_t N, typename SCALAR>
inline Vector<T, N> operator/(Vector<T, N> a, SCALAR scalar) {
	Vector<T, N> r;
	for (size_t i = 0; i < N; ++i) {
		r[i] = a[i] / scalar;
	}
	return r;
}
template<typename T, size_t N, typename OTHER>
inline Vector<T, N>& operator/=(Vector<T, N>& a, OTHER b) { a = a / b; return a; }

template<typename T, size_t N>
inline T Dot(Vector<T, N> a, Vector<T, N> b) {
	T sum{};
	for (size_t i = 0; i < N; ++i) {
		sum += a[i] * b[i];
	}
	return sum;
}

template<typename T>
inline Vector<T, 3> Cross(Vector<T, 3> a, Vector<T, 3> b) {
	return {
		a.Y*b.Z - a.Z*b.Y,
		a.Z*b.X - a.X*b.Z,
		a.X*b.Y - a.Y*b.X
	};
}

template<typename T, size_t N>
inline Vector<T, N> Abs(Vector<T, N> v) {
	for (size_t i = 0; i < N; ++i) {
		v[i] = abs(v[i]);
	}
	return v;
}

template<typename T, size_t N>
inline T MaxAbsComponent(Vector<T, N> v) {
	v = Abs(v);
	T max = v[0];
	for (size_t i = 1; i < N; ++i) {
		max = Max(max, v[i]);
	}
	return max;
}

template<typename T, size_t N>
inline T MagnitudeSq(const Vector<T, N>& v) {
	// TODO: Consider under/overflow?
	T sum{};
	for (size_t i = 0; i < N; ++i) {
		sum += v[i] * v[i];
	}
	return sum;
}

template<typename T, size_t N>
inline T MagnitudeFast(const Vector<T, N>& v) { return sqrt(MagnitudeSq(v)); }

template<typename T, size_t N>
inline T Magnitude(const Vector<T, N>& v) {
	// http://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2012/07/Vector-length-and-normalization-difficulties.pdf
	// Scales components to avoid under/overflow when squaring
	T max_comp = MaxAbsComponent(v);
	if (max_comp > 0.0f) {
		return max_comp * MagnitudeFast(v / max_comp);
	}
	else {
		return 0.0f;
	}
}

template<typename T, size_t N>
inline Vector<T, N> NormalizeFast(const Vector<T, N>& v) {
	return v / MagnitudeFast(v);
}

template<typename T, size_t N>
inline Vector<T, N> Normalize(const Vector<T, N>& v) {
	T max_comp = MaxAbsComponent(v);
	if (max_comp > 0.0f) {
		return NormalizeFast(v / max_comp);
	}
	else {
		return v; // Optionally return an arbitrary unit length vector
	}
}

template<typename T, typename U, size_t ROWS, size_t COLUMNS >
inline Vector<U, ROWS> operator*(const Matrix<T, ROWS, COLUMNS>& m, const Vector<U, COLUMNS>& v) {
	Vector<U, ROWS> r;
	for (size_t i = 0; i < ROWS; ++i) {
		r[i] = Dot(m.Row(i), v);
	}
	return r;
}

template<typename T, typename U, size_t ROWS, size_t COLUMNS, size_t COLUMNS2>
inline auto operator*(const Matrix<T, ROWS, COLUMNS>& m1, const Matrix<U, COLUMNS, COLUMNS2>& m2) {
	Matrix<decltype(T{}*U{}), ROWS, COLUMNS2 > m;
	for (size_t i = 0; i < ROWS; ++i) {
		for (size_t j = 0; j < COLUMNS2; ++j) {
			m(i, j) = Dot(m1.Row(i), m2.Column(j));
		}
	}
	return m;
}

template<typename T, typename U, size_t ROWS, size_t COLUMNS>
inline bool operator==(Matrix<T, ROWS, COLUMNS> a, Matrix<U, ROWS, COLUMNS> b) {
	for (size_t i = 0; i < ROWS * COLUMNS; ++i) {
		if (a.Data[i] != b.Data[i]) {
			return false;
		}
	}
	return true;
}
template<typename T, typename U, size_t ROWS, size_t COLUMNS>
inline bool operator!=(Matrix<T, ROWS, COLUMNS> a, Matrix<U, ROWS, COLUMNS> b) { return !(a == b); }

template<typename T, size_t ROWS, size_t COLUMNS>
inline Matrix<T, COLUMNS, ROWS> Transpose(const Matrix<T, ROWS, COLUMNS>& m) {
	Matrix<T, COLUMNS, ROWS> t;
	for (size_t i = 0; i < ROWS; ++i) {
		for (size_t j = 0; j < COLUMNS; ++j) {
			t(j, i) = m(i, j);
		}
	}
	return t;
}

// Transpose a column matrix into a 1xN matrix
template<typename T, size_t N>
inline Matrix<T, 1, N> Transpose(const Vector<T, N>& v) {
	Matrix<T, 1, N> m;
	for (size_t i = 0; i < N; ++i) {
		m(0, i) = v[i];
	}
	return m;
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/Tests_Vectors.inl"
#endif

#pragma warning (pop)
