#pragma once

#include "backend/cdb.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "instruction.hpp"
#include "constants.hpp"
#include "logger.hpp"
#include "utils/logger/logger.hpp"
#include <cstdint>
struct BranchResult{
    RobIDType rob_id;
    bool is_taken;
    PCType target_pc;
};

#pragma once


class BranchUnit {
private:
  // Input
  Channel<FilledInstruction>& ins_in_c;
  Bus<bool>& global_flush_bus;

  // Outputs
  Channel<BranchResult>& branch_result_out_c;
  Channel<CDBResult>& cdb_out_c;

  BranchResult resolve_branch_outcome(const FilledInstruction& instr) {
    const auto& ins = instr.ins;
    const auto& v_rs1 = instr.v_rs1;
    const auto& v_rs2 = instr.v_rs2;
    const auto& imm = ins.imm;
    const auto& pc = ins.pc;

    bool is_taken = false;
    PCType target_pc = 0;

    // Determine if the branch is taken
    switch (ins.op) {
      // B-Type (Conditional Branches)
      case OpType::BEQ:  is_taken = (v_rs1 == v_rs2); break;
      case OpType::BNE:  is_taken = (v_rs1 != v_rs2); break;
      case OpType::BLT:  is_taken = (static_cast<int32_t>(v_rs1) < static_cast<int32_t>(v_rs2)); break;
      case OpType::BGE:  is_taken = (static_cast<int32_t>(v_rs1) >= static_cast<int32_t>(v_rs2)); break;
      case OpType::BLTU: is_taken = (static_cast<uint32_t>(v_rs1) < static_cast<uint32_t>(v_rs2)); break;
      case OpType::BGEU: is_taken = (static_cast<uint32_t>(v_rs1) >= static_cast<uint32_t>(v_rs2)); break;
      // J-Type & I-Type Jumps are always taken
      case OpType::JAL:
      case OpType::JALR:
        is_taken = true;
        break;
      default:
        logger.With("Op", to_string(ins.op)).Warn("BranchUnit received non-branch instruction.");
        break;
    }

    switch (ins.op) {
      case OpType::BEQ:
      case OpType::BNE:
      case OpType::BLT:
      case OpType::BGE:
      case OpType::BLTU:
      case OpType::BGEU:
      case OpType::JAL:
        target_pc = pc + imm;
        break;
      case OpType::JALR:
        target_pc = (v_rs1 + imm);
        break;
      default:
        logger.With("Type", to_string(ins.op)).Warn("BranchUnit received non-branch instruction.");
        target_pc = pc;
        break;
    }

    return {instr.id, is_taken, target_pc};
  }

public:
  BranchUnit(Channel<FilledInstruction>& ins_channel,
             Channel<BranchResult>& branch_res_channel,
             Channel<CDBResult>& cdb_channel,
             Bus<bool>& flush_bus)
      : ins_in_c(ins_channel),
        global_flush_bus(flush_bus),
        branch_result_out_c(branch_res_channel),
        cdb_out_c(cdb_channel) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  void work() {
    if (global_flush_bus.get()) {
      flush();
      return;
    }

    auto ins_peek = ins_in_c.peek();
    if (!ins_peek) {
      return;
    }

    const FilledInstruction& instr = *ins_peek;
    bool needs_cdb = (instr.ins.op == OpType::JAL || instr.ins.op == OpType::JALR);
    bool can_send_branch_result = branch_result_out_c.can_send();
    bool can_send_cdb_result = needs_cdb ? cdb_out_c.can_send() : true;

    if (can_send_branch_result && can_send_cdb_result) {
      ins_in_c.receive();

      logger.With("ROB_ID", instr.id)
            .With("Op", to_string(instr.ins.op))
            .Info("BranchUnit executing instruction.");

      BranchResult branch_res = resolve_branch_outcome(instr);
      branch_result_out_c.send(branch_res);
      logger.With("ROB_ID", branch_res.rob_id)
            .With("Taken", branch_res.is_taken)
            .With("TargetPC", branch_res.target_pc)
            .Info("BranchUnit sent branch result.");

      if (needs_cdb) {
        RegDataType link_address = instr.ins.pc + 4;
        CDBResult cdb_res = {instr.id, link_address};
        cdb_out_c.send(cdb_res);
        logger.With("ROB_ID", cdb_res.rob_id)
              .With("LinkAddr", cdb_res.data)
              .Info("BranchUnit (JAL/JALR) sent link address to its CDB channel.");
      }
    }
  }

  void flush() {
    ins_in_c.clear();
  }
};