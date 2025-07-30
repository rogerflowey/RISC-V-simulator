#pragma once

#include "utils/clock.hpp"
#include "utils/bus.hpp"
#include "utils/ints.hpp"
#include "constants.hpp"
#include "logger.hpp"
#include "backend/cdb.hpp"

enum MemoryRequestType { READ, WRITE };

struct MemoryRequest {
  MemoryRequestType type;
  bool is_signed;
  RobIDType rob_id;
  RegDataType address;
  uint8_t size;
  MemDataType data;

  static MemoryRequest CreateReadRequest(RobIDType rob_id, RegDataType address, uint8_t size, bool is_signed) {
    return MemoryRequest{READ, is_signed, rob_id, address, size, {}};
  }

  static MemoryRequest CreateWriteRequest(RobIDType rob_id, RegDataType address, uint8_t size, MemDataType data) {
    return MemoryRequest{WRITE, false, rob_id, address, size, data};
  }
};

class Memory {
  alignas(8) std::byte memory[MEMORY_SIZE]{};
  int time_cnt = 0;
  MemoryRequest request;

  HandshakeChannel<MemoryRequest>& request_c;
  Channel<CDBResult>& response_c;
  Bus<bool>& global_flush_bus;

public:
  Memory(HandshakeChannel<MemoryRequest>& req_channel, Channel<CDBResult>& resp_channel, Bus<bool>& flush_bus)
      : request_c(req_channel), response_c(resp_channel), global_flush_bus(flush_bus) {
    Clock::getInstance().subscribe([this] { this->tick(); });
  }

  void tick() {
    if (global_flush_bus.get()) {
      handle_flush();
      return;
    }

    if (time_cnt > 0) {
      handle_busy_state();
    } else {
      handle_idle_state();
    }
  }

private:
  void handle_flush() {
    if (time_cnt > 0 && request.type == READ) {
      time_cnt = 0;
    }

    if (auto incoming_request = request_c.receive()) {
      if (incoming_request->type == WRITE) {
        request = *incoming_request;
        time_cnt = 3;
      }
    }
    if (time_cnt == 0) {
      request_c.ready();
    }
  }

  void handle_idle_state() {
    request_c.ready();

    if (auto new_request = request_c.receive()) {
      request = *new_request;
      time_cnt = 3;
    }
  }

  void handle_busy_state() {
    time_cnt--;
    if (time_cnt == 0) {
      process_completed_request();
    }
  }

  void process_completed_request() {
    if (request.type == READ) {
      if (!response_c.can_send()) {
        time_cnt++; // Stall
        return;
      }

      auto value =
          request.is_signed
              ? static_cast<MemDataType>(
                    bytes_to_sint(&memory[request.address],
                                  &memory[request.address + request.size]))
              : bytes_to_uint(&memory[request.address],
                              &memory[request.address + request.size]);

      response_c.send(CDBResult{request.rob_id, value});
      logger.With("ROB_ID", request.rob_id).With("Value", value).Info("Memory read");

    } else {
      auto bytes = uint_to_bytes(request.data);
      std::copy(bytes.begin() + (4 - request.size), bytes.end(), &memory[request.address]);
      logger.With("ROB_ID", request.rob_id).With("Value", request.data).Info("Memory write");
    }
  }
};