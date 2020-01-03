#version 450

layout(binding=0, set = 1) uniform texture2D diffuse;
layout(binding=0, set = 2) uniform sampler diffuse_sampler;

layout(location = 0) in vec2 diffuse_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color = texture(sampler2D(diffuse, diffuse_sampler), diffuse_coord);
}