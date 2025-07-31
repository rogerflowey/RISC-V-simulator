#include <iostream>
#include "cpu.hpp"
#include "loader.hpp"
#include "logger.hpp"
#include "utils/logger/logger.hpp"

int main() {
    //std::ofstream log_file("cpu_sim.log");
    //logger.SetStream(log_file);
    logger.SetStream(std::cerr);
    //logger.SetLevel(LogLevel::INFO);
    logger.SetLevel(LogLevel::ERROR);
    //std::ifstream data_file("../data/testcases/qsort.data");
    try {
        //auto initial_memory_image = Loader::parse_memory_image(data_file);
        auto initial_memory_image = Loader::parse_memory_image(std::cin);
        CPU cpu(initial_memory_image);

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