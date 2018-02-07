#include "SolverModel.h"
#include "RigidSolver.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/ext.hpp"
#include <cstring>
#include <vector>
#include "soil.h"

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
	if (particlePositions != NULL) {
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

	GLuint depthTextures[NUM_DEPTH_PEEL_PASSES];
	GLuint depthFBOs[NUM_DEPTH_PEEL_PASSES];

	glm::ivec3 gridResolution = grid->getGridResolution();
	glm::vec3 gridSize = grid->getGridSize();
	glm::vec3 modelSize = getModelSize();
	float voxelLength = grid->getVoxelLength();
	glm::vec3 btmLeftFrontCorner = grid->getBtmLeftFront();
	glm::vec3 topRightBackCorner = grid->getTopRightBack();
	
	float bias = 0.1f;
	float zNear = bias;
	float zFar = gridSize.z + bias;

	// Setup orthographic view scaled to grid size, looking into positive z direction (y up)
	glm::vec3 eye = glm::vec3(0.f, 0.f, btmLeftFrontCorner.z - bias);
	glm::mat4 viewMatrix = glm::lookAt(eye, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
	// glm::mat4 viewMatrix = glm::mat4(1.f);
	glm::mat4 projMatrix = glm::ortho(btmLeftFrontCorner.x, topRightBackCorner.x, btmLeftFrontCorner.y, topRightBackCorner.y, zNear, zFar);
	
	// Scale the model to fit the grid (so we use the full resolution)
	glm::mat4 modelMatrix = grid->getModelMatrix();
	// float scaling = std::min(std::min(gridSize.x / modelSize.x, gridSize.y / modelSize.y), gridSize.z / modelSize.z);
	float scaling = 1.f;
	modelMatrix = glm::scale(modelMatrix, scaling, scaling, scaling);
	
	// --------------------------------------------------
	//  Creating the FBO and depth peeling textures
	// -------------------------------------------------- 

	glGenFramebuffers(NUM_DEPTH_PEEL_PASSES, depthFBOs);

	// Allocate number of depth textures
	glGenTextures(NUM_DEPTH_PEEL_PASSES, depthTextures);

	for (int i = 0; i < NUM_DEPTH_PEEL_PASSES; i++) {
		
		glBindFramebuffer(GL_FRAMEBUFFER, depthFBOs[i]);

		// Disable any color attachment
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		// Depth texture
		glBindTexture(GL_TEXTURE_2D, depthTextures[i]);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, gridResolution.x, gridResolution.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextures[i], 0);
		
		RigidSolver::checkFBOStatus(std::string("Depth Peeling FBO - Pass ") + std::to_string(i));

	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// --------------------------------------------------
	//  Rendering the depth peels
	// -------------------------------------------------- 

	// All depth values since they will be discarded in the shader
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.f);
	glDepthFunc(GL_ALWAYS);
	glDepthMask(true);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	for (int i = 0; i < NUM_DEPTH_PEEL_PASSES; i++) {

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depthFBOs[i]);

		peelingShader.Bind();

		if (i == 0) glUniform1i(peelingShader.GetUniformLocation("enabled"), 0);
		else {
			glUniform1i(peelingShader.GetUniformLocation("enabled"), 1);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTextures[i - 1]);
			glUniform1i(peelingShader.GetUniformLocation("lastDepth"), 0);
		}

		glUniformMatrix4fv(peelingShader.GetUniformLocation("projMX"), 1, GL_FALSE, glm::value_ptr(projMatrix));
		glUniformMatrix4fv(peelingShader.GetUniformLocation("viewMX"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
		glUniformMatrix4fv(peelingShader.GetUniformLocation("modelMX"), 1, GL_FALSE, glm::value_ptr(modelMatrix));

		this->Bind();

		glClear(GL_DEPTH_BUFFER_BIT);
		glViewport(0.f, 0.f, gridResolution.x, gridResolution.y);
		glDrawElements(GL_TRIANGLES, this->GetNumVertices() * 3, GL_UNSIGNED_INT, nullptr);

		this->Release();

		peelingShader.Release();

		if (DEBUGGING) {
			bool result = RigidSolver::saveTextureToBMP(
				std::string("/depthPeelingPass_color_") + std::to_string(i),
				depthTextures[i],
				gridResolution.x, gridResolution.y,
				1,
				GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);

			if (!result) printf("SOIL saving error: '%s'\n", SOIL_last_result());
		}
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthFunc(GL_LESS);

	// --------------------------------------------------
	//  Rendering the final evaluation of the particle positions
	// -------------------------------------------------- 
	
	GLuint depthPeelingEvalFBO;
	glGenFramebuffers(1, &depthPeelingEvalFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depthPeelingEvalFBO);

	// Creating a 3D texture to render the results to
	GLuint gridTex;
	glGenTextures(1, &gridTex);
	glBindTexture(GL_TEXTURE_2D_ARRAY, gridTex);

	// Set the wrap and interpolation parameter
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Init empty image (to currently bound FBO)
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RED, gridResolution.x, gridResolution.y, gridResolution.z, 0, GL_RED, GL_FLOAT, NULL);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gridTex, 0);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	peelingEvaluationShader.Bind();

	// Add the uniforms
	glUniform1i(peelingEvaluationShader.GetUniformLocation("resolutionY"), gridResolution.y);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("resolutionZ"), gridResolution.z);
	glUniform1f(peelingEvaluationShader.GetUniformLocation("zNear"), zNear);
	glUniform1f(peelingEvaluationShader.GetUniformLocation("zFar"), zFar);

	// Add the lookup depth textures
	// TODO: Change from hard coded to dynamic
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthTextures[0]);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("depth1"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depthTextures[1]);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("depth2"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, depthTextures[2]);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("depth2"), 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, depthTextures[3]);
	glUniform1i(peelingEvaluationShader.GetUniformLocation("depth3"), 3);

	glDisable(GL_DEPTH_TEST);

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

		glUniform1f(peelingEvaluationShader.GetUniformLocation("z"), zDepth);
		glUniform1i(peelingEvaluationShader.GetUniformLocation("zLayer"), z);

		glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gridTex, 0, z);

		GLuint attachments[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, attachments);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		RigidSolver::drawAbstractData(gridResolution.x, gridResolution.y, peelingEvaluationShader);
	}

	glEnable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	peelingEvaluationShader.Release();

	// --------------------------------------------------
	//  Determine the particles from the drawn texture
	// -------------------------------------------------- 

	// Set number of particles and their positions
	float * gridData;
	std::vector<float> particles;

	gridData = new float[gridResolution.x * gridResolution.y* gridResolution.z];

	// Read the texture from the GPU
	glBindTexture(GL_TEXTURE_2D_ARRAY, gridTex);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RED, GL_FLOAT, gridData);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	// Get the voxels which are particles and determine their positions
	for (unsigned int x = 0u; x < gridResolution.x; x++) {
		for (unsigned int y = 0u; y < gridResolution.y; y++) {
			for (unsigned int z = 0u; z < gridResolution.z; z++) {
				int idx = linearIndexFromCoordinate(x, y, z, gridResolution.x, gridResolution.y, 1);

				float r = gridData[idx];
				float halfVoxelLength = 0.5 * voxelLength;

				if (r > 0.5) {
					// TODO: Is this the right corner?
					// 0.5 offset for voxel center offset
					particles.push_back(btmLeftFrontCorner.x + x * voxelLength + halfVoxelLength);
					particles.push_back(btmLeftFrontCorner.y + y * voxelLength + halfVoxelLength);
					particles.push_back(btmLeftFrontCorner.z + z * voxelLength + halfVoxelLength);
				}
			}
		}
	}

	// Write the new values to instance variables
	if (this->particlePositions != NULL) delete[] particlePositions;

	this->particlePositions = new float[particles.size()];
	if (particles.size() > 0) {
		particlePositions = &particles[0];
	}

	this->numParticles = particles.size() / 3;

	if (DEBUGGING) {
		if (numParticles == 0) {
			// Adding some dummy particles for testing - if the model can't be depth peeled
			particlePositions = new float[6 * 3];
			numParticles = 6;

			float halfVoxelLength = voxelLength / 2.f;

			particlePositions[0] = -halfVoxelLength;
			particlePositions[1] = 0.f;
			particlePositions[2] = 0.f;

			particlePositions[3] = halfVoxelLength;
			particlePositions[4] = 0.f;
			particlePositions[5] = 0.f;

			particlePositions[6] = 0.f;
			particlePositions[7] = 0.f;
			particlePositions[8] = -halfVoxelLength;

			particlePositions[9] = 0.f;
			particlePositions[10] = 0.f;
			particlePositions[11] = halfVoxelLength;

			particlePositions[12] = 0.f;
			particlePositions[13] = halfVoxelLength;
			particlePositions[14] = 0.f;

			particlePositions[15] = 0.f;
			particlePositions[16] = -halfVoxelLength;
			particlePositions[17] = 0.f;

			std::cout << "Depth peeling failed. Debug mode created dummy particles!" << std::endl;
		}

		// Save them
		// RigidSolver::saveArrayToTXT(RigidSolver::debugDirectory + std::string("/determinedParticlePositions.txt"), particlePositions, numParticles * 3, 3);

	}

	// Status print
	std::cout << "Model particles created. " << getNumParticles() << " particles per rigid model determined!" << std::endl;

	if (gridData) {
		delete[] gridData;
	}

	// --------------------------------------------------
	// Cleanup
	// -------------------------------------------------- 

	// Delete texture
	glDeleteTextures(1, &gridTex);
	glDeleteTextures(NUM_DEPTH_PEEL_PASSES, depthTextures);

	// Delete FBO and unbind
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(NUM_DEPTH_PEEL_PASSES, depthFBOs);
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
	std::string peelingEvaluationGeomShaderName = currentDirectory + std::string("/resources/depthPeelingEval.geom");
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
