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
	virtual bool Idle(void);
    virtual bool Render(void);
	virtual bool Resize(int width, int height);
	virtual bool Keyboard(unsigned char key, int x, int y);

	// Helper for this and other classes
	static std::string debugDirectory;

	static bool checkFBOStatus(std::string fboName);
	static bool saveFramebufferPNG(std::string filename, GLuint texture, int width, int height, GLenum format, GLenum type);
	static bool saveDepthTexturePNG(std::string filename, GLuint texture, int width, int height);
	static bool saveTextureToBMP(std::string filenname, GLuint texture, int width, int height, int channels, GLenum format, GLenum type);
	template <class T> static bool saveArrayToTXT(std::string filename, T * array, int num, int chunkSize);
	static void drawAbstractData(unsigned int width, unsigned int height, GLShader &shader);

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
	virtual bool momentaPass(void);
	virtual bool beautyPass(void);
	virtual bool solverPass(void);

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
	APIVar<RigidSolver, FloatVarPolicy> springCoefficient;
	APIVar<RigidSolver, FloatVarPolicy> dampingCoefficient;
	APIVar<RigidSolver, IntVarPolicy> spawnTime;

	// Paths - needed for reloadShaders()
	std::string commonFunctionsVertShaderName;
	std::string particleValuesVertShaderName, particleValuesFragShaderName, particleValuesGeomShaderName;
	std::string beautyVertShaderName, beautyFragShaderName;
	std::string momentaVertShaderName, momentaFragShaderName;
	std::string collisionVertShaderName, collisionFragShaderName;
	std::string collisionGridVertShaderName, collisionGridFragShaderName, collisionGridGeomShaderName;
	std::string solverVertShaderName, solverFragShaderName;

	// Transformations
	float aspectRatio = 1.f;
	glm::mat4 viewMX, projMX;
	int windowWidth, windowHeight;

	// Solver
	unsigned int spawnedObjects = 1u; // Always starts with one instance
	std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now(), lastSpawn = time, lastRender = time;
	std::chrono::duration<double, std::milli> timeSpanRender, timeSpanSpawn;

	// --------------------------------------------------
	//  OpenGL variables
	// --------------------------------------------------  

	// Shaders
	GLShader shaderBeauty, shaderSolver, shaderParticleValues, shaderMomentaCalculation, shaderCollisionGrid, shaderCollision;

	// Vertex Arrays
	SolverModel vaModel;
	VertexArray vaParticles;
	VertexArray vaVertex;

	// FBOs
	GLuint rigidBodyFBO;
	GLuint particlesFBO;
	GLuint gridFBO;

	// Grid
	SolverGrid grid;

	// Textures
	GLuint beautyDepthTex;
	GLuint gridTex, gridDepthTex;

	bool texSwitch = false; // false=1, true=2

	GLuint initialParticlePositionsTex;
	GLuint rigidBodyPositionsTex1, rigidBodyPositionsTex2;
	GLuint rigidBodyQuaternionsTex1, rigidBodyQuaternionsTex2;
	GLuint rigidBodyLinearMomentumTex;
	GLuint rigidBodyAngularMomentumTex;

	GLuint particlePositionsTex;
	GLuint particleVelocityTex;
	GLuint particleRelativePositionTex;
	GLuint particleForcesTex;


};

template <class T>
bool RigidSolver::saveArrayToTXT(std::string filename, T * array, int num, int chunkSize)
{
	std::ofstream file(filename.c_str());

	if (file.is_open())
	{
		for (int count = 0; count < num / chunkSize; count += chunkSize) {
			file << array[count];
			for (int chunk = 1; chunk < chunkSize; chunk++) {
				file << " " << array[count + chunk];
			}
			file << std::endl;
		}
		file.close();
		return true;
	}
	else return false;
}

extern "C" OGL4COREPLUGIN_API RenderPlugin* OGL4COREPLUGIN_CALL CreateInstance(COGL4CoreAPI *Api);