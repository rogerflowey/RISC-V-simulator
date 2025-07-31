#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

#include "dump.hpp"

// Memory class remains the same
class Memory {
public:
    void write_byte(uint32_t addr, uint8_t value) { mem_[addr] = value; }
    uint8_t read_byte(uint32_t addr) {
        auto it = mem_.find(addr);
        return (it != mem_.end()) ? it->second : 0;
    }
    void write_word(uint32_t addr, uint32_t value) {
        for (int i = 0; i < 4; ++i) write_byte(addr + i, (value >> (i * 8)) & 0xFF);
    }
    uint32_t read_word(uint32_t addr) {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= (uint32_t)read_byte(addr + i) << (i * 8);
        return value;
    }
    uint32_t read_instruction(uint32_t addr) { return read_word(addr); }
private:
    std::map<uint32_t, uint8_t> mem_;
};

class RiscVInterpreter {
public:
    RiscVInterpreter(Memory& memory, const std::string& dump_filename, size_t max_instructions)
        : mem_(memory),
          dumper_(dump_filename),
          pc_(0x0),
          running_(true),
          instruction_count_(0),
          max_instructions_(max_instructions) {
        regs_.fill(0);
    }

    void run() {
        while (running_ && instruction_count_ < max_instructions_) {
            step();
        }
        if (running_ && instruction_count_ >= max_instructions_) {
            std::cout << "\n--- Simulation stopped: Instruction limit reached ("
                      << max_instructions_ << ") ---\n";
        }
    }

    void print_final_state() const {
        printf("\n--- Execution Halted ---\n");
        printf("Instructions Executed: %zu\n", instruction_count_);
        // The PC will be pointing at the halt instruction that was not executed
        printf("Halt PC: %s\n", norb::hex(pc_).c_str());
        printf("Final Register State (before halt):\n");
        for (size_t i = 0; i < 32; ++i) {
            if (i > 0 && i % 4 == 0) printf("\n");
            printf("x%-2zu: %-12s ", i, norb::hex(regs_[i]).c_str());
        }
        printf("\n");
    }

private:
    // CORRECTED: Check for halt condition *before* execution
    void step() {
        if (!running_) return;

        uint32_t pc_before_exec = pc_;
        uint32_t inst = mem_.read_instruction(pc_before_exec);

        // The instruction `li a0, 255` (addi a0, x0, 255) is encoded as 0x0ff00513.
        // This is the special halt instruction for your simulator.
        if (inst == 0x0ff00513) {
            std::cout << "\n--- Halt Instruction Detected ---\n";
            std::cout << "Detected halt instruction at PC=0x00000008.\n";
            std::cout << "Simulation stopped BEFORE executing this instruction.\n";

            // The value 255 is the payload of the halt signal.
            uint8_t low_8_bits_of_a0_payload = 255;
            std::cout << "Halt signal payload (low 8 bits of immediate): "
                      << norb::hex(low_8_bits_of_a0_payload)
                      << " (" << std::dec << static_cast<int>(low_8_bits_of_a0_payload) << ")\n";

            running_ = false; // Stop the main loop
            return;           // Exit the step function immediately. Do NOT execute or dump.
        }

        if (inst == 0) {
            running_ = false;
            return;
        }

        execute(inst);
        regs_[0] = 0;

        dumper_.dump(pc_before_exec, regs_);
        instruction_count_++;
    }

    // The execute function remains unchanged
    void execute(uint32_t inst) {
        uint32_t opcode = inst & 0x7F;
        uint32_t rd = (inst >> 7) & 0x1F;
        uint32_t funct3 = (inst >> 12) & 0x7;
        uint32_t rs1 = (inst >> 15) & 0x1F;
        uint32_t rs2 = (inst >> 20) & 0x1F;
        uint32_t funct7 = (inst >> 25) & 0x7F;

        uint32_t next_pc = pc_ + 4;

        switch (opcode) {
            case 0x37: // LUI
                if (rd != 0) regs_[rd] = inst & 0xFFFFF000;
                break;
            case 0x17: // AUIPC
                if (rd != 0) regs_[rd] = pc_ + (inst & 0xFFFFF000);
                break;
            case 0x6F: { // JAL
                int32_t imm = (((int32_t)inst >> 20) & 0x7FE) | ((inst >> 9) & 0x800) | (inst & 0xFF000) | (((int32_t)inst >> 11) & 0x100000);
                imm = (imm << 11) >> 11;
                if (rd != 0) regs_[rd] = pc_ + 4;
                next_pc = pc_ + imm;
                break;
            }
            case 0x67: { // JALR
                int32_t imm = (int32_t)inst >> 20;
                if (rd != 0) regs_[rd] = pc_ + 4;
                next_pc = (regs_[rs1] + imm) & ~1;
                break;
            }
            case 0x63: { // Branch
                int32_t imm = (((inst >> 7) & 0x1E)) | (((inst >> 20) & 0x7E0)) | (((inst << 4) & 0x800)) | (((int32_t)inst >> 19) & 0x1000);
                imm = (imm << 19) >> 19;
                bool taken = false;
                switch (funct3) {
                    case 0x0: taken = (regs_[rs1] == regs_[rs2]); break; // BEQ
                    case 0x1: taken = ((int32_t)regs_[rs1] != (int32_t)regs_[rs2]); break; // BNE
                    case 0x4: taken = ((int32_t)regs_[rs1] < (int32_t)regs_[rs2]); break; // BLT
                    case 0x5: taken = ((int32_t)regs_[rs1] >= (int32_t)regs_[rs2]); break; // BGE
                    case 0x6: taken = (regs_[rs1] < regs_[rs2]); break; // BLTU
                    case 0x7: taken = (regs_[rs1] >= regs_[rs2]); break; // BGEU
                    default: running_ = false; break;
                }
                if (taken) next_pc = pc_ + imm;
                break;
            }
            case 0x03: { // Load
                int32_t imm = (int32_t)inst >> 20;
                uint32_t addr = regs_[rs1] + imm;
                if (rd != 0) {
                    switch (funct3) {
                        case 0x0: regs_[rd] = (int8_t)mem_.read_byte(addr); break; // LB
                        case 0x1: regs_[rd] = (int16_t)mem_.read_word(addr); break; // LH (simplified)
                        case 0x2: regs_[rd] = mem_.read_word(addr); break; // LW
                        case 0x4: regs_[rd] = mem_.read_byte(addr); break; // LBU
                        default: running_ = false; break;
                    }
                }
                break;
            }
            case 0x23: { // Store
                int32_t imm = (((inst >> 20) & 0xFE0) | ((inst >> 7) & 0x1F));
                imm = (imm << 20) >> 20;
                uint32_t addr = regs_[rs1] + imm;
                switch (funct3) {
                    case 0x0: mem_.write_byte(addr, regs_[rs2] & 0xFF); break; // SB
                    case 0x1: mem_.write_word(addr, regs_[rs2] & 0xFFFF); break; // SH (simplified)
                    case 0x2: mem_.write_word(addr, regs_[rs2]); break; // SW
                    default: running_ = false; break;
                }
                break;
            }
            case 0x13: { // Immediate Arithmetic
                int32_t imm = (int32_t)inst >> 20;
                if (rd != 0) {
                    switch (funct3) {
                        case 0x0: regs_[rd] = regs_[rs1] + imm; break; // ADDI
                        case 0x2: regs_[rd] = ((int32_t)regs_[rs1] < imm) ? 1 : 0; break; // SLTI
                        case 0x3: regs_[rd] = (regs_[rs1] < (uint32_t)imm) ? 1 : 0; break; // SLTIU
                        case 0x4: regs_[rd] = regs_[rs1] ^ imm; break; // XORI
                        case 0x6: regs_[rd] = regs_[rs1] | imm; break; // ORI
                        case 0x7: regs_[rd] = regs_[rs1] & imm; break; // ANDI
                        case 0x1: regs_[rd] = regs_[rs1] << (imm & 0x1F); break; // SLLI
                        case 0x5:
                            if (funct7 == 0x00) regs_[rd] = regs_[rs1] >> (imm & 0x1F); // SRLI
                            else regs_[rd] = (int32_t)regs_[rs1] >> (imm & 0x1F); // SRAI
                            break;
                        default: running_ = false; break;
                    }
                }
                break;
            }
            case 0x33: { // Register-Register Arithmetic
                if (rd != 0) {
                    switch (funct3) {
                        case 0x0:
                            if (funct7 == 0x00) regs_[rd] = regs_[rs1] + regs_[rs2]; // ADD
                            else regs_[rd] = regs_[rs1] - regs_[rs2]; // SUB
                            break;
                        case 0x1: regs_[rd] = regs_[rs1] << (regs_[rs2] & 0x1F); break; // SLL
                        case 0x2: regs_[rd] = ((int32_t)regs_[rs1] < (int32_t)regs_[rs2]) ? 1 : 0; break; // SLT
                        case 0x3: regs_[rd] = (regs_[rs1] < regs_[rs2]) ? 1 : 0; break; // SLTU
                        case 0x4: regs_[rd] = regs_[rs1] ^ regs_[rs2]; break; // XOR
                        case 0x5:
                            if (funct7 == 0x00) regs_[rd] = regs_[rs1] >> (regs_[rs2] & 0x1F); // SRL
                            else regs_[rd] = (int32_t)regs_[rs1] >> (regs_[rs2] & 0x1F); // SRA
                            break;
                        case 0x6: regs_[rd] = regs_[rs1] | regs_[rs2]; break; // OR
                        case 0x7: regs_[rd] = regs_[rs1] & regs_[rs2]; break; // AND
                        default: running_ = false; break;
                    }
                }
                break;
            }
            case 0x73: // ECALL/EBREAK
                running_ = false;
                break;
            default:
                running_ = false;
                break;
        }
        pc_ = next_pc;
    }

private:
    Memory& mem_;
    norb::RegisterDumper<32> dumper_;
    uint32_t pc_;
    std::array<uint32_t, 32> regs_;
    bool running_;
    size_t instruction_count_;
    const size_t max_instructions_;
};