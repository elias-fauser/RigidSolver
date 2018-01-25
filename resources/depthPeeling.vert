#version 330

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec2 in_texCoord;

uniform mat4 projMX;
uniform mat4 viewMX;
uniform mat4 modelMX;

out vec3 texCoord;

void main() {
	// Apply transformations to model
	gl_Position = projMX * viewMX * modelMX * in_position;
	
	texCoord = (in_position / in_position.w).xyz;
}