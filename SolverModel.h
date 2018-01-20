#pragma once
#include "SolverGrid.h"
#include "VertexArray.h"

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

private:

	float * particlePositions;
	int numParticles = -1;
	glm::mat3 inertiaTensor;

};

