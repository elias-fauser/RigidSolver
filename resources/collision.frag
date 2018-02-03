#version 330

#define M_PI  3.14159265

layout(location=0) out vec3 particleForce;
layout(pixel_center_integer) in vec4 gl_FragCoord;

// Uniforms
uniform float springCoefficient;
uniform float dampingCoefficient;
uniform float gravity;
uniform float voxelLength;

uniform float particleDiameter;
uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;

uniform vec3 btmLeftFrontCorner;

// Textures
uniform usampler3D collisionGrid;
uniform sampler2D rigidBodyPositions;
uniform sampler2D particlePositions;
uniform sampler2D particleVelocities;

ivec2 idxTo2DParticleCoords(int idx){

	int x = idx % particleTextureEdgeLength;
	int y = idx / particleTextureEdgeLength;

	return ivec2(x, y);

}

ivec2 idxTo2DRigidBodyCoords(int idx){

	int x = idx % rigidBodyTextureEdgeLength;
	int y = idx / rigidBodyTextureEdgeLength;

	return ivec2(x, y);

}

bool collision(vec3 particle1, vec3 particle2, float diameter){
	
	if (length(particle1 - particle2) < diameter) return true;
	else return false;
}

// --------------------------------------------------
//   main
// --------------------------------------------------
void main() {
	
	// Retrieve particle coordinates from current render position
	ivec2 particleCoords = ivec2(gl_FragCoord.xy);
	int particleID = particleCoords.y * particleTextureEdgeLength + particleCoords.x * particlesPerModel;

	int rigidBodyID_i = int(particleID / particlesPerModel);

	vec3 rigidBodyPosition_i = texelFetch(rigidBodyPositions, idxTo2DRigidBodyCoords(rigidBodyID_i), 0).xyz;
	vec3 particlePosition_i = texelFetch(particlePositions, particleCoords, 0).xyz;
	vec3 particleVelocity_i = texelFetch(particleVelocities, particleCoords, 0).xyz;
	vec3 absolutePosition_i = rigidBodyPosition_i + particlePosition_i;

	ivec3 voxelIndex = ivec3((absolutePosition_i - btmLeftFrontCorner) / voxelLength);
	
	// Particle Force - Always apply gravity
	vec3 particleForce = vec3(0.f);
	vec3 forceSpring = vec3(0.f);
	vec3 forceDamp = vec3(0.f);
	vec3 forceTangential = vec3(0.f);

	particleForce += vec3(0.f, -gravity, 0.f);

	// Determine floor collisions
	if (absolutePosition_i.y <= 0.f){

		vec3 normalizedRelativePosition = vec3(0.f, -absolutePosition_i.y, 0.f); // Distance beneath the floor
		normalizedRelativePosition /= length(normalizedRelativePosition);

		// Calculate repulsive forces
		forceSpring = -1.f * springCoefficient * (dampingCoefficient + absolutePosition_i.y) * normalizedRelativePosition;
		forceDamp = particleDiameter * - particleVelocity_i; // Relative velocity of particle to floor is inversed velocity
		forceTangential = springCoefficient * (- particleVelocity_i - (-particleVelocity_i * normalizedRelativePosition) * normalizedRelativePosition);

		particleForce += forceSpring + forceDamp + forceTangential;
	}

	// Determine collision forces
	for (int i = -1; i < 2; i++){
		for (int j = -1; j < 2; j++){
			for (int k = -1; k < 2; k++){
				
				// Look up particle indices
				uvec4 particleIndices = texelFetch(collisionGrid, voxelIndex + ivec3(i, j, k), 0);

				for (int idx = 0; idx < 4; idx++){
			
					int particleIdx = int(particleIndices[idx]);

					// Don't test on itself
					if (particleIdx == particleID){
						continue;
					}

					// Retrieve positions
					int rigidBodyID_j = particleIdx / particlesPerModel;
					vec3 rigidBodyPosition_j = texelFetch(rigidBodyPositions, idxTo2DRigidBodyCoords(rigidBodyID_j), 0).xyz;
					vec3 particlePosition_j = rigidBodyPosition_j + texelFetch(particlePositions, idxTo2DParticleCoords(particleIdx), 0).xyz;
					vec3 particleVelocity_j = texelFetch(particleVelocities, idxTo2DParticleCoords(particleIdx), 0).xyz;

					// Collision
					if (collision(particlePosition_i, particlePosition_j, particleDiameter)){
						vec3 normalizedRelativePosition_ij = abs(particlePosition_i - particlePosition_j); // Is that right?
						float particleDistance = length(normalizedRelativePosition_ij);

						// Now normalize
						normalizedRelativePosition_ij /= length(normalizedRelativePosition_ij);
						vec3 velocity_ij = particleVelocity_i - particleVelocity_j;

						forceSpring = -1.f * springCoefficient * (dampingCoefficient - particleDistance) * normalizedRelativePosition_ij;
						forceDamp = particleDiameter * velocity_ij;
						forceTangential = springCoefficient * (velocity_ij - (velocity_ij * normalizedRelativePosition_ij) * normalizedRelativePosition_ij);

						particleForce += forceSpring + forceDamp + forceTangential;
					}
				}
	}}}

	// Output to force texture
	particleForce = particleForce;
}
