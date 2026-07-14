#include "game.h"
#include "logger.h"
#include <ctime>

Entity* create_entity(GameState* gameState, Transform transform)
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

    srand((unsigned int)time(nullptr));
    uint32_t screenWidth = {};
    uint32_t screenHeight = {};
    platform_get_window_size(&screenWidth, &screenHeight);
    CAKEZ_TRACE("CLIENT WIDTH: %d, height %d", screenWidth, screenHeight);

    // creat board
    float boardX = (screenWidth - boardSize) * 0.5f;
    float boardY = (screenHeight - boardSize) * 0.5f;
    Entity* e = create_entity(gameState, {boardX, boardY, boardSize, boardSize, 1.5f, 1.5f});

    // random orbType

    // create orbs
    for (int i = 0; i < BOARD_ROWS; i++)
    {
        for (int j = 0; j < BOARD_COLS; j++)
        {
            float x = boardX + j * CellSize;
            float y = boardY + i * CellSize;
            Entity* e = create_entity(gameState, {x, y, CellSize, CellSize, 1.0f, 1.0f});
            e->orbType = (rand() % 2 == 0) ? ORB_FIRE : ORB_WATER;
        }
    }

    return true;
}

void update_game(GameState* gameState)
{
    // for (uint32_t i = 0; i < gameState->entityCount; i++)
    // {
    //     Entity* e = &gameState->entities[i];

    //     CAKEZ_TRACE("entity[%d] x: %f, y: %f", i, e->transform.xPos, e->transform.yPos);
    // }
}