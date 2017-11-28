#include "Vectors.h"
#include "TransformMatrices.h"
#include "CoreMath.h"

Mat4x4 CreateOrthographicProjection(
	float L, float R, float T, float B, float N, float F
) {
	return Mat4x4{
		2 / (R - L),	0,				0,				-(R + L) / (R - L),
		0,				2 / (T - B),	0,				-(T + B) / (T - B),
		0,				0,				1 / (F - N),	-N / (F - N),
		0,				0,				0,				1
	};
}

Mat4x4 CreatePerspectiveProjectionAsymmetric(
	float L, float R, float T, float B, float N, float F) {
	return Mat4x4{
		2 * N / (R - L),	0,					-(R + L) / (R - L), 0,
		0,					2 * N / (T - B),	-(T + B) / (T - B), 0,
		0,					0,					F / (F - N),		-F*N / (F - N),
		0,					0,					1,					0
	};
}
Mat4x4 CreatePerspectiveProjectionSymmetric(
	float W, float H, float N, float F) {
	return Mat4x4{
		N / W,	0,		0,				0,
		0,		N / H,	0,				0,
		0,		0,		F / (F - N),	-F*N / (F - N),
		0,		0,		1,				0
	};
}

Mat4x4 CreatePerspectiveProjection(f32 VFOV, f32 AspectRatio, f32 N, f32 F) {
	float H = 2 * Tan(VFOV / 2.0f) * N;
	float W = AspectRatio * H;
	return CreatePerspectiveProjectionSymmetric(W, H, N, F);
}

Mat4x4 CreateRotationX(float Radians) {
	float c = Cos(Radians), s = Sin(Radians);
	return Mat4x4{
		1, 0, 0, 0,
		0, c, -s, 0,
		0, s, c, 0,
		0, 0, 0, 1
	};
}
Mat4x4 CreateRotationY(float Radians) {
	float c = Cos(Radians), s = Sin(Radians);
	return Mat4x4{
		c, 0, s, 0,
		0, 1, 0, 0,
		-s, 0, c, 0,
		0, 0, 0, 1
	};
}
Mat4x4 CreateRotationZ(float Radians) {
	float c = Cos(Radians), s = Sin(Radians);
	return Mat4x4{
		c, -s, 0, 0,
		s, c, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
}


Mat4x4 CreateRotationX(double Radians) {
	float c = (f32)Cos(Radians), s = (f32)Sin(Radians);
	return Mat4x4{
		1, 0, 0, 0,
		0, c, -s, 0,
		0, s, c, 0,
		0, 0, 0, 1
	};
}
Mat4x4 CreateRotationY(double Radians) {
	float c = (f32)Cos(Radians), s = (f32)Sin(Radians);
	return Mat4x4{
		c, 0, s, 0,
		0, 1, 0, 0,
		-s, 0, c, 0,
		0, 0, 0, 1
	};
}
Mat4x4 CreateRotationZ(double Radians) {
	float c = (f32)Cos(Radians), s = (f32)Sin(Radians);
	return Mat4x4{
		c, -s, 0, 0,
		s, c, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
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
