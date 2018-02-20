# Rigid Body Solver for OGL4Core

The is an implementation of a rigid solver described in [Nvidias GPU Gems 3](https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch29.html). It was developed for the Module "Practical Course Visual Computing" of the University of Stuttgart, WS2017/18. 

__Sadly, the assignment was not finished completly at the time of submission. In debug mode I was able to verify that the rendering stages perform the required output but my calculations still contain an error and that is why with this version the rigid bodies fly off the screen.__

__Feel free to fork the project to fix the code. I currently don't have time to further engage in the development!__

## Disclaimer

This project was successfully submitted in the Practical Course Visual Computing at the University of Stuttgart and is not allowed to be used or submitted as whole or in parts at any other educational facility! 

## Getting started

### Installing

The plugin uses a framework called "OGL4Core" of the University of Stuttgart which provides UI functionality as well as some Event methods.
The framework can be obtained at the [Visus Homepage](https://www.vis.uni-stuttgart.de/projekte/ogl4core-framework).

After downloading and unpacking the version of your system the plugin may be added to the `plugins` folder. Note that the plugin folder need to be nested twice: `plugins\<plugins structure folder>\<Plugin Name>\`. Pull this repository into the plugins subfolder.

### Deployment

The code may be doployed by your favourite compiler. The `packages.config` may be used to obtain the used dependencies through NuGet.

The code was compiled with VS2015. A Visual Studio project is added to the repository.

### Usage
 
In OGL4Core the plugin is the "RigidSolver" plugin.

When the plugin is opened just the ground plane is rendered. After loading a OBJ file from the "resources/models" folder with the
dropdown menu item "Model".

Further UI attributes:

* fovY: The y field of view angle
* Active: Switch if the simulation is running
* Reset: Button which resets the simulation
* SpawnTime: The number of seconds between each spawn of a new rigid body
* Gravity: The gravity force
* Mass: The mass of a rigid body
* springCoefficient: The spring Coefficient used in the collision force calculation
* dampingCoefficient: The damping Coefficient used in the collision force calculation
* NumRigidBodies: The maximum number of rigid bodies which will be spawned
* ParticleSize: The diameter of a particle which corresponds to the voxelsize of the solver grid
* DrawParticles: Switch to enable drawing the particles -- NOT IMPLEMENTED --

The view may be altered using the mouse.

### Implementation:

The implementation consits of three different classes:

* The RigidSolver class which is derived from the OGL4Core Plugin. It contains the implementations of the virtual functions for Rendering, Keyboard and Setup functionality.
* The SolverModel class is a subclass of VertexArray and extends the functionality of the depth peeling implementation providing access functions for the particle positions and the number of particles. It also associates the shaders for the depth peeling algorithm
* The SolverGrid implementation is the representation for the solver cube in which the simulation takes place. It contains information the space occupied by the grid - the corner points, the model matrix and size attributes - as well as a resolution method to retrieve the number of voxels per dimension

With no model loaded the program just executes the beautyPass() function which renders the ground plane.
After loading a model with the loadModel() function. The Implementation uses an OBj loaded to get the vertices, texture coordinats, indices
and normals. After that it centers the model, scales it appropriatly and performs depth peeling to determine the particle positions.

With the loaded model, simulation and rendering is performed in six steps:
* particleValuePass(): Calculating the particle values from the current rigid body position and quaternion: particle position, particle velocity
* collisionGridPass(): Filling the 3D grid textures with the particle indices for each voxel
* collisionPass(): Performing the lookup of the 27 voxel around a particle and checking for collision returning in the particle forces.
* momentaPass(): Calculating the resulting linear and angular momentum by summing up the individual forces of the particles assigned to a rigid body
* solverPass(): Calculating the new position and quaternion based on the previously computed momenta
* beautyPass(): Rendering the rigid bodies

The FBOs - there are three of them: one for the particles, one for the rigid bodies and one for the collision grid - are initialized in the
initSolverFBOs() function which subsequently calls the underlying updateParticles(), updateGrid() and updateRigidBodies() function which contain
the actual texture creations.

### Properties:
***********

* Maximum number of rigid bodies defined by 128x128 texture = 16384
* Spheric particles
* Lookup grid voxel length == diameter of particles

### Resources:

External Libraries:
* OBJ Loader taken from https://github.com/Bly7/OBJ-Loader

Models were taken from the OpenGL repository of @McNopper (https://github.com/McNopper/OpenGL).
* Chess Pawn OBJ: https://github.com/scenevr/chess/blob/master/models/pawn.obj
* Teapot OBJ: https://github.com/McNopper/OpenGL/blob/master/Binaries/teapot.obj

## Authors

Elias Fauser - Initial Work - [elias-fauser](https://github.com/elias-fauser)