#version 430

// Outputs
layout(location = 0) out uvec4 gridOut;

uniform float voxelLength;
uniform float zCoord;

flat in int particleID;
in vec4 particlePosition;

void main() {
	
	gridOut = uvec4(0);

	// Only considering particles which are in the current z layer
	if (particlePosition.z >= zCoord && particlePosition.z < zCoord + voxelLength){

		// Adding one to the ids so that idx=0 is the null index
		int incrParticleID = particleID + 1;

		// Write to RGBA 
		gridOut = uvec4(incrParticleID);
	}
	// Don't consider particles outside the current voxel slice
	else discard;
}