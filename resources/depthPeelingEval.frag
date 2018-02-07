#version 430

#define M_PI  3.14159265

layout(location=0) out float evaluationValue;

layout(pixel_center_integer) in vec4 gl_FragCoord;

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
	
	ivec2 texCoords = ivec2(gl_FragCoord.xy);

	float depth1Value = texelFetch(depth1, texCoords, 0).x;
	float depth2Value = texelFetch(depth2, texCoords, 0).x;
	float depth3Value = texelFetch(depth3, texCoords, 0).x;
	float depth4Value = texelFetch(depth4, texCoords, 0).x;

	if (z > depth1Value && z < depth2Value){
		evaluationValue = 1.f;
	}
	else if (z > depth3Value && z < depth4Value){
		evaluationValue = 1.f;
	}
	else {
		evaluationValue = 0.f;
	}
}
