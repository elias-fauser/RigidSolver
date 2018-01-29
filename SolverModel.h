#pragma once
#include "SolverGrid.h"
#include "VertexArray.h"
#include "GLShader.h"

class SolverModel: public VertexArray
{
public:
	SolverModel();
	~SolverModel();

	bool createParticles(const SolverGrid * grid);

	int  getNumParticles(void);
	float const * getParticlePositions(void);
	void setInertiaTensor(glm::mat3 tensor);
	glm::mat3 getInertiaTensor(void);

	glm::vec3 getTopRightBack(void) const;
	glm::vec3 getBtmLeftFront(void) const;
	glm::vec3 getModelSize(void) const;
	void setBoundingBox(float xl, float xr, float yb, float yt, float zn, float zf);

	bool reloadShaders(void);

private:

	unsigned int linearIndexFromCoordinate(float x, float y, float z, int max_x, int max_y, int offset);

	std::string currentDirectory;

	// Particles
	float * particlePositions = NULL;
	int numParticles = 0;

	// Other properties
	glm::mat3 inertiaTensor;
	glm::vec3 topRightBack = glm::vec3(0.f);
	glm::vec3 btmLeftFront = glm::vec3(0.f);

	GLShader peelingShader, peelingEvaluationShader;

};

