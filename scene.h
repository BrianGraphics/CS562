////////////////////////////////////////////////////////////////////////
// The scene class contains all the parameters needed to define and
// draw a simple scene, including:
//   * Geometry
//   * Light parameters
//   * Material properties
//   * Viewport size parameters
//   * Viewing transformation values
//   * others ...
//
// Some of these parameters are set when the scene is built, and
// others are set by the framework in response to user mouse/keyboard
// interactions.  All of them can be used to draw the scene.

#include "shapes.h"
#include "object.h"
#include "texture.h"
#include "fbo.h"
#include "ComputeShader.h"

enum ObjectIds {
    nullId	= 0,
    skyId	= 1,
    seaId	= 2,
    groundId	= 3,
    roomId	= 4,
    boxId	= 5,
    frameId	= 6,
    lPicId	= 7,
    rPicId	= 8,
    teapotId	= 9,
    spheresId	= 10,
    floorId     = 11
};

class Shader;

// N: the number of pairs of point
struct RND_Points {
    float N;
    unsigned int id;
    std::vector<float> hammersley;
};

struct SSBO {
    int N;
    unsigned int id;
    std::vector<glm::vec4> data;
};

class Scene
{
public:
    GLFWwindow* window;

    // @@ Declare interactive viewing variables here. (spin, tilt, ry, front back, ...)

    // Light parameters
    float lightSpin, lightTilt, lightDist;
    glm::vec3 lightPos;
    // @@ Perhaps declare additional scene lighting values here. (lightVal, lightAmb)
    glm::vec3 lightVal;
    glm::vec3 lightAmb;


    bool drawReflective;
    bool nav;
    int key;
    float spin, tilt, speed, ry, front, back;
    glm::vec3 eye, tr;
    float last_time;
    int smode; // Shadow on/off/debug mode
    int rmode; // Extra reflection indicator hooked up some keys and sent to shader
    int lmode; // BRDF mode
    int tmode; // Texture mode
    int imode; // Image Based Lighting mode
    bool flatshade;
    int mode; // Extra mode indicator hooked up to number keys and sent to shader
    
    // Viewport
    int width, height;

    // Transformations
    glm::mat4 WorldProj, WorldView, WorldInverse;

    // All objects in the scene are children of this single root object.
    Object* objectRoot;
    Object *central, *anim, *room, *floor, *teapot, *podium, *sky,
           *ground, *sea, *spheres, *leftFrame, *rightFrame,
           *bunny, *dragon;

    Object* lightsRoot;
    Object* localLight1, *localLight2, *localLight3;

    std::vector<Object*> animated;
    ProceduralGround* proceduralground;

    // Shader programs
    ShaderProgram* gbufferProgram;
    ShaderProgram* lightingProgram;
    ShaderProgram* localLightsProgram;
    ShaderProgram* shadowProgram;
    ShaderProgram* computeProgram_v, *computeProgram_h;
    ShaderProgram* preCalProgram, *irrProgram;


    // Options menu stuff
    bool show_demo_window;

    // fbos
    FBO* G_Buffer;
    int drawID;
    int flipToggle;
    Shape* screen;
    bool debugToggle;

    // shadow 
    glm::vec3 lookAtPos;
    glm::mat4 ShadowMatrix;
    FBO* shadowFBO;
    ComputeShader* computeShader_v;
    ComputeShader* computeShader_h;
    int blur_size;
    std::vector<float> weights;
    FBO* vFBO, *hFBO;
    glm::vec3 centerPos;
    float centerRadius;

    float Contrast;
    float Exposure;
    Texture* irradianceMap, *skyTex;
    bool specularOn;
    RND_Points block;
    FBO* coeffFBO;
    FBO* irrFBO;
    SSBO ssbo;
    SSBO uniSSBO;

    void InitializeScene();
    void BuildTransforms();
    void CalculateSH();
    void DrawMenu();
    void DrawScene();
    ~Scene();
};
