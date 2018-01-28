#pragma once
#include "glm/gtc/matrix_transform.hpp"
#include "VertexArray.h"



class SolverGrid
{
public:
	SolverGrid() : SolverGrid(128, 128, 128) {};
	SolverGrid(int xVoxels, int yVoxels, int zVoxels);
	~SolverGrid();

	glm::mat4 getModelMatrix(void) const;
	void translate(glm::vec3 translation);
	void translate(float x, float y, float z);
	void scale(glm::vec3 scaling);

	float getVoxelLength(void) const;
	void  setVoxelLength(float size);

	glm::vec3 getTopRightFront(void) const;
	glm::vec3 getBtmLeftBack(void) const;
	glm::vec3 getGridSize(void) const;
	glm::ivec3 getGridResolution(void) const;

	glm::vec3 getEmitterVelocity(void);
	glm::vec3 getEmitterPosition(void);

private:

	glm::mat4 modelMX = glm::mat4(1.f);
	glm::vec3 emitterPosition = glm::vec3(0.f, .5f, 0.f);
	glm::vec3 emitterVelocity = glm::vec3(0.f, -1.f, 0.f);
	glm::vec3 topRightFront = glm::vec3(.5f, .5f, .5f), btmLeftBack = glm::vec3(-.5f, -.5f, -.5f);

	float voxelLength = .05f;
	float sizeX, sizeY, sizeZ;

	VertexArray vaBox, vaEmitter;

};

