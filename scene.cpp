////////////////////////////////////////////////////////////////////////
// The scene class contains all the parameters needed to define and
// draw a simple scene, including:
//   * Geometry
//   * Light parameters
//   * Material properties
//   * viewport size parameters
//   * Viewing transformation values
//   * others ...
//
// Some of these parameters are set when the scene is built, and
// others are set by the framework in response to user mouse/keyboard
// interactions.  All of them can be used to draw the scene.

const bool fullPolyCount = true; // Use false when emulating the graphics pipeline in software

#include "math.h"
#include <iostream>
#include <stdlib.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
using namespace gl;

#include <glu.h>                // For gluErrorString

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/ext.hpp>          // For printing GLM objects with to_string

#include "framework.h"
#include "shapes.h"
#include "object.h"
#include "texture.h"
#include "transform.h"

const float PI = 3.14159f;
const float rad = PI/180.0f;    // Convert degrees to radians

glm::mat4 Identity;

const float grndSize = 100.0;    // Island radius;  Minimum about 20;  Maximum 1000 or so
const float grndOctaves = 4.0;  // Number of levels of detail to compute
const float grndFreq = 0.03;    // Number of hills per (approx) 50m
const float grndPersistence = 0.03; // Terrain roughness: Slight:0.01  rough:0.05
const float grndLow = -3.0;         // Lowest extent below sea level
const float grndHigh = 5.0;        // Highest extent above sea level

////////////////////////////////////////////////////////////////////////
// This macro makes it easy to sprinkle checks for OpenGL errors
// throughout your code.  Most OpenGL calls can record errors, and a
// careful programmer will check the error status *often*, perhaps as
// often as after every OpenGL call.  At the very least, once per
// refresh will tell you if something is going wrong.
#define CHECKERROR {GLenum err = glGetError(); if (err != GL_NO_ERROR) { fprintf(stderr, "OpenGL error (at line scene.cpp:%d): %s\n", __LINE__, gluErrorString(err)); exit(-1);} }

// Create an RGB color from human friendly parameters: hue, saturation, value
glm::vec3 HSV2RGB(const float h, const float s, const float v)
{
    if (s == 0.0)
        return glm::vec3(v,v,v);

    int i = (int)(h*6.0) % 6;
    float f = (h*6.0f) - i;
    float p = v*(1.0f - s);
    float q = v*(1.0f - s*f);
    float t = v*(1.0f - s*(1.0f-f));
    if      (i == 0)     return glm::vec3(v,t,p);
    else if (i == 1)  return glm::vec3(q,v,p);
    else if (i == 2)  return glm::vec3(p,v,t);
    else if (i == 3)  return glm::vec3(p,q,v);
    else if (i == 4)  return glm::vec3(t,p,v);
    else   /*i == 5*/ return glm::vec3(v,p,q);
}

////////////////////////////////////////////////////////////////////////
// Constructs a hemisphere of spheres of varying hues
Object* SphereOfSpheres(Shape* SpherePolygons)
{
    Object* ob = new Object(NULL, nullId);
    
    for (float angle=0.0;  angle<360.0;  angle+= 18.0)
        for (float row=0.075;  row<PI/2.0;  row += PI/2.0/6.0) {   
            glm::vec3 hue = HSV2RGB(angle/360.0, 1.0f-2.0f*row/PI, 1.0f);

            Object* sp = new Object(SpherePolygons, spheresId,
                                    hue, glm::vec3(1.0, 1.0, 1.0), 120.0);
            float s = sin(row);
            float c = cos(row);
            ob->add(sp, Rotate(2,angle)*Translate(c,0,s)*Scale(0.075*c,0.075*c,0.075*c));
        }
    return ob;
}

////////////////////////////////////////////////////////////////////////
// Constructs a -1...+1  quad (canvas) framed by four (elongated) boxes
Object* FramedPicture(const glm::mat4& modelTr, const int objectId, 
                      Shape* BoxPolygons, Shape* QuadPolygons)
{
    // This draws the frame as four (elongated) boxes of size +-1.0
    float w = 0.05;             // Width of frame boards.
    
    Object* frame = new Object(NULL, nullId);
    Object* ob;
    
    glm::vec3 woodColor(87.0/255.0,51.0/255.0,35.0/255.0);
    ob = new Object(BoxPolygons, frameId,
                    woodColor, glm::vec3(0.2, 0.2, 0.2), 10.0);
    frame->add(ob, Translate(0.0, 0.0, 1.0+w)*Scale(1.0, w, w));
    frame->add(ob, Translate(0.0, 0.0, -1.0-w)*Scale(1.0, w, w));
    frame->add(ob, Translate(1.0+w, 0.0, 0.0)*Scale(w, w, 1.0+2*w));
    frame->add(ob, Translate(-1.0-w, 0.0, 0.0)*Scale(w, w, 1.0+2*w));

    ob = new Object(QuadPolygons, objectId,
                    woodColor, glm::vec3(0.0, 0.0, 0.0), 10.0);
    frame->add(ob, Rotate(0,90));

    return frame;
}

Scene::~Scene() {
    delete gbufferProgram;
    delete lightingProgram;
    delete shadowProgram;
    delete localLightsProgram;
    delete computeProgram_h;
    delete computeProgram_v;    

    delete irradianceMap;
    delete skyTex;
}

////////////////////////////////////////////////////////////////////////
// InitializeScene is called once during setup to create all the
// textures, shape VAOs, and shader programs as well as setting a
// number of other parameters.
void Scene::InitializeScene()
{
    // @@ Initialize interactive viewing variables here. (spin, tilt, ry, front back, ...)
    
    // Set initial light parameters
    lightSpin = 150.0;
    lightTilt = -45.0;
    lightDist = 100.0;
    // @@ Perhaps initialize additional scene lighting values here. (lightVal, lightAmb)
    lightVal = glm::vec3(3,     3,   3);
    lightAmb = glm::vec3(0.1, 0.1, 0.1);

    key = 0;
    nav = false;
    spin = 0.0;
    tilt = 30.0;
    eye = glm::vec3(0.0, -20.0, 0.0);
    speed = 300.0/30.0;
    last_time = glfwGetTime();
    tr = glm::vec3(0.0, 0.0, 25.0);

    ry = 0.4;
    front = 0.5;
    back = 5000.0;

    centerRadius = 15.0f;
    weights.assign(101, 0);

    specularOn = false;
    Exposure = 1.0f;
    block.N = 20.0;
    block.hammersley.assign(200, 0.0);
    glGenBuffers(1, &block.id);
    CHECKERROR;

    AO_contrast = 1.0f;
    AO_scale = 1.0f;
    CHECKERROR;

    objectRoot = new Object(NULL, nullId);

    // Create the lighting shader program from source code files.
    // @@ Initialize additional shaders if necessary
    gbufferProgram = new ShaderProgram();
    gbufferProgram->AddShader("shaders\\GBuffer.vert", GL_VERTEX_SHADER);
    gbufferProgram->AddShader("shaders\\GBuffer.frag", GL_FRAGMENT_SHADER);

    glBindAttribLocation(gbufferProgram->programId, 0, "vertex");
    glBindAttribLocation(gbufferProgram->programId, 1, "vertexNormal");
    glBindAttribLocation(gbufferProgram->programId, 2, "vertexTexture");
    glBindAttribLocation(gbufferProgram->programId, 3, "vertexTangent");
    gbufferProgram->LinkProgram();

    lightingProgram = new ShaderProgram();
    lightingProgram->AddShader("shaders\\Lighting.vert", GL_VERTEX_SHADER);
    lightingProgram->AddShader("shaders\\Lighting.frag", GL_FRAGMENT_SHADER);
    lightingProgram->AddShader("shaders\\BRDF_global.vert", GL_VERTEX_SHADER);
    lightingProgram->AddShader("shaders\\BRDF_global.frag", GL_FRAGMENT_SHADER);
    

    glBindAttribLocation(lightingProgram->programId, 0, "vertex");
    lightingProgram->LinkProgram();

    localLightsProgram = new ShaderProgram();
    localLightsProgram->AddShader("shaders\\LocalLights.vert", GL_VERTEX_SHADER);
    localLightsProgram->AddShader("shaders\\LocalLights.frag", GL_FRAGMENT_SHADER);
    localLightsProgram->AddShader("shaders\\BRDF_local.vert",  GL_VERTEX_SHADER);
    localLightsProgram->AddShader("shaders\\BRDF_local.frag",  GL_FRAGMENT_SHADER);

    glBindAttribLocation(localLightsProgram->programId, 0, "vertex");
    glBindAttribLocation(localLightsProgram->programId, 1, "vertexNormal");
    localLightsProgram->LinkProgram();

    shadowProgram = new ShaderProgram();
    shadowProgram->AddShader("shaders\\shadow.vert", GL_VERTEX_SHADER);
    shadowProgram->AddShader("shaders\\shadow.frag", GL_FRAGMENT_SHADER);

    glBindAttribLocation(shadowProgram->programId, 0, "vertex");
    shadowProgram->LinkProgram();

    computeProgram_h = new ShaderProgram();
    computeProgram_h->AddShader("shaders\\convolution_h.comp", GL_COMPUTE_SHADER);
    computeProgram_h->LinkProgram();

    computeShader_h = new ComputeShader();
    computeShader_h->SetShader(computeProgram_h);

    computeProgram_v = new ShaderProgram();
    computeProgram_v->AddShader("shaders\\convolution_v.comp", GL_COMPUTE_SHADER);
    computeProgram_v->LinkProgram();

    computeShader_v = new ComputeShader();
    computeShader_v->SetShader(computeProgram_v);

    preCalProgram = new ShaderProgram();
    preCalProgram->AddShader("shaders\\preCalculation.comp", GL_COMPUTE_SHADER);
    preCalProgram->LinkProgram();

    irrProgram = new ShaderProgram();
    irrProgram->AddShader("shaders\\irradiance.comp", GL_COMPUTE_SHADER);
    irrProgram->LinkProgram();
    CHECKERROR;
    
    // Create all the Polygon shapes
    proceduralground = new ProceduralGround(grndSize, 400,
                                     grndOctaves, grndFreq, grndPersistence,
                                     grndLow, grndHigh);
    
    Shape* TeapotPolygons =  new Teapot(fullPolyCount?12:2);
    Shape* BoxPolygons = new Box();
    Shape* SpherePolygons = new Sphere(32);
    Shape* RoomPolygons = new Ply("models//room.ply");
    Shape* FloorPolygons = new Plane(10.0, 10);
    Shape* QuadPolygons = new Quad();
    Shape* SeaPolygons = new Plane(2000.0, 50);
    Shape* GroundPolygons = proceduralground;

    Shape* BunnyPolygons  = new Ply("models//bunny.ply");
    //Shape* DragonPolygons = new Ply("models//dragon.ply");

    // Various colors used in the subsequent models
    glm::vec3 woodColor(87.0/255.0, 51.0/255.0, 35.0/255.0);
    glm::vec3 brickColor(134.0/255.0, 60.0/255.0, 56.0/255.0);
    glm::vec3 floorColor(6*16/255.0, 5.5*16/255.0, 3*16/255.0);
    glm::vec3 brassColor(0.5, 0.5, 0.1);
    glm::vec3 grassColor(62.0/255.0, 102.0/255.0, 38.0/255.0);
    glm::vec3 waterColor(0.3, 0.3, 1.0);

    glm::vec3 black(0.0, 0.0, 0.0);
    glm::vec3 brightSpec(0.5, 0.5, 0.5);
    glm::vec3 polishedSpec(0.3, 0.3, 0.3);
 
    // Creates all the models from which the scene is composed.  Each
    // is created with a polygon shape (possibly NULL), a
    // transformation, and the surface lighting parameters Kd, Ks, and
    // alpha.

    // @@ This is where you could read in all the textures and
    // associate them with the various objects being created in the
    // next dozen lines of code.

    // @@ To change an object's surface parameters (Kd, Ks, or alpha),
    // modify the following lines.
    
    central    = new Object(NULL, nullId);
    anim       = new Object(NULL, nullId);
    room = new Object(RoomPolygons, roomId, brickColor, black, 1); room->drawMe = false;
    floor      = new Object(FloorPolygons, floorId, floorColor, black, 200);
    teapot     = new Object(TeapotPolygons, teapotId, glm::vec3(1.0f, 0.0, 0.0), brightSpec, 120.0);
    //teapot     = new Object(TeapotPolygons, teapotId, brassColor, brightSpec, 120);
    podium     = new Object(BoxPolygons, boxId, glm::vec3(woodColor), polishedSpec, 10); 
    sky        = new Object(SpherePolygons, skyId, glm::vec3(1.0, 0.0, 0.0), black, 0);
    ground = new Object(GroundPolygons, groundId, grassColor, black, 1); ground->drawMe = false;
    sea = new Object(SeaPolygons, seaId, waterColor, brightSpec, 120); sea->drawMe = false;

    bunny  = new Object(BunnyPolygons, nullId, glm::vec3(1.0, 1.0, 1.0), brightSpec, 120.0);
    //dragon = new Object(DragonPolygons, nullId, glm::vec3(1.0, 1.0, 1.0), brightSpec, 120.0);

    leftFrame  = FramedPicture(Identity, lPicId, BoxPolygons, QuadPolygons);
    rightFrame = FramedPicture(Identity, rPicId, BoxPolygons, QuadPolygons); 
    spheres    = SphereOfSpheres(SpherePolygons);

    lightsRoot = new Object(NULL, nullId);
    localLight1 = new Object(SpherePolygons, nullId, glm::vec3(24.0, 0.0, 0.0), lightAmb, 1);
    localLight1->position = glm::vec3(-2.0, 0.0, 2.0);
    localLight1->range = 4.0;

    localLight2 = new Object(SpherePolygons, nullId, glm::vec3(64.0, 64.0, 64.0), lightAmb, 1);
    localLight2->position = glm::vec3(0.1, 0.0, 5.0);
    localLight2->range = 6.0;

    localLight3 = new Object(SpherePolygons, nullId, glm::vec3(0.0, 0.0, 16.0), lightAmb, 1);
    localLight3->position = glm::vec3(2.0, 0.0, 2.0);
    localLight3->range = 4.0;

#if REFL
    spheres->drawMe = true;
#else
    spheres->drawMe = false;
#endif


    // @@ To change the scene hierarchy, examine the hierarchy created
    // by the following object->add() calls and adjust as you wish.
    // The objects being manipulated and their polygon shapes are
    // created above here.

    // Scene is composed of sky, ground, sea, room and some central models
    if (fullPolyCount) {
        objectRoot->add(sky, Scale(2000.0, 2000.0, 2000.0));
        objectRoot->add(sea); 
        objectRoot->add(ground); }
    objectRoot->add(central);
#ifndef REFL
    objectRoot->add(room,  Translate(0.0, 0.0, 0.02));
#endif
    //objectRoot->add(floor, Translate(0.0, 0.0, 0.02));

    // Central model has a rudimentary animation (constant rotation on Z)
    animated.push_back(anim);

    // Central contains a teapot on a podium and an external sphere of spheres
    central->add(anim, Translate(0.0, 0,0));
 
    if (fullPolyCount)
        anim->add(spheres, Translate(0.0, 0.0, 0.0)*Scale(16, 16, 16));
    
    // Room contains two framed pictures
    if (fullPolyCount) {
        room->add(leftFrame, Translate(-1.5, 9.85, 1.)*Scale(0.8, 0.8, 0.8));
        room->add(rightFrame, Translate( 1.5, 9.85, 1.)*Scale(0.8, 0.8, 0.8)); }


    objectRoot->add(bunny, Translate(6.0, -6.0, 0.0) * Rotate(2, 45.0) * Scale(3.0, 3.0, 3.0));
    //objectRoot->add(dragon, Translate(-6.0, 6.0, 0.0) * Rotate(2, 45.0) * Scale(3.0, 3.0, 3.0));
    objectRoot->add(teapot, Identity);

    lightsRoot->add(localLight1, Translate(localLight1->position.x, localLight1->position.y, localLight1->position.z)
                                 * Scale(localLight1->range, localLight1->range, localLight1->range));
    lightsRoot->add(localLight2, Translate(localLight2->position.x, localLight2->position.y, localLight2->position.z)
                                 * Scale(localLight2->range, localLight2->range, localLight2->range));
    lightsRoot->add(localLight3, Translate(localLight3->position.x, localLight3->position.y, localLight3->position.z)
                                 * Scale(localLight3->range, localLight3->range, localLight3->range));
    CHECKERROR;

    // Options menu stuff
    show_demo_window = false;

    // FBO setup
    G_Buffer = new FBO();

    glfwGetFramebufferSize(window, &width, &height);
    G_Buffer->CreateGBuffer(width, height);
    CHECKERROR;

    drawID     = 0;
    flipToggle = 0;
    debugToggle = false;
    screen = new Screen();
    

    // shadow fbo setup
    shadowFBO = new FBO();
    shadowFBO->CreateFBO(1280, 1280, 4); // 0~3 are for G-Buffer
    CHECKERROR;

    hFBO = new FBO();
    hFBO->CreateFBO(1280, 1280, 5);
    CHECKERROR;

    vFBO = new FBO();
    vFBO->CreateFBO(1280, 1280, 6);
    CHECKERROR;

    // compute shader stuff
    blur_size = 4;

    irradianceMap = new Texture("skys//Newport_Loft_Ref.irr.hdr");
    skyTex = new Texture("skys//Newport_Loft_Ref.hdr");

    irrFBO = new FBO();
    irrFBO->CreateFBO(200, 100, 7);

    coeffFBO = new FBO();
    coeffFBO->CreateFBO(9, 1, 7);
    CHECKERROR;
}

void Scene::DrawMenu()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        // This menu demonstrates how to provide the user a list of toggleable settings.
        if (ImGui::BeginMenu("Objects")) {
            if (ImGui::MenuItem("Draw spheres", "", spheres->drawMe))  {spheres->drawMe ^= true; }
            if (ImGui::MenuItem("Draw walls", "", room->drawMe))       {room->drawMe ^= true; }
            if (ImGui::MenuItem("Draw ground/sea", "", ground->drawMe)){ground->drawMe ^= true;
            sea->drawMe = ground->drawMe;}
            ImGui::EndMenu(); }
                	
        // This menu demonstrates how to provide the user a choice
        // among a set of choices.  The current choice is stored in a
        // variable named "mode" in the application, and sent to the
        // shader to be used as you wish.
        
        if (ImGui::BeginMenu("Menu ")) {
            if (ImGui::MenuItem("<sample menu of choices>", "",	false, false)) {}
            if (ImGui::MenuItem("Do nothing 0", "",		mode==0)) { mode=0; }
            if (ImGui::MenuItem("Do nothing 1", "",		mode==1)) { mode=1; }
            if (ImGui::MenuItem("Do nothing 2", "",		mode==2)) { mode=2; }
            ImGui::SliderInt("Switch", &drawID, 0, 4);
            ImGui::SliderInt("Toggle", &flipToggle, 0, 2);

            ImGui::Checkbox("specular", &specularOn);
            ImGui::DragFloat("exposure", &Exposure, 0.1f, 0.0f, 10.0f, "%.1f");
            ImGui::DragFloat("Number of paris", &block.N, 1.0f, 0.0f, 100.0f);
            ImGui::DragFloat("AO_scale", &AO_scale, 1.0f, 0.0f, 100.0f);
            ImGui::DragFloat("AO_contrast", &AO_contrast, 1.0f, 0.0f, 100.0f);
            ImGui::EndMenu(); }
        
        ImGui::EndMainMenuBar(); }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Scene::BuildTransforms()
{
    
    // Work out the eye position as the user move it with the WASD keys.
    float now = glfwGetTime();
    float dist = (now-last_time)*speed;
    last_time = now;
    if (key == GLFW_KEY_W)
        eye += dist*glm::vec3(sin(spin*rad), cos(spin*rad), 0.0);
    if (key == GLFW_KEY_S)
        eye -= dist*glm::vec3(sin(spin*rad), cos(spin*rad), 0.0);
    if (key == GLFW_KEY_D)
        eye += dist*glm::vec3(cos(spin*rad), -sin(spin*rad), 0.0);
    if (key == GLFW_KEY_A)
        eye -= dist*glm::vec3(cos(spin*rad), -sin(spin*rad), 0.0);

    eye[2] = proceduralground->HeightAt(eye[0], eye[1]) + 2.0;

    CHECKERROR;

    if (nav)
        WorldView = Rotate(0, tilt-90)*Rotate(2, spin) *Translate(-eye[0], -eye[1], -eye[2]);
    else
        WorldView = Translate(tr[0], tr[1], -tr[2]) *Rotate(0, tilt-90)*Rotate(2, spin);
    WorldProj = Perspective((ry*width)/height, ry, front, (mode==0) ? back : 1000);


    // @@ Print the two matrices (in column-major order) for
    // comparison with the project document.
    //std::cout << "WorldView: " << glm::to_string(WorldView) << std::endl;
    //std::cout << "WorldProj: " << glm::to_string(WorldProj) << std::endl;
}

void Scene::CalculateSH()
{
    unsigned int programId, loc;
    preCalProgram->UseShader();
    programId = irrProgram->programId;

    loc = glGetUniformLocation(programId, "src");
    glBindImageTexture(0, skyTex->textureId, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glUniform1i(loc, 0);

    loc = glGetUniformLocation(programId, "dst");
    glBindImageTexture(1, coeffFBO->textureId, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUniform1i(loc, 1);

    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    preCalProgram->UnuseShader();

    irrProgram->UseShader();
    programId = irrProgram->programId;

    loc = glGetUniformLocation(programId, "src");
    glBindImageTexture(2, coeffFBO->textureId, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

    loc = glGetUniformLocation(programId, "dst");
    glBindImageTexture(3, irrFBO->textureId, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    glDispatchCompute(irrFBO->width, irrFBO->height, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    irrProgram->UnuseShader();
}

////////////////////////////////////////////////////////////////////////
// Procedure DrawScene is called whenever the scene needs to be
// drawn. (Which is often: 30 to 60 times per second are the common
// goals.)
void Scene::DrawScene()
{
    // Set the viewport
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    CHECKERROR;
    // Calculate the light's position from lightSpin, lightTilt, lightDist
    lightPos = glm::vec3(lightDist*cos(lightSpin*rad)*sin(lightTilt*rad),
                         lightDist*sin(lightSpin*rad)*sin(lightTilt*rad), 
                         lightDist*cos(lightTilt*rad));

    float dist = glm::distance(lightPos, centerPos);
    float minDist = dist - centerRadius;
    float maxDist = dist + centerRadius;

    // comput LookAt Maxtrix
    lookAtPos = glm::vec3(0.1, 0.0, 1.5);
    glm::vec3 lookAtV = glm::normalize(lookAtPos - lightPos);
    glm::vec3 lookAtA = glm::normalize(glm::cross(lookAtV, glm::vec3(0.0, 0.0, 1.0)));
    glm::vec3 lookAtB = glm::cross(lookAtA, lookAtV);    
    float lightRy = 40.0 / lightDist;
    float lightRx = lightRy;
    glm::mat4 ProjectionMatrix = Perspective(lightRx, lightRy, front, back);
    glm::mat4 ViewMatrix = glm::mat4(lookAtA.x, lookAtB.x, -lookAtV.x, 0.0,
                                     lookAtA.y, lookAtB.y, -lookAtV.y, 0.0,
                                     lookAtA.z, lookAtB.z, -lookAtV.z, 0.0,
                                     0.0, 0.0, 0.0, 1.0);
    ViewMatrix *= Translate(-lightPos.x, -lightPos.y, -lightPos.z);
    ShadowMatrix = Translate(0.5, 0.5, 0.5) * Scale(0.5, 0.5, 0.5);
    ShadowMatrix = ShadowMatrix * ProjectionMatrix * ViewMatrix;

    // Update position of any continuously animating objects
    double atime = 360.0*glfwGetTime()/36;
    for (std::vector<Object*>::iterator m=animated.begin();  m<animated.end();  m++)
        (*m)->animTr = Rotate(2, atime);

    BuildTransforms();

    // The lighting algorithm needs the inverse of the WorldView matrix
    WorldInverse = glm::inverse(WorldView);
    CHECKERROR;

    int loc, programId;

    ///////////////////
    // G-Buffer pass //
    ///////////////////

    // Enable & Disable
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);    
    CHECKERROR;

    // Choose the shader
    gbufferProgram->UseShader();
    programId = gbufferProgram->programId;

    // bind FBO
    G_Buffer->BindFBO();
    CHECKERROR;

    // Set the viewport, and clear the screen
    glViewport(0, 0, width, height);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);

    // Bind Texture
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, skyTex->textureId);



    // Uniforms
    loc = glGetUniformLocation(programId, "WorldProj");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldProj));
    loc = glGetUniformLocation(programId, "WorldView");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldView));
    loc = glGetUniformLocation(programId, "WorldInverse");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldInverse));
    loc = glGetUniformLocation(programId, "skyTex");
    glUniform1i(loc, 11);
    CHECKERROR;

    // Draw all objects
    objectRoot->Draw(gbufferProgram, Identity);
    CHECKERROR; 

    // unbind texture
    skyTex->UnbindTexture(11);

    // unbind FBO
    G_Buffer->UnbindFBO();
    
    // Turn off the shader
    gbufferProgram->UnuseShader();

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Shadow pass
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    shadowProgram->UseShader();
    programId = shadowProgram->programId;

    shadowFBO->BindFBO();

    // Set the viewport, and clear the screen
    glViewport(0, 0, shadowFBO->width, shadowFBO->height);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    loc = glGetUniformLocation(programId, "ViewMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ViewMatrix));
    loc = glGetUniformLocation(programId, "ProjectionMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ProjectionMatrix));
    loc = glGetUniformLocation(programId, "minDist");
    glUniform1f(loc, minDist);
    loc = glGetUniformLocation(programId, "maxDist");
    glUniform1f(loc, maxDist);
    CHECKERROR;

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    // Draw all objects (This recursively traverses the object hierarchy.)
    objectRoot->Draw(shadowProgram, Identity);
    CHECKERROR;

    glDisable(GL_CULL_FACE);

    //Unbind FBO
    shadowFBO->UnbindFBO();

    // Turn off the shader
    shadowProgram->UnuseShader();



    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // compute shader pass horizontal
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // calculate weight for shadow blur
    
    float sum = 0.0f;
    if (blur_size == 0) {
        weights[0] = 1.0;
    }
    else {
        for (int i = -blur_size; i <= blur_size; ++i) {
            float s = (float)blur_size / 2.0;            
            weights[i + blur_size] = powf(glm::e<float>(), -0.5 * ((float)i / s) * ((float)i / s));
            sum += weights[i + blur_size];
        }

        // normalize weight
        for (auto& w : weights) { w /= sum; }
    }

    unsigned int bindPoint = 0;
    unsigned int blockID   = 0;

    computeProgram_h->UseShader();
    programId = computeProgram_h->programId;

    // bind fbo
    shadowFBO->BindTexture(4, programId, "shadowMap");
    hFBO->BindTexture(5, programId, "msmH");

    bindPoint = computeShader_v->GetBindPoint();
    blockID = computeShader_h->GetBlockID();

    loc = glGetUniformBlockIndex(programId, "blurKernel");
    glUniformBlockBinding(programId, loc, bindPoint);

    glBindBuffer(GL_UNIFORM_BUFFER, blockID);
    glBindBufferBase(GL_UNIFORM_BUFFER, bindPoint, blockID);

    glBufferData(GL_UNIFORM_BUFFER, (2*blur_size + 1) * sizeof(float), weights.data(), GL_STATIC_DRAW);

    loc = glGetUniformLocation(programId, "w");
    glUniform1i(loc, blur_size);

    loc = glGetUniformLocation(programId, "src");
    glBindImageTexture(0, shadowFBO->textureId, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glUniform1i(loc, 0);

    loc = glGetUniformLocation(programId, "dst");
    glBindImageTexture(1, hFBO->textureId, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUniform1i(loc, 1);

    glDispatchCompute(shadowFBO->width / 128, shadowFBO->height, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    //unbind fbo
    shadowFBO->UnbindFBO();
    hFBO->UnbindFBO();
    computeProgram_h->UnuseShader();

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // compute shader pass vertical
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    computeProgram_v->UseShader();
    programId = computeProgram_h->programId;
    
    // bind fbo
    hFBO->BindTexture(5, programId, "msmH");
    vFBO->BindTexture(6, programId, "msmV");

    bindPoint = computeShader_h->GetBindPoint();
    blockID = computeShader_h->GetBlockID();

    loc = glGetUniformBlockIndex(programId, "blurKernel");
    glUniformBlockBinding(programId, loc, bindPoint);

    glBindBuffer(GL_UNIFORM_BUFFER, blockID);
    glBindBufferBase(GL_UNIFORM_BUFFER, bindPoint, blockID);

    glBufferData(GL_UNIFORM_BUFFER, (2 * blur_size + 1) * sizeof(float), weights.data(), GL_STATIC_DRAW);

    loc = glGetUniformLocation(programId, "w");
    glUniform1i(loc, blur_size);

    loc = glGetUniformLocation(programId, "src");
    glBindImageTexture(0, hFBO->textureId, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glUniform1i(loc, 0);

    loc = glGetUniformLocation(programId, "dst");
    glBindImageTexture(1, vFBO->textureId, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUniform1i(loc, 1);

    glDispatchCompute(vFBO->width, vFBO->height / 128, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    //unbind fbo
    hFBO->UnbindFBO();
    vFBO->UnbindFBO();    
    computeProgram_v->UnuseShader();

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // lighting pass 
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // build block for Monte Carlo
    int pos = 0;
    int num_pairs = static_cast<int>(block.N);
    for (int k = 0; k < num_pairs; ++k) {
        float p = 0.5f;
        float u = 0.0f;
        for (int kk = k; kk; p *= 0.5f, kk >>= 1) {
            if (kk & 1)
                u += p;
        }

        float v = (k + 0.5) / num_pairs;
        block.hammersley[pos++] = u;
        block.hammersley[pos++] = v;
    }

    // Enable & Disable
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    CHECKERROR;

    // Choose Shader
    lightingProgram->UseShader();
    programId = lightingProgram->programId;

    // Set the viewport, and clear the screen
    glViewport(0, 0, width, height);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CHECKERROR;

    // bind texture
    G_Buffer->BindTexture( 0, programId, "g_buffer_world_pos");
    G_Buffer->BindTexture( 1, programId, "g_buffer_world_norm");
    G_Buffer->BindTexture( 2, programId, "g_buffer_diffuse_color");
    G_Buffer->BindTexture( 3, programId, "g_buffer_specular_color");
    shadowFBO->BindTexture(4, programId, "shadowMap");
    hFBO->BindTexture(5, programId, "msmH");
    vFBO->BindTexture(6, programId, "MSMap");

    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, irradianceMap->textureId);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, skyTex->textureId);
    irrFBO->BindTexture(7, programId, "SHCoeff");
    CHECKERROR;

    // for BRDF
    loc = glGetUniformLocation(programId, "WorldInverse");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldInverse));
    loc = glGetUniformLocation(programId, "lightPos");
    glUniform3fv(loc, 1, &(lightPos[0]));
    loc = glGetUniformLocation(programId, "lightVal");
    glUniform3fv(loc, 1, &(lightVal[0]));
    loc = glGetUniformLocation(programId, "lightAmb");
    glUniform3fv(loc, 1, &(lightAmb[0]));

    // for final output
    loc = glGetUniformLocation(programId, "width");
    glUniform1ui(loc, width);
    loc = glGetUniformLocation(programId, "height");
    glUniform1ui(loc, height);
    loc = glGetUniformLocation(programId, "ID");
    glUniform1i(loc, drawID);
    loc = glGetUniformLocation(programId, "Toggle");
    glUniform1i(loc, flipToggle);
    loc = glGetUniformLocation(programId, "ShadowMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));
    CHECKERROR;

    loc = glGetUniformLocation(programId, "minDist");
    glUniform1f(loc, minDist);
    loc = glGetUniformLocation(programId, "maxDist");
    glUniform1f(loc, maxDist);
    CHECKERROR;

    loc = glGetUniformLocation(programId, "exposure");
    glUniform1f(loc, Exposure);

    // irradiance map & skydome texture
    loc = glGetUniformLocation(programId, "irrMap");
    glUniform1i(loc, 10);
    loc = glGetUniformLocation(programId, "skyTex");
    glUniform1i(loc, 11);
    loc = glGetUniformLocation(programId, "skyWidth");
    glUniform1f(loc, static_cast<float>(skyTex->width));
    loc = glGetUniformLocation(programId, "skyHeight");
    glUniform1f(loc, static_cast<float>(skyTex->height));

    // for random points
    unsigned int bindpoint = 1; 
    glBindBufferBase(GL_UNIFORM_BUFFER, bindpoint, block.id);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(block), &block, GL_STATIC_DRAW);
    loc = glGetUniformBlockIndex(programId, "HammersleyBlock");
    glUniformBlockBinding(programId, loc, bindpoint);
    loc = glGetUniformLocation(programId, "specularOn");
    glUniform1ui(loc, specularOn);

    // for renderdoc to see the value
    loc = glGetUniformLocation(programId, "testblock");
    glUniform1fv(loc, block.hammersley.size(), &(block.hammersley[0]));
    CHECKERROR;

    loc = glGetUniformLocation(programId, "AO_scale");
    glUniform1f(loc, AO_scale);
    glUniform1f(loc, AO_scale);
    loc = glGetUniformLocation(programId, "AO_contrast");
    glUniform1f(loc, AO_contrast);
    CHECKERROR;

    //loc = glGetUniformLocation(programId, "SH");
    //glBindImageTexture(7, irrFBO->textureId, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    //glUniform1i(loc, 7);
    CHECKERROR;

    screen->DrawVAO();
    CHECKERROR;

    // unbind textures
    G_Buffer->UnbindTexture(0);
    G_Buffer->UnbindTexture(1);
    G_Buffer->UnbindTexture(2);
    G_Buffer->UnbindTexture(3);
    shadowFBO->UnbindTexture(4);
    hFBO->UnbindTexture(5);
    vFBO->UnbindTexture(6);
    irrFBO->UnbindTexture(7);
    
    irradianceMap->UnbindTexture(10);
    skyTex->UnbindTexture(11);    
    CHECKERROR;

    // Turn off the shader
    lightingProgram->UnuseShader();
}
