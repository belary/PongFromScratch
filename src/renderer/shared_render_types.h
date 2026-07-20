
#ifndef SHARED_RENDER_TYPES_H
#define SHARED_RENDER_TYPES_H

#ifndef __cplusplus
#define vec4 vec4

#else
#include "my_math.h"
#define vec4 Vec4

#endif

struct GlobalData
{
    int screenSizeX;
    int screenSizeY;
};

struct Transform
{
    float xPos;
    float yPos;
    float sizeX;
    float sizeY;
    int materialIdx;
};

struct PushData
{
    int transformIdx;
};

struct MaterialData
{
    vec4 color;
};

#endif // SHARED_RENDER_TYPES_H