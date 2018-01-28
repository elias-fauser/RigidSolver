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

	bool reloadShaders(void);

private:

	std::string currentDirectory;
	unsigned int linearIndexFromCoordinate(float x, float y, float z, int max_x, int max_y, int offset);
	float * particlePositions = NULL;
	int numParticles = 0;
	glm::mat3 inertiaTensor;
	GLShader peelingShader, peelingEvaluationShader;

};

