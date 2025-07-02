#version 450

#extension GL_EXT_buffer_reference : require


layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;


struct Vertex {

	vec3 position;
	float uvX;
	vec3 normal;
	float uvY;
	vec4 color;
}; 


layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};


layout(push_constant) uniform constants
{	
	mat4 transform;
	VertexBuffer vertexBuffer;
} PushConstants;


#define GET_VERTEX(IDX) PushConstants.vertexBuffer.vertices[IDX]


void main() 
{
	const Vertex v = GET_VERTEX(gl_VertexIndex);

	outColor = v.color.rgb;
	outUV = vec2(v.uvX, v.uvY);
	gl_Position = PushConstants.transform * vec4(v.position, 1.f);
}