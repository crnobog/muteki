#include "CubePrimitive.h"

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

const Vec2 cube_texcoords[] = {
	// Top face
	{ 0, 0 },
	{ 1, 0 },
	{ 0, 1 },
	{ 1, 1 },

	// Bottom face
	{ 0, 0 },
	{ 1, 0 },
	{ 0, 1 },
	{ 1, 1 },

	// Front face
	{ 0, 0 },
	{ 1, 0 },
	{ 0, 1 },
	{ 1, 1 },

	// Back face
	{ 0, 0 },
	{ 1, 0 },
	{ 0, 1 },
	{ 1, 1 },

	// Right face
	{ 0, 0 },
	{ 1, 0 },
	{ 0, 1 },
	{ 1, 1 },

	// Left face
	{ 0, 0 },
	{ 1, 0 },
	{ 0, 1 },
	{ 1, 1 },
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
