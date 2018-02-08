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

SolverGrid::SolverGrid()
{
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

/** @brief Returns the transformation matrix of the grid
* @returns Transformation matrix
*/
glm::mat4 SolverGrid::getModelMatrix(void) const
{
	return modelMX;
}

/** @brief Translates the grid by the given (x,y,z) translation
*/
void SolverGrid::translate(glm::vec3 translation)
{
	modelMX = glm::translate(modelMX, translation);
}

/** @brief Translates the grid by the given x,y,z translation
*/
void SolverGrid::translate(float x, float y, float z)
{
	this->translate(glm::vec3(x, y, z));
}

/** @brief Scales the grid by the given (x,y,z) translation
*/
void SolverGrid::scale(glm::vec3 scaling)
{
	modelMX = glm::scale(modelMX, scaling);
}

/** @brief Returns the current set voxel length on the grid
*/
float SolverGrid::getVoxelLength(void) const
{
	return voxelLength;
}

/** @brief Sets the current voxel length on the grid
*/
void SolverGrid::setVoxelLength(float size)
{
	voxelLength = size;
}

/**
* @brief Returns the corner which the largest coordinates - aka TopRightBack
*/
glm::vec3 SolverGrid::getTopRightBack(void) const
{
	glm::vec4 translated = this->modelMX * glm::vec4(topRightBack, 1.0f);
	return glm::vec3(translated.x, translated.y, translated.z);
}

/**
* @brief Returns the corner which the smallest coordinates - aka BtmLeftFront
*/
glm::vec3 SolverGrid::getBtmLeftFront(void) const
{
	glm::vec4 translated = this->modelMX * glm::vec4(btmLeftFront, 1.0f);
	return glm::vec3(translated.x, translated.y, translated.z);
}
/**
* @brief Returns the size of the grid
*
*/
glm::vec3 SolverGrid::getGridSize(void) const
{
	return glm::abs(getBtmLeftFront() - getTopRightBack());
}

/**
* @brief Returns the resolution - how many voxel per axis - for each axis
*
*/
glm::ivec3 SolverGrid::getGridResolution(void) const
{
	return glm::ivec3(getGridSize() / getVoxelLength());
}

/** @brief Returns the emitters initial velocity
*/
glm::vec3 SolverGrid::getEmitterVelocity(void)
{
	return emitterVelocity;
}

/** @brief Returns the emitter position
*/
glm::vec3 SolverGrid::getEmitterPosition(void)
{
	return emitterPosition;
}
