#pragma once
#include "glm/gtc/matrix_transform.hpp"

class SolverGrid
{
public:
	SolverGrid() : SolverGrid(128, 128, 128) {};
	SolverGrid(int xVoxels, int yVoxels, int zVoxels);
	~SolverGrid();

	glm::mat4 getModelMatrix(void);
	void translate(glm::vec3 translation);
	void translate(float x, float y, float z);
	void scale(glm::vec3 scaling);

	glm::vec3 getTopLeft();

	float getVoxelLength(void);
	void  setVoxelLength(float size);

	glm::vec3 getTopLeftFront(void);
	glm::vec3 getBtmRightBack(void);

private:

	glm::mat4 modelMX = glm::mat4(1.f);
	float voxelLength = 1.f;
	float sizeX, sizeY, sizeZ;
};

