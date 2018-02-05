#version 430
layout(points )in;
layout(points , max_vertices=1 )out;

// Taken from https://devtalk.nvidia.com/default/topic/818946/opengl/render-to-3d-texture/

uniform int z;

flat in int out_particleID [];
in vec4 out_particlePosition [];

flat out int particleID;
out vec4 particlePosition;

void main(){

	particleID = out_particleID[0];
	particlePosition = out_particlePosition[0];

	gl_Position = gl_in[0].gl_Position;
	gl_Layer = z;

	EmitVertex();
	EndPrimitive();
}