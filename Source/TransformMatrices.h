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

// Creates a rotation + translation matrix which moves origin to (0,0,0),
// origin and target are points, up is a vector which must not be parallel with (target-origin)
// points +Z parallel with (target-origin)
// points +X perpendicular to (target-origin) and up
// points +Y perpendicular to both
Mat4x4 CreateLookAt(
	Vec3 origin, Vec3 target, Vec3 up
);

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
