#version 330

// Uniforms
uniform Sampler2D particlePostions;

uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;

// Inputs
layout(location = 0) in vec4  in_position;

flat out int rigidBodyID;
flat out int particleID;

vec2 invocationID2particleCoords(int idx, int rigidID, int particleID){

	int x = int((rigidID * particlesPerModel) / particleTextureEdgeLength);
	int y = idx % (x * particleTextureEdgeLength);

	return vec2(x, y);

}

vec2 rigidBodyIdx2TextureCoords(int idx) {
	int x = idx / rigidBodyTextureEdgeLength;
	int y = idx % rigidBodyTextureEdgeLength;
	return vec2(x,y);
}

void main() {    

	// Determine which rigid Body we are and what particle
	int idx = gl_InstanceID;
	rigidBodyID = int(idx / particlesPerModel);
	particleID = idx % particlesPerModel;

	// Mapping of invocation id to particle coord
    gl_Position = modelMX * texelFetch(particlePositions, invocationID2particleCoords(idx, rigidBodyID, particleID), 0);

}
