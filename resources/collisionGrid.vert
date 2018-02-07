#version 330

// Uniforms
uniform mat4 modelMX;
uniform mat4 projMX;
uniform mat4 viewMX;

uniform sampler2D particlePositions;

uniform vec3 btmLeftFrontCorner;
uniform vec3 gridSize;

uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;

uniform float voxelLength;

// Inputs
layout(location = 0) in vec4  in_position;

// Outputs
flat out int particleID;
out vec4 particlePosition;

ivec2 idxTo2DParticleCoords(int idx){

	int x = idx % particleTextureEdgeLength;
	int y = idx / particleTextureEdgeLength;

	return ivec2(x, y);

}

void main() {    

	// Determine which rigid Body we are and what particle
	particleID = gl_InstanceID;

	// Mapping of invocation id to particle coord
	// Model transformed to consider any grid tranformations
    particlePosition = modelMX * vec4(texelFetch(particlePositions, idxTo2DParticleCoords(particleID), 0).xyz, 1.f);

	gl_Position = projMX * particlePosition;
}
