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
    if (gameState->entityCount < MAX_ENTITIES)
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
    for (uint32_t i = 0; i < 10; i++)
    {

        for (uint32_t j = 0; j < 10; j++)
        {
            Entity* e = create_entity(gameState, {i * 60.0f, j * 60.0f, 60.0f, 60.0f});
        }
    }
    return true;
}

void update_game(GameState* gameState)
{
    for (uint32_t i = 0; i < gameState->entityCount; i++)
    {
        Entity* e = &gameState->entities[i];
        e->transform.xPos += 0.01f;
    }
}