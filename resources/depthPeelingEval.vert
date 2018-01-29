#version 330

layout(location = 0) in vec2  in_position;

uniform mat4 projMX;
out vec2 texCoords;

void main() {    
    gl_Position = projMX * vec4(in_position,0,1);       
    texCoords = in_position;
}
