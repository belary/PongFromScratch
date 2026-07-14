#ifndef GAME_H
#define GAME_H

#include "defines.h"
#include "renderer/shared_render_types.h"
#include <cstdlib>

enum OrbType
{
    ORB_FIRE = 0,
    ORB_WATER = 1
};

struct Entity
{
    Transform transform;
    OrbType orbType;
};

struct GameState
{
    uint32_t entityCount;
    Entity entities[MAX_ENTITIES];
};

constexpr float boardSize = 504.0f;
constexpr uint32_t BOARD_ROWS = 6;
constexpr uint32_t BOARD_COLS = 6;
constexpr float CellSize = 84.0f;

// Function declarations
Entity* create_entity(GameState* gameState, Transform transform);
bool init_game(GameState* gameState);
void update_game(GameState* gameState);

#endif // GAME_H
