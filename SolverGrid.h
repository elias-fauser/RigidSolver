#pragma once
#include "glm/gtc/matrix_transform.hpp"
#include "VertexArray.h"



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

	float getVoxelLength(void);
	void  setVoxelLength(float size);

	glm::vec3 getTopLeftFront(void);
	glm::vec3 getBtmRightBack(void);

	glm::vec3 getEmitterVelocity(void);
	glm::vec3 getEmitterPosition(void);

private:

	glm::mat4 modelMX = glm::mat4(1.f);
	glm::vec3 emitterPosition = glm::vec3(0.f, .5f, 0.f);
	glm::vec3 emitterVelocity = glm::vec3(0.f, -1.f, 0.f);
	glm::vec3 topLeftFront = glm::vec3(-.5f, .5f, -.5f), btmRightBack = glm::vec3(.5f, -.5f, .5f);

	float voxelLength = .05f;
	float sizeX, sizeY, sizeZ;

	VertexArray vaBox, vaEmitter;

};

