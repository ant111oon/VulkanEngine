#version 450


layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;


layout(set = 0, binding = 0) uniform sampler2D TEXTURE;


void main() 
{
	outFragColor = texture(TEXTURE, inUV); 
}