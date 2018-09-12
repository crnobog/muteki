#include "Vectors.h"
#include "TransformMatrices.h"
#include "CoreMath.h"

Mat4x4 CreateOrthographicProjection(
	f32 L, f32 R, f32 T, f32 B, f32 N, f32 F
) {
	return Mat4x4{
		2 / (R - L),	0,				0,				-(R + L) / (R - L),
		0,				2 / (T - B),	0,				-(T + B) / (T - B),
		0,				0,				1 / (F - N),	-N / (F - N),
		0,				0,				0,				1
	};
}

Mat4x4 CreatePerspectiveProjectionAsymmetric(
	f32 L, f32 R, f32 T, f32 B, f32 N, f32 F) {
	return Mat4x4{
		2 * N / (R - L),	0,					-(R + L) / (R - L), 0,
		0,					2 * N / (T - B),	-(T + B) / (T - B), 0,
		0,					0,					F / (F - N),		-F * N / (F - N),
		0,					0,					1,					0
	};
}
Mat4x4 CreatePerspectiveProjectionSymmetric(
	f32 W, f32 H, f32 N, f32 F) {
	return Mat4x4{
		N / W,	0,		0,				0,
		0,		N / H,	0,				0,
		0,		0,		F / (F - N),	-F * N / (F - N),
		0,		0,		1,				0
	};
}

Mat4x4 CreatePerspectiveProjection(f32 VFOV, f32 AspectRatio, f32 N, f32 F) {
	f32 H = 2 * Tan(VFOV / 2.0f) * N;
	f32 W = AspectRatio * H;
	return CreatePerspectiveProjectionSymmetric(W, H, N, F);
}

Mat4x4 CreateRotationX(f32 radians) {
	f32 c = Cos(radians), s = Sin(radians);
	return Mat4x4{
		1, 0, 0, 0,
		0, c, -s, 0,
		0, s, c, 0,
		0, 0, 0, 1
	};
}
Mat4x4 CreateRotationY(f32 radians) {
	f32 c = Cos(radians), s = Sin(radians);
	return Mat4x4{
		c, 0, s, 0,
		0, 1, 0, 0,
		-s, 0, c, 0,
		0, 0, 0, 1
	};
}
Mat4x4 CreateRotationZ(f32 radians) {
	f32 c = Cos(radians), s = Sin(radians);
	return Mat4x4{
		c, -s, 0, 0,
		s, c, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
}


Mat4x4 CreateRotationX(f64 radians) {
	f32 c = (f32)Cos(radians), s = (f32)Sin(radians);
	return Mat4x4{
		1, 0, 0, 0,
		0, c, -s, 0,
		0, s, c, 0,
		0, 0, 0, 1
	};
}
Mat4x4 CreateRotationY(f64 radians) {
	f32 c = (f32)Cos(radians), s = (f32)Sin(radians);
	return Mat4x4{
		c, 0, s, 0,
		0, 1, 0, 0,
		-s, 0, c, 0,
		0, 0, 0, 1
	};
}
Mat4x4 CreateRotationZ(f64 radians) {
	f32 c = (f32)Cos(radians), s = (f32)Sin(radians);
	return Mat4x4{
		c, -s, 0, 0,
		s, c, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
}

Mat4x4 CreateRotationAxisAngle(Vec3 axis, f32 radians) {
	f32 c = (f32)Cos(radians), s = (f32)Sin(radians);
	f32 x = axis.X, y = axis.Y, z = axis.Z;
	return Mat4x4{
		c + (1-c)*x*x,		(1-c)*x*y - s*z,	(1-c)*x*z + s*y,	0,
		(1-c)*x*y + s*z,	c + (1-c)*y*y,		(1-c)*y*z - s*x,	0,
		(1-c)*x*z - s*y,	(1-c)*y*z + s*x,	c + (1-c)*z*z,		0,
		0,					0,					0,					1.0
	};
}


Mat4x4 CreateTranslation(Vec3 translation) {
	return Mat4x4{
		1, 0, 0, translation.X,
		0, 1, 0, translation.Y,
		0, 0, 1, translation.Z,
		0, 0, 0, 1
	};
}
