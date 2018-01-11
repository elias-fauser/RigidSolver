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
	 .5f,  .5f, 0.0f, 1.0f,
	 .5f, -.5f, 0.0f, 1.0f,
	-.5f, -.5f, 0.0f, 1.0f,
	-.5f,  .5f, 0.0f, 1.0f,
};

const float PLANE_TEXCOORDS[] = {
	 1.f,  1.f,
	 1.f, -1.f,
	-1.f, -1.f,
	-1.f,  1.f,
};

const float PLANE_INDICES[] = {
	0, 1, 2,
	3, 0, 2
};

// --------------------------------------------------
//  Constants
// --------------------------------------------------

const int MAX_NUMBER_OF_RIGID_BODIES = 64 * 64; // 4096
const int MAX_NUMBER_OF_PARTICLES = MAX_NUMBER_OF_RIGID_BODIES * 4; // Assuming voxel size == particle diameter -> so max of 4 particles

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
	this->SetManipulatorRotation(camHandle, glm::vec3(1, 0, 0), 50.0f);
	this->SetManipulatorDolly(camHandle, -1.5f);

	// --------------------------------------------------
	//  Registration of UI attributes
	// --------------------------------------------------  

	// Initialize file selector
	modelFiles.Set(this, "Model", (pathName + "/resources/models").c_str(), ".obj", &RigidSolver::fileChanged);
	modelFiles.Register();

	fovY.Set(this, "fovY");
	fovY.Register();
	fovY.SetMinMax(1.0, 100.0);

	// Solver settings
	solverStatus.Set(this, "Active");
	solverStatus.Register();
	solverStatus.SetValue(false);

	spawnTime.Set(this, "SpawnTime(sec)");
	spawnTime.Register();
	spawnTime.SetMinMax(1.0, 300.0);

	gravity.Set(this, "Gravity");
	gravity.Register();
	gravity = 9.807f; // m/s^2

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
	commonFunctionsVertShaderName = pathName + std::string("/resources/beauty.vert");
	particleValuesVertShaderName = pathName + std::string("/resources/particleValues.vert");
	particleValuesFragShaderName = pathName + std::string("/resources/particleValues.vert");
	beautyVertShaderName = pathName + std::string("/resources/beauty.vert");
	beautyFragShaderName = pathName + std::string("/resources/beauty.frag");

	vaPlane.Create(4);
	vaPlane.SetArrayBuffer(0, GL_FLOAT, 4, PLANE_VERTICES);
	vaPlane.SetElementBuffer(0, 3 * 2, PLANE_INDICES);
	vaPlane.SetArrayBuffer(1, GL_FLOAT, 2, PLANE_TEXCOORDS);

	reloadShaders();

	// --------------------------------------------------
	//  Init
	// --------------------------------------------------  

	initSolverFBO();
	initRigidDataStructures();

    return true;
}

bool RigidSolver::Deactivate(void) {
    return true;
}

bool RigidSolver::Init(void) {
	if (gl3wInit()) {
		fprintf(stderr, "Error: Failed to initialize gl3w.\n");
		return false;
	}
	return true;
}

bool RigidSolver::Render(void) {

	// --------------------------------------------------
	//  Setup
	// --------------------------------------------------  

	if (solverStatus && modelFiles.GetValue() != NULL) {
		// Get current time and eventually spawn a new particle
		time = std::time(0);
		if (time - lastSpawn >= spawnTime) {
			spawnedObjects = std::max((int)(spawnedObjects + 1), MAX_NUMBER_OF_RIGID_BODIES);
		}
		lastSpawn = time;
	}

	// --------------------------------------------------
	//  Passes
	// --------------------------------------------------  

	if (solverStatus && modelFiles.GetValue() != NULL) {
		// All the calculation steps are done on the solverFBO
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, solverFBO);

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
*/
bool RigidSolver::loadModel(float * vertices, int * indices, int num) {

	// Delete the old one
	vaModel.Delete();

	// Calculate Center of mass
	glm::vec3 com = glm::vec3(0.0f);

	for (int i = 0; i < num; i++) {
		com += glm::vec3(vertices[i * 4], vertices[i * 4 + 1], vertices[i * 4 + 2]);
	}
	com /= num;

	// Move the model so it is centered in the CoM
	float * movedVertices;
	movedVertices = new float[num * 4];
	
	for (unsigned int i = 0; i < num; i++) {
		movedVertices[i * 4] = vertices[i * 4] - com.x;
		movedVertices[i * 4 + 1] = vertices[i * 4 + 1] - com.y;
		movedVertices[i * 4 + 2] = vertices[i * 4 + 2] - com.z;
		movedVertices[i * 4 + 3] = vertices[i * 4 + 3];
	}

	// Create the new one
	vaModel.Create(num);
	vaModel.SetArrayBuffer(0, GL_TRIANGLES, 4, movedVertices);
	vaModel.SetElementBuffer(0, num, indices);

	// Reset simulation to restart everything
	resetSimulation();

	delete[] movedVertices;

	return true;

}

/**
* @brief Overload of loadModel to extend the function with texture coordinates
*/
bool RigidSolver::loadModel(float * vertices, int * indices, float * texCoords, int num) {

	this->loadModel(vertices, indices, num);
	vaModel.SetArrayBuffer(1, GL_FLOAT, 2, texCoords);

	return true;
}

// --------------------------------------------------
//  RENDER PASSES
// --------------------------------------------------   

bool RigidSolver::particleValuePass(void)
{
	
	shaderParticleValues.Bind();

	glPatchParameteri(GL_PATCH_VERTICES, 1);
	glDrawArraysInstanced(GL_PATCHES, 0, 1, spawnedObjects);

	shaderParticleValues.Release();

	return false;
}

bool RigidSolver::collisionGridPass(void)
{
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

	// Draw ground plane
	vaPlane.Bind();
	glDrawElements(GL_TRIANGLES, 2, GL_UNSIGNED_INT, 0);
	vaPlane.Release();

	// Instanced drawing of rigid bodies
	vaModel.Bind();
	// Instanced drawing of the rigid bodies
	// glDrawElementsInstanced(GL_TRIANGLES, vaModel.GetNumVertices(), GL_UNSIGNED_INT, 0, spawnedObjects); // numInstances);
	vaModel.Release();

	shaderBeauty.Release();

	if (drawParticles) {
		// Turn off wireframe mode
		glPolygonMode(GL_FRONT, GL_FILL);
		glPolygonMode(GL_BACK, GL_FILL);

		// TODO: Draw particles
	}

	return false;
}


bool RigidSolver::collisionPass() {
	// Use particle VA
	// Shader calculates grid positions
	// Tests for collisions
	// Calculates forces and writes them back to force texture

	return false;
}

bool RigidSolver::solverPass() {
	// Uses particle VA for current position
	// Calculates inertia tensor
	// Determines

	return false;
}

// --------------------------------------------------
//  UPDATES
// --------------------------------------------------   

/**
* @brief Inits the solver FBO with the needed textures

Textures:
	* COLOR_ATTACHMENT0 = rigid body positions
	* COLOR_ATTACHMENT1 = rigid body quaternions
	* COLOR_ATTACHMENT2 = particlePositions
	* COLOR_ATTACHMENT3 = particleVelocities
*/
bool RigidSolver::initSolverFBO() {

	glGenFramebuffers(1, &solverFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, solverFBO);


	// --------------------------------------------------
	//  Rigidbody Textures
	// --------------------------------------------------   

	//  Calculating back the square count
	int rigidTexEdgeLength = floor(sqrt(MAX_NUMBER_OF_RIGID_BODIES));

	createFBOTexture(rigidBodyPositionsTex, GL_RGBA, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength);
	createFBOTexture(rigidBodyQuaternionsTex, GL_RGBA, GL_RGBA, GL_FLOAT, GL_NEAREST, rigidTexEdgeLength, rigidTexEdgeLength);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rigidBodyPositionsTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, rigidBodyQuaternionsTex, 0);

	// --------------------------------------------------
	//  Particle Textures
	// --------------------------------------------------   

	int particleTexEdgeLength = rigidTexEdgeLength * 4;

	createFBOTexture(particlePositionsTex, GL_RGB, GL_RGB, GL_FLOAT, GL_NEAREST, particleTexEdgeLength, particleTexEdgeLength);
	createFBOTexture(particleVelocityTex, GL_RGB, GL_RGB, GL_FLOAT, GL_NEAREST, particleTexEdgeLength, particleTexEdgeLength);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, particlePositionsTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, particleVelocityTex, 0);

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
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16I, rigidTexEdgeLength, rigidTexEdgeLength, rigidTexEdgeLength, 0, GL_RGBA , GL_INT, NULL);
	glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_3D, gridTex, 0, 0);

	glBindTexture(GL_TEXTURE_3D, 0);

	// Finally Check FBO status
	checkFBOStatus();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return false;

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
	// FIXME: May have to create Shader here only and then link it later
	shaderBeauty.AttachShaderFromFile(commonFunctionsVertShaderName.c_str(), GL_VERTEX_SHADER, false);
	shaderBeauty.CreateProgramFromFile(beautyVertShaderName.c_str(), beautyFragShaderName.c_str());

	shaderBeauty.PrintInfo();
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

			// Only supports a single mesh for now
			objl::Mesh mesh = loader.LoadedMeshes[0];

			int numVertices = mesh.Vertices.size();
			int numIndices = mesh.Indices.size();

			vertices = new float[numVertices * 4];
			texCoords = new float[numVertices * 2];
			indices = new int[numIndices];

			for (int i = 0; i < numVertices; i++) {

				vertices[i * 4] = mesh.Vertices[i].Position.X;
				vertices[i * 4 + 1] = mesh.Vertices[i].Position.Y;
				vertices[i * 4 + 2] = mesh.Vertices[i].Position.Z;
				vertices[i * 4 + 3] = 1.f;

				texCoords[i * 2] = mesh.Vertices[i].TextureCoordinate.X;
				texCoords[i * 2 + 1] = mesh.Vertices[i].TextureCoordinate.Y;

			}

			for (unsigned int j = 0; j < mesh.Indices.size(); j += 3){
				indices[j] = mesh.Indices[j];
				indices[j + 1] = mesh.Indices[j + 1];
				indices[j + 2] = mesh.Indices[j + 2];
			}
			
			// TODO: Scale the model here
			loadModel(vertices, indices, texCoords, numVertices);

			delete[] indices;
			delete[] vertices;
			delete[] texCoords;
		}
	}
}

void RigidSolver::particleSizeChanged(APIVar<RigidSolver, FloatVarPolicy> &var)
{
	
}


// --------------------------------------------------
//  HELPERS
// --------------------------------------------------   

void RigidSolver::createFBOTexture(GLuint &outID, const GLenum internalFormat, const GLenum format, const GLenum type, GLint filter, int width, int height) {

	glGenTextures(1, &outID);
	glBindTexture(GL_TEXTURE_2D, outID);

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	// Init empty image (to currently bound FBO)
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void RigidSolver::checkFBOStatus() {
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	switch (status) {
	case GL_FRAMEBUFFER_UNDEFINED: {
		fprintf(stderr, "FBO: undefined.\n");
		break;
	}
	case GL_FRAMEBUFFER_COMPLETE: {
		fprintf(stderr, "FBO: complete.\n");
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
}