#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <stdexcept>

namespace Loader {

static uint8_t hex_to_byte(const std::string& hex) {
    if (hex.length() > 2) {
        throw std::invalid_argument("Hex string must be 1 or 2 characters long.");
    }
    return static_cast<uint8_t>(std::stoul(hex, nullptr, 16));
}

inline std::vector<uint8_t> parse_memory_image(std::istream& in) {
    std::vector<uint8_t> memory;
    std::string line;
    uint32_t current_addr = 0;

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        if (line[0] == '@') {
            current_addr = std::stoul(line.substr(1), nullptr, 16);
        } else {
            std::stringstream ss(line);
            std::string hex_byte_str;
            while (ss >> hex_byte_str) {
                if (current_addr >= memory.size()) {
                    memory.resize(current_addr + 1, 0);
                }
                memory[current_addr++] = hex_to_byte(hex_byte_str);
            }
        }
    }
    return memory;
}


inline std::vector<uint32_t> convert_bytes_to_words(const std::vector<uint8_t>& bytes) {
    if (bytes.size() % 4 != 0) {
        throw std::invalid_argument("Byte vector size must be a multiple of 4 for word conversion.");
    }

    std::vector<uint32_t> words(bytes.size() / 4, 0);
    for (size_t i = 0; i < bytes.size(); i += 4) {
        // Assemble the 32-bit word in little-endian format from four bytes.
        uint32_t word = (uint32_t)bytes[i+3] << 24 |
                        (uint32_t)bytes[i+2] << 16 |
                        (uint32_t)bytes[i+1] << 8  |
                        (uint32_t)bytes[i];
        words[i / 4] = word;
    }
    return words;
}

} // namespace Loader