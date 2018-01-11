#include "SolverModel.h"

const int NUM_DEPTH_PEEL_PASSES = 2;

SolverModel::SolverModel()
{
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

	// Custom FBO for that
	GLuint depthFBO;
	glGenFramebuffers(1, &depthFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);

	GLuint textures[NUM_DEPTH_PEEL_PASSES];
	glGenTextures(NUM_DEPTH_PEEL_PASSES, textures);

	// Allocate number of depth textures
	for (int i = 0; i < NUM_DEPTH_PEEL_PASSES; i++) {
		
		glBindTexture(GL_TEXTURE_2D, textures[i]);

		// Set the wrap and interpolation parameter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// Init empty image (to currently bound FBO)
		// TODO: Fix the dimensions
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i], 0);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// Render the depth passes
	Bind();
	for (int i = 0; i < NUM_DEPTH_PEEL_PASSES; i++) {

		int A = i % 2;
		int B = (i + 1) % 2;
		
		// First depth map just write it
		if (i == 0) glDisable(GL_DEPTH_TEST); 
		else glEnable(GL_DEPTH_TEST);
		
		// Depth unit 0
		GLuint attachments[] = { GL_COLOR_ATTACHMENT0 + A };
		glDrawBuffers(1, attachments);

		glDepthMask(false);
		glDepthFunc(GL_GREATER);

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

		// FIXME: Does not translate the model. This may cause the model to be outside of grid
		glDrawElements(GL_TRIANGLES, GetNumVertices() * 3, GL_UNSIGNED_INT, 0);

	}
	Release();

	// Render final pass which determines the particle positions


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
