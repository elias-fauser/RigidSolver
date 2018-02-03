#version 330

#define M_PI  3.14159265

layout(location=0) out vec3 linearMomentumOut;
layout(location=1) out vec3 angularMomentumOut;

uniform sampler2D particlePositions;
uniform sampler2D particleForces;

// Uniforms
uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;

ivec2 idxTo2DParticleCoords(int idx){

	int x = idx % particleTextureEdgeLength;
	int y = idx / particleTextureEdgeLength;

	return ivec2(x, y);

}

// --------------------------------------------------
//   main
// --------------------------------------------------
void main() {
	
	ivec2 rigidBodyTexCoords = ivec2(gl_FragCoord.xy);
	int rigidBodyID = rigidBodyTexCoords.y * rigidBodyTextureEdgeLength + rigidBodyTexCoords.x;

	vec3 linearMomentum = vec3(0.f);
	vec3 angularMomentum = vec3(0.f);

	int particleStartIndex = rigidBodyID * particlesPerModel;

	for (int particleOffset = 0; particleOffset < particlesPerModel; particleOffset++){
		int particleIdx = particleStartIndex + particleOffset;
		ivec2 particleTexCoords = idxTo2DParticleCoords(particleIdx);
		
		vec3 particleRelativePosition = texelFetch(particlePositions, particleTexCoords, 0).xyz;
		vec3 particleForces = texelFetch(particleForces, particleTexCoords, 0).xyz;

		linearMomentum += particleForces;
		angularMomentum += cross(particleRelativePosition, particleForces);
	}
	
	linearMomentumOut = linearMomentum;
	angularMomentumOut = angularMomentum;
}
