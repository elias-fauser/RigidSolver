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

// --------------------------------------------------
//  Ground plane
// --------------------------------------------------

const float PLANE_VERTICES[] = {
	 .5f, 0.0f,  .5f, 1.0f,
	 .5f, 0.0f, -.5f, 1.0f,
	-.5f, 0.0f, -.5f, 1.0f,
	-.5f, 0.0f,  .5f, 1.0f,
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

// --------------------------------------------------
//  Constants
// --------------------------------------------------

const int MAX_NUMBER_OF_RIGID_BODIES = 64 * 64; // 4096
const int MAX_NUMBER_OF_PARTICLES = MAX_NUMBER_OF_RIGID_BODIES * 4; // Assuming voxel size == particle diameter -> so max of 4 particles

// FBO attachments
enum ColorAttachments {
	RigidBodyPositionAttachment1 = GL_COLOR_ATTACHMENT0,
	RigidBodyPositionAttachment2 = GL_COLOR_ATTACHMENT1,
	RigidBodyQuaternionAttachment1 = GL_COLOR_ATTACHMENT2,
	RigidBodyQuaternionAttachment2 = GL_COLOR_ATTACHMENT3,
	RigidBodyLinearMomentumAttachment1 = GL_COLOR_ATTACHMENT4,
	RigidBodyLinearMomentumAttachment2 = GL_COLOR_ATTACHMENT5,
	RigidBodyAngularMomentumAttachment1 = GL_COLOR_ATTACHMENT6,
	RigidBodyAngularMomentumAttachment2 = GL_COLOR_ATTACHMENT7,
	ParticlePositionAttachment = GL_COLOR_ATTACHMENT8,
	ParticleVelocityAttachment = GL_COLOR_ATTACHMENT9,
	GridIndiceAttachment = GL_COLOR_ATTACHMENT10
};

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

	gravity.Set(this, "Gravity");
	gravity.Register();
	gravity = 9.807f; // m/s^2

	modelMass.Set(this, "Mass");
	modelMass.Register();
	modelMass.SetMinMax(0.1, 100.0);
	modelMass = 1.0f;
	
	numRigidBodies.Set(this, "NumRigidBodies");
	numRigidBodies.Register();
	numRigidBodies.SetMinMax(1.0, MAX_NUMBER_OF_RIGID_BODIES);
	numRigidBodies = 100;

	// All about particles
	particleSize.Set(this, "ParticleSize", &RigidSolver::particleSizeChanged);
	particleSize.Register();
	particleSize.SetMinMax(0.5, 2.0);

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
	
	vaPlane.Create(4);
	vaPlane.SetArrayBuffer(0, GL_FLOAT, 4, PLANE_VERTICES);
	vaPlane.SetElementBuffer(0, 2 * 3, PLANE_INDICES);
	vaPlane.SetArrayBuffer(1, GL_FLOAT, 2, PLANE_TEXCOORDS);
	vaPlane.SetArrayBuffer(2, GL_FLOAT, 3, PLANE_NORMALS);

	reloadShaders();

	// --------------------------------------------------
	//  Init
	// --------------------------------------------------  

	bool status = initSolverFBO();
	// initRigidDataStructures();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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

    return status;
}

bool RigidSolver::Deactivate(void) {
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
		time = std::time(0);
		if (time - lastSpawn >= spawnTime && spawnedObjects <= numRigidBodies) {
			spawnedObjects = std::max((int)(spawnedObjects + 1), MAX_NUMBER_OF_RIGID_BODIES);
		}
		lastSpawn = time;
	}

	// --------------------------------------------------
	//  Passes
	// --------------------------------------------------  

	if (solverStatus && modelFiles.GetValue() != NULL) {
		// All the calculation steps are done on the solverFBO
		glBindFramebuffer(GL_FRAMEBUFFER, solverFBO);

		// Setting to orthographic since the calculations are view independent
		// The ortho is used for depth peeling and grid lookup
		// QUESTION: Do those coordinates need to be the world coords of the grid?
		projMX = glm::ortho(0.f, 1.f, 0.f, 1.f);

		// Physical values - Determine rigid positions and particle attributes
		particleValuePass();

		// Generate Lookup grid - Assign the particles to the voxels
		collisionGridPass();

		// Collision - Find collision and calculate forces
		collisionPass();

		// Particle positions - Determine the momenta and quaternions
		solverPass();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
	float Ixx, Iyy, Izz;
	float Ixy, Ixz, Iyz;
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

	// Reset simulation to restart everything
	resetSimulation();

	delete[] movedVertices;

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
	shaderParticleValues.Bind();

	// Bind for reading and writing
	if (texSwitch == false) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex1);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyPositions"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex1);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyQuaternions"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, rigidBodyLinearMomentum1);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyLinearMomentums"), 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, rigidBodyAngularMomentum1);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyAngularMomentums"), 3);

	}
	else {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex2);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyPositions"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex2);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyQuaternions"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, rigidBodyLinearMomentum2);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyLinearMomentums"), 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, rigidBodyAngularMomentum2);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyAngularMomentums"), 3);

	}

	int sideLength = getParticleTextureSideLength();
	projMX = glm::ortho(0, sideLength, 0, sideLength);

	// Clearing
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Viewporting to output texture size
	glViewport(0, 0, sideLength, sideLength);

	// Define the two outputs
	GLuint attachments[] = { ParticlePositionAttachment, ParticleVelocityAttachment };
	glDrawBuffers(2, attachments);

	// Uniforms
	glUniformMatrix4fv(shaderParticleValues.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMX));
	glUniformMatrix3fv(shaderParticleValues.GetUniformLocation("invInertiaTensor"), 1, false, glm::value_ptr(glm::inverse(vaModel.getInertiaTensor())));
	glUniform1i(shaderParticleValues.GetUniformLocation("particlesPerModel"), vaModel.getNumParticles());
	glUniform1i(shaderParticleValues.GetUniformLocation("particleTextureEdgeLength"), sideLength);
	glUniform1i(shaderParticleValues.GetUniformLocation("rigidBodyTextureEdgeLength"), getRigidBodyTextureSizeLength());
	glUniform1f(shaderParticleValues.GetUniformLocation("mass"), modelMass);

	glPatchParameteri(GL_PATCH_VERTICES, 1);
	glDrawArraysInstanced(GL_PATCHES, 0, 1, spawnedObjects * vaModel.getNumParticles());

	shaderParticleValues.Release();

	return false;
}

bool RigidSolver::collisionPass() {
	// Use particle VA
	// Shader calculates grid positions
	// Tests for collisions
	// Calculates forces and writes them back to force texture

	return false;
}

bool RigidSolver::solverPass(void)
{
	// Uses particle VA for current position
	// Calculates inertia tensor
	// Determines

	shaderMomentaCalculation.Bind();

	glUniform1f(shaderMomentaCalculation.GetUniformLocation("mass"), modelMass);

	// Output textures
	GLuint attachments[] = { RigidBodyPositionAttachment1, RigidBodyQuaternionAttachment1 };
	glDrawBuffers(2, attachments);

	// Activate the input textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, particleRelativePositionsTex);
	glUniform1i(shaderBeauty.GetUniformLocation("particlePositions"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, particleVelocityTex);
	glUniform1i(shaderBeauty.GetUniformLocation("particleVelocities"), 1);

	// Update the particles and rigid body positions
	glPatchParameteri(GL_PATCH_VERTICES, 1);
	glDrawArraysInstanced(GL_PATCHES, 0, 1, spawnedObjects * vaModel.getNumParticles());

	shaderMomentaCalculation.Release();
	return false;
}

bool RigidSolver::collisionGridPass(void)
{
	// Initialize the grid texture to zeros
	glFramebufferTexture3D(GL_DRAW_FRAMEBUFFER, GridIndiceAttachment, GL_TEXTURE_3D, gridTex, 0, 0);
	glDrawBuffer(GridIndiceAttachment);
	GLuint clearColor[] = { 0, 0, 0, 0 };
	glClearBufferuiv(GL_COLOR, 0, clearColor);

	// Render pass
	vaModel.createParticles(&grid);

	std::cout << "Model particles created. " << vaModel.getNumParticles() << "per rigid model determined!\n";

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
	glUniformMatrix4fv(shaderBeauty.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMX));
	glUniformMatrix4fv(shaderBeauty.GetUniformLocation("viewMX"), 1, GL_FALSE, glm::value_ptr(viewMX));

	glUniformMatrix4fv(shaderBeauty.GetUniformLocation("modelMX"), 1, GL_FALSE, glm::value_ptr(grid.getModelMatrix()));
	glUniformMatrix4fv(shaderBeauty.GetUniformLocation("invModelViewMX"), 1, GL_FALSE, glm::value_ptr(glm::inverse(viewMX * grid.getModelMatrix())));

	glUniform3fv(shaderBeauty.GetUniformLocation("lightDirection"), 1, glm::value_ptr(glm::vec3(.2f, -1.f, -.2f)));

	glUniform3fv(shaderBeauty.GetUniformLocation("ambientColor"), 1, glm::value_ptr(glm::vec3(1.f, 1.f, 1.f)));
	glUniform3fv(shaderBeauty.GetUniformLocation("diffuseColor"), 1, glm::value_ptr(glm::vec3(.8f, .8f, .8f)));
	glUniform3fv(shaderBeauty.GetUniformLocation("specularColor"), 1, glm::value_ptr(glm::vec3(1.f, 1.f, 1.f)));
	glUniform1f(shaderBeauty.GetUniformLocation("k_amb"), 0.2f);
	glUniform1f(shaderBeauty.GetUniformLocation("k_diff"), 0.8f);
	glUniform1f(shaderBeauty.GetUniformLocation("k_spec"), 1.0f);
	glUniform1f(shaderBeauty.GetUniformLocation("k_exp"), 2.0f);

	// Draw ground plane
	glUniform1i(shaderBeauty.GetUniformLocation("positionByTexture"), 0);

	vaPlane.Bind();
	glDrawElements(GL_TRIANGLES, 2 * 3, GL_UNSIGNED_INT, nullptr);
	vaPlane.Release();

	// Instanced drawing of rigid bodies
	if (modelFiles.GetValue() != NULL){

		// Change mode
		glUniform1i(shaderBeauty.GetUniformLocation("positionByTexture"), 1);

		// Activate textures
		glActiveTexture(GL_TEXTURE0);
		if (texSwitch == false) glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex1);
		else glBindTexture(GL_TEXTURE_2D, rigidBodyPositionsTex2);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyPositions"), 0);
		
		glActiveTexture(GL_TEXTURE1);
		if (texSwitch == false) glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex1);
		else glBindTexture(GL_TEXTURE_2D, rigidBodyQuaternionsTex2);
		glUniform1i(shaderBeauty.GetUniformLocation("rigidBodyQuaternions"), 1);

		vaModel.Bind();
		// Instanced drawing of the rigid bodies
		// glDrawElements(GL_TRIANGLES, vaModel.GetNumVertices() * 3, GL_UNSIGNED_INT, 0);
		glDrawElementsInstanced(GL_TRIANGLES, vaModel.GetNumVertices() * 3, GL_UNSIGNED_INT, 0, spawnedObjects);
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
bool RigidSolver::initSolverFBO() {

	glGenFramebuffers(1, &solverFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, solverFBO);


	// --------------------------------------------------
	//  Rigid Body Textures
	// --------------------------------------------------   

	glm::vec3 position = grid.getEmitterPosition();
	glm::quat quaternion = glm::quat(1.f, grid.getEmitterVelocity());

	//  Calculating back the square count
	int rigidTexEdgeLength = getRigidBodyTextureSizeLength();

	createFBOTexture(rigidBodyPositionsTex1, GL_RGBA, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyPositionsTex2, GL_RGBA, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyQuaternionsTex1, GL_RGBA, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyQuaternionsTex2, GL_RGBA, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyLinearMomentum1, GL_RGB, GL_RGB, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyLinearMomentum2, GL_RGB, GL_RGB, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyAngularMomentum1, GL_RGB, GL_RGB, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);
	createFBOTexture(rigidBodyAngularMomentum2, GL_RGB, GL_RGB, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyPositionAttachment1, GL_TEXTURE_2D, rigidBodyPositionsTex1, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyPositionAttachment2, GL_TEXTURE_2D, rigidBodyPositionsTex2, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyQuaternionAttachment1, GL_TEXTURE_2D, rigidBodyQuaternionsTex1, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyQuaternionAttachment2, GL_TEXTURE_2D, rigidBodyQuaternionsTex2, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyLinearMomentumAttachment1, GL_TEXTURE_2D, rigidBodyLinearMomentum1, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyLinearMomentumAttachment2, GL_TEXTURE_2D, rigidBodyLinearMomentum2, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyAngularMomentumAttachment1, GL_TEXTURE_2D, rigidBodyAngularMomentum1, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, RigidBodyAngularMomentumAttachment2, GL_TEXTURE_2D, rigidBodyAngularMomentum2, 0);

	// Clearing the values
	float positionClear[4];
	positionClear[0] = position.x;
	positionClear[1] = position.y;
	positionClear[2] = position.z;
	positionClear[3] = 1.0f;

	float quaternionClear[4];
	quaternionClear[0] = quaternion.x;
	quaternionClear[1] = quaternion.y;
	quaternionClear[2] = quaternion.z;
	quaternionClear[3] = quaternion.w;

	float zeroClear[3] = { 0.f, 0.f, 0.f };

	glDrawBuffer(RigidBodyPositionAttachment1);
	glClearBufferfv(GL_COLOR, 0, positionClear);

	glDrawBuffer(RigidBodyPositionAttachment2);
	glClearBufferfv(GL_COLOR, 0, positionClear);

	glDrawBuffer(RigidBodyQuaternionAttachment1);
	glClearBufferfv(GL_COLOR, 0, quaternionClear);

	glDrawBuffer(RigidBodyQuaternionAttachment2);
	glClearBufferfv(GL_COLOR, 0, quaternionClear);

	glDrawBuffer(RigidBodyLinearMomentumAttachment1);
	glClearBufferfv(GL_COLOR, 0, zeroClear);

	glDrawBuffer(RigidBodyLinearMomentumAttachment2);
	glClearBufferfv(GL_COLOR, 0, zeroClear);

	glDrawBuffer(RigidBodyAngularMomentumAttachment1);
	glClearBufferfv(GL_COLOR, 0, zeroClear);

	glDrawBuffer(RigidBodyAngularMomentumAttachment2);
	glClearBufferfv(GL_COLOR, 0, zeroClear);

	// --------------------------------------------------
	//  Particle Textures
	// --------------------------------------------------   

	// Texture must be numRegidBodies * numParticlesPerRigidBody long
	int particleTexEdgeLength = getParticleTextureSideLength();

	createFBOTexture(particleRelativePositionsTex, GL_RGB, GL_RGB, GL_FLOAT, GL_NEAREST, particleTexEdgeLength, particleTexEdgeLength, NULL);
	createFBOTexture(particleVelocityTex, GL_RGB, GL_RGB, GL_FLOAT, GL_NEAREST, particleTexEdgeLength, particleTexEdgeLength, NULL);

	glFramebufferTexture2D(GL_FRAMEBUFFER, ParticlePositionAttachment, GL_TEXTURE_2D, particleRelativePositionsTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, ParticleVelocityAttachment, GL_TEXTURE_2D, particleVelocityTex, 0);

	// --------------------------------------------------
	//  Grid Texture
	// --------------------------------------------------   

	// Assuming diameter particle = voxel edge length -> max 4x particles per voxel
	glGenTextures(1, &gridTex);
	glBindTexture(GL_TEXTURE_3D, gridTex);

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Init empty image (to currently bound FBO)
	int gridSizeX, gridSizeY, gridSizeZ;
	glm::vec3 gridSize = grid.getBtmRightBack() - grid.getTopLeftFront();
	glm::ivec3 gridDimensions = glm::abs(glm::ivec3(gridSize / grid.getVoxelLength()));

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, gridDimensions.x, gridDimensions.y, gridDimensions.z, 0, GL_RGBA, GL_UNSIGNED_INT, NULL);
	glFramebufferTexture3D(GL_FRAMEBUFFER, GridIndiceAttachment, GL_TEXTURE_3D, gridTex, 0, 0);

	glBindTexture(GL_TEXTURE_3D, 0);

	// Finally Check FBO status
	bool result = checkFBOStatus();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return result;

}

bool RigidSolver::initRigidDataStructures(void)
{
	return false;
}

bool RigidSolver::resetSimulation(void)
{
	solverStatus = true;
	spawnedObjects = 0;
	lastSpawn = std::time(0);

	initSolverFBO();

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
	// FIXME: May have to create Shader here only and then link it later
	shaderBeauty.CreateProgramFromFile(beautyVertShaderName.c_str(), beautyFragShaderName.c_str());
	shaderMomentaCalculation.CreateProgramFromFile(momentaVertShaderName.c_str(), momentaFragShaderName.c_str());
	shaderParticleValues.CreateProgramFromFile(particleValuesVertShaderName.c_str(), particleValuesFragShaderName.c_str(), particleValuesGeomShaderName.c_str());
	
	return true;
}

bool RigidSolver::updateParticles() {
	// Must be done when new particles are spawned
	return false;
}

bool RigidSolver::createParticles(void)
{
	return false;
}

bool RigidSolver::updateGrid() {
	// Must be done on init or when voxel size changes
	return false;
}

// --------------------------------------------------
//  TRIGGERS
// --------------------------------------------------   

/**
* @brief Callback function which is invoked when the file dropdown list changed
*/
void RigidSolver::fileChanged(FileEnumVar<RigidSolver> &var) {

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

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	// Init empty image (to currently bound FBO)
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data);

	glBindTexture(GL_TEXTURE_2D, 0);
}

bool RigidSolver::checkFBOStatus() {
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	bool r = false;

	switch (status) {
	case GL_FRAMEBUFFER_UNDEFINED: {
		fprintf(stderr, "FBO: undefined.\n");
		break;
	}
	case GL_FRAMEBUFFER_COMPLETE: {
		fprintf(stderr, "FBO: complete.\n");
		r = true;
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: {
		fprintf(stderr, "FBO: incomplete attachment.\n");
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: {
		fprintf(stderr, "FBO: no buffers are attached to the FBO.\n");
		break;
	}
	case GL_FRAMEBUFFER_UNSUPPORTED: {
		fprintf(stderr, "FBO: combination of internal buffer formats is not supported.\n");
		break;
	}
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: {
		fprintf(stderr, "FBO: number of samples or the value for ... does not match.\n");
		break;
	}
	}

	return r;
}

int RigidSolver::getRigidBodyTextureSizeLength(void)
{
	return floor(sqrt(MAX_NUMBER_OF_RIGID_BODIES));
}

int RigidSolver::getParticleTextureSideLength(void)
{
	return std::ceil(std::sqrt(getRigidBodyTextureSizeLength() * vaModel.getNumParticles()));
}
