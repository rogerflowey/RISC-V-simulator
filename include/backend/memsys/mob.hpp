#pragma once

#include "backend/cdb.hpp"
#include "constants.hpp"
#include "logger.hpp"
#include "middlend/rob.hpp"
#include "utils/bus.hpp"
#include "utils/queue.hpp"
#include "memory.hpp"

struct MOBEntry {
  MemoryRequest req;
  bool ready;
  bool committed;
};

class MemoryOrderBuffer {
  queue<MOBEntry, LSB_SIZE> queue;

  Channel<std::pair<RobIDType, MemoryRequestType>>& mark_in_c;
  Channel<MemoryRequest>& fill_in_c;
  Channel<MemoryRequest>& mem_request_out_c;

  Bus<ROBEntry>& commit_channel;

public:
  MemoryOrderBuffer(Channel<std::pair<RobIDType, MemoryRequestType>>& mark_channel,
                    Channel<MemoryRequest>& fill_channel,
                    Channel<MemoryRequest>& mem_req_out_channel,
                    Bus<ROBEntry>& commit_channel)
      : mark_in_c(mark_channel),
        fill_in_c(fill_channel),
        mem_request_out_c(mem_req_out_channel),
        commit_channel(commit_channel) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  void work() {
    // phase 0: regular work
    // first mark, then fill
    if (!queue.full()) {
      auto result = mark_in_c.receive();
      if (result) {
        auto [rob_id, type] = result.value();
        queue.push_back(MOBEntry{{type, false, rob_id, 0, 0, 0}, false, false});
        logger.With("ROB_ID", rob_id)
            .With("Type", type == MemoryRequestType::READ ? "READ" : "WRITE")
            .Info("MOBEntry added");
      }
    }
    auto result = fill_in_c.receive();
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
    auto commit_result = commit_channel.get();
    if (commit_result) {
      for (size_t i = 0; i < queue.size(); ++i) {
        if (queue[i].req.rob_id == commit_result->id) {
          queue[i].committed = true;
        }
      }
    }

    // phase 2: send new memory request
    if (!queue.empty()) {
      MOBEntry entry = queue.front();
      if (entry.ready && (entry.req.type == READ || entry.committed)) {
        if (mem_request_out_c.can_send()) {
          mem_request_out_c.send(entry.req);
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