#version 330

#define M_PI  3.14159265

layout(location=0) out vec3 outParticleForce;
layout(pixel_center_integer) in vec4 gl_FragCoord;

// Uniforms
uniform float springCoefficient;
uniform float dampingCoefficient;
uniform float gravity;
uniform float mass;
uniform float deltaT; // time diff in second
uniform float voxelLength;
uniform float particleDiameter;

uniform int particlesPerModel;
uniform int particleTextureEdgeLength;
uniform int rigidBodyTextureEdgeLength;
uniform int spawnedObjects;

uniform vec3 btmLeftFrontCorner;

// Textures
uniform usampler2DArray collisionGrid;
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
	uint particleID = uint(particleCoords.y * particleTextureEdgeLength + particleCoords.x);
	uint rigidBodyID_i = uint(particleID / uint(particlesPerModel));

	if (rigidBodyID_i >= uint(spawnedObjects)){
		discard;
		return;
	}
	
	// vec3 rigidBodyPosition_i = texelFetch(rigidBodyPositions, idxTo2DRigidBodyCoords(rigidBodyID_i), 0).xyz;
	vec3 particlePosition_i = texelFetch(particlePositions, particleCoords, 0).xyz;
	vec3 particleVelocity_i = texelFetch(particleVelocities, particleCoords, 0).xyz;
	// vec3 paticleWorldPosition_i = rigidBodyPosition_i + particlePosition_i;

	ivec3 voxelIndex = ivec3((particlePosition_i - btmLeftFrontCorner) / voxelLength);
	
	// Particle Force
	vec3 particleForce = vec3(0.f);
	vec3 forceSpring = vec3(0.f);
	vec3 forceDamp = vec3(0.f);
	vec3 forceTangential = vec3(0.f);

	// Always apply gravity (To position so ground collisions are detected)
	particleForce += vec3(0.f, -gravity * mass / particlesPerModel, 0.f);

	// Determine collision forces
	for (int i = -1; i < 2; i++){
		for (int j = -1; j < 2; j++){
			for (int k = -1; k < 2; k++){
				
				// Look up particle indices
				uvec4 particleIndices = texelFetch(collisionGrid, voxelIndex + ivec3(i, j, k), 0);
				
				// Subtract off the offset of one
				particleIndices -= uvec4(1);

				for (int idx = 0; idx < 4; idx++){
			
					uint particleIdx = particleIndices[idx];

					// Don't test on itself or on an empty id
					if (particleIdx == particleID || particleIdx == 0u){
						continue;
					}

					// Retrieve positions
					// int rigidBodyID_j = particleIdx / particlesPerModel;
					// vec3 rigidBodyPosition_j = texelFetch(rigidBodyPositions, idxTo2DRigidBodyCoords(rigidBodyID_j), 0).xyz;
					vec3 particlePosition_j = texelFetch(particlePositions, idxTo2DParticleCoords(int(particleIdx)), 0).xyz;
					vec3 particleVelocity_j = texelFetch(particleVelocities, idxTo2DParticleCoords(int(particleIdx)), 0).xyz;

					// Apply gravity
					// particleVelocity_j += vec3(0.f, -gravity * deltaT * mass, 0.f);

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
	
	// Determine floor collisions (considering applied forces)
	if (particlePosition_i.y < btmLeftFrontCorner.y){

		vec3 normalizedRelativePosition = -vec3(0.f, particlePosition_i.y - btmLeftFrontCorner.y, 0.f); // Distance beneath the floor
		normalizedRelativePosition /= length(normalizedRelativePosition);
		vec3 velocity_ij = -particleVelocity_i; // Velocity negativ with respect to floor

		// Calculate repulsive forces
		forceSpring = -1.f * (dampingCoefficient + particlePosition_i.y) * normalizedRelativePosition; // No spring efficient, full return force
		forceDamp = particleDiameter * velocity_ij;
		forceTangential = velocity_ij - (velocity_ij * normalizedRelativePosition) * normalizedRelativePosition;

		particleForce += forceSpring + forceDamp + forceTangential;
	}

	// Output to force texture
	outParticleForce = particleForce;
}
