#pragma once

#include "mu-core/Utils.h"

#pragma warning (push)
#pragma warning (disable : 4201) // Disable warning about nonstandard anonymous structs 

template<typename VEC, typename T>
inline void VectorInitList(VEC& vec, std::initializer_list<T>) {
	size_t i = 0;
	for (auto it = list.begin(); i < ArraySize(vec.Data) && it != list.end(); ++i, ++it) {
		vec.Data[i] = *it;
	}
	for (; i < ArraySize(vec.Data); ++i) {
		vec.Data[i] = T{};
	}
}

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
};

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
};

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
	T& operator[](size_t i) { return Data[i]; }
	const T& operator[](size_t i) const { return Data[i]; }
};

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
			Data[i] = t;
			++i;
		}
		for (; i < ROWS * COLUMNS; ++i) {
			Data[i] = T{};
		}
	}

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
};

using Vec2 = Vector<float, 2>;
using Vec3 = Vector<float, 3>;
using Vec4 = Vector<float, 4>;

using Mat3x3 = Matrix<float, 3, 3>;
using Mat3x4 = Matrix<float, 3, 4>;
using Mat4x4 = Matrix<float, 4, 4>;

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

#pragma warning (pop)