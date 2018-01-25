#version 330

// Outputs
layout(location = 0) out vec4 gridOut;

flat in int particleID;

void main() {
	
	gridOut = vec4(particleID);

}
