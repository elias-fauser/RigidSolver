#version 330

// Uniforms
uniform mat4 modelMX;
uniform mat4 projMX;

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
flat out int rigidBodyID;
flat out int particleID;
out vec4 particlePosition;

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

	// Determine which rigid Body we are and what particle
	int particleID = gl_InstanceID;
	rigidBodyID = int(particleID / particlesPerModel);

	// Mapping of invocation id to particle coord
    particlePosition = modelMX * texelFetch(particlePositions, idxTo2DParticleCoords(particleID), 0);
	
	// Determening the output position in the grid -  adding half a voxel offset to be in the middle
	// FIXME: Is this really right? Since I have GL_NEAREST interpolation the grid value may be assigned to the wrong voxel
	float halfVoxelLenghth = voxelLength / 2.f;
	
	float normalizedX = (int((particlePosition.x - btmLeftFrontCorner.x) / voxelLength) * voxelLength) / gridSize.x;
	float normalizedY = (int((particlePosition.y - btmLeftFrontCorner.y) / voxelLength) * voxelLength) / gridSize.y;

	// float normalizedX = (int((particlePosition.x - btmLeftFrontCorner.x) / voxelLength) * voxelLength + halfVoxelLength) / gridSize.x;
	// float normalizedY = (int((particlePosition.y - btmLeftFrontCorner.y) / voxelLength) * voxelLength + halfVoxelLength) / gridSize.y;

	// Determine voxel coord in NDC
	vec3 voxelCoords =  vec3(normalizedX, normalizedY, 0.f);

	gl_Position = projMX * vec4(voxelCoords, 1.f);
}
