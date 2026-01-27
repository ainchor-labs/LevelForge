// 3D Tennis Target Demo - Raylib + Jolt Physics
// Move paddle with WASD/Arrow keys, hit targets to score points

#include <iostream>
#include <vector>
#include <cstdarg>
#include <thread>
#include <cmath>

// Raylib - include first and save Color type
#include "raylib.h"
#include "raymath.h"

// Alias raylib's Color before Jolt pollutes the namespace
typedef ::Color RayColor;

// Jolt Physics
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;
using namespace std;

// Screen dimensions
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

// Game constants
const float PADDLE_SPEED = 8.0f;
const float PADDLE_WIDTH = 2.0f;
const float PADDLE_HEIGHT = 0.3f;
const float PADDLE_DEPTH = 1.5f;
const float BALL_RADIUS = 0.3f;
const float BALL_SPEED = 15.0f;
const float TARGET_SIZE = 1.0f;
const float ARENA_WIDTH = 20.0f;
const float ARENA_DEPTH = 30.0f;
const int NUM_TARGETS = 5;

// Raylib color constants (to avoid ambiguity with JPH::Color)
const RayColor COLOR_BG         = { 40, 40, 50, 255 };
const RayColor COLOR_FLOOR      = { 60, 100, 60, 255 };
const RayColor COLOR_WALL       = { 100, 100, 150, 100 };
const RayColor COLOR_UI_BG      = { 0, 0, 0, 150 };
const RayColor COLOR_UI_BG_DARK = { 0, 0, 0, 200 };
const RayColor COLOR_WHITE      = { 255, 255, 255, 255 };
const RayColor COLOR_BLACK      = { 0, 0, 0, 255 };
const RayColor COLOR_RED        = { 230, 41, 55, 255 };
const RayColor COLOR_GREEN      = { 0, 228, 48, 255 };
const RayColor COLOR_BLUE       = { 0, 121, 241, 255 };
const RayColor COLOR_YELLOW     = { 253, 249, 0, 255 };
const RayColor COLOR_GOLD       = { 255, 203, 0, 255 };
const RayColor COLOR_ORANGE     = { 255, 161, 0, 255 };
const RayColor COLOR_SKYBLUE    = { 102, 191, 255, 255 };
const RayColor COLOR_DARKBLUE   = { 0, 82, 172, 255 };
const RayColor COLOR_GRAY       = { 130, 130, 130, 255 };

// Trace callback for Jolt
static void TraceImpl(const char *inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    cout << buffer << endl;
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine) {
    cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << endl;
    return true;
}
#endif

// Collision layers
namespace Layers {
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr ObjectLayer PADDLE = 2;
    static constexpr ObjectLayer TARGET = 3;
    static constexpr ObjectLayer NUM_LAYERS = 4;
};

namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS(2);
};

// Layer filtering
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
        switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING;
        case Layers::MOVING:
            return true; // Ball collides with everything
        case Layers::PADDLE:
            return inObject2 == Layers::MOVING;
        case Layers::TARGET:
            return inObject2 == Layers::MOVING;
        default:
            return false;
        }
    }
};

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::PADDLE] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::TARGET] = BroadPhaseLayers::NON_MOVING;
    }

    virtual uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
        switch ((BroadPhaseLayer::Type)inLayer) {
        case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
        default: return "INVALID";
        }
    }
#endif

private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        case Layers::PADDLE:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::TARGET:
            return inLayer2 == BroadPhaseLayers::MOVING;
        default:
            return false;
        }
    }
};

// Target structure
struct Target {
    BodyID bodyId;
    Vector3 position;
    RayColor color;
    int points;
    bool active;
};

// Game state
struct GameState {
    int score = 0;
    int ballsRemaining = 10;
    bool ballInPlay = false;
    vector<Target> targets;
};

// Global for contact detection
static GameState* gGameState = nullptr;
static BodyID gBallId;

// Contact listener for scoring
class GameContactListener : public ContactListener {
public:
    virtual ValidateResult OnContactValidate(const Body &inBody1, const Body &inBody2, RVec3Arg inBaseOffset, const CollideShapeResult &inCollisionResult) override {
        return ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {
        if (gGameState == nullptr) return;

        BodyID id1 = inBody1.GetID();
        BodyID id2 = inBody2.GetID();

        // Check if ball hit a target
        for (auto& target : gGameState->targets) {
            if (!target.active) continue;
            if ((id1 == gBallId && id2 == target.bodyId) || (id2 == gBallId && id1 == target.bodyId)) {
                gGameState->score += target.points;
                target.active = false;
                cout << "Target hit! +" << target.points << " points. Total: " << gGameState->score << endl;
                break;
            }
        }
    }

    virtual void OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override {}
    virtual void OnContactRemoved(const SubShapeIDPair &inSubShapePair) override {}
};

// Body activation listener
class GameBodyActivationListener : public BodyActivationListener {
public:
    virtual void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override {}
    virtual void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override {}
};

// Convert Jolt position to raylib Vector3
Vector3 JoltToRaylib(const RVec3& v) {
    return { (float)v.GetX(), (float)v.GetY(), (float)v.GetZ() };
}

// Spawn a new target at random position
Target CreateTarget(BodyInterface& bodyInterface, int index) {
    Target target;

    // Random position in the far half of the arena
    float x = (float)GetRandomValue((int)(-ARENA_WIDTH/2 + 2), (int)(ARENA_WIDTH/2 - 2));
    float y = (float)GetRandomValue(1, 4);
    float z = (float)GetRandomValue((int)(-ARENA_DEPTH/2), (int)(-ARENA_DEPTH/4));

    target.position = { x, y, z };
    target.active = true;

    // Different colors and point values based on height
    if (y >= 3) {
        target.color = COLOR_GOLD;
        target.points = 30;
    } else if (y >= 2) {
        target.color = COLOR_RED;
        target.points = 20;
    } else {
        target.color = COLOR_GREEN;
        target.points = 10;
    }

    // Create physics body
    BoxShapeSettings targetShapeSettings(Vec3(TARGET_SIZE/2, TARGET_SIZE/2, TARGET_SIZE/2));
    targetShapeSettings.SetEmbedded();
    ShapeRefC targetShape = targetShapeSettings.Create().Get();

    BodyCreationSettings targetSettings(targetShape, RVec3(x, y, z), Quat::sIdentity(), EMotionType::Static, Layers::TARGET);
    Body* targetBody = bodyInterface.CreateBody(targetSettings);
    bodyInterface.AddBody(targetBody->GetID(), EActivation::DontActivate);

    target.bodyId = targetBody->GetID();

    return target;
}

// Reset targets
void ResetTargets(BodyInterface& bodyInterface, GameState& gameState) {
    // Remove old target bodies
    for (auto& target : gameState.targets) {
        if (bodyInterface.IsAdded(target.bodyId)) {
            bodyInterface.RemoveBody(target.bodyId);
            bodyInterface.DestroyBody(target.bodyId);
        }
    }
    gameState.targets.clear();

    // Create new targets
    for (int i = 0; i < NUM_TARGETS; i++) {
        gameState.targets.push_back(CreateTarget(bodyInterface, i));
    }
}

int main(void) {
    // Initialize raylib
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "3D Tennis Target Demo - Raylib + Jolt Physics");
    SetTargetFPS(60);

    // Initialize Jolt
    RegisterDefaultAllocator();
    Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)
    Factory::sInstance = new Factory();
    RegisterTypes();

    // Physics allocators
    TempAllocatorImpl tempAllocator(10 * 1024 * 1024);
    JobSystemThreadPool jobSystem(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

    // Physics configuration
    const uint cMaxBodies = 1024;
    const uint cNumBodyMutexes = 0;
    const uint cMaxBodyPairs = 1024;
    const uint cMaxContactConstraints = 1024;

    // Layer interfaces
    BPLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphaseLayerFilter;
    ObjectLayerPairFilterImpl objectVsObjectLayerFilter;

    // Create physics system
    PhysicsSystem physicsSystem;
    physicsSystem.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
                       broadPhaseLayerInterface, objectVsBroadphaseLayerFilter, objectVsObjectLayerFilter);

    // Listeners
    GameBodyActivationListener bodyActivationListener;
    GameContactListener contactListener;
    physicsSystem.SetBodyActivationListener(&bodyActivationListener);
    physicsSystem.SetContactListener(&contactListener);

    BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

    // Create floor
    BoxShapeSettings floorShapeSettings(Vec3(ARENA_WIDTH/2, 0.5f, ARENA_DEPTH/2));
    floorShapeSettings.SetEmbedded();
    ShapeRefC floorShape = floorShapeSettings.Create().Get();
    BodyCreationSettings floorSettings(floorShape, RVec3(0.0_r, -0.5_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
    Body* floor = bodyInterface.CreateBody(floorSettings);
    bodyInterface.AddBody(floor->GetID(), EActivation::DontActivate);

    // Create walls
    // Back wall
    BoxShapeSettings backWallSettings(Vec3(ARENA_WIDTH/2, 5.0f, 0.5f));
    backWallSettings.SetEmbedded();
    ShapeRefC backWallShape = backWallSettings.Create().Get();
    BodyCreationSettings backWallBodySettings(backWallShape, RVec3(0.0_r, 5.0_r, -ARENA_DEPTH/2), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
    Body* backWall = bodyInterface.CreateBody(backWallBodySettings);
    bodyInterface.AddBody(backWall->GetID(), EActivation::DontActivate);

    // Side walls
    BoxShapeSettings sideWallSettings(Vec3(0.5f, 5.0f, ARENA_DEPTH/2));
    sideWallSettings.SetEmbedded();
    ShapeRefC sideWallShape = sideWallSettings.Create().Get();

    BodyCreationSettings leftWallSettings(sideWallShape, RVec3(-ARENA_WIDTH/2, 5.0_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
    Body* leftWall = bodyInterface.CreateBody(leftWallSettings);
    bodyInterface.AddBody(leftWall->GetID(), EActivation::DontActivate);

    BodyCreationSettings rightWallSettings(sideWallShape, RVec3(ARENA_WIDTH/2, 5.0_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
    Body* rightWall = bodyInterface.CreateBody(rightWallSettings);
    bodyInterface.AddBody(rightWall->GetID(), EActivation::DontActivate);

    // Create paddle (kinematic - player controlled)
    BoxShapeSettings paddleShapeSettings(Vec3(PADDLE_WIDTH/2, PADDLE_HEIGHT/2, PADDLE_DEPTH/2));
    paddleShapeSettings.SetEmbedded();
    ShapeRefC paddleShape = paddleShapeSettings.Create().Get();

    Vector3 paddlePos = { 0.0f, 1.0f, ARENA_DEPTH/2 - 3.0f };
    BodyCreationSettings paddleSettings(paddleShape, RVec3(paddlePos.x, paddlePos.y, paddlePos.z), Quat::sIdentity(), EMotionType::Kinematic, Layers::PADDLE);
    Body* paddle = bodyInterface.CreateBody(paddleSettings);
    bodyInterface.AddBody(paddle->GetID(), EActivation::Activate);
    BodyID paddleId = paddle->GetID();

    // Create ball (dynamic)
    SphereShapeSettings ballShapeSettings(BALL_RADIUS);
    ballShapeSettings.SetEmbedded();
    ShapeRefC ballShape = ballShapeSettings.Create().Get();

    Vector3 ballStartPos = { paddlePos.x, paddlePos.y + 1.0f, paddlePos.z - 1.0f };
    BodyCreationSettings ballSettings(ballShape, RVec3(ballStartPos.x, ballStartPos.y, ballStartPos.z), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
    ballSettings.mRestitution = 0.8f;
    ballSettings.mFriction = 0.2f;
    Body* ball = bodyInterface.CreateBody(ballSettings);
    bodyInterface.AddBody(ball->GetID(), EActivation::Activate);
    gBallId = ball->GetID();

    // Game state
    GameState gameState;
    gGameState = &gameState;

    // Create initial targets
    ResetTargets(bodyInterface, gameState);

    // Optimize broad phase
    physicsSystem.OptimizeBroadPhase();

    // Camera setup - third person
    Camera3D camera = { 0 };
    camera.position = { 0.0f, 15.0f, 25.0f };
    camera.target = { 0.0f, 2.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    const float deltaTime = 1.0f / 60.0f;
    bool gameOver = false;

    // Main game loop
    while (!WindowShouldClose()) {
        // Input handling - paddle movement
        float moveX = 0.0f;
        float moveZ = 0.0f;

        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  moveX -= PADDLE_SPEED * deltaTime;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) moveX += PADDLE_SPEED * deltaTime;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    moveZ -= PADDLE_SPEED * deltaTime;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  moveZ += PADDLE_SPEED * deltaTime;

        // Update paddle position
        RVec3 currentPaddlePos = bodyInterface.GetPosition(paddleId);
        float newX = Clamp((float)currentPaddlePos.GetX() + moveX, -ARENA_WIDTH/2 + PADDLE_WIDTH, ARENA_WIDTH/2 - PADDLE_WIDTH);
        float newZ = Clamp((float)currentPaddlePos.GetZ() + moveZ, 0.0f, ARENA_DEPTH/2 - 2.0f);
        bodyInterface.SetPosition(paddleId, RVec3(newX, currentPaddlePos.GetY(), newZ), EActivation::Activate);
        paddlePos = { newX, (float)currentPaddlePos.GetY(), newZ };

        // Launch ball with space
        if (IsKeyPressed(KEY_SPACE) && !gameState.ballInPlay && gameState.ballsRemaining > 0) {
            gameState.ballInPlay = true;
            gameState.ballsRemaining--;

            // Reset ball position above paddle
            bodyInterface.SetPosition(gBallId, RVec3(paddlePos.x, paddlePos.y + 1.0f, paddlePos.z - 1.0f), EActivation::Activate);

            // Launch ball forward with slight upward angle
            bodyInterface.SetLinearVelocity(gBallId, Vec3(0.0f, 3.0f, -BALL_SPEED));
        }

        // Reset game with R
        if (IsKeyPressed(KEY_R)) {
            gameState.score = 0;
            gameState.ballsRemaining = 10;
            gameState.ballInPlay = false;
            gameOver = false;

            // Reset ball
            bodyInterface.SetPosition(gBallId, RVec3(paddlePos.x, paddlePos.y + 1.0f, paddlePos.z - 1.0f), EActivation::Activate);
            bodyInterface.SetLinearVelocity(gBallId, Vec3(0.0f, 0.0f, 0.0f));

            // Reset targets
            ResetTargets(bodyInterface, gameState);
        }

        // Check if ball is out of bounds
        RVec3 ballPos = bodyInterface.GetPosition(gBallId);
        if (gameState.ballInPlay) {
            if (ballPos.GetY() < -2.0f || ballPos.GetZ() > ARENA_DEPTH/2 + 5.0f ||
                ballPos.GetZ() < -ARENA_DEPTH/2 - 5.0f ||
                abs(ballPos.GetX()) > ARENA_WIDTH/2 + 5.0f) {
                gameState.ballInPlay = false;

                // Reset ball to paddle
                bodyInterface.SetPosition(gBallId, RVec3(paddlePos.x, paddlePos.y + 1.0f, paddlePos.z - 1.0f), EActivation::Activate);
                bodyInterface.SetLinearVelocity(gBallId, Vec3(0.0f, 0.0f, 0.0f));

                if (gameState.ballsRemaining <= 0) {
                    gameOver = true;
                }
            }
        }

        // Check if all targets hit - respawn them
        bool allHit = true;
        for (const auto& target : gameState.targets) {
            if (target.active) {
                allHit = false;
                break;
            }
        }
        if (allHit && !gameState.targets.empty()) {
            ResetTargets(bodyInterface, gameState);
        }

        // Update physics
        const int cCollisionSteps = 1;
        physicsSystem.Update(deltaTime, cCollisionSteps, &tempAllocator, &jobSystem);

        // Update camera to follow paddle (third person)
        camera.target = { paddlePos.x, 2.0f, paddlePos.z - 5.0f };
        camera.position = { paddlePos.x, 12.0f, paddlePos.z + 15.0f };

        // Drawing
        BeginDrawing();
        ClearBackground(COLOR_BG);

        BeginMode3D(camera);

        // Draw floor
        DrawPlane({ 0.0f, 0.0f, 0.0f }, { ARENA_WIDTH, ARENA_DEPTH }, COLOR_FLOOR);

        // Draw floor grid lines
        for (float x = -ARENA_WIDTH/2; x <= ARENA_WIDTH/2; x += 2.0f) {
            DrawLine3D({ x, 0.01f, -ARENA_DEPTH/2 }, { x, 0.01f, ARENA_DEPTH/2 }, COLOR_WHITE);
        }
        for (float z = -ARENA_DEPTH/2; z <= ARENA_DEPTH/2; z += 2.0f) {
            DrawLine3D({ -ARENA_WIDTH/2, 0.01f, z }, { ARENA_WIDTH/2, 0.01f, z }, COLOR_WHITE);
        }

        // Draw walls (semi-transparent)
        DrawCubeV({ 0.0f, 5.0f, -ARENA_DEPTH/2 }, { ARENA_WIDTH, 10.0f, 1.0f }, COLOR_WALL);
        DrawCubeWiresV({ 0.0f, 5.0f, -ARENA_DEPTH/2 }, { ARENA_WIDTH, 10.0f, 1.0f }, COLOR_BLUE);
        DrawCubeV({ -ARENA_WIDTH/2, 5.0f, 0.0f }, { 1.0f, 10.0f, ARENA_DEPTH }, COLOR_WALL);
        DrawCubeWiresV({ -ARENA_WIDTH/2, 5.0f, 0.0f }, { 1.0f, 10.0f, ARENA_DEPTH }, COLOR_BLUE);
        DrawCubeV({ ARENA_WIDTH/2, 5.0f, 0.0f }, { 1.0f, 10.0f, ARENA_DEPTH }, COLOR_WALL);
        DrawCubeWiresV({ ARENA_WIDTH/2, 5.0f, 0.0f }, { 1.0f, 10.0f, ARENA_DEPTH }, COLOR_BLUE);

        // Draw paddle
        RVec3 pPos = bodyInterface.GetPosition(paddleId);
        Vector3 paddleDrawPos = JoltToRaylib(pPos);
        DrawCubeV(paddleDrawPos, { PADDLE_WIDTH, PADDLE_HEIGHT, PADDLE_DEPTH }, COLOR_SKYBLUE);
        DrawCubeWiresV(paddleDrawPos, { PADDLE_WIDTH, PADDLE_HEIGHT, PADDLE_DEPTH }, COLOR_DARKBLUE);

        // Draw ball
        RVec3 bPos = bodyInterface.GetPosition(gBallId);
        Vector3 ballDrawPos = JoltToRaylib(bPos);
        DrawSphere(ballDrawPos, BALL_RADIUS, COLOR_YELLOW);
        DrawSphereWires(ballDrawPos, BALL_RADIUS, 8, 8, COLOR_ORANGE);

        // Draw targets
        for (const auto& target : gameState.targets) {
            if (target.active) {
                DrawCubeV(target.position, { TARGET_SIZE, TARGET_SIZE, TARGET_SIZE }, target.color);
                DrawCubeWiresV(target.position, { TARGET_SIZE, TARGET_SIZE, TARGET_SIZE }, COLOR_BLACK);
            }
        }

        EndMode3D();

        // Draw UI
        DrawRectangle(10, 10, 250, 120, COLOR_UI_BG);
        DrawRectangleLines(10, 10, 250, 120, COLOR_WHITE);

        DrawText("3D TENNIS TARGET", 20, 20, 20, COLOR_WHITE);
        DrawText(TextFormat("Score: %d", gameState.score), 20, 50, 20, COLOR_YELLOW);
        DrawText(TextFormat("Balls: %d", gameState.ballsRemaining), 20, 75, 20, COLOR_SKYBLUE);

        if (!gameState.ballInPlay && gameState.ballsRemaining > 0) {
            DrawText("Press SPACE to launch!", 20, 100, 16, COLOR_GREEN);
        }

        // Controls help
        DrawRectangle(SCREEN_WIDTH - 220, 10, 210, 100, COLOR_UI_BG);
        DrawRectangleLines(SCREEN_WIDTH - 220, 10, 210, 100, COLOR_WHITE);
        DrawText("Controls:", SCREEN_WIDTH - 210, 20, 16, COLOR_WHITE);
        DrawText("WASD/Arrows - Move paddle", SCREEN_WIDTH - 210, 40, 14, COLOR_GRAY);
        DrawText("SPACE - Launch ball", SCREEN_WIDTH - 210, 58, 14, COLOR_GRAY);
        DrawText("R - Reset game", SCREEN_WIDTH - 210, 76, 14, COLOR_GRAY);
        DrawText("ESC - Quit", SCREEN_WIDTH - 210, 94, 14, COLOR_GRAY);

        // Target legend
        DrawRectangle(10, SCREEN_HEIGHT - 90, 180, 80, COLOR_UI_BG);
        DrawRectangleLines(10, SCREEN_HEIGHT - 90, 180, 80, COLOR_WHITE);
        DrawText("Target Points:", 20, SCREEN_HEIGHT - 80, 14, COLOR_WHITE);
        DrawRectangle(20, SCREEN_HEIGHT - 60, 15, 15, COLOR_GOLD);
        DrawText("High (30 pts)", 40, SCREEN_HEIGHT - 60, 14, COLOR_GOLD);
        DrawRectangle(20, SCREEN_HEIGHT - 42, 15, 15, COLOR_RED);
        DrawText("Mid (20 pts)", 40, SCREEN_HEIGHT - 42, 14, COLOR_RED);
        DrawRectangle(20, SCREEN_HEIGHT - 24, 15, 15, COLOR_GREEN);
        DrawText("Low (10 pts)", 40, SCREEN_HEIGHT - 24, 14, COLOR_GREEN);

        // Game over screen
        if (gameOver) {
            DrawRectangle(SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 60, 300, 120, COLOR_UI_BG_DARK);
            DrawRectangleLines(SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 60, 300, 120, COLOR_RED);
            DrawText("GAME OVER", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 40, 30, COLOR_RED);
            DrawText(TextFormat("Final Score: %d", gameState.score), SCREEN_WIDTH/2 - 75, SCREEN_HEIGHT/2, 20, COLOR_WHITE);
            DrawText("Press R to restart", SCREEN_WIDTH/2 - 70, SCREEN_HEIGHT/2 + 30, 16, COLOR_YELLOW);
        }

        EndDrawing();
    }

    // Cleanup physics
    bodyInterface.RemoveBody(gBallId);
    bodyInterface.DestroyBody(gBallId);
    bodyInterface.RemoveBody(paddleId);
    bodyInterface.DestroyBody(paddleId);
    bodyInterface.RemoveBody(floor->GetID());
    bodyInterface.DestroyBody(floor->GetID());
    bodyInterface.RemoveBody(backWall->GetID());
    bodyInterface.DestroyBody(backWall->GetID());
    bodyInterface.RemoveBody(leftWall->GetID());
    bodyInterface.DestroyBody(leftWall->GetID());
    bodyInterface.RemoveBody(rightWall->GetID());
    bodyInterface.DestroyBody(rightWall->GetID());

    for (auto& target : gameState.targets) {
        if (bodyInterface.IsAdded(target.bodyId)) {
            bodyInterface.RemoveBody(target.bodyId);
            bodyInterface.DestroyBody(target.bodyId);
        }
    }

    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    CloseWindow();
    return 0;
}
