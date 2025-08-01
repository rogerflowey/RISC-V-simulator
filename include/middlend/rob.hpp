#pragma once

#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"
#include "constants.hpp"
#include "instruction.hpp"
#include "logger.hpp"
#include "utils/clock.hpp"
#include "utils/port.hpp"
#include "utils/queue.hpp"
#include <vector>
#include <optional>

enum ROBState { ISSUED, COMMIT_READY, ISHALT };

struct ROBEntry {
  RobIDType id;
  OpType type;
  PCType pc;
  RegIDType reg_id;
  RegDataType value;
  ROBState state = ISSUED;

  bool is_branch = false;
  bool predicted_taken = false;
  bool is_taken = false; // Initialize to a default value
  PCType target_pc = 0;  // Initialize to a default value
};

class ReorderBuffer {
  queue<ROBEntry, ROB_SIZE> buffer;
  RobIDType next_id = 1;

  // Write ports from other modules
  std::vector<WritePort<ROBEntry>*> allocate_ports;
  std::vector<WritePort<CDBResult>*> cdb_ports;
  std::vector<WritePort<BranchResult>*> branch_ports;
  std::vector<WritePort<bool>*> pop_ports;

public:
  ReorderBuffer() {
    Clock::getInstance().subscribe([this] { this->update_state(); }, FALLING);
  }

  // --- Port Creation for Consumers ---

  // For Rename: Check if a value is ready for bypass
  ReadPort<RobIDType, std::optional<RegDataType>> create_get_port() {
    return ReadPort<RobIDType, std::optional<RegDataType>>(
        [this](RobIDType id) { return this->get(id); });
  }

  // For Rename: Get the ID for the next instruction
  ReadPort<bool, RobIDType> create_next_id_port() {
    return ReadPort<bool, RobIDType>(
        [this](bool) { return this->next_id; });
  }
  
  // For Rename: Check if the ROB is full
  ReadPort<bool, bool> create_stall_port() {
      return ReadPort<bool, bool>(
          [this](bool) { return this->buffer.full(); }
      );
  }

  // For Commit: Get the instruction at the head of the ROB
  ReadPort<bool, std::optional<ROBEntry>> create_front_port() {
    return ReadPort<bool, std::optional<ROBEntry>>(
        [this](bool) -> std::optional<ROBEntry> {
          if (!buffer.empty())
            return this->front();
          return std::nullopt;
        });
  }

  // --- Port Creation for Producers ---

  // For Rename: To allocate a new entry
  WritePort<ROBEntry>& create_allocate_port() {
    auto* port = new WritePort<ROBEntry>();
    allocate_ports.push_back(port);
    return *port;
  }

  // For Commit: To write back CDB results
  WritePort<CDBResult>& create_cdb_port() {
    auto* port = new WritePort<CDBResult>();
    cdb_ports.push_back(port);
    return *port;
  }

  // For Commit: To write back branch results
  WritePort<BranchResult>& create_branch_port() {
    auto* port = new WritePort<BranchResult>();
    branch_ports.push_back(port);
    return *port;
  }

  // For Commit: To pop the head entry after commit
  WritePort<bool>& create_pop_port() {
    auto* port = new WritePort<bool>();
    pop_ports.push_back(port);
    return *port;
  }

  // --- State Logic ---

  void update_state() {
    // Process incoming results first
    for (auto* port : cdb_ports) {
      if (auto result = port->consume()) {
        process_cdb(*result);
      }
    }
    for (auto* port : branch_ports) {
      if (auto result = port->consume()) {
        process_branch(*result);
      }
    }
    // Process commit request
    for (auto* port : pop_ports) {
      if (port->consume()) {
        pop_front();
      }
    }
    // Process new allocations last
    for (auto* port : allocate_ports) {
        if(auto entry = port->consume()) {
            allocate(*entry);
        }
    }
  }

  void flush() {
    logger.Warn("Reorder Buffer flushed.");
    buffer.clear();
    next_id = 1; // BUG FIX: Reset the ID counter on flush
  }

private:
  const ROBEntry& front() const { return buffer.front(); }

  void pop_front() {
    if (!buffer.empty()) {
      logger.With("ROB_ID", buffer.front().id).Info("Popping committed entry from ROB.");
      buffer.pop_front();
    }
  }

  void allocate(ROBEntry e) {
    if (buffer.full()) {
        logger.Error("Attempted to allocate into a full ROB. This should be prevented by stall logic.");
        return;
    }
    e.id = next_id++;
    if (next_id == 0) next_id = 1; // Wrap around, avoiding 0
    buffer.push_back(e);
    logger.With("PC", e.pc).With("ROB_ID", e.id).Info("Instruction allocated in ROB.");
  }

  std::optional<RegDataType> get(RobIDType id) {
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == id) {
        if (buffer[i].state == COMMIT_READY) {
          return buffer[i].value;
        }
        return std::nullopt; // Found but not ready
      }
    }
    return std::nullopt; // Not found
  }

  void process_cdb(CDBResult result) {
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == result.rob_id) {
        buffer[i].value = result.data;
        buffer[i].state = COMMIT_READY;
        logger.With("ROB_ID", buffer[i].id)
            .With("Value", buffer[i].value)
            .Info("ROB entry updated from CDB, ready to commit.");
        return;
      }
    }
  }

  void process_branch(BranchResult result) {
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == result.rob_id) {
        buffer[i].is_taken = result.is_taken;
        buffer[i].target_pc = result.target_pc;
        // For branches that don't write a register (e.g., BEQ), they are ready once resolved.
        // For branches that do (JAL, JALR), they also need their value from the CDB.
        if (buffer[i].reg_id == 0) {
          buffer[i].state = COMMIT_READY;
        }
        logger.With("ROB_ID", buffer[i].id)
            .With("Taken", buffer[i].is_taken)
            .With("TargetPC", buffer[i].target_pc)
            .Info("ROB branch entry updated.");
        return;
      }
    }
  }
};