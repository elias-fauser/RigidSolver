#version 330

// Outputs
layout(location = 0) out vec3 particlePosition;
layout(location = 1) out vec4 particleVelocity;

uniform mat3 invIntertiaTensor;

in vec3 particlePosition;
in vec4 rigidQuaternion;
in vec3 rigidPosition;
in vec3 rigidLinearMomentum;
in vec3 rigidAngularMomentum;

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

void main() {
	
	mat3 quaternionRotation = quaternion2rotation(rigidQuaternion);
	vec3 relativePostition = quaternionRotation * particlePosition;

	// Calculate velocity
	rigidVelocity = rigidLinearMomentum / mass;

	// Calculate angular velocity
	vec3 rigidAngularVelocity = quaternionRotation * invIntertiaTensor * quaternionRotation * rigidAngularMomentum;

	particlePosition = rigidPosition + relativePosition;
	particleVelocity = rigidVelocity + cross(rigidAngularVelocity, relativePosition);

	
}
