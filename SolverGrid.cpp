#include "SolverGrid.h"
#include "glm/gtc/matrix_transform.hpp"



SolverGrid::SolverGrid(int xVoxels, int yVoxels, int zVoxels)
{
	sizeX = xVoxels;
	sizeY = yVoxels;
	sizeZ = zVoxels;
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

float SolverGrid::getVoxelLength(void)
{
	return voxelLength;
}

void SolverGrid::setVoxelLength(float size)
{
	voxelLength = size;
}
