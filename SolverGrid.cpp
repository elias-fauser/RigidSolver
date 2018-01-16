#include "SolverGrid.h"
#include "glm/gtc/matrix_transform.hpp"

const float BOX_VERTICES[] = {
	-0.5f, -0.5f, -0.5f, 1.0f,
	0.5f, -0.5f, -0.5f, 1.0f,
	0.5f,  0.5f, -0.5f, 1.0f,
	-0.5f,  0.5f, -0.5f, 1.0f,
	-0.5f, -0.5f,  0.5f, 1.0f,
	0.5f, -0.5f,  0.5f, 1.0f,
	0.5f,  0.5f,  0.5f, 1.0f,
	-0.5f,  0.5f,  0.5f, 1.0f 
};

const GLuint BOX_EDGES[] = {
	0,1, 1,2, 2,3, 3,0,
	4,5, 5,6, 6,7, 7,4,
	0,4, 1,5, 2,6, 3,7 
};

const float EMITTER_VERTICES[] = {
	 0.0f,  0.0f,  0.0f, 1.0f,
	-0.5f, -1.0f,  0.0f, 1.0f,
	-0.7f, -1.0f, -0.7f, 1.0f,
	-0.7f, -1.0f,  0.7f, 1.0f
};

const int EMITTER_INDICES[] = {
	0, 1,
	0, 2,
	0, 3
};

SolverGrid::SolverGrid(int xVoxels, int yVoxels, int zVoxels)
{
	sizeX = xVoxels;
	sizeY = yVoxels;
	sizeZ = zVoxels;

	// Setup drawable grid
	vaBox.Create(sizeof(BOX_VERTICES) / (sizeof(float) * 4));
	vaBox.SetArrayBuffer(0, GL_FLOAT, 4, BOX_VERTICES);
	vaBox.SetElementBuffer(0, 12 * 3, BOX_EDGES);

	// Setup drawable emitter
	vaEmitter.Create(16);
	vaEmitter.SetArrayBuffer(0, GL_FLOAT, 4, EMITTER_VERTICES);
	vaEmitter.SetElementBuffer(0, 3 * 2, EMITTER_INDICES);

}


SolverGrid::~SolverGrid()
{
}

glm::mat4 SolverGrid::getModelMatrix(void)
{
	return modelMX;
}

void SolverGrid::translate(glm::vec3 translation)
{
	modelMX = glm::translate(modelMX, translation);
}

void SolverGrid::translate(float x, float y, float z)
{
	this->translate(glm::vec3(x, y, z));
}

void SolverGrid::scale(glm::vec3 scaling)
{
	modelMX = glm::scale(modelMX, scaling);
}

float SolverGrid::getVoxelLength(void)
{
	return voxelLength;
}

void SolverGrid::setVoxelLength(float size)
{
	voxelLength = size;
}

glm::vec3 SolverGrid::getTopLeftFront(void)
{
	glm::vec4 translated = this->modelMX * glm::vec4(topLeftFront, 1.0f);
	return glm::vec3(translated.x, translated.y, translated.z);
}

glm::vec3 SolverGrid::getBtmRightBack(void)
{
	glm::vec4 translated = this->modelMX * glm::vec4(btmRightBack, 1.0f);
	return glm::vec3(translated.x, translated.y, translated.z);
}

glm::vec3 SolverGrid::getEmitterVelocity(void)
{
	return emitterVelocity;
}

glm::vec3 SolverGrid::getEmitterPosition(void)
{
	return emitterPosition;
}
