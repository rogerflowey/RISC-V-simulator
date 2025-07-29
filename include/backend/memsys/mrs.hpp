#pragma once

#include "backend/cdb.hpp"
#include "constants.hpp"
#include "utils/hive.hpp"
#include "utils/bus.hpp"
#include "memory.hpp"

struct MemoryIns {
  MemoryRequest req;
  RegDataType v_rs1;
  RobIDType q_rs1;
  RegIDType rs1;

  RegIDType rd;

  RegDataType v_rs2;
  RobIDType q_rs2;
  RegIDType rs2;

  ImmType imm;
};

class MemoryRS {
  hive<MemoryIns, LSB_SIZE> buffer;

  CommonDataBus& cdb;

  Channel<MemoryIns>& ins_in_c;
  Channel<MemoryRequest>& fill_out_c;

public:
  MemoryRS(CommonDataBus& cdb,
           Channel<MemoryIns>& ins_channel,
           Channel<MemoryRequest>& fill_out_channel)
      : cdb(cdb),
        ins_in_c(ins_channel),
        fill_out_c(fill_out_channel) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  void work() {
    // phase 1: Process incoming instructions
    if (!buffer.full()) {
      auto result = ins_in_c.receive();
      if (result) {
        buffer.insert(*result);
      }
    }

    // phase 2: listen CDB to update entries
    auto cdb_result = cdb.get();
    if (cdb_result) {
      for (auto it = buffer.begin(); it != buffer.end(); ++it) {
        if (it->q_rs1 == cdb_result->rob_id) {
          it->v_rs1 = cdb_result->data;
          it->q_rs1 = 0;
        }
        if (it->q_rs2 == cdb_result->rob_id) {
          it->v_rs2 = cdb_result->data;
          it->q_rs2 = 0;
        }
      }
    }

    // phase 3: send memory requests
    for (auto it = buffer.begin(); it != buffer.end(); ++it) {
      if (it->q_rs1 == 0 && (it->req.type == READ || it->q_rs2 == 0)) {
        if (fill_out_c.can_send()) {
          MemoryRequest full_req = it->req;
          full_req.address = it->v_rs1 + it->imm;
          if (full_req.type == WRITE) {
            full_req.data = it->v_rs2;
          }
          fill_out_c.send(full_req);
          it = buffer.erase(it);
          break;
        }
      }
    }
  }
};