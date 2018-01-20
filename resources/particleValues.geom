#version 430

// We are getting some patches and retrieving the particle position from the shader and outputing those in texture coordinates
layout(points) in;
layout(points, max_vertices = 1) out;

uniform int particlesPerModel;
uniform int particleTextureEdgeLength;

flat out int rigidBodyID;
flat out int particleID;

vec2 invocationID2particleCoords(int rigidID, int particleID){

	int x = int((rigidID * particlesPerModel) / particleTextureEdgeLength);
	int y = idx % (x * particleTextureEdgeLength);

	return vec2(x, y)

}

void main()
{	
	// Determine which rigid Body we are and what particle
	int idx = gl_InvocationID;
	rigidBodyID = int(idx / particlesPerModel);
	particleID = idx % particlePerModel;

	// Mapping of invocation id to particle coord
    gl_Position = vec4(invocationID2particleCoords(rigidBodyID, particleID), 0.0, 1.0);

    EmitVertex();
    EndPrimitive();
}
