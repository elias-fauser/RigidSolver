#include "SolverModel.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/ext.hpp"

const int NUM_DEPTH_PEEL_PASSES = 4;

SolverModel::SolverModel()
{
	// Create the shader
	std::string file_path = __FILE__;
	std::string dir_path = file_path.substr(0, file_path.rfind("\\"));

	std::string peelingVertShaderName = dir_path + std::string("/resources/depthPeeling.vert");
	std::string peelingFragShaderName = dir_path + std::string("/resources/depthPeeling.frag");

	peelingShader.CreateProgramFromFile(peelingVertShaderName.c_str(), peelingFragShaderName.c_str());

}


SolverModel::~SolverModel()
{
	if (particlePositions != NULL) delete[] particlePositions;
}

/**
 *	Uses the depth peel algorithm to create the particle positions for this model
 */
bool SolverModel::createParticles(const SolverGrid * grid)
{
	// Setup orthographic view scaled to grid size, looking into positive x direction (y up)
	// FIXME: Lookat just assumes a distance far away istead of considering the grids translation
	glm::mat4 viewMatrix = glm::lookAt(glm::vec3(-100.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 projMatrix = glm::ortho(0.f, 1.f, 0.f, 1.f);

	GLuint textures[NUM_DEPTH_PEEL_PASSES];
	glGenTextures(NUM_DEPTH_PEEL_PASSES, textures);
	glm::ivec3 resolution = grid->getGridResolution();

	// Allocate number of depth textures
	for (int i = 0; i < NUM_DEPTH_PEEL_PASSES; i++) {
		
		glBindTexture(GL_TEXTURE_2D, textures[i]);

		// Set the wrap and interpolation parameter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// Init empty image (to currently bound FBO)
		// TODO: Check if resolution of y and z is really the right one
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,resolution.y , resolution.z, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// Custom FBO
	GLuint depthFBO;
	glGenFramebuffers(1, &depthFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

	// Render the depth passes
	peelingShader.Bind();

	glUniformMatrix4fv(peelingShader.GetUniformLocation("projMX"), 0, false, glm::value_ptr(projMatrix));
	glUniformMatrix4fv(peelingShader.GetUniformLocation("viewMX"), 0, false, glm::value_ptr(viewMatrix));
	glUniformMatrix4fv(peelingShader.GetUniformLocation("modelMX"), 0, false, glm::value_ptr(grid->getModelMatrix()));

	for (int i = 0; i < NUM_DEPTH_PEEL_PASSES; i++) {

		int A = i % 2;
		int B = (i + 1) % 2;
		
		// First depth map just write it
		if (i == 0) {
			glDisable(GL_DEPTH_TEST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[i], 0);
		}
		else {
			glEnable(GL_DEPTH_TEST);

			// Depth unit 0 - The one that is read from
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, textures[i]);
			glUniform1i(peelingShader.GetUniformLocation("lastDepth"), 0);

			// Depth unit 1 - The one that is written to
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[i+1], 0);

			// glDepthMask(true);
			glDepthFunc(GL_LESS);
			glClear(GL_DEPTH_BUFFER_BIT);


			/**
					set depth func to GREATER
					depth unit 1:
				bind buffer B
					clear depth buffer
					enable depth writes
					enable depth test
					set depth func to LESS
					render scene
					save color buffer RGBA as layer i
			*/
		}

		glDrawElements(GL_TRIANGLES, GetNumVertices() * 3, GL_UNSIGNED_INT, 0);
	}
	peelingShader.Release();

	// Render final pass which determines the particle positions
	Bind();

	Release();

	// Set number of particles

	// Status print
	std::cout << "Model particles created. " << getNumParticles() << "per rigid model determined!\n";

	// Reset
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &depthFBO);

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
