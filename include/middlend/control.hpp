#pragma once

#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"
#include "constants.hpp"
#include "instruction.hpp"
#include "middlend/reg.hpp"
#include "middlend/rob.hpp"
#include "utils/bus.hpp"

struct FetchedInstruction {
  Instruction ins;
  RobIDType id;
  RegDataType v_rs1 = 0;
  RegDataType v_rs2 = 0;
  RobIDType q_rs1 = 0;
  RobIDType q_rs2 = 0;
};

class Control {
  // in
  Channel<Instruction> &ins_channel;
  Channel<BranchResult> &branch_result_channel;

  // out
  Channel<FetchedInstruction> &alu_channel;
  Channel<FetchedInstruction> &mem_channel;
  Channel<FetchedInstruction> &branch_channel;

  Bus<ROBEntry> &commit_bus;
  Bus<bool> &flush_bus;

  Channel<PCType> &flush_pc_channel;

  // internal
  ReorderBuffer rob;
  CommonDataBus cdb;
  RegisterFile reg;

public:
  Control(Channel<Instruction> &ins_channel,
          Channel<BranchResult> &branch_result_channel,
          Channel<FetchedInstruction> &alu_channel,
          Channel<FetchedInstruction> &mem_channel,
          Channel<FetchedInstruction> &branch_channel,
          Bus<ROBEntry> &commit_bus,
          Bus<bool> &flush_bus,
          Channel<PCType> &flush_pc_channel)
      : ins_channel(ins_channel), branch_result_channel(branch_result_channel),
        alu_channel(alu_channel), mem_channel(mem_channel),
        branch_channel(branch_channel), commit_bus(commit_bus),
        flush_bus(flush_bus), flush_pc_channel(flush_pc_channel) {}

  void flush(){
    reg.flush();
    rob.flush();
    ins_channel.receive();//clear the input channels
    branch_result_channel.receive();
  }

  void work() {
    // if flush, flush and terminate
    if(flush_bus.get()){
      flush();
      return;
    }

    //check cdb for rob update
    auto cdb_result = cdb.get();
    if(cdb_result){
      rob.process_cdb(*cdb_result);
    }
    //check branch result
    auto branch_result = branch_result_channel.receive();
    if(branch_result){
      rob.process_branch(*branch_result);
    }

    //try commit
    auto commit_result = rob.process_commit();
    if(commit_result){
      reg.fill(commit_result->id, commit_result->value);
      commit_bus.send(*commit_result);
      if(is_branch(commit_result->type)){
        if(commit_result->predicted_taken!=commit_result->is_taken){
          flush_pc_channel.send(commit_result->pc);
          flush_bus.send(true);
          return;
        }
      }
    }


    // Fetch instruction and process registers
    auto ins_result = ins_channel.peek();
    if (ins_result) {
      Instruction ins = *ins_result;
      bool can_send = false;
      if (is_alu(ins.op)) {
        can_send = alu_channel.can_send();
      } else if (is_mem(ins.op)) {
        can_send = mem_channel.can_send();
      } else if (is_branch(ins.op)) {
        can_send = branch_channel.can_send();
      }
      if (can_send) {
        ins_channel.receive();
        auto id =
            rob.allocate({0, ins.op, ins.pc ,0,ISSUE, ins.is_branch, ins.predicted_taken, false});
        FetchedInstruction fetched = {ins, id};
        if (ins.rs1 != 0) {
          auto rs1 = reg.get(ins.rs1);
          fetched.v_rs1 = rs1.first;
          fetched.q_rs1 = rs1.second;
          if (fetched.q_rs1 != 0) {
            auto rob_value = rob.get(fetched.q_rs1);
            if (rob_value) {
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
            auto rob_value = rob.get(fetched.q_rs2);
            if (rob_value) {
              fetched.v_rs2 = *rob_value;
              fetched.q_rs2 = 0;
            }
          }
        }
        if (ins.rd != 0) {
          reg.preset(ins.rd, fetched.id);
        }
        if (is_alu(ins.op)) {
          alu_channel.send(fetched);
        } else if (is_mem(ins.op)) {
          mem_channel.send(fetched);
        } else if (is_branch(ins.op)) {
          branch_channel.send(fetched);
        }
      }
    }
  }
};