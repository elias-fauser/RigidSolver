#version 330

#define M_PI  3.14159265

layout(pixel_center_integer) in vec4 gl_FragCoord;

uniform sampler2D lastDepth;
uniform int enabled;

// --------------------------------------------------
//   main
// --------------------------------------------------
void main() {
	
	// Bit-exact comparison between FP32 z-buffer and fragment depth
	// http://www.java2s.com/Open-Source/Java_Free_Code/Graph_3D/Download_modern_jogl_examples_Free_Java_Code.htm
	
	if (enabled > 0){
		float frontDepth = texelFetch(lastDepth, ivec2(gl_FragCoord.xy), 0).r;
		if (gl_FragCoord.z <= frontDepth) {
			discard;
		}
	}

	gl_FragColor = vec4(1.f);
}
