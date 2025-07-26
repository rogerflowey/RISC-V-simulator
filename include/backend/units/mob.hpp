#pragma once

#include "backend/cdb.hpp"
#include "constants.hpp"
#include "logger.hpp"
#include "backend/units/memory.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "utils/queue.hpp"

struct MOBEntry {
  MemoryRequest req;
  bool ready;
  bool committed;
};

class MemoryOrderBuffer {
  queue<MOBEntry, LSB_SIZE> queue;
  CommonDataBus &cdb;
  Memory &mem;

  Channel<std::pair<RobIDType, MemoryRequestType>> mark_c;
  Channel<MemoryRequest> fill_c;

public:
  MemoryOrderBuffer(CommonDataBus &cdb, Memory &mem) : cdb(cdb), mem(mem) {
    Clock::getInstance().subscribe([this]{ this->work(); });
    cdb.connect(mem.get_response_channel());
  }

  void work() {
    // phase 0: regular work
    if (!queue.full()) {
      auto result = mark_c.receive();
      if (result) {
        auto [rob_id, type] = result.value();
        queue.push_back(MOBEntry{{type, false, rob_id, 0, 0, 0}, false, false});
        logger.With("ROB_ID", rob_id)
            .With("Type", type == MemoryRequestType::READ ? "READ" : "WRITE")
            .Info("MOBEntry added");
      }
    }
    auto result = fill_c.receive();
    if (result) {
      auto id = result.value().rob_id;
      for (size_t i = 0; i < queue.size(); ++i) {
        if (queue[i].req.rob_id == id) {
          queue[i].req = result.value();
          queue[i].ready = true;

          logger.With("ROB_ID", queue[i].req.rob_id)
              .With("Type", queue[i].req.type == MemoryRequestType::READ
                                ? "READ"
                                : "WRITE")
              .With("Addr", queue[i].req.address)
              .Info("MOBEntry updated");
        }
      }
    }

    // phase 1: check cdb for STORE commit
    auto cdb_result = cdb.get();
    if (cdb_result) {
      for (size_t i = 0; i < queue.size(); ++i) {
        if (queue[i].req.rob_id == cdb_result->rob_id) {
          queue[i].committed = true;
        }
      }
    }

    // phase 2: send new memory request
    if (!queue.empty()) {
      MOBEntry entry = queue.front();
      if (entry.req.type == READ || entry.committed) {
        if (mem.get_request_channel().can_send()) {
          mem.get_request_channel().send(entry.req);
          queue.pop_front();

          logger.With("ROB_ID", entry.req.rob_id)
              .With("Type", entry.req.type == MemoryRequestType::READ
                                ? "READ"
                                : "WRITE")
              .With("Addr", entry.req.address)
              .Info("Sending memory request");
        }
      }
    }
  }
};
