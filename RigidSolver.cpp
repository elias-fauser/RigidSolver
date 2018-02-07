// RigidSolver.cpp 
//

#include "stdafx.h"
#include "RigidSolver.h"
#include "GL/gl3w.h"
#include "GLHelpers.h"
#include "Defs.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/ext.hpp"
#include "OBJ_Loader.h"
#include <iosfwd>
#include "lodepng.h"
#include "soil.h"
#include <windows.h>
#include <direct.h>
#include <iostream>
#include <fstream>

// --------------------------------------------------
//  Ground plane
// --------------------------------------------------

const float PLANE_VERTICES[] = {
	 .5f, -.5f,  .5f, 1.0f,
	 .5f, -.5f, -.5f, 1.0f,
	-.5f, -.5f, -.5f, 1.0f,
	-.5f, -.5f,  .5f, 1.0f,
};

/*
          z
		 /
      y
   3 -|---- 0
   /  |    /
  /   *   /    - x
 /       /
2 ----- 1

*/
const float PLANE_TEXCOORDS[] = {
	 1.f,  1.f,
	 0.f,  1.f,
	 0.f,  0.f,
	 1.f,  0.f,
};

const int PLANE_INDICES[] = {
	0, 1, 2,
	0, 2, 3
};

const float PLANE_NORMALS[] = {
	0.f, 1.f, 0.f,
	0.f, 1.f, 0.f,
	0.f, 1.f, 0.f,
	0.f, 1.f, 0.f
};

const float PARTICLE_BASE_VERTICE[] = {
	0.f, 0.f, 0.f, 1.0f
};

// --------------------------------------------------
//  Constants
// --------------------------------------------------

const int MAX_NUMBER_OF_RIGID_BODIES = 64 * 64; // 4096
const int MAX_NUMBER_OF_PARTICLES = MAX_NUMBER_OF_RIGID_BODIES * 4; // Assuming voxel size == particle diameter -> so max of 4 particles

// FBO attachments
enum RigidBodyAttachments {
	RigidBodyPositionAttachment1 = GL_COLOR_ATTACHMENT0,
	RigidBodyPositionAttachment2 = GL_COLOR_ATTACHMENT1,
	RigidBodyQuaternionAttachment1 = GL_COLOR_ATTACHMENT2,
	RigidBodyQuaternionAttachment2 = GL_COLOR_ATTACHMENT3,
	RigidBodyLinearMomentumAttachment = GL_COLOR_ATTACHMENT4,
	RigidBodyAngularMomentumAttachment = GL_COLOR_ATTACHMENT5,
};

enum ParticleAttachments {
	ParticlePositionAttachment = GL_COLOR_ATTACHMENT0,
	ParticleVelocityAttachment = GL_COLOR_ATTACHMENT1,
	ParticleForceAttachment = GL_COLOR_ATTACHMENT2,
	ParticleRelativePositionAttachment = GL_COLOR_ATTACHMENT3,
	initialParticlePositionsAttachment = GL_COLOR_ATTACHMENT6


};

enum GridAttachments {
	GridIndiceAttachment = GL_COLOR_ATTACHMENT0
};

// Declaration of static vertex arrays
VertexArray RigidSolver::vaQuad = VertexArray();
VertexArray RigidSolver::vaPlane = VertexArray();
std::string RigidSolver::debugDirectory = "";

// Declaration of CreateInstance
OGL4COREPLUGIN_API RenderPlugin* OGL4COREPLUGIN_CALL CreateInstance(COGL4CoreAPI *Api) {
	return new RigidSolver(Api);
}

RigidSolver::RigidSolver(COGL4CoreAPI *Api) : RenderPlugin(Api) {
    this->myName = "RigidSolver";
    this->myDescription = "Rigid Solver which allows the usage of custom objects as instances";

	viewMX = glm::mat4(1.f);
	projMX = glm::mat4(1.f);

}

RigidSolver::~RigidSolver() {

}

bool RigidSolver::Activate(void) {

	// --------------------------------------------------
	//  Setting up the paths
	// --------------------------------------------------
	std::string pathName = this->GetCurrentPluginPath();
	RigidSolver::debugDirectory = pathName + std::string("/debug");

	// Create debug directory
	if (DEBUGGING) {
		if (mkdir(RigidSolver::debugDirectory.c_str()) != 0) {
			std::cout << "Could not create debug directory!" << std::endl;
		}
	}

	// --------------------------------------------------
	//  Registration of view manipulator
	// --------------------------------------------------
	int camHandle = this->AddManipulator("view", &this->viewMX, Manipulator::MANIPULATOR_ORBIT_VIEW_3D);
	this->SelectCurrentManipulator(camHandle);
	this->SetManipulatorRotation(camHandle, glm::vec3(1, 0, 0), -50.0);
	this->SetManipulatorDolly(camHandle, -2.5f);

	// --------------------------------------------------
	//  Registration of UI attributes
	// --------------------------------------------------  

	// Initialize file selector
	modelFiles.Set(this, "Model", (pathName + "/resources/models").c_str(), ".obj", &RigidSolver::fileChanged);
	modelFiles.Register();

	fovY.Set(this, "fovY");
	fovY.Register();
	fovY.SetMinMax(1.0, 100.0);
	fovY = 50.0f;

	// Solver settings
	solverStatus.Set(this, "Active");
	solverStatus.Register();
	solverStatus = false;

	spawnTime.Set(this, "SpawnTime(sec)");
	spawnTime.Register();
	spawnTime.SetMinMax(1.0, 300.0);
	spawnTime = 5;

	gravity.Set(this, "Gravity");
	gravity.Register();
	gravity = 9.807f; // m/s^2

	modelMass.Set(this, "Mass");
	modelMass.Register();
	modelMass.SetMinMax(0.1, 100.0);
	modelMass = 0.1f; // kg

	springCoefficient.Set(this, "SpringCoefficient");
	springCoefficient.Register();
	springCoefficient.SetMinMax(0.01, 10);
	springCoefficient = 1.f;

	dampingCoefficient.Set(this, "dampCoefficient");
	dampingCoefficient.Register();
	dampingCoefficient.SetMinMax(0.01, 10);
	dampingCoefficient = 1.f;

	numRigidBodies.Set(this, "NumRigidBodies");
	numRigidBodies.Register();
	numRigidBodies.SetMinMax(1.0, MAX_NUMBER_OF_RIGID_BODIES);
	numRigidBodies = 100;

	// All about particles
	particleSize.Set(this, "ParticleSize", &RigidSolver::particleSizeChanged);
	particleSize.Register();
	particleSize.SetMinMax(0.5, 2.0);
	particleSize = 0.01;

	drawParticles.Set(this, "DrawParticles");
	drawParticles.Register();
	drawParticles = false;


	// --------------------------------------------------
	//  Creating shaders and geometry
	// --------------------------------------------------    
	commonFunctionsVertShaderName = pathName + std::string("/resources/common.fncs");
	particleValuesVertShaderName = pathName + std::string("/resources/particleValues.vert");
	particleValuesFragShaderName = pathName + std::string("/resources/particleValues.frag");
	particleValuesGeomShaderName = pathName + std::string("/resources/particleValues.geom");
	beautyVertShaderName = pathName + std::string("/resources/beauty.vert");
	beautyFragShaderName = pathName + std::string("/resources/beauty.frag");
	momentaVertShaderName = pathName + std::string("/resources/momenta.vert");
	momentaFragShaderName = pathName + std::string("/resources/momenta.frag");
	collisionVertShaderName = pathName + std::string("/resources/collision.vert");
	collisionFragShaderName = pathName + std::string("/resources/collision.frag");
	collisionGridGeomShaderName = pathName + std::string("/resources/collisionGrid.geom");
	collisionGridVertShaderName = pathName + std::string("/resources/collisionGrid.vert");
	collisionGridFragShaderName = pathName + std::string("/resources/collisionGrid.frag");
	solverVertShaderName = pathName + std::string("/resources/solver.vert");
	solverFragShaderName = pathName + std::string("/resources/solver.frag");

	// Setup Quad geometry
	float quadVertices[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };
	vaQuad.Create(4);
	vaQuad.SetArrayBuffer(0, GL_FLOAT, 2, quadVertices);

	// Setup base plane geometry
	vaPlane.Create(4);
	vaPlane.SetArrayBuffer(0, GL_FLOAT, 4, PLANE_VERTICES);
	vaPlane.SetElementBuffer(0, 2 * 3, PLANE_INDICES);
	vaPlane.SetArrayBuffer(1, GL_FLOAT, 2, PLANE_TEXCOORDS);
	vaPlane.SetArrayBuffer(2, GL_FLOAT, 3, PLANE_NORMALS);

	// Setup particle geometry
	vaVertex.Create(1);
	vaVertex.SetArrayBuffer(0, GL_FLOAT, 4, PARTICLE_BASE_VERTICE);

	reloadShaders();

	// --------------------------------------------------
	//  Init
	// --------------------------------------------------  

	initSolverFBOs();

	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	glClearDepth(1.0);
	glEnable(GL_DEPTH_TEST);

	// --------------------------------------------------
	//  Query opengl limits just to be sure
	// --------------------------------------------------  

	GLint maxColAtt;
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColAtt);
	fprintf(stderr, "Maximum number of color attachments: %d\n", maxColAtt);

	GLint maxGeomOuputVerts;
	glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES, &maxGeomOuputVerts);
	fprintf(stderr, "Maximum number of geometry output vertices: %d\n", maxGeomOuputVerts);

    return true;
}

bool RigidSolver::Deactivate(void) {
	// Detach Shaders
	shaderBeauty.RemoveAllShaders();
	shaderMomentaCalculation.RemoveAllShaders();
	shaderParticleValues.RemoveAllShaders();
	shaderCollision.RemoveAllShaders();
	shaderCollisionGrid.RemoveAllShaders();
	shaderSolver.RemoveAllShaders();

	// Delete VertexArrays
	vaPlane.Delete();
	vaModel.Delete();
	vaQuad.Delete();
	vaVertex.Delete();

	// Delete Textures
	glDeleteTextures(1, &beautyDepthTex);
	glDeleteTextures(1, &gridTex);
	glDeleteTextures(1, &initialParticlePositionsTex);
	glDeleteTextures(1, &rigidBodyPositionsTex1);
	glDeleteTextures(1, &rigidBodyPositionsTex2);
	glDeleteTextures(1, &rigidBodyQuaternionsTex1);
	glDeleteTextures(1, &rigidBodyQuaternionsTex2);
	glDeleteTextures(1, &rigidBodyLinearMomentumTex);
	glDeleteTextures(1, &rigidBodyAngularMomentumTex);
	glDeleteTextures(1, &particlePositionsTex);
	glDeleteTextures(1, &particleVelocityTex);
	glDeleteTextures(1, &particleRelativePositionTex);
	glDeleteTextures(1, &particleForcesTex);

	// Delete Framebuffers
	glDeleteFramebuffers(1, &rigidBodyFBO);
	glDeleteFramebuffers(1, &particlesFBO);
	glDeleteFramebuffers(1, &gridFBO);

	glDisable(GL_DEPTH_TEST);
    return true;
}

bool RigidSolver::Init(void) {
	if (gl3wInit()) {
		fprintf(stderr, "Error: Failed to initialize gl3w.\n");
		return false;
	}

	if (MAX_NUMBER_OF_RIGID_BODIES * 4 > GL_MAX_FRAMEBUFFER_WIDTH || MAX_NUMBER_OF_RIGID_BODIES * 4 > GL_MAX_FRAMEBUFFER_HEIGHT) {
		std::cout << "Unable to create textures. Maximum GL Framebuffer dimensions exceeded!\n";
		return false;
	}
	return true;
}

bool RigidSolver::Idle(void)
{
	if (solverStatus && modelFiles.GetValue() != NULL) PostRedisplay();
	return true;
}

bool RigidSolver::Render(void) {

	// --------------------------------------------------
	//  Setup
	// --------------------------------------------------  

	// Switching the texture switch to use it the other way around
	// The one that is active (false=1, true=2) means that it is read from
	if (texSwitch == false) texSwitch = true;
	else texSwitch = false;

	if (solverStatus && modelFiles.GetValue() != NULL) {
		// Get current time and eventually spawn a new particle
		time = std::chrono::high_resolution_clock::now();
		time_span = time - lastSpawn;
		if (time_span.count() * 1000  >= spawnTime && spawnedObjects <= numRigidBodies) {
			spawnedObjects = std::max((int)(spawnedObjects + 1), MAX_NUMBER_OF_RIGID_BODIES);
		}
		lastSpawn = time;
	}

	// --------------------------------------------------
	//  Passes
	// --------------------------------------------------  

	if (solverStatus && modelFiles.GetValue() != NULL && vaModel.getNumParticles() > 0) {

		// Physical values - Determine rigid positions and particle attributes
		particleValuePass();

		// Generate Lookup grid - Assign the particles to the voxels
		collisionGridPass();

		// Collision - Find collision and calculate forces
		collisionPass();

		// Particle positions - Determine the momenta and quaternions
		momentaPass();

		// Calculate the new rigid body positions
		solverPass();

	}

	// --------------------------------------------------
	//  Rendering
	// --------------------------------------------------  

	// Render beauty
	beautyPass();

    return false;
}

bool RigidSolver::Resize(int width, int height) {

	windowHeight = height;
	windowWidth = width;

	// Set the new aspect ratio
	aspectRatio = windowWidth / float(windowHeight);

	return false;
}

bool RigidSolver::Keyboard(unsigned char key, int x, int y) {
	std::string pathName = this->GetCurrentPluginPath();

	if (key == 'r') reloadShaders();

	PostRedisplay();
	return false;
}

// --------------------------------------------------
//  FILE IO
// --------------------------------------------------   

/**
* @brief Load a model with the given vertices and indices
* This function only supports triangle meshes
* It calculates the center of mass and moves the model so it is centered around it. It also calculates the bounding box
* at scales the model so it fits into the solver grid.
*/
bool RigidSolver::loadModel(float * vertices, int * indices, int num) {

	// Delete the old one
	vaModel.Delete();

	// Calculate Center of mass
	glm::vec3 com = glm::vec3(0.0f);

	// Inertia Tensor
	float Ixx = 0.f, Iyy = 0.f, Izz = 0.f;
	float Ixy = 0.f, Ixz = 0.f, Iyz = 0.f;
	glm::mat3 intertiaTensor;

	// Bounding Box
	glm::vec3 maximum = glm::vec3(0.f);
	glm::vec3 minimum = glm::vec3(0.f);

	for (int i = 0; i < num; i++) {
		com += glm::vec3(vertices[i * 4], vertices[i * 4 + 1], vertices[i * 4 + 2]);

		maximum.x = std::max(maximum.x, vertices[i * 4]);
		maximum.y = std::max(maximum.y, vertices[i * 4 + 1]);
		maximum.z = std::max(maximum.z, vertices[i * 4 + 2]);

		minimum.x = std::min(minimum.x, vertices[i * 4]);
		minimum.y = std::min(minimum.y, vertices[i * 4 + 1]);
		minimum.z = std::min(minimum.z, vertices[i * 4 + 2]);
	}
	com /= num;

	// Calculate scaling
	float preferredModelSize = 0.1;
	glm::vec3 size = maximum - minimum;
	float scale = preferredModelSize / std::max(std::max(size.x, size.y), size.z);

	// Move the model so it is centered in the CoM
	float * movedVertices;
	movedVertices = new float[num * 4];
	
	for (unsigned int i = 0; i < num; i++) {

		float x = (vertices[i * 4] - com.x) * scale;
		float y = (vertices[i * 4 + 1] - com.y) * scale;
		float z = (vertices[i * 4 + 2] - com.z) * scale;

		// Scale the model vertices
		movedVertices[i * 4] = x;
		movedVertices[i * 4 + 1] = y;
		movedVertices[i * 4 + 2] = z;
		movedVertices[i * 4 + 3] = vertices[i * 4 + 3];

		// Calculate intertia tensor
		Ixx += y * y + z * z;
		Ixy += x * y;
		Iyy += x * x + z * z;
		Ixz += x * z;
		Iyz += y * z;
		Izz += x * x + y * y;

	}

	// Create the new one
	vaModel.Create(num);
	vaModel.SetArrayBuffer(0, GL_FLOAT, 4, movedVertices);
	vaModel.SetElementBuffer(0, num * 3, indices);

	vaModel.setInertiaTensor(glm::mat3(
		 Ixx, -Ixy, -Ixz,
		-Ixy,  Iyy, -Iyz,
		-Ixz, -Iyz,  Izz));
	
	minimum *= scale;
	maximum *= scale;
	vaModel.setBoundingBox(minimum.x, maximum.x, minimum.y, maximum.y, minimum.z, maximum.z);

	delete[] movedVertices;
	
	// Create the particles
	vaModel.createParticles(&grid);

	// Reset simulation to restart everything - this also initiates the new FBOs
	resetSimulation();

	return true;

}

/**
* @brief Overload of loadModel to extend the function with texture coordinates
*/
bool RigidSolver::loadModel(float * vertices, int * indices, float * texCoords, int num) {

	loadModel(vertices, indices, num);
	vaModel.SetArrayBuffer(1, GL_FLOAT, 2, texCoords);

	return true;
}

/**
* @brief Overload of loadModel to extend the function with texture coordinates
*/
bool RigidSolver::loadModel(float * vertices, int * indices, float * texCoords, float * normals, int num) {

	loadModel(vertices, indices, texCoords, num);
	vaModel.SetArrayBuffer(2, GL_FLOAT, 3, normals);

	return true;
}

// --------------------------------------------------
//  RENDER PASSES
// --------------------------------------------------   

bool RigidSolver::particleValuePass(void)
{

	glBindFramebuffer(GL_FRAMEBUFFER, particlesFBO);

	shaderParticleValues.Bind();

	// Bind for reading and writing
	if (texSwitch == false) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex1);
		glUniform1i(shaderParticleValues.GetUniformLocation("rigidBodyPositions"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex1);
		glUniform1i(shaderParticleValues.GetUniformLocation("rigidBodyQuaternions"), 1);


	}
	else {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex2);
		glUniform1i(shaderParticleValues.GetUniformLocation("rigidBodyPositions"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex2);
		glUniform1i(shaderParticleValues.GetUniformLocation("rigidBodyQuaternions"), 1);

	}

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, rigidBodyLinearMomentumTex);
	glUniform1i(shaderParticleValues.GetUniformLocation("rigidBodyLinearMomentums"), 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, rigidBodyAngularMomentumTex);
	glUniform1i(shaderParticleValues.GetUniformLocation("rigidBodyAngularMomentums"), 3);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_1D, initialParticlePositionsTex);
	glUniform1i(shaderParticleValues.GetUniformLocation("relativeParticlePositions"), 4);

	int sideLength = getParticleTextureSideLength();
	if (sideLength == 0) return false;

	/*
	projMX = glm::ortho(0, sideLength, 0, sideLength);

	// Clearing
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Viewporting to output texture size
	glViewport(0, 0, sideLength, sideLength);
	*/

	// Define the two outputs
	GLuint attachments[] = { ParticlePositionAttachment, ParticleVelocityAttachment, ParticleRelativePositionAttachment};
	glDrawBuffers(3, attachments);

	// Uniforms
	glm::mat3 inverseInertia = glm::inverse(vaModel.getInertiaTensor());

	glUniformMatrix4fv(shaderParticleValues.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMX));
	glUniformMatrix3fv(shaderParticleValues.GetUniformLocation("invInertiaTensor"), 1, false, glm::value_ptr(inverseInertia));
	glUniform1i(shaderParticleValues.GetUniformLocation("particlesPerModel"), vaModel.getNumParticles());
	glUniform1i(shaderParticleValues.GetUniformLocation("particleTextureEdgeLength"), sideLength);
	glUniform1i(shaderParticleValues.GetUniformLocation("rigidBodyTextureEdgeLength"), getRigidBodyTextureSizeLength());
	glUniform1f(shaderParticleValues.GetUniformLocation("mass"), modelMass);
	glUniform1f(shaderParticleValues.GetUniformLocation("gravity"), gravity);
	glUniform1f(shaderParticleValues.GetUniformLocation("deltaT"), time_span.count() / 1000.f);

	vaVertex.Bind();
	drawAbstractData(sideLength, sideLength, shaderParticleValues);
	// glDrawArraysInstanced(GL_POINTS, 0, 1, spawnedObjects * vaModel.getNumParticles());
	vaVertex.Release();

	shaderParticleValues.Release();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (DEBUGGING) {

		int particleSideLength = getParticleTextureSideLength();

		float * particlePositions;
		float * particleVelocity;

		particlePositions = new float[particleSideLength * particleSideLength * 3];
		particleVelocity = new float[particleSideLength * particleSideLength * 3];

		glBindTexture(GL_TEXTURE_2D, particlePositionsTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, particlePositions);
		saveArrayToTXT(RigidSolver::debugDirectory + std::string("/particleValues_particlePositions.txt"), particlePositions, particleSideLength * particleSideLength * 3, 3);

		glBindTexture(GL_TEXTURE_2D, particleVelocityTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, particleVelocity);
		saveArrayToTXT(RigidSolver::debugDirectory + std::string("/particleValues_particleVelocities.txt"), particleVelocity, particleSideLength * particleSideLength * 3, 3);

		glBindTexture(GL_TEXTURE_2D, 0);
		
		delete[] particlePositions;
		delete[] particleVelocity;

	}
	return false;
}

bool RigidSolver::collisionGridPass(void)
{


	// --------------------------------------------------
	//  Initialization
	// --------------------------------------------------   

	glBindFramebuffer(GL_FRAMEBUFFER, gridFBO);

	// --------------------------------------------------
	//  Rendering indices
	// -------------------------------------------------- 

	glm::vec3 gridSize = grid.getGridSize();
	int sideLength = getParticleTextureSideLength();
	glm::vec3 btmLeftFrontCorner = grid.getBtmLeftFront();
	glm::vec3 topRightBackCorner = grid.getTopRightBack();
	float voxelLength = grid.getVoxelLength();
	int numberOfParticles = vaModel.getNumParticles();

	if (sideLength == 0) return false;
	float bias = 0.1f;
	float zNear = bias;
	float zFar = gridSize.z + bias;

	glm::mat4 projMatrix = glm::ortho(btmLeftFrontCorner.x, topRightBackCorner.x, btmLeftFrontCorner.y, topRightBackCorner.y, zNear, zFar);

	// Clearing
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	shaderCollisionGrid.Bind();

	// Textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, particlePositionsTex);
	glUniform1i(shaderCollisionGrid.GetUniformLocation("particlePositions"), 0);

	// Unifroms
	glUniformMatrix4fv(shaderCollisionGrid.GetUniformLocation("modelMX"), 1, GL_FALSE, glm::value_ptr(grid.getModelMatrix()));
	glUniformMatrix4fv(shaderCollisionGrid.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMatrix));

	glUniform3fv(shaderCollisionGrid.GetUniformLocation("btmLeftFrontCorner"), 1, glm::value_ptr(btmLeftFrontCorner));
	glUniform3fv(shaderCollisionGrid.GetUniformLocation("gridSize"), 1, glm::value_ptr(gridSize));

	glUniform1f(shaderCollisionGrid.GetUniformLocation("voxelLength"), voxelLength);

	glUniform1i(shaderCollisionGrid.GetUniformLocation("particlesPerModel"), vaModel.getNumParticles());
	glUniform1i(shaderCollisionGrid.GetUniformLocation("particleTextureEdgeLength"), sideLength);
	glUniform1i(shaderCollisionGrid.GetUniformLocation("rigidBodyTextureEdgeLength"), getRigidBodyTextureSizeLength());

	GLfloat bkColor[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE, bkColor);

	// Define outputs
	GLuint attachments = { GridIndiceAttachment };
	glDrawBuffers(1, &attachments);

	for (unsigned z = 0u; z < grid.getGridResolution().z; z++) {

		// Setting z Coordinate to be able to determine voxel layer
		glUniform1i(shaderCollisionGrid.GetUniformLocation("z"), z);
		glUniform1f(shaderCollisionGrid.GetUniformLocation("zCoord"), btmLeftFrontCorner.z + z * voxelLength);

		// Set the current output layer
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GridIndiceAttachment, gridTex, 0, z);
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, gridDepthTex, 0, z);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glViewport(0, 0, sideLength, sideLength);

		//=== 1 PASS
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);

		glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthFunc(GL_LESS);

		vaVertex.Bind();
		glDrawArraysInstanced(GL_POINTS, 0, 1, spawnedObjects * numberOfParticles);
		vaVertex.Release();

		//=== 2 PASS
		glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
		glDepthFunc(GL_GREATER);
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_GREATER, 1, 0xFF); // TODO: Is this right?
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
		glClear(GL_STENCIL_BUFFER_BIT);

		vaVertex.Bind();
		glDrawArraysInstanced(GL_POINTS, 0, 1, spawnedObjects * numberOfParticles);
		vaVertex.Release();

		//=== 3 PASS
		glColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE);
		glClear(GL_STENCIL_BUFFER_BIT);

		vaVertex.Bind();
		glDrawArraysInstanced(GL_POINTS, 0, 1, spawnedObjects * numberOfParticles);
		vaVertex.Release();

		//=== 4 PASS
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
		glClear(GL_STENCIL_BUFFER_BIT);

		vaVertex.Bind();
		glDrawArraysInstanced(GL_POINTS, 0, 1, spawnedObjects * numberOfParticles);
		vaVertex.Release();
	}

	vaVertex.Release();
	shaderCollisionGrid.Release();

	// --------------------------------------------------
	//  Finishing
	// --------------------------------------------------   

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthFunc(GL_LESS);
	glDisable(GL_STENCIL_TEST);
	glClearColor(bkColor[0], bkColor[1], bkColor[2], bkColor[3]);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (DEBUGGING) {
		GLuint * gridIndices;

		glm::ivec3 gridResolution = grid.getGridResolution();
		int arraySize = std::max(gridResolution.x, 16) * std::max(gridResolution.y, 256) * std::max(gridResolution.z, 256) * 4;
		gridIndices = new GLuint[arraySize];

		glBindTexture(GL_TEXTURE_2D_ARRAY, gridTex);
		glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, gridIndices);
		saveArrayToTXT(RigidSolver::debugDirectory + std::string("/collisionGrid_gridIndices.txt"), gridIndices, arraySize, 4);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

		delete[] gridIndices;
	}

	return false;
}

bool RigidSolver::collisionPass() {

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, particlesFBO);
	// glBindFramebuffer(GL_READ_FRAMEBUFFER, particlesFBO);

	shaderCollision.Bind();

	int particleTextureEdgeLength = getParticleTextureSideLength();
	// projMX = glm::ortho(0, particleTextureEdgeLength, 0, particleTextureEdgeLength);

	// Textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, gridTex);
	glUniform1i(shaderCollision.GetUniformLocation("collisionGrid"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, particlePositionsTex);
	glUniform1i(shaderCollision.GetUniformLocation("particlePositions"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, particleVelocityTex);
	glUniform1i(shaderCollision.GetUniformLocation("particleVelocities"), 2);

	if (texSwitch == false) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex1);
		glUniform1i(shaderCollision.GetUniformLocation("rigidBodyPositions"), 3);

	}
	else {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex2);
		glUniform1i(shaderCollision.GetUniformLocation("rigidBodyPositions"), 3);
	}

	// Uniforms
	glUniform1f(shaderCollision.GetUniformLocation("gravity"), gravity);
	glUniform1f(shaderCollision.GetUniformLocation("mass"), modelMass);
	glUniform1f(shaderCollision.GetUniformLocation("deltaT"), time_span.count() / 1000.f);
	glUniform1f(shaderCollision.GetUniformLocation("voxelLength"), grid.getVoxelLength());
	glUniform1f(shaderCollision.GetUniformLocation("particleDiameter"), particleSize);
	glUniform1f(shaderCollision.GetUniformLocation("dampingCoefficient"), dampingCoefficient);
	glUniform1f(shaderCollision.GetUniformLocation("sprintCoefficient"), springCoefficient);

	glUniform1i(shaderCollision.GetUniformLocation("particlesPerModel"), vaModel.getNumParticles());
	glUniform1i(shaderCollision.GetUniformLocation("particleTextureEdgeLength"), particleTextureEdgeLength);
	glUniform1i(shaderCollision.GetUniformLocation("rigidBodyTextureEdgeLength"), getRigidBodyTextureSizeLength());

	glUniform3fv(shaderCollision.GetUniformLocation("btmLeftFrontCorner"), 1, glm::value_ptr(grid.getBtmLeftFront()));

	// Outputs
	GLuint attachments[] = { ParticleForceAttachment };
	glDrawBuffers(1, attachments);

	// Draw particles - shaders tests for collisions
	vaVertex.Bind();

	glClearColor(0.f, 0.f, 0.f, 0.f);
	RigidSolver::drawAbstractData(particleTextureEdgeLength, particleTextureEdgeLength, shaderCollision);
	// glDrawArraysInstanced(GL_POINTS, 0, 1, spawnedObjects * vaModel.getNumParticles());
	vaVertex.Release();

	shaderCollision.Release();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (DEBUGGING) {

		int particleSideLength = getParticleTextureSideLength();

		float * particleForces;

		particleForces = new float[particleSideLength * particleSideLength * 3];

		glBindTexture(GL_TEXTURE_2D, particleForcesTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, particleForces);
		saveArrayToTXT(RigidSolver::debugDirectory + std::string("/collision_particleForces.txt"), particleForces, particleSideLength * particleSideLength * 3, 3);

		glBindTexture(GL_TEXTURE_2D, 0);

		delete[] particleForces;

	}
	return false;
}

bool RigidSolver::momentaPass(void)
{
	glBindFramebuffer(GL_FRAMEBUFFER, rigidBodyFBO);

	shaderMomentaCalculation.Bind();

	// Output textures
	GLuint attachments[] = { RigidBodyLinearMomentumAttachment, RigidBodyAngularMomentumAttachment };
	glDrawBuffers(2, attachments);

	int rigidBodyTextureLength = getRigidBodyTextureSizeLength();

	// Uniforms
	glUniform1i(shaderMomentaCalculation.GetUniformLocation("rigidBodyTextureEdgeLength"), rigidBodyTextureLength);
	glUniform1i(shaderMomentaCalculation.GetUniformLocation("particleTextureEdgeLength"), getParticleTextureSideLength());
	glUniform1i(shaderMomentaCalculation.GetUniformLocation("particlesPerModel"), vaModel.getNumParticles());

	// Activate the input textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, particleRelativePositionTex);
	glUniform1i(shaderMomentaCalculation.GetUniformLocation("relativeParticlePositions"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, particleForcesTex);
	glUniform1i(shaderMomentaCalculation.GetUniformLocation("particleForces"), 1);

	// Update the particles and rigid body positions
	vaVertex.Bind();
	drawAbstractData(rigidBodyTextureLength, rigidBodyTextureLength, shaderMomentaCalculation);
	vaVertex.Release();

	shaderMomentaCalculation.Release();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	if (DEBUGGING) {

		int size = rigidBodyTextureLength * rigidBodyTextureLength * 3;

		float * linearMomenta;
		float * angularMomenta;

		linearMomenta = new float[size];
		angularMomenta = new float[size];

		glBindTexture(GL_TEXTURE_2D, rigidBodyLinearMomentumTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, linearMomenta);
		saveArrayToTXT(RigidSolver::debugDirectory + std::string("/momenta_linearMomenta.txt"), linearMomenta, size, 3);

		glBindTexture(GL_TEXTURE_2D, rigidBodyAngularMomentumTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, angularMomenta);
		saveArrayToTXT(RigidSolver::debugDirectory + std::string("/momenta_angularMomenta.txt"), angularMomenta, size, 3);

		glBindTexture(GL_TEXTURE_2D, 0);

		delete[] linearMomenta;
		delete[] angularMomenta;

	}

	return false;
}

bool RigidSolver::solverPass(void)
{

	glBindFramebuffer(GL_FRAMEBUFFER, rigidBodyFBO);

	shaderSolver.Bind();

	GLuint attachments[2];

	if (texSwitch == false) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex1);
		glUniform1i(shaderSolver.GetUniformLocation("rigidBodyPositions"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex1);
		glUniform1i(shaderSolver.GetUniformLocation("rigidBodyQuaternions"), 1);

		attachments[0] = RigidBodyPositionAttachment2;
		attachments[1] = RigidBodyQuaternionAttachment2;

	}
	else {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex2);
		glUniform1i(shaderSolver.GetUniformLocation("rigidBodyPositions"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex2);
		glUniform1i(shaderSolver.GetUniformLocation("rigidBodyQuaternions"), 1);

		attachments[0] = RigidBodyPositionAttachment1;
		attachments[1] = RigidBodyQuaternionAttachment1;

	}

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, rigidBodyLinearMomentumTex);
	glUniform1i(shaderSolver.GetUniformLocation("rigidBodyLinearMomentums"), 2);


	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, rigidBodyAngularMomentumTex);
	glUniform1i(shaderSolver.GetUniformLocation("rigidBodyAngularMomentums"), 2);

	// Uniforms
	glUniform1i(shaderSolver.GetUniformLocation("rigidBodyTextureEdgeLength"), getRigidBodyTextureSizeLength());
	glUniform1i(shaderSolver.GetUniformLocation("particleTextureEdgeLength"), getParticleTextureSideLength());
	glUniform1i(shaderSolver.GetUniformLocation("particlesPerModel"), vaModel.getNumParticles());

	glUniform1f(shaderSolver.GetUniformLocation("mass"), modelMass);
	glUniform1f(shaderSolver.GetUniformLocation("deltaT"), time_span.count() / 1000.f); // FIXME: Is this really right?

	glUniformMatrix3fv(shaderSolver.GetUniformLocation("invInertiaTensor"), 1, false, glm::value_ptr(glm::inverse(vaModel.getInertiaTensor())));

	// Output textures
	glDrawBuffers(2, attachments);

	// Update the particles and rigid body positions
	vaVertex.Bind();
	int rigidBodyTextureLength = getRigidBodyTextureSizeLength();
	drawAbstractData(rigidBodyTextureLength, rigidBodyTextureLength, shaderSolver);
	vaVertex.Release();

	shaderSolver.Release();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	if (DEBUGGING) {

		int size = rigidBodyTextureLength * rigidBodyTextureLength;

		float * rigidPositions;
		float * rigidQuaternions;

		rigidPositions = new float[size * 3];
		rigidQuaternions = new float[size * 4];

		if (texSwitch == false) glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex2);
		else glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex1);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, rigidPositions);
		saveArrayToTXT(RigidSolver::debugDirectory + std::string("/solver_rigidBodyPositions.txt"), rigidPositions, size * 3, 3);

		if (texSwitch == false) glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex2);
		else glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex1);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, rigidQuaternions);
		saveArrayToTXT(RigidSolver::debugDirectory + std::string("/solver_rigidBodyQuaternions.txt"), rigidQuaternions, size * 4, 4);

		glBindTexture(GL_TEXTURE_2D, 0);

		delete[] rigidPositions;
		delete[] rigidQuaternions;

	}
	return false;
}


bool RigidSolver::beautyPass(void) {

	// Set up the proj Matrices
	projMX = glm::perspective(static_cast<float>(fovY), aspectRatio, 0.001f, 100.f);

	// Clearing
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Viewporting to full view
	glViewport(0, 0, windowWidth, windowHeight);

	// --------------------------------------------------
	//  Draw ground planes and models
	// --------------------------------------------------

	if (drawParticles) {
		// Turn on wireframe mode
		glPolygonMode(GL_FRONT, GL_LINE);
		glPolygonMode(GL_BACK, GL_LINE);
	}

	shaderBeauty.Bind();

	// Shader Variables
	// View transformation as well as model MX of solver system
	glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyTextureEdgeLength"), getRigidBodyTextureSizeLength());

	glUniformMatrix4fv(shaderBeauty.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMX));
	glUniformMatrix4fv(shaderBeauty.GetUniformLocation("viewMX"), 1, GL_FALSE, glm::value_ptr(viewMX));
	glUniformMatrix4fv(shaderBeauty.GetUniformLocation("modelMX"), 1, GL_FALSE, glm::value_ptr(grid.getModelMatrix()));
	glUniformMatrix4fv(shaderBeauty.GetUniformLocation("invModelViewMX"), 1, GL_FALSE, glm::value_ptr(glm::inverse(viewMX * grid.getModelMatrix())));

	glUniform3fv(shaderBeauty.GetUniformLocation("lightDirection"), 1, glm::value_ptr(glm::vec3(0.f, -1.f, 0.f)));

	glUniform3fv(shaderBeauty.GetUniformLocation("ambient"), 1, glm::value_ptr(glm::vec3(1.f, 1.f, 1.f)));
	glUniform3fv(shaderBeauty.GetUniformLocation("diffuse"), 1, glm::value_ptr(glm::vec3(.8f, .8f, .8f)));
	glUniform3fv(shaderBeauty.GetUniformLocation("specular"), 1, glm::value_ptr(glm::vec3(1.f, 1.f, 1.f)));

	glUniform1f(shaderBeauty.GetUniformLocation("k_amb"), 0.2f);
	glUniform1f(shaderBeauty.GetUniformLocation("k_diff"), 0.8f);
	glUniform1f(shaderBeauty.GetUniformLocation("k_spec"), 1.0f);
	glUniform1f(shaderBeauty.GetUniformLocation("k_exp"), 2.0f);

	// Activate textures
	glActiveTexture(GL_TEXTURE0);
	if (texSwitch == false) glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex1);
	else glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex2);
	glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyPositions"), 0);

	glActiveTexture(GL_TEXTURE1);
	if (texSwitch == false) glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex1);
	else glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex2);
	glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyQuaternions"), 1);

	// Draw ground plane
	glUniform1i(shaderBeauty.GetUniformLocation("positionByTexture"), 0);

	vaPlane.Bind();
	glDrawElements(GL_TRIANGLES, 2 * 3, GL_UNSIGNED_INT, nullptr);
	vaPlane.Release();

	// Instanced drawing of rigid bodies
	if (modelFiles.GetValue() != NULL){

		// Change mode
		glUniform1i(shaderBeauty.GetUniformLocation("positionByTexture"), 1);

		// Instanced drawing of the rigid bodies
		vaModel.Bind();
		glDrawElementsInstanced(GL_TRIANGLES, vaModel.GetNumVertices() * 3, GL_UNSIGNED_INT, 0, spawnedObjects);

		// Debugging mode just draws the model at its init position
		if (DEBUGGING) {
			glUniform1i(shaderBeauty.GetUniformLocation("positionByTexture"), 0);
			glDrawElements(GL_TRIANGLES, vaModel.GetNumVertices() * 3, GL_UNSIGNED_INT, 0);
		}

		vaModel.Release();
	}

	shaderBeauty.Release();

	if (drawParticles) {
		// Turn off wireframe mode
		glPolygonMode(GL_FRONT, GL_FILL);
		glPolygonMode(GL_BACK, GL_FILL);

		// TODO: Draw particles
	}

	return false;
}


// --------------------------------------------------
//  UPDATES
// --------------------------------------------------   


/**
* @brief Inits the solver FBO with the needed textures
*/
bool RigidSolver::initSolverFBOs() {

	bool result = true;

	// Init the FBOs
	result = result && initRigidFBO();
	result = result && initParticleFBO();
	result = result && initGridFBO();

	return result;

}

bool RigidSolver::initRigidFBO(void)
{
	
	if (glIsFramebuffer(rigidBodyFBO)) glDeleteFramebuffers(1, &rigidBodyFBO);

	glGenFramebuffers(1, &rigidBodyFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, rigidBodyFBO);

	updateRigidBodies();

	// Finally Check FBO status
	bool result = checkFBOStatus(std::string("RigidFBO"));

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return result;
}

bool RigidSolver::initParticleFBO(void)
{
	if (glIsFramebuffer(particlesFBO)) glDeleteFramebuffers(1, &particlesFBO);

	glGenFramebuffers(1, &particlesFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, particlesFBO);

	updateParticles();

	// Finally Check FBO status
	bool result = checkFBOStatus(std::string("ParticleFBO"));

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

 	return result;
}

bool RigidSolver::initGridFBO(void)
{
	if (glIsFramebuffer(gridFBO)) glDeleteFramebuffers(1, &gridFBO);

	glGenFramebuffers(1, &gridFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, gridFBO);

	updateGrid();

	// Finally Check FBO status
	bool result = checkFBOStatus(std::string("GridFBO"));

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return result;
}

bool RigidSolver::resetSimulation(void)
{
	solverStatus = true;
	spawnedObjects = 0;
	lastSpawn = std::chrono::high_resolution_clock::now();

	initSolverFBOs();

	return true;
}

bool RigidSolver::stopSimulation(void)
{
	solverStatus = false;
	return true;
}

bool RigidSolver::continueSimulation(void)
{
	solverStatus = true;
	return true;
}

bool RigidSolver::reloadShaders(void)
{
	// FIXME: Creates error on reload trigger
	shaderBeauty.CreateProgramFromFile(beautyVertShaderName.c_str(), beautyFragShaderName.c_str());
	shaderMomentaCalculation.CreateProgramFromFile(momentaVertShaderName.c_str(), momentaFragShaderName.c_str());
	shaderParticleValues.CreateProgramFromFile(particleValuesVertShaderName.c_str(), particleValuesFragShaderName.c_str());
	shaderCollision.CreateProgramFromFile(collisionVertShaderName.c_str(), collisionFragShaderName.c_str());
	shaderCollisionGrid.CreateProgramFromFile(collisionGridVertShaderName.c_str(), collisionGridFragShaderName.c_str());
	shaderSolver.CreateProgramFromFile(solverVertShaderName.c_str(), solverFragShaderName.c_str());
	
	// The shaders which are used for depth peeling - for code development only
	vaModel.reloadShaders();

	return true;
}

bool RigidSolver::updateParticles() {
	
	// Texture must be numRegidBodies * numParticlesPerRigidBody long
	int particleTexEdgeLength = getParticleTextureSideLength();

	// Catch uninitialized number of bodies
	if (particleTexEdgeLength <= 0) return false;

	createFBOTexture(particlePositionsTex, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, particleTexEdgeLength, particleTexEdgeLength, NULL);
	createFBOTexture(particleVelocityTex, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, particleTexEdgeLength, particleTexEdgeLength, NULL);
	createFBOTexture(particleForcesTex, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, particleTexEdgeLength, particleTexEdgeLength, NULL);
	createFBOTexture(particleRelativePositionTex, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, particleTexEdgeLength, particleTexEdgeLength, NULL);

	// Create the initial particle position tex as 1D tex
	glGenTextures(1, &initialParticlePositionsTex);
	glBindTexture(GL_TEXTURE_1D, initialParticlePositionsTex);

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Init empty image (to currently bound FBO)
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB32F, std::max(vaModel.getNumParticles() * 3, 1024), 0, GL_RGB, GL_FLOAT, vaModel.getParticlePositions());
	glBindTexture(GL_TEXTURE_1D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, ParticlePositionAttachment, GL_TEXTURE_2D, particlePositionsTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, ParticleVelocityAttachment, GL_TEXTURE_2D, particleVelocityTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, ParticleForceAttachment, GL_TEXTURE_2D, particleForcesTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, ParticleRelativePositionAttachment, GL_TEXTURE_2D, particleRelativePositionTex, 0);

	glFramebufferTexture1D(GL_FRAMEBUFFER, initialParticlePositionsAttachment, GL_TEXTURE_1D, initialParticlePositionsTex, 0);

	return true;
}

bool RigidSolver::updateGrid() {
	// Must be done on init or when voxel size changes

	glm::ivec3 gridDimensions = grid.getGridResolution();


	// --------------------------------------------------
	//  Depth/ Stencil Texture - Needed for the collision grid indice assignment
	// --------------------------------------------------   

	glGenTextures(1, &gridDepthTex);
	glBindTexture(GL_TEXTURE_2D_ARRAY, gridDepthTex);

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Creating grid - specifing with the minium size of (16, 256, 256): Overhead is just not used
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH32F_STENCIL8, std::max(gridDimensions.x, 16), std::max(gridDimensions.y, 256), std::max(gridDimensions.z, 256), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, gridDepthTex, 0);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	// --------------------------------------------------
	//  Grid Texture
	// --------------------------------------------------   

	// Assuming diameter particle = voxel edge length -> max 4x particles per voxel
	glGenTextures(1, &gridTex);
	glBindTexture(GL_TEXTURE_2D_ARRAY, gridTex);

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Creating grid - specifing with the minium size of (16, 256, 256): Overhead is just not used
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA16UI, std::max(gridDimensions.x, 16), std::max(gridDimensions.y, 256), std::max(gridDimensions.z, 256), 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
	
	glFramebufferTexture(GL_FRAMEBUFFER, GridIndiceAttachment, gridTex, 0);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	return false;
}

bool RigidSolver::updateRigidBodies(void)
{

	// --------------------------------------------------
	//  Rigid Body Textures
	// --------------------------------------------------   

	glm::vec3 position = grid.getEmitterPosition();
	glm::vec3 initialVelocity = grid.getEmitterVelocity();
	glm::quat unitQuaternion = glm::quat(1.f, 0.f, 0.f, 0.f);

	//  Calculating back the square count
	int rigidTexEdgeLength = getRigidBodyTextureSizeLength();

	createFBOTexture(rigidBodyPositionsTex1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyPositionsTex2, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyQuaternionsTex1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyQuaternionsTex2, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyLinearMomentumTex, GL_RGBA32F, GL_RGB, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyAngularMomentumTex, GL_RGBA32F, GL_RGB, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyPositionAttachment1, GL_TEXTURE_2D, rigidBodyPositionsTex1, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyPositionAttachment2, GL_TEXTURE_2D, rigidBodyPositionsTex2, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyQuaternionAttachment1, GL_TEXTURE_2D, rigidBodyQuaternionsTex1, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyQuaternionAttachment2, GL_TEXTURE_2D, rigidBodyQuaternionsTex2, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyLinearMomentumAttachment, GL_TEXTURE_2D, rigidBodyLinearMomentumTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyAngularMomentumAttachment, GL_TEXTURE_2D, rigidBodyAngularMomentumTex, 0);

	// Clearing the values
	float positionClear[4];
	positionClear[0] = position.x;
	positionClear[1] = position.y;
	positionClear[2] = position.z;
	positionClear[3] = 1.0f;

	float quaternionClear[4];
	quaternionClear[0] = unitQuaternion.x;
	quaternionClear[1] = unitQuaternion.y;
	quaternionClear[2] = unitQuaternion.z;
	quaternionClear[3] = unitQuaternion.w;

	float zeroClear[3] = { 0.f, 0.f, 0.f };

	glDrawBuffer(RigidBodyPositionAttachment1);
	glClearBufferfv(GL_COLOR, 0, positionClear);

	glDrawBuffer(RigidBodyPositionAttachment2);
	glClearBufferfv(GL_COLOR, 0, positionClear);

	glDrawBuffer(RigidBodyQuaternionAttachment1);
	glClearBufferfv(GL_COLOR, 0, quaternionClear);

	glDrawBuffer(RigidBodyQuaternionAttachment2);
	glClearBufferfv(GL_COLOR, 0, quaternionClear);

	glDrawBuffer(RigidBodyLinearMomentumAttachment);
	glClearBufferfv(GL_COLOR, 0, glm::value_ptr(initialVelocity));

	glDrawBuffer(RigidBodyAngularMomentumAttachment);
	glClearBufferfv(GL_COLOR, 0, zeroClear);

	return true;
}

// --------------------------------------------------
//  TRIGGERS
// --------------------------------------------------   

/**
* @brief Callback function which is invoked when the file dropdown list changed
*/
void RigidSolver::fileChanged(FileEnumVar<RigidSolver> &var) {

	if (!var.GetValue()) return;

	std::string fileName = var.GetSelectedFileName();

	objl::Loader loader;
	if (loader.LoadFile(fileName)) {

		if (loader.LoadedMeshes.size() >= 1) {

			float * vertices;
			int * indices;
			float * texCoords;
			float * normals;

			// Only supports a single mesh for now
			objl::Mesh mesh = loader.LoadedMeshes[0];

			int numVertices = mesh.Vertices.size();
			int numIndices = mesh.Indices.size();

			vertices = new float[numVertices * 4];
			texCoords = new float[numVertices * 2];
			normals = new float[numVertices * 3];
			indices = new int[numIndices];

			for (int i = 0; i < numVertices; i++) {

				vertices[i * 4] = mesh.Vertices[i].Position.X;
				vertices[i * 4 + 1] = mesh.Vertices[i].Position.Y;
				vertices[i * 4 + 2] = mesh.Vertices[i].Position.Z;
				vertices[i * 4 + 3] = 1.f;

				normals[i * 3] = mesh.Vertices[i].Normal.X;
				normals[i * 3 + 1] = mesh.Vertices[i].Normal.Y;
				normals[i * 3 + 2] = mesh.Vertices[i].Normal.Z;

				texCoords[i * 2] = mesh.Vertices[i].TextureCoordinate.X;
				texCoords[i * 2 + 1] = mesh.Vertices[i].TextureCoordinate.Y;

			}

			for (unsigned int j = 0; j < mesh.Indices.size(); j += 3){
				indices[j] = mesh.Indices[j];
				indices[j + 1] = mesh.Indices[j + 1];
				indices[j + 2] = mesh.Indices[j + 2];
			}
			
			// TODO: Scale the model here
			loadModel(vertices, indices, texCoords, normals, numVertices);


			// Write out the result of the initialParticlePositions
			if (DEBUGGING) {
				int size = std::max(vaModel.getNumParticles() * 3, 1024);

				// Write out particle positions directly from array
				saveArrayToTXT(RigidSolver::debugDirectory + std::string("/relativeParticlePositions.txt"), vaModel.getParticlePositions(), size, 3);

				// Write out particle position from texture
				/*
				float * data;
				int width, height;

				data = new float[size];
				glBindTexture(GL_TEXTURE_1D, initialParticlePositionsTex);
				glGetTexImage(GL_TEXTURE_1D, 0, GL_RGB, GL_FLOAT, data);
				glBindTexture(GL_TEXTURE_1D, 0);
				saveArrayToTXT(RigidSolver::debugDirectory + std::string("/textureRelativeParticlePositions.txt"), data, size, 3);

				delete[] data;
				*/

			}
			delete[] vertices;
			delete[] normals;
			delete[] texCoords;
			delete[] indices;

		}
	}
}

void RigidSolver::particleSizeChanged(APIVar<RigidSolver, FloatVarPolicy> &var)
{
	
}


// --------------------------------------------------
//  HELPERS
// --------------------------------------------------   

void RigidSolver::createFBOTexture(GLuint &outID, const GLenum internalFormat, const GLenum format, const GLenum type, GLint filter, int width, int height, void * data) {

	glGenTextures(1, &outID);
	glBindTexture(GL_TEXTURE_2D, outID);

	// Check
	if (width > GL_MAX_FRAMEBUFFER_WIDTH) std::cout << "Unable to create texture: Width exceeds GL_MAX_FRAMEBUFFER_WIDTH\n";
	if (height > GL_MAX_FRAMEBUFFER_HEIGHT) std::cout << "Unable to create texture: Height exceeds GL_MAX_FRAMEBUFFER_HEIGHT\n";

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	// Init empty image (to currently bound FBO)
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data);

	glBindTexture(GL_TEXTURE_2D, 0);
}

bool RigidSolver::checkFBOStatus(std::string fboName) {
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	bool r = false;

	switch (status) {
	case GL_FRAMEBUFFER_UNDEFINED: {
		fprintf(stderr, "FBO '%s': undefined.\n", fboName.c_str());
		break;
	}
	case GL_FRAMEBUFFER_COMPLETE: {
		fprintf(stderr, "FBO '%s': complete.\n", fboName.c_str());
		r = true;
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: {
		fprintf(stderr, "FBO '%s': incomplete attachment.\n", fboName.c_str());
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: {
		fprintf(stderr, "FBO '%s': no buffers are attached to the FBO.\n", fboName.c_str());
		break;
	}
	case GL_FRAMEBUFFER_UNSUPPORTED: {
		fprintf(stderr, "FBO '%s': combination of internal buffer formats is not supported.\n", fboName.c_str());
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: {
		fprintf(stderr, "FBO '%s': number of samples or the value for ... does not match.\n", fboName.c_str());
		break;
	}

	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: {
		fprintf(stderr, "FBO '%s': Incomplete Layer targets.\n", fboName.c_str());
		break;
	}
	}

	return r;
}

int RigidSolver::getRigidBodyTextureSizeLength(void)
{
	return int(std::floor(std::sqrt(MAX_NUMBER_OF_RIGID_BODIES)));
}

int RigidSolver::getParticleTextureSideLength(void)
{
	return std::ceil(std::sqrt(getRigidBodyTextureSizeLength() * std::max(vaModel.getNumParticles(), 0)));
}

bool RigidSolver::saveFramebufferPNG(const std::string filename, GLuint texture, int width, int height, GLenum format, GLenum type)
{
	int channels;
	if (format == GL_RED || format == GL_BLUE || format == GL_GREEN || format == GL_ALPHA) channels = 1;
	else if (format == GL_RGB || format == GL_BGR) channels = 3;
	else if (format == GL_RGBA || format == GL_DEPTH_COMPONENT) channels = 4;
	else return false;

	// get the image data
	long imageSize = width * height * channels;

	unsigned char *data = new unsigned char[imageSize];

	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	// glReadPixels(0, 0, width, height, format, type, data);
	glGetTexImage(GL_TEXTURE_2D, 0, format, type, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Save to PNG
	std::vector<unsigned char> PngBuffer(imageSize);
	PngBuffer.assign(data, data + imageSize);

	// Change the settings
	lodepng::State state;

	// Input Settings
	state.info_raw.bitdepth = 8;
	state.info_raw.colortype = LCT_RGBA;

	if (format == GL_RED || format == GL_BLUE || format == GL_GREEN 
		|| format == GL_ALPHA) state.info_raw.colortype = LCT_GREY;
	else if (format == GL_RGB || format == GL_BGR) state.info_raw.colortype = LCT_RGB;

	// Output settings (Always 8 bit)
	state.info_png.color.colortype = LCT_RGBA;
	state.info_png.color.bitdepth = 8;

	// TODO: Does depth always assume to be 32bit
	if (channels == 1) state.info_png.color.colortype = LCT_GREY;
	else if (channels == 3) state.info_png.color.colortype = LCT_RGB;

	// Encode to 8bit
	std::vector<unsigned char> image(imageSize);
	// unsigned error = lodepng::encode(filename, image, width, height);
	// unsigned error = lodepng::encode(image, PngBuffer, width, height);
	unsigned error = lodepng::encode(image, PngBuffer, width, height, state);
	if (!error) lodepng::save_file(image, filename);
	else std::cout << "Error: " << lodepng_error_text(error) << ", while encoding PNG image!\n";

	if (data) {
		delete[] data;
		data = NULL;
	}

	if (error) return false;
	else return true;
}

bool RigidSolver::saveDepthTexturePNG(std::string filename, GLuint texture, int width, int height)
{

	// get the image data
	long imageSize = width * height;
	GLfloat *data = new GLfloat[imageSize];

	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Save to PNG
	std::vector<unsigned char> PngBuffer(imageSize);
	
	for (unsigned int w = 0u; w < width; w++){
		for (unsigned int h = 0u; h < height; h++)
		{
			PngBuffer.push_back((unsigned int)(data[h * width + w] * 255));
		}
	}

	// Encode to 8bit
	std::vector<unsigned char> image(imageSize);
	unsigned error = lodepng::encode(image, PngBuffer, width, height);
	if (!error) lodepng::save_file(image, filename);
	else std::cout << "Error: " << lodepng_error_text(error) << ", while encoding PNG image!\n";

	if (data) {
		delete[] data;
		data = NULL;
	}

	if (error) return false;
	else return true;
}

bool RigidSolver::saveTextureToBMP(std::string filenname, GLuint texture, int width, int height, int channels, GLenum format, GLenum type)
{

	std::string outputFilePath = RigidSolver::debugDirectory + filenname + std::string(".bmp");

	unsigned char * outputData;
	outputData = new unsigned char[width * height * channels];

	GLuint * bufferData;
	bufferData = new GLuint[width * height * channels];

	glBindTexture(GL_TEXTURE_2D, texture);
	glGetTexImage(GL_TEXTURE_2D, 0, format, type, bufferData);
	glBindTexture(GL_TEXTURE_2D, 0);

	float ubitsize = 255.f;
	if (type == GL_UNSIGNED_INT) ubitsize = std::pow(2, 32);
	
	for (unsigned int i = 0; i < width * height; i++) {
		int r8 = int(bufferData[i] / ubitsize * 255.f);
		outputData[i] = (unsigned char)r8;
	}
	int save_result = SOIL_save_image
	(
		outputFilePath.c_str(),
		SOIL_SAVE_TYPE_BMP,
		width, height, channels,
		outputData
	);

	delete[] bufferData;
	delete[] outputData;

	if (save_result == 0) return false;
	else return true;
	
}

/** \brief Draws data into a texture using a window filling quad
* The shader must be bound in the surrounding code!!

This would look something like:

-----------------------------
shaderData.Bind();

glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, textures[0]);
glUniform1i(shaderData.GetUniformLocation("tex"), 0);

GLuint attachments[] = { GL_COLOR_ATTACHMENT0 };
glDrawBuffers(1, attachments);
drawAbstractData(width, height, shaderData);
shaderData.Release();
-----------------------------


Should be used with the following VS code:

-----------------------------------------------
#version 330

layout(location = 0) in vec2  in_position;

uniform mat4 projMX;

void main() {
	gl_Position = projMX * vec4(in_position, 0, 1);
}

-----------------------------


And FR shader:

-----------------------------

#version 330

layout(pixel_center_integer) in vec4 gl_FragCoord;

uniform sampler2D tex;

void main() {
	gl_FragColor = vec4(texelFetch(tex, ivec2(gl_FragCoord.xy), 0).x, 0.f, 0.f, 1.f);
}
-----------------------------

*
*/
void RigidSolver::drawAbstractData(unsigned int width, unsigned int height, GLShader &shader) {

	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 orthoMX = glm::ortho(0.0f, 1.f, 0.0f, 1.f);

	glUniform1i(shader.GetUniformLocation("width"), width);
	glUniform1i(shader.GetUniformLocation("height"), height);

	glUniformMatrix4fv(shader.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(orthoMX));

	vaQuad.Bind();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	vaQuad.Release();

}