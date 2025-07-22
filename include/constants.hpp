#pragma once

#include <cstddef>
#include <sys/types.h>

using RegIDType = u_int8_t;
using WordType = u_int32_t;



constexpr size_t MEMORY_SIZE = 1024 * 1024;
constexpr size_t CACHE_LINE_SIZE = 8;
constexpr size_t BANDWIDTH = 8;
constexpr size_t MEMORY_LATENCY = 3;