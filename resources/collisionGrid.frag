#version 430

// Outputs
layout(location = 0) out uvec4 gridOut;

uniform int z;
uniform float voxelLength;
uniform float zCoord;

flat in int particleID;
in vec4 particlePosition;

void main() {

	gridOut = uvec4(40);
	return;

	// Only considering particles which are in the current z layer
	if (particlePosition.z >= zCoord && particlePosition.z < zCoord + voxelLength){
		// Write to RGBA 
		gridOut = uvec4(particleID, particleID, particleID, particleID); // uvec4(particleID);
	}
	// else discard;
}