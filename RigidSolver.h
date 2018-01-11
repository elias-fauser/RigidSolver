#include "RenderPlugin.h"
#include "GL/gl3w.h"
#include "GLShader.h"
#include "glm/glm.hpp"
#include "VertexArray.h"
#include "SolverModel.h"

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

private:

	virtual bool initSolverFBO(void);
	virtual bool initRigidDataStructures(void);

	virtual bool resetSimulation(void);
	virtual bool stopSimulation(void);
	virtual bool continueSimulation(void);

	virtual bool reloadShaders(void);

	virtual bool updateGrid(void);
	virtual bool updateParticles(void);
	virtual bool createParticles(void);
	
	virtual bool loadModel(float * vertices, int * indices, int num);
	virtual bool loadModel(float * vertices, int * indices, float * texCoords, int num);

	virtual bool particleValuePass(void);
	virtual bool collisionGridPass(void);
	virtual bool collisionPass(void);
	virtual bool solverPass(void);
	virtual bool beautyPass(void);

	virtual void createFBOTexture(GLuint &outID, const GLenum internalFormat, const GLenum format, const GLenum type, GLint filter, int width, int height);
	virtual void checkFBOStatus(void);

	void fileChanged(FileEnumVar<RigidSolver> &var);
	void particleSizeChanged(APIVar<RigidSolver, FloatVarPolicy> &var);
	
	// API Vars
	FileEnumVar<RigidSolver>  modelFiles;
	APIVar<RigidSolver, IntVarPolicy> fovY;
	APIVar<RigidSolver, BoolVarPolicy> drawParticles;
	APIVar<RigidSolver, BoolVarPolicy> solverStatus;
	APIVar<RigidSolver, FloatVarPolicy> particleSize;
	APIVar<RigidSolver, FloatVarPolicy> gravity;
	APIVar<RigidSolver, IntVarPolicy> spawnTime;

	// Paths - needed for reloadShaders()
	std::string commonFunctionsVertShaderName;
	std::string particleValuesVertShaderName;
	std::string particleValuesFragShaderName;
	std::string beautyVertShaderName;
	std::string beautyFragShaderName;

	// Transformations
	float aspectRatio = 1.f;
	glm::mat4 viewMX, projMX;
	int windowWidth, windowHeight;

	// Solver
	unsigned int spawnedObjects = 0u;
	time_t time = std::time(0), lastSpawn = time;

	// --------------------------------------------------
	//  OpenGL variables
	// --------------------------------------------------  

	// Shaders
	GLShader shaderBeauty, shaderSolver, shaderParticleValues;

	// Vertex Arrays
	SolverModel vaModel;
	VertexArray vaParticles;
	VertexArray vaPlane;
	VertexArray vaBox;

	// FBOs
	GLuint solverFBO;

	// Grid
	SolverGrid grid;

	// Textures
	GLuint gridTex;

	GLuint rigidBodyPositionsTex;
	GLuint rigidBodyQuaternionsTex;

	GLuint particlePositionsTex;
	GLuint particleVelocityTex;
	GLuint particleCoMTex; // Center of Mass

};

extern "C" OGL4COREPLUGIN_API RenderPlugin* OGL4COREPLUGIN_CALL CreateInstance(COGL4CoreAPI *Api) {
    return new RigidSolver(Api);
}
