#include "defines.h"
#include "logger.h"

#include "renderer/shared_render_types.h"

struct Entity
{
    Transform transform;
};

struct GameState
{
    uint32_t entityCount;
    Entity entities[MAX_ENTITIES];
};

internal Entity* create_entity(GameState* gameState, Transform transform)
{
    Entity* e = 0;
    if (gameState->entityCount <= MAX_ENTITIES)
    {
        e = &gameState->entities[gameState->entityCount++];
        e->transform = transform;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached Maximum amount of Entities!");
    }

    return e;
}

bool init_game(GameState* gameState)
{

    uint32_t screenWidth = {};
    uint32_t screenHeight = {};
    platform_get_window_size(&screenWidth, &screenHeight);
    CAKEZ_TRACE("CLIENT WIDTH: %d, height %d", screenWidth, screenHeight);

    float sizeX = 504.0f;
    float sizeY = 504.0f;

    float xPos = (screenWidth - sizeX) * 0.5f;
    float yPos = (screenHeight - sizeY) * 0.5f;
    Entity* e = create_entity(gameState, {xPos, yPos, sizeX, sizeY, 1.5f, 1.5f});

    float sizeOrbX = 64.0f;
    float sizeOrbY = 64.0f;

    float xOrbPos = (screenWidth - sizeOrbX) * 0.5f;
    float yOrbPos = (screenHeight - sizeOrbY) * 0.5f;
    Entity* eOrb = create_entity(gameState, {xOrbPos, yOrbPos, sizeOrbX, sizeOrbY, 1.0f, 1.0f});

    return true;
}

void update_game(GameState* gameState)
{
    // for (uint32_t i = 0; i < gameState->entityCount; i++)
    // {
    //     Entity* e = &gameState->entities[i];
    //     e->transform.xPos += 0.01f;
    // }
}