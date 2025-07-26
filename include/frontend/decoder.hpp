#pragma once
#include "constants.hpp"

enum class OpType {
    // R-Type
    ADD, SUB, OR, XOR, AND, SLL, SRL, SRA, SLT, SLTU,

    // I-Type (ALU)
    ADDI, ANDI, ORI, XORI, SLLI, SRLI, SRAI, SLTI, SLTIU, 

    // I-Type (Load)
    LW, LH, LHU, LB, LBU,

    // I-Type (Jump)
    JALR,

    // S-Type (Store)
    SW, SH, SB,

    // B-Type (Branch)
    BEQ, BNE, BLT, BGE, BLTU, BGEU,

    // U-Type
    LUI, AUIPC,

    // J-Type
    JAL,

    INVALID  // For decoding errors
};

struct Instruction {
    OpType op = OpType::INVALID;
    PCType pc = 0;

    RegIDType rd = 0;
    RegIDType rs1 = 0;
    RegIDType rs2 = 0;

    RegDataType imm = 0;
};


inline Instruction decode(uint32_t instruction_word, PCType current_pc) {
    Instruction decoded_inst;
    decoded_inst.pc = current_pc;

    const uint32_t opcode = instruction_word & 0x7F;
    const uint32_t rd     = (instruction_word >> 7)  & 0x1F;
    const uint32_t funct3 = (instruction_word >> 12) & 0x7;
    const uint32_t rs1    = (instruction_word >> 15) & 0x1F;
    const uint32_t rs2    = (instruction_word >> 20) & 0x1F;
    const uint32_t funct7 = (instruction_word >> 25) & 0x7F;

    switch (opcode) {
        case 0b0110111: // LUI
            decoded_inst.op = OpType::LUI;
            decoded_inst.rd = rd;
            decoded_inst.imm = instruction_word & 0xFFFFF000;
            break;
        case 0b0010111: // AUIPC
            decoded_inst.op = OpType::AUIPC;
            decoded_inst.rd = rd;
            decoded_inst.imm = instruction_word & 0xFFFFF000;
            break;

        case 0b1101111: // JAL
            decoded_inst.op = OpType::JAL;
            decoded_inst.rd = rd;
            {
                uint32_t imm20   = (instruction_word >> 31) & 1;
                uint32_t imm10_1 = (instruction_word >> 21) & 0x3FF;
                uint32_t imm11   = (instruction_word >> 20) & 1;
                uint32_t imm19_12= (instruction_word >> 12) & 0xFF;
                uint32_t imm_val = (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);
                decoded_inst.imm = static_cast<int32_t>(imm_val << 11) >> 11;
            }
            break;

        case 0b1100111: // JALR
            decoded_inst.op = OpType::JALR;
            decoded_inst.rd = rd;
            decoded_inst.rs1 = rs1;
            decoded_inst.imm = static_cast<int32_t>(instruction_word) >> 20;
            break;

        case 0b1100011: // BEQ, BNE, BLT, BGE, BLTU, BGEU
            decoded_inst.rs1 = rs1;
            decoded_inst.rs2 = rs2;
            {
                uint32_t imm12   = (instruction_word >> 31) & 1;
                uint32_t imm10_5 = (instruction_word >> 25) & 0x3F;
                uint32_t imm4_1  = (instruction_word >> 8)  & 0xF;
                uint32_t imm11   = (instruction_word >> 7)  & 1;
                uint32_t imm_val = (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);
                decoded_inst.imm = static_cast<int32_t>(imm_val << 19) >> 19;
            }
            switch (funct3) {
                case 0b000: decoded_inst.op = OpType::BEQ; break;
                case 0b001: decoded_inst.op = OpType::BNE; break;
                case 0b100: decoded_inst.op = OpType::BLT; break;
                case 0b101: decoded_inst.op = OpType::BGE; break;
                case 0b110: decoded_inst.op = OpType::BLTU; break;
                case 0b111: decoded_inst.op = OpType::BGEU; break;
                default:    decoded_inst.op = OpType::INVALID; break;
            }
            break;

        case 0b0000011: // LB, LH, LW, LBU, LHU
            decoded_inst.rd = rd;
            decoded_inst.rs1 = rs1;
            decoded_inst.imm = static_cast<int32_t>(instruction_word) >> 20;
            switch (funct3) {
                case 0b000: decoded_inst.op = OpType::LB; break;
                case 0b001: decoded_inst.op = OpType::LH; break;
                case 0b010: decoded_inst.op = OpType::LW; break;
                case 0b100: decoded_inst.op = OpType::LBU; break;
                case 0b101: decoded_inst.op = OpType::LHU; break;
                default:    decoded_inst.op = OpType::INVALID; break;
            }
            break;

        case 0b0100011: // SB, SH, SW
            decoded_inst.rs1 = rs1;
            decoded_inst.rs2 = rs2;
            {
                uint32_t imm11_5 = (instruction_word >> 25) & 0x7F;
                uint32_t imm4_0  = (instruction_word >> 7)  & 0x1F;
                uint32_t imm_val = (imm11_5 << 5) | imm4_0;
                decoded_inst.imm = static_cast<int32_t>(imm_val << 20) >> 20;
            }
            switch (funct3) {
                case 0b000: decoded_inst.op = OpType::SB; break;
                case 0b001: decoded_inst.op = OpType::SH; break;
                case 0b010: decoded_inst.op = OpType::SW; break;
                default:    decoded_inst.op = OpType::INVALID; break;
            }
            break;

        case 0b0010011: // ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI
            decoded_inst.rd = rd;
            decoded_inst.rs1 = rs1;
            switch (funct3) {
                case 0b000: decoded_inst.op = OpType::ADDI; break;
                case 0b010: decoded_inst.op = OpType::SLTI; break;
                case 0b011: decoded_inst.op = OpType::SLTIU; break;
                case 0b100: decoded_inst.op = OpType::XORI; break;
                case 0b110: decoded_inst.op = OpType::ORI; break;
                case 0b111: decoded_inst.op = OpType::ANDI; break;
                case 0b001: // SLLI
                    decoded_inst.op = OpType::SLLI;
                    break;
                case 0b101: // SRLI / SRAI
                    if (funct7 == 0b0100000) {
                        decoded_inst.op = OpType::SRAI;
                    } else {
                        decoded_inst.op = OpType::SRLI;
                    }
                    break;
                default:
                    decoded_inst.op = OpType::INVALID; break;
            }
            // For shifts, the immediate is just the 5-bit shamt.
            // For others, it's a 12-bit sign-extended value.
            if (funct3 == 0b001 || funct3 == 0b101) {
                decoded_inst.imm = rs2; // shamt is in the rs2 field for I-type shifts
            } else {
                decoded_inst.imm = static_cast<int32_t>(instruction_word) >> 20;
            }
            break;

        case 0b0110011: // ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
            decoded_inst.rd = rd;
            decoded_inst.rs1 = rs1;
            decoded_inst.rs2 = rs2;
            switch (funct3) {
                case 0b000: // ADD / SUB
                    if (funct7 == 0b0100000) {
                        decoded_inst.op = OpType::SUB;
                    } else { // funct7 == 0b0000000
                        decoded_inst.op = OpType::ADD;
                    }
                    break;
                case 0b001: decoded_inst.op = OpType::SLL; break;
                case 0b010: decoded_inst.op = OpType::SLT; break;
                case 0b011: decoded_inst.op = OpType::SLTU; break;
                case 0b100: decoded_inst.op = OpType::XOR; break;
                case 0b101: // SRL / SRA
                    if (funct7 == 0b0100000) {
                        decoded_inst.op = OpType::SRA;
                    } else { // funct7 == 0b0000000
                        decoded_inst.op = OpType::SRL;
                    }
                    break;
                case 0b110: decoded_inst.op = OpType::OR; break;
                case 0b111: decoded_inst.op = OpType::AND; break;
                default:    decoded_inst.op = OpType::INVALID; break;
            }
            break;

        default:
            decoded_inst.op = OpType::INVALID;
            break;
    }

    return decoded_inst;
}
