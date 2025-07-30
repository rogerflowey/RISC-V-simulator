#include <iostream>
#include <fstream>
#include "cpu.hpp"
#include "loader.hpp"
#include "logger.hpp"
#include "utils/logger/logger.hpp"

int main() {
    std::ofstream log_file("cpu_sim.log");
    logger.SetStream(log_file);
    logger.SetLevel(LogLevel::INFO);
    std::ifstream data_file("../data/testcases/hanoi.data");
    try {
        auto byte_memory = Loader::parse_memory_image(data_file);
        auto instruction_memory = Loader::convert_bytes_to_words(byte_memory);
        CPU cpu(instruction_memory);
        Clock& clock = Clock::getInstance();
        while (true) {
            clock.tick();
        }
    } catch (const std::exception& e) {
        std::cerr << "Critical error during setup or execution: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}