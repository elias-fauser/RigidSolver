#version 330

layout(location = 0) in vec4 in_position;

out vec2 texCoord;

void main() {
	
	gl_Position = in_position;
	texCoord = in_position.xy;

}