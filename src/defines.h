#pragma once
#include <cstdint>

#define KB(x) ((uint64_t)1024 * x)
#define MB(x) ((uint64_t)1024 * KB(x))
#define GB(x) ((uint64_t)1024 * MB(x))

#define BIT(x) (1 << x)

#define INVALID_IDX UINT32_MAX
#define global_variable static
#define internal static

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

// constexpr 表示编译期确定的值
uint32_t constexpr MAX_TRANSFORMS = 100;
uint32_t constexpr MAX_RENDER_COMMANDS = 100;
uint32_t constexpr MAX_DESCRIPTORS = 100;
uint32_t constexpr MAX_ENTITIES = 100;
uint32_t constexpr MAX_MATERIALS = 100;
uint32_t constexpr MAX_IMAGES = 100;
