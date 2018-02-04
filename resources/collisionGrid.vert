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
flat out int out_particleID;
out vec4 out_particlePosition;

ivec2 idxTo2DParticleCoords(int idx){

	int x = idx % particleTextureEdgeLength;
	int y = idx / particleTextureEdgeLength;

	return ivec2(x, y);

}

void main() {    

	// Determine which rigid Body we are and what particle
	out_particleID = gl_InstanceID;

	// Mapping of invocation id to particle coord
	// Model transformed to consider any grid tranformations
    out_particlePosition = modelMX * vec4(texelFetch(particlePositions, idxTo2DParticleCoords(out_particleID), 0).xyz, 1.f);
	
	// Determening the output position in the grid -  adding half a voxel offset to be in the middle
	// FIXME: Is this really right? Since I have GL_NEAREST interpolation the grid value may be assigned to the wrong voxel
	// float halfVoxelLenghth = voxelLength / 2.f;
	
	// float normalizedX = (int((out_particlePosition.x - btmLeftFrontCorner.x) / voxelLength) * voxelLength) / gridSize.x;
	// float normalizedY = (int((out_particlePosition.y - btmLeftFrontCorner.y) / voxelLength) * voxelLength) / gridSize.y;

	// float normalizedX = (int((out_particlePosition.x - btmLeftFrontCorner.x) / voxelLength) * voxelLength + halfVoxelLength) / gridSize.x;
	// float normalizedY = (int((out_particlePosition.y - btmLeftFrontCorner.y) / voxelLength) * voxelLength + halfVoxelLength) / gridSize.y;

	// Determine voxel coord in NDC
	// vec3 voxelCoords =  vec3(normalizedX, normalizedY, 1.f); // Offsetting in z so that zDepth won't be clipping
	// gl_Position = projMX * viewMX * vec4(voxelCoords, 1.f); // projMX * vec4(voxelCoords, 1.f);
	
	gl_Position = projMX * out_particlePosition; // projMX * vec4(voxelCoords, 1.f);
}
