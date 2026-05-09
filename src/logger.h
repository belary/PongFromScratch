#pragma once

#include "platform.h"

#include <stdio.h>

// ArgTypes...：一组类型
// args...：对应的一组参数
template <typename... ArgTypes>
void _log(char* prefix, TextColor color, const char* msg, ArgTypes... args)
{
    char fmtBuffer[32000] = {};
    char msgBuffer[32000] = {};
    sprintf(fmtBuffer, "%s: %s\n", prefix, msg);
    sprintf(msgBuffer, fmtBuffer, args...);
    platform_log(msgBuffer, color);
}

#define CAKEZ_TRACE(msg, ...) _log("TRACE", TEXT_COLOR_GREEN, msg, __VA_ARGS__)
#define CAKEZ_WARN(msg, ...) _log("WARN", TEXT_COLOR_YELLOW, msg, __VA_ARGS__)
#define CAKEZ_ERROR(msg, ...) _log("ERROR", TEXT_COLOR_RED, msg, __VA_ARGS__)
#define CAKEZ_FATAL(msg, ...) _log("FATAL", TEXT_COLOR_LIGHT_RED, msg, __VA_ARGS__)

#define CAKEZ_ASSERT(x, msg, ...)                                                                  \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            CAKEZ_ERROR(msg, __VA_ARGS__);                                                         \
            __debugbreak();                                                                        \
        }                                                                                          \
    }
