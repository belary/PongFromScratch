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
    COMPONENT_CAKEZ = BIT(4),
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

// ====================== components ========================
internal bool has_component(Entity* e, Components c)
{
    return e->comMask & c;
}
internal void add_component(Entity* e, Components c)
{
    e->comMask |= c;
}
internal void remove_component(Entity* e, Components c)
{
    e->comMask &= ~c;
}

// ====================== material ========================

internal uint32_t create_material(GameState* gameState, AssetTypeID assetTypeID, Vec4 color)
{
    uint32_t materialIdx = INVALID_IDX;
    if (gameState->materialCount < MAX_MATERIALS)
    {
        materialIdx = gameState->materialCount; //??
        Material* m = &gameState->materials[gameState->materialCount++];
        m->assetTypeID = assetTypeID;
        m->materialData.color = color;
    }
    else
    {
        CAKEZ_ASSERT(0, "Reached maximum amount of Materials");
    }

    return materialIdx;
}

internal uint32_t get_material(GameState* gameState, AssetTypeID assetTypeID,
                               Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f})
{
    uint32_t materialIdx = INVALID_IDX;

    for (uint32_t i = 0; i < gameState->materialCount; i++)
    {
        Material* m = &gameState->materials[i];

        if (m->assetTypeID == assetTypeID && m->materialData.color == color)
        {
            materialIdx = i;
            break;
        }
    }

    if (materialIdx == INVALID_IDX)
    {
        materialIdx = create_material(gameState, assetTypeID, color);
    }

    return materialIdx;
}

internal Material* get_material(GameState* gameState, uint32_t materialIdx)
{
    CAKEZ_ASSERT(materialIdx < gameState->materialCount, "MaterialIdx out of bounds");

    Material* m = 0;

    if (materialIdx < gameState->materialCount)
    {
        m = &gameState->materials[materialIdx];
    }
    else
    {
        // By default we return the first Material, this will default to ASSET_SPRITE_WHITE
        m = &gameState->materials[0];
    }

    return m;
}

bool init_game(GameState* gameState)
{
    float counter = 0.0f;

    for (uint32_t i = 0; i < 10; i++)
    {

        for (uint32_t j = 0; j < 10; j++)
        {
            // create color
            float r = counter / 100.0f;
            float g = 1.0f - r;
            float b = r;
            float a = g;
            Entity* e = create_entity(gameState, {i * 60.0f, j * 60.0f, 60.0f, 60.0f});
            add_component(e, COMPONENT_BALL);
            e->transform.materialIdx = get_material(gameState, ASSET_SPRITE_CAKEZ, {r, g, b, a});
            
            counter += 10.0f;
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

// bool init_game(GameState* gameState)
// {
//     float paddleSizeX = 50.0f, paddleSizeY = 100.0f, ballSize = 50.0f;

//     Entity* e = create_entity(gameState, {10.0f, 10.0f, paddleSizeX, paddleSizeY});
//     add_component(e, COMPONENT_LEFT_PADDLE);
//     e->transform.materialIdx = get_material(gameState, ASSET_SPRITE_PADDLE);

//     e = create_entity(gameState, {1000.0f - paddleSizeX - 20.0f, 10.0f, paddleSizeX,
//     paddleSizeY}); add_component(e, COMPONENT_RIGHT_PADDLE); e->transform.materialIdx =
//     get_material(gameState, ASSET_SPRITE_PADDLE);

//     e = create_entity(gameState, {1000.0f / 2.0f, 400.0f, ballSize, ballSize});
//     add_component(e, COMPONENT_BALL);
//     e->transform.materialIdx = get_material(gameState, ASSET_SPRITE_BALL);

//     return true;
// }
