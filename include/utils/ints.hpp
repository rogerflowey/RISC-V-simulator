#pragma once
#include <cstddef>
#include <cstdint>
#include <array>



//convert big endian byte stream to uint
template <typename It>
inline uint32_t bytes_to_uint(It begin,It end){
    uint32_t result = 0;
    for (auto it = begin; it != end; ++it){
        result = (result << 8) | static_cast<uint8_t>(*it);
    }
    return result;
}


//convert big endian byte stream to sint
template <typename It>
inline int32_t bytes_to_sint(It begin, It end) {
    uint32_t val = 0;
    int num_bytes = 0;
    for (auto it = begin; it != end; ++it) {
        val = (val << 8) | static_cast<uint8_t>(*it);
        num_bytes++;
    }

    if (num_bytes == 0) {
        return 0;
    }

    // Sign extend from the most significant bit of the value read.
    int shift = (4 - num_bytes) * 8;
    if (shift < 32) {
        // Re-align to 32 bits and then sign extend
        return static_cast<int32_t>(val << shift) >> shift;
    }

    return static_cast<int32_t>(val);
}

inline std::array<std::byte, 4> uint_to_bytes(uint32_t val){
    std::array<std::byte, 4> result;
    result[0] = static_cast<std::byte>((val >> 24) & 0xFF);
    result[1] = static_cast<std::byte>((val >> 16) & 0xFF);
    result[2] = static_cast<std::byte>((val >> 8) & 0xFF);
    result[3] = static_cast<std::byte>(val & 0xFF);
    return result;
}