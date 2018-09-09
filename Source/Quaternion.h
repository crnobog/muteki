#pragma once

#include "Vectors.h"
#include "CoreMath.h"

#pragma warning (push)
#pragma warning (disable : 4201) // Disable warning about nonstandard anonymous structs 

// Structure representing a unit quaternion. All operations re-normalize if necessary
struct Quat;
Quat operator*(const Quat& A, const Quat& B);

struct Quat {
	union {
		f32 Data[4];
		struct {
			f32 X, Y, Z, W;
		};
		struct {
			Vec3 Vector;
			f32 Scalar;
		};
	};

	Quat() {
		X = Y = Z = 0;
		W = 1;
	}
	Quat(Vec3 in_vector, float in_scalar) {
		Vector = in_vector; Scalar = in_scalar;
		Normalize();
	}
	Quat(f32 x, f32 y, f32 z, f32 w) {
		X = x; Y = y; Z = z; W = w;
		Normalize();
	}

	Quat Conjugate() const {
		return { -X, -Y, -Z, W };
	}

	Vec3 Rotate(Vec3 v) {
		Quat qv{ v, 0.0f };
		Quat r = *this * qv * Conjugate();
		return { r.X, r.Y, r.Z };
	}

	Mat4x4 ToMatrix4x4() const {
		return Mat4x4{
			1 - 2*Y*Y - 2*Z*Z,	2*X*Y - 2*W*Z,		2*X*Z + 2*W*Y,		0,
			2*X*Y + 2*W*Z,		1 - 2*X*X - 2*Z*Z,	2*Y*Z - 2*W*X,		0,
			2*X*Z - 2*W*Y,		2*Y*Z + 2*W*X,		1 - 2*X*X - 2*Y*Y,	0,
			0,					0,					0,					1
		};
	};

	static Quat Identity() {
		return { 0, 0, 0, 1 };
	}

	static Quat FromAxisAngle(Vec3 axis, f32 angle_radians) {
		f32 a2 = angle_radians / 2;
		return { axis * Sin(a2), Cos(a2) };
	}

private:
	f32 Norm() const {
		return Sqrt(
			X*X + Y * Y + Z * Z + W * W
		);
	}

	void Normalize() {
		f32 norm = Norm();
		X /= norm;
		Y /= norm;
		Z /= norm;
		W /= norm;
	}
};

bool operator==(const Quat& A, const Quat& B) {
	return A.X == B.X && A.Y == B.Y && A.Z == B.Z && A.W == B.W;
}

Quat operator*(const Quat& A, const Quat& B) {
	return Quat
	{
		A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y,
		A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X,
		A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W,
		A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z
	};
	//return Quat{
	//	Cross(A.Vector, B.Vector) + (A.Scalar * B.Vector) + (B.Scalar * A.Vector),
	//	(A.Scalar * B.Scalar) - Dot(A.Vector, B.Vector)
	//};
}

#pragma warning (pop)
