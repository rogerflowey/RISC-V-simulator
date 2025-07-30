#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <iterator> // For std::distance

/**
 * @brief Converts a little-endian byte stream to an unsigned 32-bit integer.
 * @tparam It Iterator type (e.g., std::byte*)
 * @param begin Iterator to the first (least significant) byte.
 * @param end Iterator to one past the last byte.
 * @return The assembled uint32_t value.
 */
template <typename It>
inline uint32_t bytes_to_uint(It begin, It end) {
    uint32_t result = 0;
    int shift = 0;
    for (auto it = begin; it != end; ++it) {
        result |= (static_cast<uint32_t>(static_cast<uint8_t>(*it)) << shift);
        shift += 8;
    }
    return result;
}

/**
 * @brief Converts a little-endian byte stream to a signed 32-bit integer, with proper sign extension.
 * @tparam It Iterator type (e.g., std::byte*)
 * @param begin Iterator to the first (least significant) byte.
 * @param end Iterator to one past the last byte.
 * @return The assembled and sign-extended int32_t value.
 */
template <typename It>
inline int32_t bytes_to_sint(It begin, It end) {
    int num_bytes = std::distance(begin, end);
    if (num_bytes == 0) {
        return 0;
    }

    // First, build the value as if it were unsigned using the little-endian function
    uint32_t val = bytes_to_uint(begin, end);

    // Now, perform sign extension if necessary
    if (num_bytes < 4) {
        int bit_width = num_bytes * 8;
        // Check the sign bit of the actual value (e.g., bit 7 for a byte, bit 15 for a half-word)
        if ((val >> (bit_width - 1)) & 1) {
            // If the sign bit is 1, extend it by filling the upper bits with 1s
            uint32_t mask = 0xFFFFFFFF << bit_width;
            val |= mask;
        }
    }

    return static_cast<int32_t>(val);
}

/**
 * @brief Converts a uint32_t to a little-endian byte array.
 * @param val The 32-bit value to convert.
 * @return A 4-byte array with the LSB at index 0.
 */
inline std::array<std::byte, 4> uint_to_bytes(uint32_t val) {
    std::array<std::byte, 4> result;
    // Little-Endian: LSB at the lowest address (index 0)
    result[0] = static_cast<std::byte>(val & 0xFF);
    result[1] = static_cast<std::byte>((val >> 8) & 0xFF);
    result[2] = static_cast<std::byte>((val >> 16) & 0xFF);
    result[3] = static_cast<std::byte>((val >> 24) & 0xFF);
    return result;
}