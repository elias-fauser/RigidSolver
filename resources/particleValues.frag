#version 330

// Outputs
layout(location = 0) out vec3 particlePosition;
layout(location = 1) out vec3 particleVelocity;

layout(pixel_center_integer) in vec4 gl_FragCoord;

// Uniforms
uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;

uniform float mass;

uniform mat3 invIntertiaTensor;

uniform sampler2D rigidBodyPositions;
uniform sampler2D rigidBodyQuaternions;
uniform sampler2D rigidBodyLinearMomentums;
uniform sampler2D rigidBodyAngularMomentums;

uniform sampler2D particlePositions;


// Functions
mat3 quaternion2rotation(vec4 q){
	return mat3(
		1.0 - 2.0 * pow(q.z, 2) - 2.0 * pow(q.w, 2),
		2.0 * q.y * q.z - 2.0 * q.x * q.w,
		2.0 * q.y * q.z + 2.0 * q.x * q.z,
		2.0 * q.y * q.z + 2.0 * q.x * q.w,
		1.0 - 2.0 * pow(q.y, 2) - 2.0 * pow(q.w, 2),
		2.0 * q.z * q.w - 2.0 * q.x * q.y,
		2.0 * q.y * q.w - 2.0 * q.x * q.z,
		2.0 * q.z * q.w + 2.0 * q.x * q.y,
		1.0 - 2.0 * pow(q.y, 2) - 2.0 * pow(q.z, 2) 
		);
}

ivec2 idxTo2DRigidBodyCoords(int idx){

	int x = idx % rigidBodyTextureEdgeLength;
	int y = idx / rigidBodyTextureEdgeLength;

	return ivec2(x, y);

}

void main() {
	
	// Determine which rigid Body we are and what particle
	ivec2 particleCoords = ivec2(gl_FragCoord.xy);
	int particleID = particleCoords.y * particleTextureEdgeLength + particleCoords.x * particlesPerModel;
	int rigidBodyID = int(particleID / particlesPerModel);
	

	// ------------------------------------------------------------
	// Fetching necessary data
	// ------------------------------------------------------------

	// Fill all the out variables needed
	ivec2 rigidBodyCoords = idxTo2DRigidBodyCoords(rigidBodyID);
	vec3 rigidPosition = texelFetch(rigidBodyPositions, rigidBodyCoords, 0).xyz;
	vec4 rigidQuaternion = texelFetch(rigidBodyQuaternions, rigidBodyCoords, 0);
	vec3 rigidAngularMomentum = texelFetch(rigidBodyAngularMomentums, rigidBodyCoords, 0).xyz;
	vec3 rigidLinearMomentum = texelFetch(rigidBodyLinearMomentums, rigidBodyCoords, 0).xyz;
	
	vec3 prevParticlePosition = texelFetch(particlePositions, particleCoords, 0).xyz;

	// ------------------------------------------------------------
	// Calculate values
	// ------------------------------------------------------------

	mat3 quaternionRotation = quaternion2rotation(rigidQuaternion);
	vec3 relativePosition = quaternionRotation * prevParticlePosition;

	// Calculate velocity
	vec3 rigidVelocity = rigidLinearMomentum / mass;

	// Calculate angular velocity
	vec3 rigidAngularVelocity = quaternionRotation * invIntertiaTensor * quaternionRotation * rigidAngularMomentum;

	particlePosition = relativePosition;
	particleVelocity = rigidVelocity + cross(rigidAngularVelocity, relativePosition);

	
}
