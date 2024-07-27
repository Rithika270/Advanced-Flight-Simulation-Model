#include "GLViewNewModule.h"
#include "WorldList.h"
#include "ManagerOpenGLState.h"
#include "Axes.h"
#include "PhysicsEngineODE.h"
#include <irrKlang.h>
#include <chrono>

// Different WO used by this module
#include "WO.h"
#include "WOStatic.h"
#include "WOStaticPlane.h"
#include "WOStaticTrimesh.h"
#include "WOTrimesh.h"
#include "WOHumanCyborg.h"
#include "WOHumanCal3DPaladin.h"
#include "WOWayPointSpherical.h"
#include "WOLight.h"
#include "WOSkyBox.h"
#include "WOCar1970sBeater.h"
#include "Camera.h"
#include "CameraStandard.h"
#include "CameraChaseActorSmooth.h"
#include "CameraChaseActorAbsNormal.h"
#include "CameraChaseActorRelNormal.h"
#include "Model.h"
#include "ModelDataShared.h"
#include "ModelMesh.h"
#include "ModelMeshDataShared.h"
#include "ModelMeshSkin.h"
#include "WONVStaticPlane.h"
#include "WONVPhysX.h"
#include "WONVDynSphere.h"
#include "WOImGui.h"
#include "AftrImGuiIncludes.h"
#include "AftrGLRendererBase.h"

using namespace Aftr;

GLViewNewModule* GLViewNewModule::New(const std::vector<std::string>& args)
{
    GLViewNewModule* glv = new GLViewNewModule(args);
    glv->init(Aftr::GRAVITY, Vector(0, 0, -1.0f), "aftr.conf", PHYSICS_ENGINE_TYPE::petODE);
    glv->onCreate();
    return glv;
}

GLViewNewModule::GLViewNewModule(const std::vector<std::string>& args) : GLView(args)
{
    count = 0;
    soundEngine = irrklang::createIrrKlangDevice();

    thrust = 0.0f;
    roll = 0.0f;
    pitch = 0.0f;
    yaw = 0.0f;
    takeOff = false;

    lastUpdateTime = std::chrono::high_resolution_clock::now();
}

void GLViewNewModule::onCreate()
{
    if (this->pe != NULL)
    {
        this->pe->setGravityNormalizedVector(Vector(0, 0, -1.0f));
        this->pe->setGravityScalar(0.1f);
    }
    this->setActorChaseType(STANDARDEZNAV);
    lastPosition = initialPosition;
}

GLViewNewModule::~GLViewNewModule()
{
    if (soundEngine)
    {
        soundEngine->drop();
    }
}

void GLViewNewModule::updateWorld()
{
    GLView::updateWorld();

    float deltaTime = getDeltaTime();

    if (recording)
    {
        recordFlightPath();
    }

    if (playingBack)
    {
        playbackFlightPath();
    }

    if (takeOff)
    {
        if (jet == nullptr)
        {
            printf("Jet is not initialized.\n");
            return;
        }

        Vector lookDirection = jet->getLookDirection();
        Vector thrustVector = lookDirection * thrust * 0.1f;
        jet->moveRelative(thrustVector);

        if (jet->getPosition().x > 30)
        {
            jet->moveRelative(Vector(0, 0, thrust * 0.05f));
        }

        checkCollision();
        updateFlightStats();
    }

    if (jet != nullptr)
    {
        jet->rotateAboutGlobalX(pitch);
        jet->rotateAboutGlobalY(roll);
        jet->rotateAboutGlobalZ(yaw);
    }

    updateCamera(); // Update the camera position and orientation

    pitch = 0.0f;
    roll = 0.0f;
    yaw = 0.0f;
}

void GLViewNewModule::onResizeWindow(GLsizei width, GLsizei height)
{
    GLView::onResizeWindow(width, height);
}

void GLViewNewModule::onMouseDown(const SDL_MouseButtonEvent& e)
{
    GLView::onMouseDown(e);
}

void GLViewNewModule::onMouseUp(const SDL_MouseButtonEvent& e)
{
    GLView::onMouseUp(e);
}

void GLViewNewModule::onMouseMove(const SDL_MouseMotionEvent& e)
{
    GLView::onMouseMove(e);
}

void GLViewNewModule::onKeyDown(const SDL_KeyboardEvent& key)
{
    GLView::onKeyDown(key);
    if (key.keysym.sym == SDLK_0)
        this->setNumPhysicsStepsPerRender(1);

    if (key.keysym.sym == SDLK_1)
    {
        startTakeoff();
    }

    if (key.keysym.sym == SDLK_SPACE)
    {
        if (!isCubePlaced)
        {
            std::string shinyRedPlasticCubeModel(ManagerEnvironmentConfiguration::getSMM() + "/models/cube4x4x4redShinyPlastic_pp.wrl");
            shinyRedPlasticCube = WO::New(shinyRedPlasticCubeModel, Vector(1, 1, 1), MESH_SHADING_TYPE::mstFLAT);
            shinyRedPlasticCube->setPosition(Vector(10, 0, 1.1f)); // Place the cube just above the runway
            shinyRedPlasticCube->renderOrderType = RENDER_ORDER_TYPE::roOPAQUE;
            shinyRedPlasticCube->setLabel("Shiny Red Plastic Cube");
            worldLst->push_back(shinyRedPlasticCube);
            isCubePlaced = true;
        }
        else
        {
            worldLst->eraseViaWOptr(shinyRedPlasticCube);
            shinyRedPlasticCube = nullptr;
            isCubePlaced = false;
        }
    }

    if (key.keysym.sym == SDLK_c)
    {
        soundEngine->stopAllSounds();
    }

    if (key.keysym.sym == SDLK_r)
    {
        recording = !recording;
    }

    if (key.keysym.sym == SDLK_p)
    {
        playingBack = !playingBack;
        playbackIndex = 0;
    }
}

void GLViewNewModule::onKeyUp(const SDL_KeyboardEvent& key)
{
    GLView::onKeyUp(key);
}

bool GLViewNewModule::checkCollision()
{
    if (shinyRedPlasticCube != nullptr && jet != nullptr)
    {
        Vector jetPos = jet->getPosition();
        Vector cubePos = shinyRedPlasticCube->getPosition();

        float distance = (jetPos - cubePos).length();
        float collisionDistance = 8.0f;

        if (distance < collisionDistance)
        {
            handleCollision();
            return true;
        }
    }
    return false;
}

void GLViewNewModule::handleCollision()
{
    takeOff = false;
    collisionDetected = true;
    collisionCooldown = 1.0f;
    printf("Collision detected! Stopping jet.");

    soundEngine->play2D((ManagerEnvironmentConfiguration::getLMM() + "/sounds/explosion.wav").c_str(), false);

    thrust = 0.0f;
}

void GLViewNewModule::resetFlight()
{
    thrust = 0.0f;
    roll = 0.0f;
    pitch = 0.0f;
    yaw = 0.0f;
    takeOff = false;
    jet->setPosition(initialPosition);
    jet->rotateToIdentity();
    soundEngine->stopAllSounds();
    totalDistance = 0.0f;
    altitude = 0.0f;
    speed = 0.0f;
    lastPosition = initialPosition;
    updateCamera(); // Reset the camera
}

void GLViewNewModule::startTakeoff()
{
    if (isCubePlaced && checkCollision())
    {
        printf("Warning: Collision detected with the obstacle!\n");
    }
    else if (!takeOff)
    {
        takeOff = true;
        thrust = 1.0f;
        count = ++count;

        if (count == 1)
        {
            this->objSnd = soundEngine->play3D((ManagerEnvironmentConfiguration::getLMM() + "/sounds/airplane-fly-by-01a.wav").c_str(), irrklang::vec3df(this->cam->getPosition().x, this->cam->getPosition().y, this->cam->getPosition().z), true);
        }

        if (this->objSnd != nullptr && count == 1)
        {
            this->objSnd->setPosition(irrklang::vec3df(jet->getPosition().x, jet->getPosition().y, jet->getPosition().z));
        }

        printf("Plane taking off!\n");
    }
}

void GLViewNewModule::recordFlightPath()
{
    flightPath.push_back({ jet->getPosition(), jet->getLookDirection() });
}

void GLViewNewModule::playbackFlightPath()
{
    if (playbackIndex < flightPath.size())
    {
        jet->setPosition(flightPath[playbackIndex].position);
        // Calculate the orientation from the look direction
        Vector lookDirection = flightPath[playbackIndex].orientation;
        float pitchAngle = atan2(lookDirection.z, sqrt(lookDirection.x * lookDirection.x + lookDirection.y * lookDirection.y));
        float yawAngle = atan2(lookDirection.y, lookDirection.x);

        jet->rotateAboutGlobalX(pitchAngle);
        jet->rotateAboutGlobalZ(yawAngle);

        playbackIndex++;
    }
    else
    {
        playingBack = false;
        printf("Playback finished.\n");
    }
}

void GLViewNewModule::toggleDayNight()
{
    isDay = !isDay;
    if (skyBox != nullptr)
    {
        worldLst->eraseViaWOptr(skyBox);
    }

    if (isDay)
    {
        skyBox = WOSkyBox::New(ManagerEnvironmentConfiguration::getSMM() + "/images/skyboxes/sky_mountains+6.jpg", this->getCameraPtrPtr());
    }
    else
    {
        skyBox = WOSkyBox::New(ManagerEnvironmentConfiguration::getSMM() + "/images/skyboxes/space_gray_matter+6.jpg", this->getCameraPtrPtr());
    }

    skyBox->setPosition(Vector(0, 0, 0));
    skyBox->setLabel("Sky Box");
    skyBox->renderOrderType = RENDER_ORDER_TYPE::roOPAQUE;
    worldLst->push_back(skyBox);
}

Vector GLViewNewModule::calculateRotationAngles(const Vector& direction)
{
    float pitch = atan2(direction.z, sqrt(direction.x * direction.x + direction.y * direction.y));
    float yaw = atan2(direction.y, direction.x);
    return Vector(pitch, yaw, 0);
}

void GLViewNewModule::updateFlightStats()
{
    altitude = jet->getPosition().z;
    Vector currentPosition = jet->getPosition();
    Vector distanceVector = currentPosition - lastPosition;
    float distanceTraveled = distanceVector.length();
    totalDistance += distanceTraveled;
    speed = distanceTraveled / getDeltaTime();
    lastPosition = currentPosition;
}

float GLViewNewModule::getDeltaTime()
{
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> deltaTime = now - lastUpdateTime;
    lastUpdateTime = now;
    return deltaTime.count();
}

void GLViewNewModule::updateCamera()
{
    if (jet != nullptr)
    {
        Vector jetPosition = jet->getPosition();
        Vector lookDirection = jet->getLookDirection();

        // Adjust the camera position to be behind and slightly above the jet
        Vector cameraPosition = jetPosition - lookDirection * 10 + Vector(0, 0, 5);

        this->cam->setPosition(cameraPosition);

        // Calculate the camera's new look direction
        Vector cameraLookDirection = jetPosition - cameraPosition;
        cameraLookDirection.normalize();

        // Update the camera's orientation to look at the jet
        this->cam->setCameraLookDirection(cameraLookDirection);
    }
}

void Aftr::GLViewNewModule::loadMap()
{
    this->worldLst = new WorldList();
    this->actorLst = new WorldList();
    this->netLst = new WorldList();

    ManagerOpenGLState::GL_CLIPPING_PLANE = 1000.0;
    ManagerOpenGLState::GL_NEAR_PLANE = 0.1f;
    ManagerOpenGLState::enableFrustumCulling = false;
    Axes::isVisible = true;
    this->glRenderer->isUsingShadowMapping(false);

    this->cam->setPosition(15, 15, 10);

    std::string shinyRedPlasticCube(ManagerEnvironmentConfiguration::getSMM() + "/models/cube4x4x4redShinyPlastic_pp.wrl");
    std::string wheeledCar(ManagerEnvironmentConfiguration::getSMM() + "/models/rcx_treads.wrl");
    std::string grass(ManagerEnvironmentConfiguration::getSMM() + "/models/grassFloor400x400_pp.wrl");
    std::string human(ManagerEnvironmentConfiguration::getSMM() + "/models/human_chest.wrl");
    std::string jetModel(ManagerEnvironmentConfiguration::getSMM() + "/models/jet_wheels_down_PP.wrl");
    std::string runwayTexture(ManagerEnvironmentConfiguration::getSMM() + "/models/road26x10.wrl");

    std::vector<std::string> skyBoxImageNames;
    skyBoxImageNames.push_back(ManagerEnvironmentConfiguration::getSMM() + "/images/skyboxes/sky_mountains+6.jpg");

    {
        float ga = 0.1f;
        ManagerLight::setGlobalAmbientLight(aftrColor4f(ga, ga, ga, 1.0f));
        WOLight* light = WOLight::New();
        light->isDirectionalLight(true);
        light->setPosition(Vector(0, 0, 100));
        light->getModel()->setDisplayMatrix(Mat4::rotateIdentityMat({ 0, 1, 0 }, 90.0f * Aftr::DEGtoRAD));
        light->setLabel("Light");
        worldLst->push_back(light);
    }

    {
        WO* wo = WOSkyBox::New(skyBoxImageNames.at(0), this->getCameraPtrPtr());
        wo->setPosition(Vector(0, 0, 0));
        wo->setLabel("Sky Box");
        wo->renderOrderType = RENDER_ORDER_TYPE::roOPAQUE;
        worldLst->push_back(wo);
        skyBox = wo;
    }

    WO* grassPlane = WO::New(grass, Vector(1, 1, 1), MESH_SHADING_TYPE::mstFLAT);
    grassPlane->setPosition(Vector(0, 0, 0));
    grassPlane->renderOrderType = RENDER_ORDER_TYPE::roOPAQUE;
    grassPlane->upon_async_model_loaded([grassPlane]()
        {
            ModelMeshSkin& grassSkin = grassPlane->getModel()->getModelDataShared()->getModelMeshes().at(0)->getSkins().at(0);
            grassSkin.getMultiTextureSet().at(0).setTexRepeats(5.0f);
            grassSkin.setAmbient(aftrColor4f(0.4f, 0.4f, 0.4f, 1.0f));
            grassSkin.setDiffuse(aftrColor4f(1.0f, 1.0f, 1.0f, 1.0f));
            grassSkin.setSpecular(aftrColor4f(0.4f, 0.4f, 0.4f, 1.0f));
            grassSkin.setSpecularCoefficient(10);
        });
    grassPlane->setLabel("Grass");
    worldLst->push_back(grassPlane);

    WO* runway = WO::New(runwayTexture, Vector(10, 1, 1), MESH_SHADING_TYPE::mstFLAT);
    runway->setPosition(Vector(0, 0, 0.1f));
    runway->renderOrderType = RENDER_ORDER_TYPE::roOPAQUE;
    runway->upon_async_model_loaded([runway]()
        {
            ModelMeshSkin& runwaySkin = runway->getModel()->getModelDataShared()->getModelMeshes().at(0)->getSkins().at(0);
            runwaySkin.getMultiTextureSet().at(0).setTexRepeats(1.0f);
            runwaySkin.setAmbient(aftrColor4f(0.5f, 0.5f, 0.5f, 1.0f));
            runwaySkin.setDiffuse(aftrColor4f(1.0f, 1.0f, 1.0f, 1.0f));
            runwaySkin.setSpecular(aftrColor4f(0.4f, 0.4f, 0.4f, 1.0f));
            runwaySkin.setSpecularCoefficient(10);
        });
    runway->setLabel("Runway");
    worldLst->push_back(runway);

    initialPosition = Vector(0, 0, 1.1f);

    jet = WO::New(jetModel, Vector(1, 1, 1), MESH_SHADING_TYPE::mstFLAT);
    jet->setPosition(initialPosition);
    jet->renderOrderType = RENDER_ORDER_TYPE::roOPAQUE;
    jet->setLabel("jet1");
    actorLst->push_back(jet);
    worldLst->push_back(jet);

    WOImGui* gui = WOImGui::New(nullptr);
    gui->setLabel("My Gui");
    gui->subscribe_drawImGuiWidget(
        [this, gui]()
        {
            ImGui::Begin("My GUI");

            ImGui::SliderFloat("Thrust", &thrust, 0.0f, 1.0f);
            ImGui::SliderFloat("Roll", &roll, -1.0f, 1.0f);
            ImGui::SliderFloat("Pitch", &pitch, -1.0f, 1.0f);
            ImGui::SliderFloat("Yaw", &yaw, -1.0f, 1.0f);

            ImGui::Text("Altitude: %.2f", altitude);
            ImGui::Text("Speed: %.2f", speed);
            ImGui::Text("Distance Traveled: %.2f", totalDistance);

            if (ImGui::Button("Reset"))
            {
                resetFlight();
            }

            if (ImGui::Button("Land"))
            {
                resetFlight();
            }

            if (ImGui::Button("Start Recording"))
            {
                recording = true;
                flightPath.clear();
            }

            if (ImGui::Button("Stop Recording"))
            {
                recording = false;
            }

            if (ImGui::Button("Playback"))
            {
                playingBack = true;
                playbackIndex = 0;
            }

            if (ImGui::Button("Toggle Day/Night"))
            {
                toggleDayNight();
            }

            ImGui::End();
        });
    this->worldLst->push_back(gui);
}
