#version 330

uniform mat4 projMX;
uniform mat4 viewMX;
uniform mat4 modelMX;

layout(location = 0) in vec4  in_position;
layout(location = 1) in vec2  in_texCoords;

out vec2 texCoords;

void main() {    
    gl_Position = projMX * viewMX * modelMX * in_position;
	texCoords = in_texCoords;
}
