#pragma once

#include <cstddef>
#include <cstdint>

using RegIDType = uint8_t;
using RegDataType = uint32_t;
using WordType = uint32_t;

// 0 is left for unused
using RobIDType = uint32_t; // for simplicity to avoid considering circular situations


using MemAddrType = uint32_t;
using MemDataType = uint32_t;
using ImmType = uint16_t;
using PCType = uint32_t;

constexpr size_t MEMORY_SIZE = 1024 * 1024;
constexpr size_t CACHE_LINE_SIZE = 8;
constexpr size_t BANDWIDTH = 8;
constexpr size_t MEMORY_LATENCY = 3;

constexpr RobIDType REG_SIZE = 32;

constexpr size_t ROB_SIZE = 32;

constexpr size_t LSB_SIZE = 32;


constexpr size_t RS_MEM_SIZE = 32;
constexpr size_t RS_ALU_SIZE = 32;
constexpr size_t RS_BRANCH_SIZE = 32;