#pragma once

#include "backend/cdb.hpp"
#include "backend/memsys/memory.hpp"
#include "utils/hive.hpp"
#include "utils/bus.hpp"
#include "instruction.hpp" 
#include <optional>        
#include "logger.hpp"

inline std::optional<MemoryRequestType> get_mem_req_type(OpType op) {
    switch (op) {
        case OpType::LW:
        case OpType::LH:
        case OpType::LHU:
        case OpType::LB:
        case OpType::LBU:
            return MemoryRequestType::READ;
        case OpType::SW:
        case OpType::SH:
        case OpType::SB:
            return MemoryRequestType::WRITE;
        default:
            return std::nullopt;
    }
}


template <size_t BufferSize>
class MemoryReservationStation {
  hive<FilledInstruction, BufferSize> buffer;

  //input
  CommonDataBus& cdb;
  Channel<FilledInstruction>& ins_in_c;

  //output
  Channel<FilledInstruction>& exec_out_c;
  Channel<std::pair<RobIDType, MemoryRequestType>>& mob_mark_out_c;

  Bus<bool>& global_flush_bus;
public:
  MemoryReservationStation(CommonDataBus& cdb,
                           Channel<FilledInstruction>& ins_channel,
                           Channel<FilledInstruction>& exec_channel,
                           Channel<std::pair<RobIDType, MemoryRequestType>>& mob_mark_channel,
                           Bus<bool>& global_flush_bus)
      : cdb(cdb),
        ins_in_c(ins_channel),
        exec_out_c(exec_channel),
        mob_mark_out_c(mob_mark_channel),
        global_flush_bus(global_flush_bus) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  void work() {
    if (global_flush_bus.get()) {
      if (!buffer.empty()) {
          logger.Info("Flushing MemoryReservationStation");
      }
      buffer.clear();
      ins_in_c.clear();
      return;
    }

    if (!buffer.full()) {
      auto ins_peek = ins_in_c.peek();
      if (ins_peek) {
        if (mob_mark_out_c.can_send()) {
          auto result = ins_in_c.receive(); 
          logger.With("ROB_ID", result->id)
                .Info("MemoryReservationStation received new instruction");
          auto mem_type = get_mem_req_type(result->ins.op);
          if (mem_type) {
            mob_mark_out_c.send({result->id, *mem_type});
            logger.With("ROB_ID", result->id)
                  .With("Type", *mem_type == MemoryRequestType::READ ? "READ" : "WRITE")
                  .Info("Marking MOB for memory operation");
          }
          buffer.insert(*result);
        }
      }
    }
    auto cdb_result = cdb.get();
    if (cdb_result) {
      logger.With("SourceROB_ID", cdb_result->rob_id)
            .With("Value", cdb_result->data)
            .Info("MemoryReservationStation received CDB broadcast");
      for (auto it = buffer.begin(); it != buffer.end(); ++it) {
        if (it->q_rs1 != 0 && it->q_rs1 == cdb_result->rob_id) {
          logger.With("UpdatedROB_ID", it->id)
                .With("Operand", "rs1")
                .With("SourceROB_ID", cdb_result->rob_id)
                .Info("Updating operand from CDB");
          it->v_rs1 = cdb_result->data;
          it->q_rs1 = 0;
        }
        if (it->q_rs2 != 0 && it->q_rs2 == cdb_result->rob_id) {
          logger.With("UpdatedROB_ID", it->id)
                .With("Operand", "rs2")
                .With("SourceROB_ID", cdb_result->rob_id)
                .Info("Updating operand from CDB");
          it->v_rs2 = cdb_result->data;
          it->q_rs2 = 0;
        }
      }
    }
    for (auto it = buffer.begin(); it != buffer.end(); ++it) {
      if (it->q_rs1 == 0 && it->q_rs2 == 0) {
        if (exec_out_c.can_send()) {
          logger.With("ROB_ID", it->id)
                .Info("Dispatching instruction from MemoryReservationStation to execution unit");
          exec_out_c.send(*it);
          it = buffer.erase(it);
          break;
        }
      }
    }
  }
};