#pragma once
#include "constants.hpp"
#include <ostream> // Required for std::ostream
#include <string>  // Required for std::string

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

// Function to convert OpType to a string representation
inline const char *to_string(OpType op) {
  switch (op) {
  case OpType::ADD: return "ADD";
  case OpType::SUB: return "SUB";
  case OpType::OR: return "OR";
  case OpType::XOR: return "XOR";
  case OpType::AND: return "AND";
  case OpType::SLL: return "SLL";
  case OpType::SRL: return "SRL";
  case OpType::SRA: return "SRA";
  case OpType::SLT: return "SLT";
  case OpType::SLTU: return "SLTU";
  case OpType::ADDI: return "ADDI";
  case OpType::ANDI: return "ANDI";
  case OpType::ORI: return "ORI";
  case OpType::XORI: return "XORI";
  case OpType::SLLI: return "SLLI";
  case OpType::SRLI: return "SRLI";
  case OpType::SRAI: return "SRAI";
  case OpType::SLTI: return "SLTI";
  case OpType::SLTIU: return "SLTIU";
  case OpType::LW: return "LW";
  case OpType::LH: return "LH";
  case OpType::LHU: return "LHU";
  case OpType::LB: return "LB";
  case OpType::LBU: return "LBU";
  case OpType::JALR: return "JALR";
  case OpType::SW: return "SW";
  case OpType::SH: return "SH";
  case OpType::SB: return "SB";
  case OpType::BEQ: return "BEQ";
  case OpType::BNE: return "BNE";
  case OpType::BLT: return "BLT";
  case OpType::BGE: return "BGE";
  case OpType::BLTU: return "BLTU";
  case OpType::BGEU: return "BGEU";
  case OpType::LUI: return "LUI";
  case OpType::AUIPC: return "AUIPC";
  case OpType::JAL: return "JAL";
  // Default
  case OpType::INVALID: return "INVALID";
  default: return "UNKNOWN";
  }
}

// Overload the << operator for easy printing of OpType
inline std::ostream &operator<<(std::ostream &os, OpType op) {
  os << to_string(op);
  return os;
}

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
  case OpType::AUIPC:
  case OpType::LUI:
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
  case OpType::JAL:
  case OpType::JALR:
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


struct FilledInstruction {
  Instruction ins;
  RobIDType id;
  RegDataType v_rs1 = 0;
  RegDataType v_rs2 = 0;
  RobIDType q_rs1 = 0;
  RobIDType q_rs2 = 0;
};
#include <sstream>

inline std::string to_string(const Instruction &ins) {
  std::stringstream ss;
  ss << "PC: 0x" << std::hex << ins.pc << std::dec << " | " << ins.op
     << " | rd: " << std::to_string(ins.rd) << ", rs1: " << std::to_string(ins.rs1) << ", rs2: " << std::to_string(ins.rs2)
     << " | imm: 0x" << std::hex << ins.imm << std::dec;
  if (ins.is_branch) {
    ss << " | Branch (Predicted "
       << (ins.predicted_taken ? "Taken" : "Not Taken") << ")";
  }
  return ss.str();
}

inline std::string to_string(const FilledInstruction &fins) {
  std::stringstream ss;
  ss << to_string(fins.ins) << " | ROB ID: " << fins.id << " | v_rs1: 0x"
     << std::hex << fins.v_rs1 << ", v_rs2: 0x" << fins.v_rs2 << std::dec
     << " | q_rs1: " << fins.q_rs1 << ", q_rs2: " << fins.q_rs2;
  return ss.str();
}

inline std::ostream &operator<<(std::ostream &os, const Instruction &ins) {
  os << to_string(ins);
  return os;
}

inline std::ostream &operator<<(std::ostream &os,
                                const FilledInstruction &fins) {
  os << to_string(fins);
  return os;
}