#version 330

// Uniforms
uniform mat4 projMX;
uniform int rigidBodyTextureEdgeLength;
uniform float mass;

uniform sampler2D rigidBodyPositions;
uniform sampler2D rigidBodyQuaternions;
uniform sampler2D rigidBodyLinearMomentums;
uniform sampler2D rigidBodyAngularMomentums;

// Inputs
layout(location = 0) in vec4  in_position;
in int rigidBodyID;
in int particleID;

// Outputs
out vec3 particlePosition;
out vec3 rigidPosition;
out vec4 rigidQuaternion;
out vec3 rigidLinearMomentum;
out vec3 rigidAngularMomentum;

vec2 rigidBodyIdx2TextureCoords(int idx) {
	int x = idx / rigidBodyTextureEdgeLength;
	int y = idx % rigidBodyTextureEdgeLength;
	return vec2(x,y);
}

void main() {    
	gl_Position = in_position; // ProjMX * in_position;

	vec2 rigidBodyCoords = rigidBodyIdx2TextureCoords(rigidBodyID);
	rigidPosition = texture(rigidBodyPositions, rigidBodyCoords).xyz;
	rigidQuaternion = texture(rigidBodyQuaternions, rigidBodyCoords);
	rigidAngularMomentum = texture(rigidBodyAngularMomentums, rigidBodyCoords).xyz;
	rigidLinearMomentum = texture(rigidBodyLinearMomentums, rigidBodyCoords).xyz;
}
