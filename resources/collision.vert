#version 330

// Uniforms
uniform mat4 projMX;

// Inputs
layout(location = 0) in vec2  in_position;

void main() {    
    gl_Position = projMX * vec4(in_position, 0, 1);

}
