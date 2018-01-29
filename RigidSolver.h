#pragma once

#include "RenderPlugin.h"
#include "GL/gl3w.h"
#include "GLShader.h"
#include "glm/glm.hpp"
#include "VertexArray.h"
#include "SolverModel.h"

// Global variables
const bool DEBUGGING = true;

// This class is exported from the RigidSolver.dll
class OGL4COREPLUGIN_API RigidSolver : public RenderPlugin {
public:
    RigidSolver(COGL4CoreAPI *Api);
    ~RigidSolver(void);

    virtual bool Activate(void);
    virtual bool Deactivate(void);
    virtual bool Init(void);
    virtual bool Render(void);
	virtual bool Resize(int width, int height);
	virtual bool Keyboard(unsigned char key, int x, int y);

	// Helper for this and other classes
	static bool checkFBOStatus(std::string fboName);
	static bool saveFramebufferPNG(std::string filename, GLuint texture, int width, int height, GLenum format, GLenum type);
	static bool saveDepthTexturePNG(std::string filename, GLuint texture, int width, int height);

	// Public static vertex arrays
	static VertexArray vaQuad;
	static VertexArray vaPlane;

private:

	virtual bool initSolverFBOs(void);
	virtual bool initRigidFBO(void);
	virtual bool initParticleFBO(void);
	virtual bool initGridFBO(void);

	virtual bool resetSimulation(void);
	virtual bool stopSimulation(void);
	virtual bool continueSimulation(void);

	virtual bool reloadShaders(void);

	virtual bool updateGrid(void);
	virtual bool updateRigidBodies(void);
	virtual bool updateParticles(void);
	
	virtual bool loadModel(float * vertices, int * indices, int num);
	virtual bool loadModel(float * vertices, int * indices, float * texCoords, int num);
	virtual bool loadModel(float * vertices, int * indices, float * texCoords, float * normals, int num);

	virtual bool particleValuePass(void);
	virtual bool collisionGridPass(void);
	virtual bool collisionPass(void);
	virtual bool solverPass(void);
	virtual bool beautyPass(void);

	virtual void createFBOTexture(GLuint &outID, const GLenum internalFormat, const GLenum format, const GLenum type, GLint filter, int width, int height, void * data);
	virtual int getRigidBodyTextureSizeLength(void);
	virtual int getParticleTextureSideLength(void);

	void fileChanged(FileEnumVar<RigidSolver> &var);
	void particleSizeChanged(APIVar<RigidSolver, FloatVarPolicy> &var);

	// API Vars
	FileEnumVar<RigidSolver>  modelFiles;
	APIVar<RigidSolver, IntVarPolicy> fovY;
	APIVar<RigidSolver, BoolVarPolicy> drawParticles;
	APIVar<RigidSolver, BoolVarPolicy> solverStatus;
	APIVar<RigidSolver, FloatVarPolicy> particleSize;
	APIVar<RigidSolver, IntVarPolicy> numRigidBodies;
	APIVar<RigidSolver, FloatVarPolicy> gravity;
	APIVar<RigidSolver, FloatVarPolicy> modelMass;
	APIVar<RigidSolver, IntVarPolicy> spawnTime;

	// Paths - needed for reloadShaders()
	std::string commonFunctionsVertShaderName;
	std::string particleValuesVertShaderName, particleValuesFragShaderName, particleValuesGeomShaderName;
	std::string beautyVertShaderName, beautyFragShaderName;
	std::string momentaVertShaderName, momentaFragShaderName;
	std::string collisionVertShaderName, collisionFragShaderName;
	std::string collisionGridVertShaderName, collisionGridFragShaderName;

	// Transformations
	float aspectRatio = 1.f;
	glm::mat4 viewMX, projMX;
	int windowWidth, windowHeight;

	// Solver
	unsigned int spawnedObjects = 1u; // Always starts with one instance
	time_t time = std::time(0), lastSpawn = time;

	// --------------------------------------------------
	//  OpenGL variables
	// --------------------------------------------------  

	// Shaders
	GLShader shaderBeauty, shaderSolver, shaderParticleValues, shaderMomentaCalculation, shaderCollisionGrid, shaderCollision;

	// Vertex Arrays
	SolverModel vaModel;
	VertexArray vaParticles;
	VertexArray vaParticleVertice;

	// FBOs
	GLuint rigidBodyFBO;
	GLuint particlesFBO;
	GLuint gridFBO;

	// Grid
	SolverGrid grid;

	// Textures
	GLuint gridTex;

	bool texSwitch = false; // false=1, true=2
	GLuint rigidBodyPositionsTex1, rigidBodyPositionsTex2;
	GLuint rigidBodyQuaternionsTex1, rigidBodyQuaternionsTex2;
	GLuint rigidBodyLinearMomentum1, rigidBodyLinearMomentum2;
	GLuint rigidBodyAngularMomentum1, rigidBodyAngularMomentum2;

	GLuint particlePositionsTex;
	GLuint particleVelocityTex;
	GLuint particleForcesTex;


};

extern "C" OGL4COREPLUGIN_API RenderPlugin* OGL4COREPLUGIN_CALL CreateInstance(COGL4CoreAPI *Api);