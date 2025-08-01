#pragma once

#include "backend/cdb.hpp"
#include "constants.hpp"
#include "instruction.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "middlend/rob.hpp"
#include "utils/bus.hpp"
#include "utils/queue.hpp"
#include <optional>
#include <utility>

std::optional<MemoryRequest>
translate_to_memory_request(const FilledInstruction &filled_ins);

struct MOBEntry {
  MemoryRequest req;
  bool ready = false;
  bool committed = false;
};

class MemoryOrderBuffer {
  queue<MOBEntry, LSB_SIZE> buffer;

  Channel<std::pair<RobIDType, MemoryRequestType>> &mark_in_c;
  Channel<FilledInstruction> &fill_in_c;

  HandshakeChannel<MemoryRequest> &mem_request_out_c;
  Channel<CDBResult> &write_commit_out_c;

  Bus<ROBEntry> &commit_bus;
  Bus<bool> &global_flush_bus;

public:
  // Corrected constructor parameter types
  MemoryOrderBuffer(
      Channel<std::pair<RobIDType, MemoryRequestType>> &mark_channel,
      Channel<FilledInstruction> &fill_channel,
      HandshakeChannel<MemoryRequest> &mem_req_out_channel,
      Channel<CDBResult> &write_commit_out_channel, Bus<ROBEntry> &commit_bus,
      Bus<bool> &global_flush_bus)
      : mark_in_c(mark_channel), fill_in_c(fill_channel),
        mem_request_out_c(mem_req_out_channel),
        write_commit_out_c(write_commit_out_channel), commit_bus(commit_bus),
        global_flush_bus(global_flush_bus) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  void work() {
    auto commit_result = commit_bus.get();
    if (commit_result) {
      for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i].req.rob_id == commit_result->id) {
          buffer[i].committed = true;
          logger.With("ROB_ID", commit_result->id)
              .Info("MOBEntry marked as committed");
        }
      }
    }
    
    if (global_flush_bus.get()) {
      while (!buffer.empty() && !buffer.back().committed) {
          buffer.pop_back();
      }
      for (size_t i = 0; i < buffer.size(); ++i) {
        logger.With("ROB_ID", buffer[i].req.rob_id)
        .Info("MOBEntry not flushed because it is committed.");
      }


      mark_in_c.clear();
      fill_in_c.clear();
      logger.Warn("MOB flushed of speculative entries.");
      return;
    }

    // Mark a new request
    if (!buffer.full()) {
      auto mark_result = mark_in_c.receive();
      if (mark_result) {
        auto [rob_id, type] = mark_result.value();
        // Placeholder
        buffer.push_back(MOBEntry{MemoryRequest{type, false, rob_id, 0, 0, {}},
                                 false, false});
        logger.With("ROB_ID", rob_id)
            .With("Type", type == MemoryRequestType::READ ? "READ" : "WRITE")
            .Info("MOBEntry marked");
      }
    }

    // Fill data if ready
    auto fill_result = fill_in_c.peek();
    if (fill_result) {
      const auto &filled_ins = fill_result.value();
      auto mem_req_opt = translate_to_memory_request(filled_ins);
      if (mem_req_opt) {
        const auto &new_req = mem_req_opt.value();
        if (new_req.type == MemoryRequestType::READ ||
            write_commit_out_c.can_send()) {
          fill_in_c.receive();
          if (new_req.type == MemoryRequestType::WRITE) {
            write_commit_out_c.send(CDBResult{new_req.rob_id, 0});
          }

          for (size_t i = 0; i < buffer.size(); ++i) {
            if (buffer[i].req.rob_id == new_req.rob_id) {
              buffer[i].req = new_req;
              buffer[i].ready = true;
              logger.With("ROB_ID", new_req.rob_id)
                  .With("Type", new_req.type == MemoryRequestType::READ
                                    ? "READ"
                                    : "WRITE")
                  .With("Addr", new_req.address)
                  .Info("MOBEntry filled and ready");
              break;
            }
          }
        }
      } else {
        logger.With("ROB_ID", filled_ins.id)
            .Warn("Non-memory instruction sent to MOB");
      }
    }

    

    if (!buffer.empty()) {
      MOBEntry &entry = buffer.front();
      if (entry.ready && (entry.req.type == READ || entry.committed)) {
        if (mem_request_out_c.can_send()) {
          mem_request_out_c.send(entry.req);
          logger.With("ROB_ID", entry.req.rob_id)
              .With("Type", entry.req.type == MemoryRequestType::READ ? "READ"
                                                                      : "WRITE")
              .With("Addr", entry.req.address)
              .Info("Sending memory request to Memory Unit");
          buffer.pop_front();
        }
      }
    }
  }
};



inline std::optional<MemoryRequest>
translate_to_memory_request(const FilledInstruction &filled_ins) {
  if (!is_mem(filled_ins.ins.op)) {
    return std::nullopt;
  }
  const RegDataType address = filled_ins.v_rs1 + filled_ins.ins.imm;
  switch (filled_ins.ins.op) {
  case OpType::LW: // Load Word (32-bit, signed)
    return MemoryRequest::CreateReadRequest(filled_ins.id, address, 4, true);

  case OpType::LH: // Load Half-word (16-bit, signed)
    return MemoryRequest::CreateReadRequest(filled_ins.id, address, 2, true);

  case OpType::LHU: // Load Half-word Unsigned (16-bit, unsigned)
    return MemoryRequest::CreateReadRequest(filled_ins.id, address, 2, false);

  case OpType::LB: // Load Byte (8-bit, signed)
    return MemoryRequest::CreateReadRequest(filled_ins.id, address, 1, true);

  case OpType::LBU: // Load Byte Unsigned (8-bit, unsigned)
    return MemoryRequest::CreateReadRequest(filled_ins.id, address, 1, false);
  // --- Store Instructions (WRITE) ---
  case OpType::SW: // Store Word (32-bit)
    // The data to be stored is in the second source register (v_rs2).
    return MemoryRequest::CreateWriteRequest(filled_ins.id, address, 4,
                                             filled_ins.v_rs2);

  case OpType::SH: // Store Half-word (16-bit)
    return MemoryRequest::CreateWriteRequest(filled_ins.id, address, 2,
                                             filled_ins.v_rs2);

  case OpType::SB: // Store Byte (8-bit)
    return MemoryRequest::CreateWriteRequest(filled_ins.id, address, 1,
                                             filled_ins.v_rs2);

  default:
    return std::nullopt;
  }
}
