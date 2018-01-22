#version 330

// Uniforms
uniform mat4 projMX;

uniform sampler2D rigidBodyPositions;
uniform sampler2D rigidBodyQuaternions;
uniform sampler2D rigidBodyLinearMomentums;
uniform sampler2D rigidBodyAngularMomentums;

uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;
uniform float mass;

// Inputs
layout(location = 0) in vec4  in_position;

// Outputs
out vec3 initParticlePosition;
out vec3 rigidPosition;
out vec4 rigidQuaternion;
out vec3 rigidLinearMomentum;
out vec3 rigidAngularMomentum;

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
    gl_Position = projMX * vec4(invocationID2particleCoords(idx, rigidBodyID, particleID), 0.0, 1.0);
	initParticlePosition = (gl_Position / gl_Position.w).xyz;

	// Fill all the out variables needed
	vec2 rigidBodyCoords = rigidBodyIdx2TextureCoords(rigidBodyID);
	rigidPosition = texture(rigidBodyPositions, rigidBodyCoords).xyz;
	rigidQuaternion = texture(rigidBodyQuaternions, rigidBodyCoords);
	rigidAngularMomentum = texture(rigidBodyAngularMomentums, rigidBodyCoords).xyz;
	rigidLinearMomentum = texture(rigidBodyLinearMomentums, rigidBodyCoords).xyz;
}
