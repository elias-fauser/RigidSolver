#version 330

layout(location = 0) in vec4 in_position;

uniform mat4 projMX;
uniform mat4 viewMX;
uniform mat4 modelMX;

void main() {
	// Apply transformations to model
	gl_Position = projMX * viewMX * modelMX * in_position;	
}