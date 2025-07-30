#pragma once

#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"
#include "constants.hpp"
#include "instruction.hpp"
#include "logger.hpp"
#include "middlend/reg.hpp"
#include "middlend/rob.hpp"
#include "utils/bus.hpp"
#include <cstdlib>
#include <iostream>

class Control {
  // in
  Channel<Instruction> &ins_channel;
  Channel<BranchResult> &branch_result_channel;
  CommonDataBus &cdb;

  // out
  Channel<FilledInstruction> &alu_channel;
  Channel<FilledInstruction> &mem_channel;
  Channel<FilledInstruction> &branch_channel;

  Bus<ROBEntry> &commit_bus;
  Bus<bool> &flush_bus;

  Channel<PCType> &flush_pc_channel;

  // internal
  ReorderBuffer rob;
  RegisterFile reg;

public:
  Control(Channel<Instruction> &ins_channel,
          Channel<BranchResult> &branch_result_channel,
          Channel<FilledInstruction> &alu_channel,
          Channel<FilledInstruction> &mem_channel,
          Channel<FilledInstruction> &branch_channel, Bus<ROBEntry> &commit_bus,
          Bus<bool> &flush_bus, Channel<PCType> &flush_pc_channel,
          CommonDataBus &cdb)
      : ins_channel(ins_channel), branch_result_channel(branch_result_channel),
        cdb(cdb), alu_channel(alu_channel), mem_channel(mem_channel),
        branch_channel(branch_channel), commit_bus(commit_bus),
        flush_bus(flush_bus), flush_pc_channel(flush_pc_channel) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  // test use only
  auto &get_reg() { return reg; }

  void flush() {
    logger.Warn("Control unit and pipeline flushed.");
    reg.flush();
    rob.flush();
    ins_channel.clear();
    branch_result_channel.clear();
  }

  void work() {
    if (flush_bus.get()) {
      flush();
      return;
    }

    // Check CDB for completed instructions
    if (auto cdb_result = cdb.get()) {
      logger.With("ROB_ID", cdb_result->rob_id)
          .Info("CDB result received by Control.");
      rob.process_cdb(*cdb_result);
    }
    // Check for resolved branches
    if (auto branch_result = branch_result_channel.receive()) {
      logger.With("ROB_ID", branch_result->rob_id)
          .Info("Branch result received by Control.");
      rob.process_branch(*branch_result);
    }
    if (!rob.empty()) {
      const auto &head_entry = rob.front();

      if (head_entry.state == HALT) {
        auto a0_state = reg.get(10);
        RegDataType a0_value = a0_state.first;
        std::cout << (a0_value & 0xff) << std::endl;
        exit(0);
      }
      if (head_entry.type == OpType::INVALID) {
        throw logger.With("PC", head_entry.pc).Error("Attempted to commit an INVALID instruction. Halting.");
      }

      // Normal commit logic
      if (head_entry.state == COMMIT) {
        ROBEntry commit_result = head_entry;
        rob.pop_front();
      
        logger.With("ROB_ID", commit_result.id)
            .With("PC", commit_result.pc)
            .With("RegID", static_cast<int>(commit_result.reg_id))
            .With("Value", commit_result.value)
            .Info("Instruction committed.");
        reg.fill(commit_result.id, commit_result.reg_id, commit_result.value);
        commit_bus.send(commit_result);

        if (is_branch(commit_result.type)) {
          if (commit_result.predicted_taken != commit_result.is_taken) {
            PCType correct_pc = commit_result.is_taken ? commit_result.target_pc
                                                       : (commit_result.pc + 4);
            logger.With("ROB_ID", commit_result.id)
                .With("Predicted", commit_result.predicted_taken)
                .With("Actual", commit_result.is_taken)
                .With("CorrectPC", correct_pc)
                .Warn("Branch misprediction detected. Triggering flush.");
            flush_pc_channel.send(correct_pc);
            flush_bus.send(true);
            return;
          }
        }
      }
    }

    // --- ISSUE STAGE ---
    auto ins_result = ins_channel.peek();
    if (ins_result && rob.can_allocate()) {
      Instruction ins = *ins_result;

      bool is_halt_instruction = (ins.op == OpType::ADDI && ins.rd == 10 &&
                                  ins.rs1 == 0 && ins.imm == 255);
      bool can_send = is_halt_instruction;
      if (!can_send) {
        if (is_alu(ins.op))
          can_send = alu_channel.can_send();
        else if (is_mem(ins.op))
          can_send = mem_channel.can_send();
        else if (is_branch(ins.op))
          can_send = branch_channel.can_send();
        else if (ins.op == OpType::INVALID)
          can_send = true;
      }

      if (can_send) {
        ins_channel.receive();

        ROBEntry new_entry = {0,
                              ins.op,
                              ins.pc,
                              ins.rd,
                              0,
                              is_halt_instruction ? HALT : ISSUE,
                              ins.is_branch,
                              ins.predicted_taken,
                              false,
                              0};

        auto id = rob.allocate(new_entry);
        logger.With("PC", ins.pc)
            .With("ROB_ID", id)
            .Info("Instruction issued and allocated in ROB.");

        if (is_halt_instruction) {
          logger.With("ROB_ID", id)
              .Warn("HALT instruction identified and placed in ROB.");
          return;
        }

        FilledInstruction fetched = {ins, id};
        if (ins.rs1 != 0) {
          auto rs1 = reg.get(ins.rs1);
          fetched.v_rs1 = rs1.first;
          fetched.q_rs1 = rs1.second;
          if (fetched.q_rs1 != 0) {
            if (auto rob_value = rob.get(fetched.q_rs1)) {
              fetched.v_rs1 = *rob_value;
              fetched.q_rs1 = 0;
            }
          }
        }
        if (ins.rs2 != 0) {
          auto rs2 = reg.get(ins.rs2);
          fetched.v_rs2 = rs2.first;
          fetched.q_rs2 = rs2.second;
          if (fetched.q_rs2 != 0) {
            if (auto rob_value = rob.get(fetched.q_rs2)) {
              fetched.v_rs2 = *rob_value;
              fetched.q_rs2 = 0;
            }
          }
        }
        if (ins.rd != 0) {
          reg.preset(ins.rd, fetched.id);
        }

        logger.With("Filled", to_string(fetched))
            .Info("Fetched register values for instruction.");

        if (is_alu(ins.op)) {
          logger.With("ROB_ID", fetched.id)
              .Info("Dispatching instruction to ALU.");
          alu_channel.send(fetched);
        } else if (is_mem(ins.op)) {
          logger.With("ROB_ID", fetched.id)
              .Info("Dispatching instruction to MEM.");
          mem_channel.send(fetched);
        } else if (is_branch(ins.op)) {
          logger.With("ROB_ID", fetched.id)
              .Info("Dispatching instruction to Branch Unit.");
          branch_channel.send(fetched);
        } else if (ins.op == OpType::INVALID) {
          logger.With("ROB_ID", fetched.id).Warn("INVALID instruction issued to ROB.");
        } else {
          logger.Warn("Unknown instruction type");
        }
      }
    }
  }
};