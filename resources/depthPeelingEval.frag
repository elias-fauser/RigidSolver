#version 330

#define M_PI  3.14159265

in vec2 texCoords;

uniform float z;
uniform float zNear;
uniform float zFar;

uniform sampler2D depth1;
uniform sampler2D depth2;
uniform sampler2D depth3;
uniform sampler2D depth4;

// --------------------------------------------------
//   main
// --------------------------------------------------
void main() {

	float depth1Value = texture(depth1, texCoords).x;
	float depth2Value = texture(depth2, texCoords).x;
	float depth3Value = texture(depth3, texCoords).x;
	float depth4Value = texture(depth4, texCoords).x;

	if (z > depth1Value && z < depth2Value){
		gl_FragColor = vec4(1.0);
	}
	else if (z > depth3Value && z < depth4Value){
		gl_FragColor = vec4(1.0);
	}
	else {
		gl_FragColor = vec4(0.0);
	}
}
