#include "CubePrimitive.h"

// Texture mapping for cube primitive:
// +--------------------+ v=0
// |      |      |      |
// |  +X  |  +Y  |  +Z  |
// |      |      |      |
// +--------------------+ v=1/2
// |      |      |      |
// |  -X  |  -Y  |  -Z  |
// |      |      |      |
// +--------------------+ v=1
// u=0    u=1/3  u=1/6  u=1


// Unit cube centered around the origin
const Vec3 cube_positions[] = {
	// Top face
	{ -0.5, +0.5, -0.5 }, // Back left		0
	{ +0.5, +0.5, -0.5 }, // Back right		1
	{ -0.5, +0.5, +0.5 }, // Front left		2
	{ +0.5, +0.5, +0.5 }, // Front right	3

						  // Bottom face
	{ -0.5, -0.5, +0.5 }, // Front left		4
	{ +0.5, -0.5, +0.5 }, // Front right	5
	{ -0.5, -0.5, -0.5 }, // Back left		6
	{ +0.5, -0.5, -0.5 }, // Back right		7

						  // Front face
	{ -0.5, +0.5, +0.5 }, // Top left		8
	{ +0.5, +0.5, +0.5 }, // Top right		9
	{ -0.5, -0.5, +0.5 }, // Bottom left	10	
	{ +0.5, -0.5, +0.5 }, // Bottom right	11

						  // Back face
	{ +0.5, +0.5, -0.5 }, // Top right		12
	{ -0.5, +0.5, -0.5 }, // Top left		13
	{ +0.5, -0.5, -0.5 }, // Bottom right	14
	{ -0.5, -0.5, -0.5 }, // Bottom left	15

						  // Right face
	{ +0.5, +0.5, +0.5 }, // Top front		16
	{ +0.5, +0.5, -0.5 }, // Top back		17
	{ +0.5, -0.5, +0.5 }, // Bottom front	18
	{ +0.5, -0.5, -0.5 }, // Bottom back	19

						  // Left face
	{ -0.5, +0.5, -0.5 }, // Top back		20
	{ -0.5, +0.5, +0.5 }, // Top front		21
	{ -0.5, -0.5, -0.5 }, // Bottom back	22
	{ -0.5, -0.5, +0.5 }, // Bottom front	23
};

const Vec3 cube_normals[] = {
	// Top face
	Vec3::UnitY,
	Vec3::UnitY,
	Vec3::UnitY,
	Vec3::UnitY,

	// Bottom face
	-Vec3::UnitY,
	-Vec3::UnitY,
	-Vec3::UnitY,
	-Vec3::UnitY,

	// Front face
	Vec3::UnitZ,
	Vec3::UnitZ,
	Vec3::UnitZ,
	Vec3::UnitZ,

	// Back face
	-Vec3::UnitZ,
	-Vec3::UnitZ,
	-Vec3::UnitZ,
	-Vec3::UnitZ,

	// Right face
	Vec3::UnitX,
	Vec3::UnitX,
	Vec3::UnitX,
	Vec3::UnitX,

	// Left face
	-Vec3::UnitX,
	-Vec3::UnitX,
	-Vec3::UnitX,
	-Vec3::UnitX
};

namespace c {
	constexpr float u0 = 0.0f;
	constexpr float u1 = 1.0f / 3.0f;
	constexpr float u2 = 2.0f / 3.0f;
	constexpr float u3 = 1.0f;

	constexpr float v0 = 0.0f;
	constexpr float v1 = 0.5f;
	constexpr float v2 = 1.0f;
}

// TODO: Gaps between?
//const Vec2 cube_texcoords[] = {
//
//						  // Top face
//	{ 0.0000f, 0.0000f }, // Back left		0
//	{ 0.3333f, 0.0000f }, // Back right		1
//	{ 0.0000f, 0.5000f }, // Front left		2
//	{ 0.3333f, 0.5000f }, // Front right	3
//
//						  // Bottom face
//	{ 0.0000f, 0.5000f }, // Front left		4
//	{ 0.3333f, 0.5000f }, // Front right	5
//	{ 0.0000f, 1.0000f }, // Back left		6
//	{ 0.3333f, 1.0000f }, // Back right		7
//
//						  // Front face
//	{ 0.3333f, 0.0000f }, // Top left		8
//	{ 0.6666f, 0.0000f }, // Top right		9
//	{ 0.3333f, 0.5000f }, // Bottom left	10
//	{ 0.6666f, 0.5000f }, // Bottom right	11
//
//						  // Back face
//	{ 0.3333f, 0.5000f }, // Top right		12
//	{ 0.6666f, 0.5000f }, // Top left		13
//	{ 0.3333f, 1.0000f }, // Bottom right	14
//	{ 0.6666f, 1.0000f }, // Bottom left	15
//
//	                      // Right face
//	{ 0.6666f, 0.0000f }, // Top front		16
//	{ 1.0000f, 0.0000f }, // Top back		17
//	{ 0.6666f, 0.5000f }, // Bottom front	18
//	{ 1.0000f, 0.5000f }, // Bottom back	19
//
//						  // Left face
//	{ 0.6666f, 0.5000f }, // Top back		20
//	{ 1.0000f, 0.5000f }, // Top front		21
//	{ 0.6666f, 1.0000f }, // Bottom back	22
//	{ 1.0000f, 1.0000f }, // Bottom front	23
//};
const Vec2 cube_texcoords[] = {

					  // Top face
	{ c::u1, c::v1 }, // Back left		0
	{ c::u2, c::v1 }, // Back right		1
	{ c::u1, c::v0 }, // Front left		2
	{ c::u2, c::v0 }, // Front right	3

					  // Bottom face
	{ c::u2, c::v1 }, // Front left		4
	{ c::u1, c::v1 }, // Front right	5
	{ c::u2, c::v2 }, // Back left		6
	{ c::u1, c::v2 }, // Back right		7

					  // Front face
	{ c::u3, c::v0 }, // Top left		8
	{ c::u2, c::v0 }, // Top right		9
	{ c::u3, c::v1 }, // Bottom left	10
	{ c::u2, c::v1 }, // Bottom right	11

					  // Back face
	{ c::u3, c::v1 }, // Top right		12
	{ c::u2, c::v1 }, // Top left		13
	{ c::u3, c::v2 }, // Bottom right	14
	{ c::u2, c::v2 }, // Bottom left	15

	                  // Right face
	{ c::u1, c::v0 }, // Top front		16
	{ c::u0, c::v0 }, // Top back		17
	{ c::u1, c::v1 }, // Bottom front	18
	{ c::u0, c::v1 }, // Bottom back	19

					  // Left face
	{ c::u1, c::v1 }, // Top back		20
	{ c::u0, c::v1 }, // Top front		21
	{ c::u1, c::v2 }, // Bottom back	22
	{ c::u0, c::v2 }, // Bottom front	23
};


const u16 cube_indices[] = {
	// Top face
	2, 1, 0,
	2, 3, 1,

	// Bottom face
	6, 5, 4,
	5, 6, 7,

	// Front face
	9, 8, 10,
	9, 10, 11,

	// Back face
	13, 12, 14,
	13, 14, 15,

	// Right face
	17, 16, 18,
	17, 18, 19,

	// Left face
	21, 20, 22,
	21, 22, 23
};
