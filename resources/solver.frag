#version 330

// Outputs
layout(location = 0) out vec3 rigidBodyPosition;
layout(location = 1) out vec4 rigidBodyQuaternion;

layout(pixel_center_integer) in vec4 gl_FragCoord;

// Uniforms
uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;
uniform int spawnedObjects;

uniform float mass;
uniform float deltaT;

uniform mat3 invIntertiaTensor;

uniform sampler2D rigidBodyPositions;
uniform sampler2D rigidBodyQuaternions;
uniform sampler2D rigidBodyLinearMomentums;
uniform sampler2D rigidBodyAngularMomentums;


mat3 quaternion2rotation(vec4 q){

	mat3 rotation;

	rotation[0] = vec3(
		1.0 - 2.0 * pow(q.z, 2) - 2.0 * pow(q.w, 2),
		2.0 * q.y * q.z + 2.0 * q.x * q.w,
		2.0 * q.y * q.w - 2.0 * q.x * q.z
	);

	rotation[1] = vec3(
		2.0 * q.y * q.z - 2.0 * q.x * q.w,
		1.0 - 2.0 * pow(q.y, 2) - 2.0 * pow(q.w, 2),
		2.0 * q.z * q.w + 2.0 * q.x * q.y
	);

	rotation[2] = vec3(
		2.0 * q.y * q.z + 2.0 * q.x * q.z,
		2.0 * q.z * q.w - 2.0 * q.x * q.y,
		1.0 - 2.0 * pow(q.y, 2) - 2.0 * pow(q.z, 2) 
	);

	return rotation;
}

vec4 quaternionMultiplication(vec4 q1, vec4 q2){
	
	float s = q1.x * q2.x - dot(q1.yzw, q2.yzw);
	vec3 v = q1.x * q2.yzw + q2.x * q1.yzw + cross(q1.yzw, q2.yzw);
	return vec4(s, v.x, v.y, v.z);
}

ivec2 idxTo2DRigidBodyCoords(int idx){

	int x = idx % rigidBodyTextureEdgeLength;
	int y = idx / rigidBodyTextureEdgeLength;

	return ivec2(x, y);

}

void main() {
	
	// Determine which rigid Body we are and what particle
	ivec2 particleCoords = ivec2(gl_FragCoord.xy);
	int particleID = particleCoords.y * particleTextureEdgeLength + particleCoords.x;
	int rigidBodyID = int(particleID / particlesPerModel);
	
	if (rigidBodyID >= spawnedObjects){
		discard;
		return;
	}

	// ------------------------------------------------------------
	// Fetching necessary data
	// ------------------------------------------------------------

	// Fill all the out variables needed
	ivec2 rigidBodyCoords = idxTo2DRigidBodyCoords(rigidBodyID);
	vec3 rigidPosition = texelFetch(rigidBodyPositions, rigidBodyCoords, 0).xyz;
	vec4 rigidQuaternion = texelFetch(rigidBodyQuaternions, rigidBodyCoords, 0);
	vec3 rigidAngularMomentum = texelFetch(rigidBodyAngularMomentums, rigidBodyCoords, 0).xyz;
	vec3 rigidLinearMomentum = texelFetch(rigidBodyLinearMomentums, rigidBodyCoords, 0).xyz;
	
	// ------------------------------------------------------------
	// Calculate values
	// ------------------------------------------------------------

	mat3 quaternionRotation = quaternion2rotation(normalize(rigidQuaternion));
	mat3 inertiaInverse_t = quaternionRotation * invIntertiaTensor * transpose(quaternionRotation);
	vec3 angularVelocity =  inertiaInverse_t * rigidAngularMomentum;

	// Differential Quaternion
	float theta = length(angularVelocity * deltaT);

	vec3 a;
	if (length(angularVelocity) <= 0.001) a = vec3(0.f);
	else a = angularVelocity / length(angularVelocity);

	vec4 dq = vec4(cos(theta / 2.0), a * sin(theta / 2.0));

	// FIXME: Is the quaternion calculation right?
	// FIXME: Is it right to weight the velocity with deltaT? Thought because it is the position derivative with respect to time
	rigidBodyPosition = rigidPosition + rigidLinearMomentum / mass * deltaT;
	rigidBodyQuaternion = quaternionMultiplication(dq, rigidQuaternion);
	
}
