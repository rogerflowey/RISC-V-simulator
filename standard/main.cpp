#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "interpreter.hpp"

// Function to load the program from the specified text file format
void load_program(const std::string& filename, Memory& memory) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open program file: " + filename);
  }

  std::string line;
  uint32_t current_addr = 0;
  bool addr_set = false;

  while (std::getline(file, line)) {
    if (line.empty()) continue;

    if (line[0] == '@') {
      current_addr = std::stoul(line.substr(1), nullptr, 16);
      addr_set = true;
    } else if (addr_set) {
      std::stringstream ss(line);
      unsigned int byte_val;
      while (ss >> std::hex >> byte_val) {
        memory.write_byte(current_addr++, static_cast<uint8_t>(byte_val));
      }
    }
  }
}

int main() {

  std::string program_filename = "../data/testcases/naive.data";
  std::string dump_filename = "../dump/std.dump";

  try {
    Memory memory;
    std::cout << "Loading program from " << program_filename << "..." << std::endl;
    load_program(program_filename, memory);
    std::cout << "Program loaded." << std::endl;

    RiscVInterpreter interpreter(memory, dump_filename,100000);
    std::cout << "Starting interpreter. Dumping state to " << dump_filename << "..." << std::endl;
    interpreter.run();

    interpreter.print_final_state();

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}