Manual:
*******
 
The plugin is the "RigidSolver" plugin.

When the plugin is opened just the ground plane is rendered. After loading a OBJ file from the "resources/models" folder with the
dropdown menu item "Model".

Further UI attributes:
	* fovY: The y field of view angle
	* Active: Switch if the simulation is running
	* SpawnTime: The number of seconds between each spawn of a new rigid body
	* Gravity: The gravity force
	* Mass: The mass of a rigid body
	* NumRigidBodies: The maximum number of rigid bodies which will be spawned
	* ParticleSize: The diameter of a particle which corresponds to the voxelsize of the solver grid
	* DrawParticles: Switch to enable drawing the particles -- NOT IMPLEMENTED --

Implementation:
***************

The implementation consits of three different classes:
	* The RigidSolver class which is derived from the OGL4Core Plugin. It contains the implementations of the virtual functions for Rendering, Keyboard and Setup functionality.
	* The SolverModel class is a subclass of VertexArray and extends the functionality of the depth peeling 
	  implementation providing access functions for the particle positions and the number of particles. It also associates the shaders
	  for the depth peeling algorithm
	* The SolverGrid implementation is the representation for the solver cube in which the simulation takes place. It contains information
	  the space occupied by the grid - the corner points, the model matrix and size attributes - as well as a resolution method
	  to retrieve the number of voxels per dimension

With no model loaded the program just executes the beautyPass() function which renders the ground plane.
After loading a model with the loadModel() function. The Implementation uses an OBj loaded to get the vertices, texture coordinats, indices
and normals. After that it centers the model, scales it appropriatly and performs depth peeling to determine the particle positions.

With the loaded model, simulation and rendering is performed in six steps:
	* particleValuePass(): Calculating the particle values from the current rigid body position and quaternion: particle position, particle velocity
	* collisionGridPass(): Filling the 3D grid textures with the particle indices for each voxel
	* collisionPass(): Performing the lookup of the 27 voxel around a particle and checking for collision returning in the particle forces.
	* momentaPass(): Calculating the resulting linear and angular momentum by summing up the individual forces of the particles assigned to
	  a rigid body
	* solverPass(): Calculating the new position and quaternion based on the previously computed momenta
	* beautyPass(): Rendering the rigid bodies

Properties:
***********
	* Maximum number of rigid bodies defined by 128x128 texture = 16384
	* Spheric particles
	* Lookup grid voxel length == diameter of particles

Resources:
**********

External Libraries:
	* OBJ Loader taken from https://github.com/Bly7/OBJ-Loader

Models were taken from the OpenGL repository of @McNopper (https://github.com/McNopper/OpenGL).
	* Chess Pawn OBJ: https://github.com/scenevr/chess/blob/master/models/pawn.obj
	* Teapot OBJ: https://github.com/McNopper/OpenGL/blob/master/Binaries/teapot.obj