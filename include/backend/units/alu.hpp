#pragma once

#include "backend/cdb.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "instruction.hpp"
#include "constants.hpp"
#include "logger.hpp"
#include <cstdint>

class ALU {
private:
  // Input
  Channel<FilledInstruction>& ins_in_c;

  // Output
  Channel<CDBResult>& cdb_out_c;

  Bus<bool>& global_flush_bus;

  RegDataType calculate_result(const FilledInstruction& instr) {
    const auto& ins = instr.ins;
    const auto& v_rs1 = instr.v_rs1;
    const auto& v_rs2 = instr.v_rs2;
    const auto& imm = ins.imm;

    switch (ins.op) {
      // R-Type
      case OpType::ADD:  return v_rs1 + v_rs2;
      case OpType::SUB:  return v_rs1 - v_rs2;
      case OpType::AND:  return v_rs1 & v_rs2;
      case OpType::OR:   return v_rs1 | v_rs2;
      case OpType::XOR:  return v_rs1 ^ v_rs2;
      case OpType::SLL:  return v_rs1 << (v_rs2 & 0x1F);
      case OpType::SRL:  return static_cast<uint32_t>(v_rs1) >> (v_rs2 & 0x1F);
      case OpType::SRA:  return static_cast<int32_t>(v_rs1) >> (v_rs2 & 0x1F);
      case OpType::SLT:  return (static_cast<int32_t>(v_rs1) < static_cast<int32_t>(v_rs2)) ? 1 : 0;
      case OpType::SLTU: return (static_cast<uint32_t>(v_rs1) < static_cast<uint32_t>(v_rs2)) ? 1 : 0;

      // I-Type (ALU)
      case OpType::ADDI:  return v_rs1 + imm;
      case OpType::ANDI:  return v_rs1 & imm;
      case OpType::ORI:   return v_rs1 | imm;
      case OpType::XORI:  return v_rs1 ^ imm;
      case OpType::SLLI:  return v_rs1 << (imm & 0x1F);
      case OpType::SRLI:  return static_cast<uint32_t>(v_rs1) >> (imm & 0x1F);
      case OpType::SRAI:  return static_cast<int32_t>(v_rs1) >> (imm & 0x1F);
      case OpType::SLTI:  return (static_cast<int32_t>(v_rs1) < imm) ? 1 : 0;
      // FIX: Compare unsigned v_rs1 with signed imm. C++ promotion rules handle this correctly.
      case OpType::SLTIU: return (static_cast<uint32_t>(v_rs1) < imm) ? 1 : 0;

      // U-Type
      // FIX: The immediate from the decoder is already shifted. The ALU just passes it through or adds the PC.
      // For AUIPC, v_rs1 must be the PC value.
      case OpType::AUIPC: return v_rs1 + imm;
      case OpType::LUI:   return imm;

      default:
        logger.With("Op", to_string(ins.op)).Warn("ALU received an unsupported instruction type.");
        return 0;
    }
  }

public:
  ALU(Channel<FilledInstruction>& ins_channel, Channel<CDBResult>& cdb_channel, Bus<bool>& global_flush_bus)
      : ins_in_c(ins_channel), cdb_out_c(cdb_channel), global_flush_bus(global_flush_bus) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  void work() {
    if (global_flush_bus.get()) {
      ins_in_c.clear();
      return;
    }
    if (cdb_out_c.can_send()) {
      if (auto result = ins_in_c.receive()) {
        FilledInstruction instr = *result;

        logger.With("ROB_ID", instr.id)
              .With("Op", to_string(instr.ins.op))
              .Info("ALU executing instruction.");

        RegDataType calc_result = calculate_result(instr);
        CDBResult cdb_result = {instr.id, calc_result};
        cdb_out_c.send(cdb_result);

        logger.With("ROB_ID", cdb_result.rob_id)
              .With("Result", cdb_result.data)
              .Info("ALU sent result to its CDB channel.");
      }
    }
  }
  
};