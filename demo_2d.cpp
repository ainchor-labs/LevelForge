// Breakout Clone Demo - raylib + Box2D
// A classic brick-breaking game with physics

#include "raylib.h"
#include "box2d/box2d.h"
#include <vector>
#include <cstdlib>

// Screen dimensions
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Physics scale (pixels per meter)
const float SCALE = 30.0f;

// Game dimensions in physics units (meters)
const float WORLD_WIDTH = SCREEN_WIDTH / SCALE;
const float WORLD_HEIGHT = SCREEN_HEIGHT / SCALE;

// Paddle settings
const float PADDLE_WIDTH = 3.0f;
const float PADDLE_HEIGHT = 0.4f;
const float PADDLE_Y = 1.5f;
const float PADDLE_SPEED = 15.0f;

// Ball settings
const float BALL_RADIUS = 0.3f;
const float BALL_INITIAL_SPEED = 10.0f;

// Brick settings
const float BRICK_WIDTH = 1.8f;
const float BRICK_HEIGHT = 0.5f;
const int BRICK_ROWS = 5;
const int BRICK_COLS = 10;
const float BRICK_START_Y = WORLD_HEIGHT - 4.0f;
const float BRICK_SPACING = 0.15f;

// Coordinate conversion functions
float toScreenX(float x) {
    return x * SCALE;
}

float toScreenY(float y) {
    return SCREEN_HEIGHT - (y * SCALE);
}

// Brick data structure
struct Brick {
    b2BodyId bodyId;
    b2ShapeId shapeId;
    Color color;
    bool destroyed;
    int hitPoints;
};

// Game state
struct GameState {
    b2WorldId worldId;
    b2BodyId paddleId;
    b2BodyId ballId;
    b2BodyId wallIds[4]; // left, right, top, bottom
    std::vector<Brick> bricks;
    int score;
    int lives;
    bool gameOver;
    bool gameWon;
    bool ballLaunched;
};

// Color palette for bricks based on row
Color getBrickColor(int row) {
    switch (row) {
        case 0: return RED;
        case 1: return ORANGE;
        case 2: return YELLOW;
        case 3: return GREEN;
        case 4: return BLUE;
        default: return PURPLE;
    }
}

// Initialize the physics world
b2WorldId createWorld() {
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = (b2Vec2){0.0f, 0.0f}; // No gravity for Breakout
    return b2CreateWorld(&worldDef);
}

// Create walls around the play area
void createWalls(GameState& game) {
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.material.friction = 0.0f;
    shapeDef.material.restitution = 1.0f; // Perfect bounce

    float wallThickness = 0.5f;

    // Left wall
    {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = (b2Vec2){-wallThickness / 2.0f, WORLD_HEIGHT / 2.0f};
        game.wallIds[0] = b2CreateBody(game.worldId, &bodyDef);
        b2Polygon box = b2MakeBox(wallThickness / 2.0f, WORLD_HEIGHT / 2.0f);
        b2CreatePolygonShape(game.wallIds[0], &shapeDef, &box);
    }

    // Right wall
    {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = (b2Vec2){WORLD_WIDTH + wallThickness / 2.0f, WORLD_HEIGHT / 2.0f};
        game.wallIds[1] = b2CreateBody(game.worldId, &bodyDef);
        b2Polygon box = b2MakeBox(wallThickness / 2.0f, WORLD_HEIGHT / 2.0f);
        b2CreatePolygonShape(game.wallIds[1], &shapeDef, &box);
    }

    // Top wall
    {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = (b2Vec2){WORLD_WIDTH / 2.0f, WORLD_HEIGHT + wallThickness / 2.0f};
        game.wallIds[2] = b2CreateBody(game.worldId, &bodyDef);
        b2Polygon box = b2MakeBox(WORLD_WIDTH / 2.0f + wallThickness, wallThickness / 2.0f);
        b2CreatePolygonShape(game.wallIds[2], &shapeDef, &box);
    }

    // Bottom wall (death zone - sensor)
    {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = (b2Vec2){WORLD_WIDTH / 2.0f, -wallThickness / 2.0f};
        game.wallIds[3] = b2CreateBody(game.worldId, &bodyDef);
        b2ShapeDef sensorDef = b2DefaultShapeDef();
        sensorDef.isSensor = true; // Ball passes through but we detect it
        b2Polygon box = b2MakeBox(WORLD_WIDTH / 2.0f + wallThickness, wallThickness / 2.0f);
        b2CreatePolygonShape(game.wallIds[3], &sensorDef, &box);
    }
}

// Create the player paddle
b2BodyId createPaddle(b2WorldId worldId) {
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_kinematicBody; // Player controlled
    bodyDef.position = (b2Vec2){WORLD_WIDTH / 2.0f, PADDLE_Y};
    bodyDef.motionLocks.angularZ = true;
    b2BodyId paddleId = b2CreateBody(worldId, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.material.friction = 0.0f;
    shapeDef.material.restitution = 1.0f;

    b2Polygon box = b2MakeBox(PADDLE_WIDTH / 2.0f, PADDLE_HEIGHT / 2.0f);
    b2CreatePolygonShape(paddleId, &shapeDef, &box);

    return paddleId;
}

// Create the ball
b2BodyId createBall(b2WorldId worldId, float x, float y) {
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = (b2Vec2){x, y};
    bodyDef.motionLocks.angularZ = true;
    bodyDef.isBullet = true; // Enable CCD for fast ball
    b2BodyId ballId = b2CreateBody(worldId, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.material.friction = 0.0f;
    shapeDef.material.restitution = 1.0f; // Perfect bounce
    shapeDef.enableContactEvents = true;

    b2Circle circle = {.center = {0.0f, 0.0f}, .radius = BALL_RADIUS};
    b2CreateCircleShape(ballId, &shapeDef, &circle);

    return ballId;
}

// Create all bricks
void createBricks(GameState& game) {
    float totalWidth = BRICK_COLS * (BRICK_WIDTH + BRICK_SPACING) - BRICK_SPACING;
    float startX = (WORLD_WIDTH - totalWidth) / 2.0f + BRICK_WIDTH / 2.0f;

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.material.friction = 0.0f;
    shapeDef.material.restitution = 1.0f;
    shapeDef.enableContactEvents = true;

    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLS; col++) {
            float x = startX + col * (BRICK_WIDTH + BRICK_SPACING);
            float y = BRICK_START_Y - row * (BRICK_HEIGHT + BRICK_SPACING);

            b2BodyDef bodyDef = b2DefaultBodyDef();
            bodyDef.position = (b2Vec2){x, y};
            b2BodyId brickBodyId = b2CreateBody(game.worldId, &bodyDef);

            b2Polygon box = b2MakeBox(BRICK_WIDTH / 2.0f, BRICK_HEIGHT / 2.0f);
            b2ShapeId brickShapeId = b2CreatePolygonShape(brickBodyId, &shapeDef, &box);

            Brick brick;
            brick.bodyId = brickBodyId;
            brick.shapeId = brickShapeId;
            brick.color = getBrickColor(row);
            brick.destroyed = false;
            brick.hitPoints = (BRICK_ROWS - row); // Top rows worth more

            game.bricks.push_back(brick);
        }
    }
}

// Initialize game state
void initGame(GameState& game) {
    game.worldId = createWorld();
    createWalls(game);
    game.paddleId = createPaddle(game.worldId);

    // Ball starts on top of paddle
    b2Vec2 paddlePos = b2Body_GetPosition(game.paddleId);
    game.ballId = createBall(game.worldId, paddlePos.x, paddlePos.y + PADDLE_HEIGHT / 2.0f + BALL_RADIUS + 0.1f);

    createBricks(game);

    game.score = 0;
    game.lives = 3;
    game.gameOver = false;
    game.gameWon = false;
    game.ballLaunched = false;
}

// Reset ball to paddle
void resetBall(GameState& game) {
    b2Vec2 paddlePos = b2Body_GetPosition(game.paddleId);
    b2Body_SetTransform(game.ballId,
        (b2Vec2){paddlePos.x, paddlePos.y + PADDLE_HEIGHT / 2.0f + BALL_RADIUS + 0.1f},
        b2MakeRot(0.0f));
    b2Body_SetLinearVelocity(game.ballId, (b2Vec2){0.0f, 0.0f});
    game.ballLaunched = false;
}

// Launch the ball
void launchBall(GameState& game) {
    if (!game.ballLaunched) {
        // Launch at an angle
        float angle = ((float)GetRandomValue(-30, 30)) * DEG2RAD;
        float vx = BALL_INITIAL_SPEED * sinf(angle);
        float vy = BALL_INITIAL_SPEED * cosf(angle);
        b2Body_SetLinearVelocity(game.ballId, (b2Vec2){vx, vy});
        game.ballLaunched = true;
    }
}

// Check if a brick was hit and destroy it
void checkBrickCollisions(GameState& game) {
    b2ContactEvents contactEvents = b2World_GetContactEvents(game.worldId);

    for (int i = 0; i < contactEvents.beginCount; i++) {
        b2ContactBeginTouchEvent* event = &contactEvents.beginEvents[i];

        // Check if ball hit a brick
        for (auto& brick : game.bricks) {
            if (brick.destroyed) continue;

            b2ShapeId brickShapeId = brick.shapeId;
            if (B2_ID_EQUALS(event->shapeIdA, brickShapeId) ||
                B2_ID_EQUALS(event->shapeIdB, brickShapeId)) {
                brick.destroyed = true;
                game.score += brick.hitPoints * 10;
                b2DestroyBody(brick.bodyId);
                break;
            }
        }
    }
}

// Check for ball going out of bounds
bool checkBallLost(GameState& game) {
    b2Vec2 ballPos = b2Body_GetPosition(game.ballId);
    return ballPos.y < 0.0f;
}

// Check win condition
bool checkWin(GameState& game) {
    for (const auto& brick : game.bricks) {
        if (!brick.destroyed) return false;
    }
    return true;
}

// Ensure ball maintains constant speed
void maintainBallSpeed(GameState& game) {
    if (!game.ballLaunched) return;

    b2Vec2 vel = b2Body_GetLinearVelocity(game.ballId);
    float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);

    if (speed > 0.1f && speed != BALL_INITIAL_SPEED) {
        float scale = BALL_INITIAL_SPEED / speed;
        b2Body_SetLinearVelocity(game.ballId, (b2Vec2){vel.x * scale, vel.y * scale});
    }

    // Prevent ball from going too horizontal
    vel = b2Body_GetLinearVelocity(game.ballId);
    if (fabsf(vel.y) < 2.0f) {
        float sign = vel.y >= 0 ? 1.0f : -1.0f;
        vel.y = sign * 2.0f;
        float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
        float scale = BALL_INITIAL_SPEED / speed;
        b2Body_SetLinearVelocity(game.ballId, (b2Vec2){vel.x * scale, vel.y * scale});
    }
}

// Update game logic
void updateGame(GameState& game, float dt) {
    if (game.gameOver || game.gameWon) {
        if (IsKeyPressed(KEY_R)) {
            // Cleanup and restart
            b2DestroyWorld(game.worldId);
            game.bricks.clear();
            initGame(game);
        }
        return;
    }

    // Paddle movement
    b2Vec2 paddlePos = b2Body_GetPosition(game.paddleId);
    float paddleVelX = 0.0f;

    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        paddleVelX = -PADDLE_SPEED;
    }
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        paddleVelX = PADDLE_SPEED;
    }

    // Clamp paddle position
    float minX = PADDLE_WIDTH / 2.0f;
    float maxX = WORLD_WIDTH - PADDLE_WIDTH / 2.0f;

    if (paddlePos.x <= minX && paddleVelX < 0) paddleVelX = 0;
    if (paddlePos.x >= maxX && paddleVelX > 0) paddleVelX = 0;

    b2Body_SetLinearVelocity(game.paddleId, (b2Vec2){paddleVelX, 0.0f});

    // Ball follows paddle before launch
    if (!game.ballLaunched) {
        b2Vec2 newPaddlePos = b2Body_GetPosition(game.paddleId);
        b2Body_SetTransform(game.ballId,
            (b2Vec2){newPaddlePos.x, newPaddlePos.y + PADDLE_HEIGHT / 2.0f + BALL_RADIUS + 0.1f},
            b2MakeRot(0.0f));

        if (IsKeyPressed(KEY_SPACE)) {
            launchBall(game);
        }
    }

    // Physics step
    b2World_Step(game.worldId, dt, 4);

    // Check collisions
    checkBrickCollisions(game);

    // Maintain ball speed
    maintainBallSpeed(game);

    // Check if ball lost
    if (game.ballLaunched && checkBallLost(game)) {
        game.lives--;
        if (game.lives <= 0) {
            game.gameOver = true;
        } else {
            resetBall(game);
        }
    }

    // Check win condition
    if (checkWin(game)) {
        game.gameWon = true;
    }
}

// Render the game
void renderGame(const GameState& game) {
    BeginDrawing();
    ClearBackground((Color){20, 20, 30, 255}); // Dark blue background

    // Draw walls (subtle)
    DrawRectangle(0, 0, 10, SCREEN_HEIGHT, (Color){40, 40, 60, 255});
    DrawRectangle(SCREEN_WIDTH - 10, 0, 10, SCREEN_HEIGHT, (Color){40, 40, 60, 255});
    DrawRectangle(0, 0, SCREEN_WIDTH, 10, (Color){40, 40, 60, 255});

    // Draw bricks
    for (const auto& brick : game.bricks) {
        if (brick.destroyed) continue;

        b2Vec2 pos = b2Body_GetPosition(brick.bodyId);
        float screenX = toScreenX(pos.x) - (BRICK_WIDTH / 2.0f) * SCALE;
        float screenY = toScreenY(pos.y) - (BRICK_HEIGHT / 2.0f) * SCALE;

        // Brick with border
        DrawRectangle((int)screenX, (int)screenY,
            (int)(BRICK_WIDTH * SCALE), (int)(BRICK_HEIGHT * SCALE), brick.color);
        DrawRectangleLines((int)screenX, (int)screenY,
            (int)(BRICK_WIDTH * SCALE), (int)(BRICK_HEIGHT * SCALE), WHITE);
    }

    // Draw paddle
    b2Vec2 paddlePos = b2Body_GetPosition(game.paddleId);
    float paddleScreenX = toScreenX(paddlePos.x) - (PADDLE_WIDTH / 2.0f) * SCALE;
    float paddleScreenY = toScreenY(paddlePos.y) - (PADDLE_HEIGHT / 2.0f) * SCALE;
    DrawRectangle((int)paddleScreenX, (int)paddleScreenY,
        (int)(PADDLE_WIDTH * SCALE), (int)(PADDLE_HEIGHT * SCALE), WHITE);

    // Draw paddle glow effect
    DrawRectangle((int)paddleScreenX + 5, (int)paddleScreenY + 2,
        (int)(PADDLE_WIDTH * SCALE) - 10, 4, (Color){200, 200, 255, 255});

    // Draw ball
    b2Vec2 ballPos = b2Body_GetPosition(game.ballId);
    float ballScreenX = toScreenX(ballPos.x);
    float ballScreenY = toScreenY(ballPos.y);
    DrawCircle((int)ballScreenX, (int)ballScreenY, BALL_RADIUS * SCALE, WHITE);

    // Ball glow
    DrawCircle((int)ballScreenX - 2, (int)ballScreenY - 2, BALL_RADIUS * SCALE * 0.4f,
        (Color){255, 255, 200, 200});

    // Draw UI
    DrawText(TextFormat("SCORE: %d", game.score), 20, 20, 24, WHITE);
    DrawText(TextFormat("LIVES: %d", game.lives), SCREEN_WIDTH - 120, 20, 24, WHITE);

    // Instructions
    if (!game.ballLaunched && !game.gameOver && !game.gameWon) {
        const char* text = "Press SPACE to launch";
        int textWidth = MeasureText(text, 20);
        DrawText(text, (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2, 20, YELLOW);
    }

    // Game over screen
    if (game.gameOver) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 180});

        const char* gameOverText = "GAME OVER";
        int textWidth = MeasureText(gameOverText, 48);
        DrawText(gameOverText, (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2 - 50, 48, RED);

        const char* scoreText = TextFormat("Final Score: %d", game.score);
        textWidth = MeasureText(scoreText, 24);
        DrawText(scoreText, (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2 + 10, 24, WHITE);

        const char* restartText = "Press R to Restart";
        textWidth = MeasureText(restartText, 20);
        DrawText(restartText, (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2 + 60, 20, YELLOW);
    }

    // Win screen
    if (game.gameWon) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 180});

        const char* winText = "YOU WIN!";
        int textWidth = MeasureText(winText, 48);
        DrawText(winText, (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2 - 50, 48, GREEN);

        const char* scoreText = TextFormat("Final Score: %d", game.score);
        textWidth = MeasureText(scoreText, 24);
        DrawText(scoreText, (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2 + 10, 24, WHITE);

        const char* restartText = "Press R to Restart";
        textWidth = MeasureText(restartText, 20);
        DrawText(restartText, (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2 + 60, 20, YELLOW);
    }

    // Controls hint
    DrawText("A/D or Arrow Keys to Move", 20, SCREEN_HEIGHT - 30, 16, GRAY);

    EndDrawing();
}

int main(void) {
    // Initialize raylib
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Breakout - raylib + Box2D Demo");
    SetTargetFPS(60);

    // Initialize game
    GameState game;
    initGame(game);

    // Main game loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        updateGame(game, dt);
        renderGame(game);
    }

    // Cleanup
    b2DestroyWorld(game.worldId);
    CloseWindow();

    return 0;
}
