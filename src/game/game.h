#pragma once
#include "defines.h"
#include "logger.h"
#include "../renderer/vk_types.h"
#include "../renderer/shared_render_types.h"

struct Entity
{
    uint32_t comMask;
    Transform transform;
};

struct Material
{
    AssetTypeID assetTypeID;
    MaterialData materialData;
};

struct GameState
{
    uint32_t entityCount;
    Entity entities[MAX_ENTITIES];

    uint32_t materialCount;
    Material materials[MAX_MATERIALS];
};

enum Components
{
    COMPONENT_BALL = BIT(1),
    COMPONENT_LEFT_PADDLE = BIT(2),
    COMPONENT_RIGHT_PADDLE = BIT(3),
};

internal Entity* create_entity(GameState* gameState, Transform transform);

// ====================== components ========================
internal bool has_component(Entity* e, Components c);
internal bool add_component(Entity* e, Components c);
internal bool remove_component(Entity* e, Components c);

// ====================== material ========================

internal uint32_t create_material(GameState* gameState, AssetTypeID assetTypeID,
                                  Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f});
internal uint32_t get_material(GameState* gameState, AssetTypeID assetTypeID,
                               Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f});
internal Material* get_material(GameState* gameState, uint32_t materialIdx);

bool init_game(GameState* gameState);

void update_game(GameState* gameState);