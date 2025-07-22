#pragma once
#include <sys/types.h>


template <typename It>
inline u_int32_t bytes_to_uint(It begin,It end){
    u_int32_t result = 0;
    int shift = 0;
    for (auto it = begin; it != end && shift < 32; it++){
        shift+=8;
        result |= static_cast<u_int32_t>(static_cast<unsigned char>(*it)) << shift;
    }
    return result;
}