#version 330

// Uniforms
uniform mat4 projMX;

uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;
uniform float mass;

// Inputs
layout(location = 0) in vec4  in_position;

// Outputs
flat out int rigidBodyID;
flat out int particleID;

ivec2 idxTo2DParticleCoords(int idx){

	int x = idx % particleTextureEdgeLength;
	int y = idx / particleTextureEdgeLength;

	return ivec2(x, y);

}

ivec2 rigidBodyIdx2TextureCoords(int idx) {
	int x = idx / rigidBodyTextureEdgeLength;
	int y = idx % rigidBodyTextureEdgeLength;
	return ivec2(x,y);
}

void main() {    
	
	gl_Position = projMX * in_position;

}
