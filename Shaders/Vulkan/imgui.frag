#version 450

layout(binding = 0, set = 1) uniform texture2D tex0;
layout(binding = 0, set = 2) uniform sampler sampler0;

layout(location = 0) in vec4 i_color;
layout(location = 1) in vec2 i_uv; 

layout(location = 0) out vec4 out_color;

void main() {
	out_color = i_color * texture(sampler2D(tex0, sampler0), i_uv);
}