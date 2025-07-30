#pragma once

#include <cstddef>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <stdexcept>

namespace Loader {

    static std::byte hex_to_byte(const std::string& hex) {
        if (hex.length() > 2) {
            throw std::invalid_argument("Hex string must be 1 or 2 characters long.");
        }
        return static_cast<std::byte>(std::stoul(hex, nullptr, 16));
    }

    /**
     * @brief Parses a memory image file into a vector of bytes.
     * The byte order in the file is preserved exactly as is.
     * The interpretation of these bytes (endianness) is handled by other components.
     * @param in The input stream (e.g., from a file).
     * @return A vector of bytes representing the initial memory state.
     */
    inline std::vector<std::byte> parse_memory_image(std::istream& in) {
        std::vector<std::byte> memory;
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
                        // Resize with a bit of extra capacity to reduce reallocations
                        memory.resize(current_addr + 256, std::byte{0});
                    }
                    memory[current_addr++] = hex_to_byte(hex_byte_str);
                }
            }
        }
        // Trim excess capacity
        memory.resize(current_addr);
        return memory;
    }

} // namespace Loader