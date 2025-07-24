#pragma once
#include <cstdint>




//convert little endian byte stream to uint
template <typename It>
inline uint32_t bytes_to_uint(It begin,It end){
    uint32_t result = 0;
    int shift = 0;
    for (auto it = begin; it != end && shift < 32; it++){
        shift+=8;
        result |= static_cast<uint32_t>(static_cast<unsigned char>(*it)) << shift;
    }
    return result;
}


//convert little endian byte stream to uint
template <typename It>
inline int32_t bytes_to_sint(It begin, It end) {
    uint32_t val = 0;
    int shift = 0;
    auto it = begin;
    for (; it != end && shift < 32; ++it) {
        val |= static_cast<uint32_t>(static_cast<unsigned char>(*it)) << shift;
        shift += 8;
    }

    if (it == begin || shift == 0) {
        return 0;
    }

    // Sign extend from the most significant bit of the last byte read.
    int sign_bit_pos = shift - 1;
    if ((val >> sign_bit_pos) & 1) {
        uint32_t mask = 0xFFFFFFFF << sign_bit_pos;
        val |= mask;
    }

    return static_cast<int32_t>(val);
}