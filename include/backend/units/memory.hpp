#pragma once
#include "backend/cdb.hpp"
#include "constants.hpp"
#include "logger.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "utils/ints.hpp"

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
  int time_cnt;
  MemoryRequest request;
  Channel<MemoryRequest> request_c;
  Channel<CDBResult> response_c;

public:
  Memory() {
    Clock::getInstance().subscribe([this] { this->tick(); });
  }

  Channel<MemoryRequest> &get_request_channel() { return request_c; }

  Channel<CDBResult> &get_response_channel() { return response_c; }

  void tick() {
    if (time_cnt == 0) {
      // idle, try to accept new request
      auto new_request = request_c.receive();
      if (new_request) {
        request = *new_request;
        time_cnt = 3;
        return;
      }
    } else {
      time_cnt--;
      if (time_cnt == 0) {
        // finished request, check the channel
        if (!response_c.can_send())
          return;

        if (request.type == READ) {
          auto value =
              request.is_signed
                  ? static_cast<MemDataType>(
                        bytes_to_sint(&memory[request.address],
                                      &memory[request.address + request.size]))
                  : bytes_to_uint(&memory[request.address],
                                  &memory[request.address + request.size]);
          response_c.send(CDBResult{ request.rob_id, value});
          logger.With("ROB_ID", request.rob_id)
              .With("Value", value)
              .Info("Memory read");
        } else {
          response_c.send(CDBResult{ request.rob_id, 0});
          logger.With("ROB_ID", request.rob_id)
              .With("Value", request.data)
              .Info("Memory write");
        }
      }
    }
  }
};