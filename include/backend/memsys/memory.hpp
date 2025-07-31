#pragma once

#include "utils/clock.hpp"
#include "utils/bus.hpp"
#include "utils/ints.hpp"
#include "constants.hpp"
#include "logger.hpp"
#include "backend/cdb.hpp"
#include <array> // Required for std::array

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
  std::array<std::byte, MEMORY_SIZE>& memory;
  int time_cnt = 0;
  MemoryRequest request;

  HandshakeChannel<MemoryRequest>& request_c;
  Channel<CDBResult>& response_c;
  Bus<bool>& global_flush_bus;

public:
  Memory(std::array<std::byte, MEMORY_SIZE>& unified_memory,
         HandshakeChannel<MemoryRequest>& req_channel,
         Channel<CDBResult>& resp_channel,
         Bus<bool>& flush_bus)
      : memory(unified_memory),
        request_c(req_channel),
        response_c(resp_channel),
        global_flush_bus(flush_bus) {
    Clock::getInstance().subscribe([this] { this->tick(); });
  }

  void tick() {
    if(time_cnt==0) {
      if(auto result = request_c.receive()) {
        request = *result;
        time_cnt=3;
      }
    }
    if(global_flush_bus.get()) {
      if(time_cnt>0 && request.type==READ) {
        time_cnt=0;
      }
    }
    if(time_cnt>0 && --time_cnt==0) {
      process_completed_request();
    }
    if(time_cnt==0) {
      request_c.ready();
    }
  }

private:
  void process_completed_request() {
    if (request.type == READ) {
      if (!response_c.can_send()) {
        time_cnt++; // Stall
        return;
      }

      // Safety check for out-of-bounds read
      if (request.address + request.size > MEMORY_SIZE) {
          logger.Error("Memory read out of bounds at address: " + std::to_string(request.address));
          response_c.send(CDBResult{request.rob_id, 0}); // Send a default value on error
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

    } else { // WRITE
      // Safety check for out-of-bounds write
      if (request.address + request.size > MEMORY_SIZE) {
          logger.Error("Memory write out of bounds at address: " + std::to_string(request.address));
          // This check prevents the segfault
          return;
      }

      auto bytes = uint_to_bytes(request.data);

      std::copy(bytes.begin(), bytes.begin() + request.size, &memory[request.address]);

      logger.With("ROB_ID", request.rob_id).With("Value", request.data).Info("Memory write");
    }
  }
};