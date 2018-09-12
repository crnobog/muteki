#pragma once

#include "Vectors.h"

// All transformations are built for a left handed coordinate system.

// Creates an orthographic projection matrix 
//	Left handed, F > N.
//	DirectX clip space:
//		-1 <= x <= 1
//		-1 <= y <= 1
//		 0 <= z <= 1
Mat4x4 CreateOrthographicProjection(
	f32 L, f32 R, f32 T, f32 B, f32 N, f32 F
);

// Creates a perspective projection matrix
//	Left handed
//	DirectX clip space:
//		-1 <= x <= 1
//		-1 <= y <= 1
//		 0 <= z <= 1
Mat4x4 CreatePerspectiveProjectionAsymmetric(
	f32 L, f32 R, f32 T, f32 B, f32 N, f32 F);
Mat4x4 CreatePerspectiveProjectionSymmetric(
	f32 W, f32 H, f32 N, f32 F);
Mat4x4 CreatePerspectiveProjection(
	f32 VFOV, f32 AspectRatio, f32 N, f32 F);

// Rotations are counter-clockwise around the axis when looking parallel to that axis

// Rotates from Y to Z
Mat4x4 CreateRotationX(f32 Radians);
Mat4x4 CreateRotationX(f64 Radians);
// Rotates from Z to X
Mat4x4 CreateRotationY(f32 Radians);
Mat4x4 CreateRotationY(f64 Radians);
// Rotates from X to Y
Mat4x4 CreateRotationZ(f32 Radians);
Mat4x4 CreateRotationZ(f64 Radians);

Mat4x4 CreateRotationAxisAngle(Vec3 axis, f32 Radians);

Mat4x4 CreateTranslation(Vec3 translation);
