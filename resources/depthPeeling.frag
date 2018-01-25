#version 330

#define M_PI  3.14159265

in vec3 texCoord;

uniform sampler2D lastDepth;

// --------------------------------------------------
//   main
// --------------------------------------------------
void main() {
	
	float depth = texture(lastDepth, texCoord.xy).x;
	if (texCoord.z < depth){
		gl_FragDepth = max(depth, texCoord.z);
	}
	else {
		gl_FragDepth = 0.0;
	}
}
