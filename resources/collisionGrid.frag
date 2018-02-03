#version 330

// Outputs
layout(location = 0) out uvec4 gridOut;

uniform float voxelLength;
uniform float zCoord;

flat in int particleID;
in vec4 particlePosition;

void main() {
	
	// Only considering particles which are in the current z layer
	if (particlePosition.z > zCoord && particlePosition.z < zCoord + voxelLength){
		// Write to RGBA 
		gridOut = uvec4(particleID);
	}
	else discard;