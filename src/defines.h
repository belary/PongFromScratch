#pragma once
#include <cstdint>

#define KB(x) ((uint64_t)x * 1024)
#define MB(x) ((uint64_t)x * KB(1024))
#define GB(x) ((uint64_t)x * MB(1024))

#define INVALID_IDX UINT32_MAX
#define global_variable static
#define internal static

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

// constexpr represents compile-time constant values
uint32_t constexpr MAX_ENTITIES = 37;