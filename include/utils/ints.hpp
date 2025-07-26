#pragma once
#include <cstdint>




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
