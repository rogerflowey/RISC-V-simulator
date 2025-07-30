#pragma once

#include "backend/cdb.hpp"
#include "middlend/control.hpp"
#include "utils/hive.hpp"
#include "utils/bus.hpp"
#include "logger.hpp"

template <size_t BufferSize>
class ReservationStation {
  hive<FilledInstruction, BufferSize> buffer;

  //input
  CommonDataBus& cdb;
  Channel<FilledInstruction>& ins_in_c;

  //output
  Channel<FilledInstruction>& exec_out_c;

  Bus<bool>& global_flush_bus;
public:
  ReservationStation(CommonDataBus& cdb,
                     Channel<FilledInstruction>& ins_channel,
                     Channel<FilledInstruction>& exec_channel,
                     Bus<bool>& global_flush_bus)
      : cdb(cdb),
        ins_in_c(ins_channel),
        exec_out_c(exec_channel),
        global_flush_bus(global_flush_bus) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  void work() {
    if (global_flush_bus.get()) {
      if (!buffer.empty()) {
          logger.Info("Flushing ReservationStation");
      }
      buffer.clear();
      ins_in_c.clear();
      return;
    }
    if (!buffer.full()) {
      auto result = ins_in_c.receive();
      if (result) {
        logger.With("ROB_ID", result->id)
              .Info("ReservationStation received new instruction");
        buffer.insert(*result);
      }
    }
    auto cdb_result = cdb.get();
    if (cdb_result) {
      logger.With("SourceROB_ID", cdb_result->rob_id)
            .With("Value", cdb_result->data)
            .Info("ReservationStation received CDB broadcast");
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
                .Info("Dispatching instruction from ReservationStation to execution unit");
          exec_out_c.send(*it);
          it = buffer.erase(it);
          break;
        }
      }
    }
  }
};