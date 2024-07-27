#pragma once

#include "GLView.h"
#include <irrKlang.h> // Ensure irrKlang is included
#include <vector>
#include <chrono> // Include for time tracking

namespace Aftr
{
    class Camera;
    class WO;

    class GLViewNewModule : public GLView
    {
    public:
        static GLViewNewModule* New(const std::vector< std::string >& outArgs);
        virtual ~GLViewNewModule();
        virtual void updateWorld(); ///< Called once per frame
        virtual void loadMap(); ///< Called once at startup to build this module's scene
        virtual void onResizeWindow(GLsizei width, GLsizei height);
        virtual void onMouseDown(const SDL_MouseButtonEvent& e);
        virtual void onMouseUp(const SDL_MouseButtonEvent& e);
        virtual void onMouseMove(const SDL_MouseMotionEvent& e);
        virtual void onKeyDown(const SDL_KeyboardEvent& key);
        virtual void onKeyUp(const SDL_KeyboardEvent& key);

    protected:
        GLViewNewModule(const std::vector< std::string >& args);
        virtual void onCreate();

        irrklang::ISoundEngine* soundEngine = nullptr;
        irrklang::ISound* objSnd = nullptr;
        float thrust;
        float roll;
        float pitch;
        float yaw;
        bool takeOff;
        bool isCubePlaced = false;
        int count = 0; // Counter to manage the take-off steps

        WO* jet = nullptr;
        WO* shinyRedPlasticCube = nullptr;
        Vector initialPosition;

        bool collisionDetected = false;
        float collisionCooldown = 0.0f; // Cooldown to prevent multiple detections in a short time

        struct FlightPathData
        {
            Vector position;
            Vector orientation;
        };

        std::vector<FlightPathData> flightPath;
        bool recording = false;
        bool playingBack = false;
        size_t playbackIndex = 0;

        int score = 0;
        std::vector<std::string> objectives;

        bool isDay = true; // Flag to track day or night
        WO* skyBox = nullptr;

        float altitude = 0.0f; // Current altitude of the plane
        float speed = 0.0f; // Current speed of the plane
        float totalDistance = 0.0f; // Total distance traveled by the plane
        Vector lastPosition; // Last recorded position of the plane

        std::chrono::high_resolution_clock::time_point lastUpdateTime; // For time tracking

        bool checkCollision(); // Change to return bool
        void handleCollision();
        void resetFlight();
        void startTakeoff();
        void recordFlightPath();
        void playbackFlightPath();
        void toggleDayNight(); // Method to toggle day and night
        void updateFlightStats(); // Method to update flight statistics
        float getDeltaTime(); // Method to get delta time

        Vector calculateRotationAngles(const Vector& direction); // Helper function to calculate rotation angles
        void updateCamera(); // Update the camera position and orientation
    };
}
