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

private:

	float * particlePositions;
	int numParticles = -1;

};

