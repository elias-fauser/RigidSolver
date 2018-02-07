#version 430

layout(location = 0) in vec2  in_position;

uniform mat4 projMX;
uniform mat4 viewMX;

void main() {    
    gl_Position = projMX * vec4(in_position, 0, 1);       
}
