#include "assets.h"
#include "dds_structs.h"
#include "logger.h"
#include "platform.h"


const char *get_asset(AssetTypeID typeID)
{
    switch (typeID)
    {
        case ASSET_SPRITE_WHITE:
        {
            char *white = new char[4];
            white[0] = 255;
            white[1] = 255;
            white[2] = 255;
            white[3] = 255;
            return (const char *)white;
        }
        break;

        case ASSET_SPRITE_BALL:
        {
            uint32_t size;
            const char *data = platform_read_file("assets/textures/ball.DDS", &size);
            return data;
        }
        break;

        case ASSET_SPRITE_PADDLE:
        {
            uint32_t size;
            const char *data = platform_read_file("assets/textures/paddle.DDS", &size);
            return data;
        }
        break;

        default:
            CAKEZ_ASSERT(0, "Unrecognized Asset Type ID: %d", typeID);
    }
    
    return 0;
}