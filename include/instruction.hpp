#pragma once;
#include "constants.hpp"
enum class OpType {
  // R-Type
  ADD,
  SUB,
  OR,
  XOR,
  AND,
  SLL,
  SRL,
  SRA,
  SLT,
  SLTU,

  // I-Type (ALU)
  ADDI,
  ANDI,
  ORI,
  XORI,
  SLLI,
  SRLI,
  SRAI,
  SLTI,
  SLTIU,

  // I-Type (Load)
  LW,
  LH,
  LHU,
  LB,
  LBU,

  // I-Type (Jump)
  JALR,

  // S-Type (Store)
  SW,
  SH,
  SB,

  // B-Type (Branch)
  BEQ,
  BNE,
  BLT,
  BGE,
  BLTU,
  BGEU,

  // U-Type
  LUI,
  AUIPC,

  // J-Type
  JAL,

  INVALID // For decoding errors
};

inline bool is_alu(OpType op) {
  switch (op) {
  case OpType::ADD:
  case OpType::SUB:
  case OpType::OR:
  case OpType::XOR:
  case OpType::AND:
  case OpType::SLL:
  case OpType::SRL:
  case OpType::SRA:
  case OpType::SLT:
  case OpType::SLTU:
  case OpType::ADDI:
  case OpType::ANDI:
  case OpType::ORI:
  case OpType::XORI:
  case OpType::SLLI:
  case OpType::SRLI:
  case OpType::SRAI:
  case OpType::SLTI:
  case OpType::SLTIU:
    return true;
  default:
    return false;
  }
}

inline bool is_mem(OpType op) {
  switch (op) {
  case OpType::LW:
  case OpType::LH:
  case OpType::LHU:
  case OpType::LB:
  case OpType::LBU:
  case OpType::SW:
  case OpType::SH:
  case OpType::SB:
    return true;
  default:
    return false;
  }
}

inline bool is_branch(OpType op) {
  switch (op) {
  case OpType::BEQ:
  case OpType::BNE:
  case OpType::BLT:
  case OpType::BGE:
  case OpType::BLTU:
  case OpType::BGEU:
    return true;
  default:
    return false;
  }
}

struct Instruction {
  OpType op = OpType::INVALID;
  PCType pc = 0;

  RegIDType rd = 0;
  RegIDType rs1 = 0;
  RegIDType rs2 = 0;

  RegDataType imm = 0;

  bool is_branch = false;
  bool predicted_taken = false;
};