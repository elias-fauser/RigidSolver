#include "SolverModel.h"
#include "RigidSolver.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/ext.hpp"
#include <cstring>
#include <vector>

const int NUM_DEPTH_PEEL_PASSES = 4;

// Global variables
extern const bool DEBUGGING;

SolverModel::SolverModel()
{	
	// Determine the current file path
	std::string file_path = __FILE__;
	currentDirectory = file_path.substr(0, file_path.rfind("\\"));

	reloadShaders();
}


SolverModel::~SolverModel()
{
	if (particlePositions) {
		delete[] particlePositions;
		particlePositions = NULL;
	}
}

/**
 *	Uses the depth peel algorithm to create the particle positions for this model
 */
bool SolverModel::createParticles(const SolverGrid * grid)
{

	// --------------------------------------------------
	//  Determine grid attributes and setup
	// -------------------------------------------------- 

	GLuint textures[NUM_DEPTH_PEEL_PASSES];
	glm::ivec3 gridResolution = grid->getGridResolution();
	glm::vec3 gridSize = grid->getGridSize();
	glm::vec3 modelSize = getModelSize();
	float voxelLength = grid->getVoxelLength();
	glm::vec3 btmLeftFrontCorner = grid->getBtmLeftFront();
	glm::vec3 topRightBackCorner = grid->getTopRightBack();
	
	float bias = 0.001f;
	float zNear = bias;
	float zFar = gridSize.z * 2.f;

	GLuint depthPeelingFBO;
	glGenFramebuffers(1, &depthPeelingFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depthPeelingFBO);

	// --------------------------------------------------
	//  Creating the depth peeling textures
	// -------------------------------------------------- 

	// Add one color attachment so that the buffer is not incomplete
	GLuint colorTexture;
	glGenTextures(1, &colorTexture);
	glBindTexture(GL_TEXTURE_2D, colorTexture);

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gridResolution.x, gridResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Allocate number of depth textures
	glGenTextures(NUM_DEPTH_PEEL_PASSES, textures);

	for (int i = 0; i < NUM_DEPTH_PEEL_PASSES; i++) {
		
		glBindTexture(GL_TEXTURE_2D, textures[i]);

		// Set the wrap and interpolation parameter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// Init empty image (to currently bound FBO)
		// TODO: Check if resolution of y and z is really the right one
		// glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, gridResolution.x, gridResolution.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		// TODO: Switch back to 32F
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, gridResolution.x, gridResolution.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
	}


	// --------------------------------------------------
	//  Rendering the depth peels
	// -------------------------------------------------- 
	// Setup orthographic view scaled to grid size, looking into positive z direction (y up)
	glm::vec3 eye = glm::vec3(0.f, 0.f, btmLeftFrontCorner.z - bias);
	glm::mat4 viewMatrix = glm::lookAt(eye, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 projMatrix = glm::ortho(btmLeftFrontCorner.x, topRightBackCorner.x, btmLeftFrontCorner.y, topRightBackCorner.y, zNear, zFar);

	// Scale the model to fit the grid (so we use the full resolution)
	glm::mat4 modelMatrix = grid->getModelMatrix();
	float scaling = std::max(std::max(modelSize.x / gridSize.x, modelSize.x / gridSize.x), modelSize.x / gridSize.x);
	modelMatrix = glm::scale(modelMatrix, scaling, scaling, scaling);

	// Render the depth passes
	peelingShader.Bind();

	glUniformMatrix4fv(peelingShader.GetUniformLocation("projMX"), 0, false, glm::value_ptr(projMatrix));
	glUniformMatrix4fv(peelingShader.GetUniformLocation("viewMX"), 0, false, glm::value_ptr(viewMatrix));
	glUniformMatrix4fv(peelingShader.GetUniformLocation("modelMX"), 0, false, glm::value_ptr(modelMatrix));

	glDepthMask(true);

	for (int i = 0; i < NUM_DEPTH_PEEL_PASSES; i++) {

		int A = i % 2;
		int B = (i + 1) % 2;
		
		// First depth map just write it
		if (i == 0) {
			glDisable(GL_DEPTH_TEST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[i], 0);

			// Clear the write depth texture once before rendering
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		else {
			glEnable(GL_DEPTH_TEST);

			// Depth unit 0 - The one that is read from
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, textures[i-1]);
			glUniform1i(peelingShader.GetUniformLocation("lastDepth"), 0);

			// Depth unit 1 - The one that is written to
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[i], 0);

			// Clear the write depth texture once before rendering
			glClear(GL_DEPTH_BUFFER_BIT);

		}

		RigidSolver::checkFBOStatus(std::string("Depth Peeling FBO - Pass ") + std::to_string(i));

		glDrawElements(GL_TRIANGLES, GetNumVertices() * 3, GL_UNSIGNED_INT, 0);

		if (DEBUGGING) {
			std::string fileName = currentDirectory + std::string("depthPeelingPass_") + std::to_string(i) + std::string(".png");
			RigidSolver::saveDepthTexturePNG(fileName, textures[i], gridResolution.x, gridResolution.y);
		}

	}
	peelingShader.Release();


	// --------------------------------------------------
	//  Create a 3D texture for the output
	// -------------------------------------------------- 

	// Creating a 3D texture to render the results to
	GLuint gridTex;
	glGenTextures(1, &gridTex);
	glBindTexture(GL_TEXTURE_3D, gridTex);

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Init empty image (to currently bound FBO)
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, gridResolution.x, gridResolution.y, gridResolution.z, 0, GL_RGB, GL_FLOAT, NULL);

	// --------------------------------------------------
	//  Rendering the final evaluation of the particle positions
	// -------------------------------------------------- 
	
	GLuint depthPeelingEvalFBO;
	glGenFramebuffers(1, &depthPeelingEvalFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depthPeelingEvalFBO);

	// Viewporting to get right grid coordinates
	glViewport(0, 0, gridResolution.x, gridResolution.y);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 orthoMX = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);

	peelingEvaluationShader.Bind();

	// Add the uniforms
	glUniformMatrix4fv(peelingEvaluationShader.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(orthoMX));
	glUniform1i(peelingEvaluationShader.GetUniformLocation("resolutionY"), gridResolution.y);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("resolutionZ"), gridResolution.z);
	glUniform1f(peelingEvaluationShader.GetUniformLocation("zNear"), zNear);
	glUniform1f(peelingEvaluationShader.GetUniformLocation("zFar"), zFar);

	// Add the lookup depth textures
	// TODO: Change from hard coded to dynamic
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("depth1"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("depth2"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, textures[2]);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("depth2"), 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, textures[3]);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("depth3"), 3);


	RigidSolver::vaQuad.Bind();
	
	for (unsigned int z = 0u; z < gridResolution.x; z++) {

		// Add a uniform for the current depth
		// TODO: Check if this is really right. I need an z buffer value in between [-1, 1] here I think
		float a = zFar / (zNear - zFar);
		float b = (zFar * zNear) / (zNear - zFar);

		// Determining depth of z-layer of grid - adding half voxelLength to be in the middle of the voxel
		float depth = grid->getBtmLeftFront().z + z * voxelLength + voxelLength / 2.0f;

		// Calulating view and proj transformations to get viewspace z
		glm::vec4 viewSpace = projMatrix * viewMatrix * glm::vec4(0.f, 0.f, depth, 1.f);
		float zDepth = (a * viewSpace.z + b) / -viewSpace.z;
		// Calculate to float

		glUniform1i(peelingEvaluationShader.GetUniformLocation("z"), zDepth);

		// Setup a z slice to render to
		glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, gridTex, 0, z);

		GLuint attachments[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, attachments);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	glBindTexture(GL_TEXTURE_3D, 0);
	RigidSolver::vaQuad.Release();
	peelingEvaluationShader.Release();


	// --------------------------------------------------
	//  Determine the particles from the drawn texture
	// -------------------------------------------------- 

	// Set number of particles and their positions
	float * gridData;
	std::vector<float> particles;

	gridData = new float[gridResolution.x * gridResolution.y* gridResolution.z * 3];

	// Read the texture from the GPU
	glBindTexture(GL_TEXTURE_3D, gridTex);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glGetTexImage(GL_TEXTURE_3D, 0, GL_RGB, GL_FLOAT, gridData);
	glBindTexture(GL_TEXTURE_3D, 0);

	// Get the voxels which are particles and determine their positions
	for (unsigned int x = 0u; x < gridResolution.x; x++) {
		for (unsigned int y = 0u; y < gridResolution.y; y++) {
			for (unsigned int z = 0u; z < gridResolution.z; z++) {
				int idx = linearIndexFromCoordinate(x, y, z, gridResolution.x, gridResolution.y, 3);

				float r = gridData[idx];
				float g = gridData[idx + 1];
				float b = gridData[idx + 2];

				if (r > .5f && g > .5f && b > .5f) {
					// TODO: Is this the right corner?
					// 0.5 offset for voxel center offset
					particles.push_back(btmLeftFrontCorner.x + x * voxelLength + 0.5 * voxelLength);
					particles.push_back(btmLeftFrontCorner.y + y * voxelLength + 0.5 * voxelLength);
					particles.push_back(btmLeftFrontCorner.z + z * voxelLength + 0.5 * voxelLength);
				}
			}
		}
	}

	// Write the new values to instance variables
	particlePositions = new float[particles.size()];
	if (particles.size() > 0) {
		particlePositions = &particles[0];
	}

	numParticles = particles.size() / 3;

	// Status print
	std::cout << "Model particles created. " << getNumParticles() << " particles per rigid model determined!\n";

	if (gridData) {
		delete[] gridData;
	}

	// --------------------------------------------------
	// Cleanup
	// -------------------------------------------------- 

	// Delete texture
	glDeleteTextures(1, &gridTex);
	glDeleteTextures(1, &colorTexture);

	// Delete FBO and unbind
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &depthPeelingFBO);
	glDeleteFramebuffers(1, &depthPeelingEvalFBO);

	glEnable(GL_DEPTH_TEST);

	return false;
}

/** 
* \brief Returns the number of particles for the objects which were determined during particle creation
*
* \returns Number of particles as int
*/
int SolverModel::getNumParticles(void)
{
	return numParticles;
}

/**
* \brief Returns a pointer to the models particles positions. The number of particles can be queried with `getNumParticles()`
* \note The positions are a vector of size 3, relative to the models center of mass
*
* \returns Number of particles as int
*/
float const * SolverModel::getParticlePositions(void)
{
	return particlePositions;
}

void SolverModel::setInertiaTensor(glm::mat3 tensor)
{
	inertiaTensor = tensor;
}

glm::mat3 SolverModel::getInertiaTensor(void)
{
	return inertiaTensor;
}

glm::vec3 SolverModel::getTopRightBack(void) const
{
	return topRightBack;
}

glm::vec3 SolverModel::getBtmLeftFront(void) const
{
	return btmLeftFront;
}

glm::vec3 SolverModel::getModelSize(void) const
{
	return glm::abs(getBtmLeftFront() - getTopRightBack());
}

void SolverModel::setBoundingBox(float xl, float xr, float yb, float yt, float zn, float zf)
{
	btmLeftFront = glm::vec3(xl, yb, zn);
	topRightBack = glm::vec3(xr, yt, zf);
}

bool SolverModel::reloadShaders(void)
{
	// Create the shader
	std::string peelingVertShaderName = currentDirectory + std::string("/resources/depthPeeling.vert");
	std::string peelingFragShaderName = currentDirectory + std::string("/resources/depthPeeling.frag");

	std::string peelingEvaluationVertShaderName = currentDirectory + std::string("/resources/depthPeelingEval.vert");
	std::string peelingEvaluationFragShaderName = currentDirectory + std::string("/resources/depthPeelingEval.frag");

	peelingShader.CreateProgramFromFile(peelingVertShaderName.c_str(), peelingFragShaderName.c_str());
	peelingEvaluationShader.CreateProgramFromFile(peelingEvaluationVertShaderName.c_str(), peelingEvaluationFragShaderName.c_str());

	return true;
}

unsigned int SolverModel::linearIndexFromCoordinate(float x, float y, float z, int max_x, int max_y, int offset)
{
	int b = max_x;
	int c = max_x * max_y;

	return x * offset + b * offset * y + c * offset * z;
}
