#version 330

// Uniforms
uniform mat4 projMX;
uniform mat4 viewMX;
uniform mat4 modelMX;

uniform int positionByTexture;
uniform int rigidBodyTextureEdgeLength;

uniform sampler2D rigidBodyPositions;
uniform sampler2D rigidBodyQuaternions;

// Inputs
layout(location = 0) in vec4  in_position;
layout(location = 1) in vec2  in_texCoords;
layout(location = 2) in vec3  in_normal;

// Outputs
flat out int instanceID;
out vec2 texCoords;
out vec3 normal;
out vec3 position;

ivec2 rigidBodyIdx2TextureCoords(int idx) {
	int x = idx / rigidBodyTextureEdgeLength;
	int y = idx % rigidBodyTextureEdgeLength;
	return ivec2(x,y);
}

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
void main() {    

	
	texCoords = in_texCoords;
	instanceID = gl_InstanceID;

	if (positionByTexture == 0){
		gl_Position = projMX * viewMX * modelMX * in_position;
	}
	else {
		ivec2 positionCoords = rigidBodyIdx2TextureCoords(gl_InstanceID);
		vec4 position = texelFetch(rigidBodyPositions, positionCoords, 0);
		vec4 quaternion = texelFetch(rigidBodyQuaternions, positionCoords, 0);

		mat3 rotation = quaternion2rotation(quaternion);

		// Rotate around CoM - because model is centered at origin with CoM
		vec4 relativeRotated = vec4(rotation * (in_position / in_position.w).xyz, 1.0f);

		// Move the CoM then apply view transformation
		gl_Position = projMX * viewMX * modelMX * (in_position + position);
	}

	position = (gl_Position / gl_Position.w).xyz;
	normal = in_normal;
}
