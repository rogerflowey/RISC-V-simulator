#pragma once

#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"
#include "constants.hpp"
#include "instruction.hpp"
#include "logger.hpp"
#include "utils/queue.hpp"

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
  bool is_taken;
  PCType target_pc;
};

class ReorderBuffer {
  queue<ROBEntry, ROB_SIZE> buffer;
  RobIDType next_id = 1;

public:
  bool can_allocate() {
    return !buffer.full();
  }

  bool empty() const {
    return buffer.empty();
  }

  const ROBEntry& front() const {
    return buffer.front();
  }

  void pop_front() {
    if (!buffer.empty()) {
      buffer.pop_front();
    }
  }

  RobIDType allocate(ROBEntry e) {
    e.id = next_id++;
    buffer.push_back(e);
    return e.id;
  }

  std::optional<RegDataType> get(RobIDType id) {
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == id) {
        if (buffer[i].state == COMMIT_READY) {
          return buffer[i].value;
        }
        return std::nullopt;
      }
    }
    return std::nullopt;
  }

  void flush(){
    logger.Warn("Reorder Buffer flushed.");
    buffer.clear();
  }

  void process_cdb(CDBResult result) {
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == result.rob_id) {
        buffer[i].value = result.data;
        buffer[i].state = COMMIT_READY;
        logger.With("ROB_ID", buffer[i].id)
            .With("Value", buffer[i].value)
            .Info("ROB entry updated from CDB, ready to commit.");
      }
    }
  }

  void process_branch(BranchResult result){
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == result.rob_id) {
        buffer[i].is_taken = result.is_taken;
        buffer[i].target_pc = result.target_pc;
        if (buffer[i].reg_id == 0) {
          buffer[i].state = COMMIT_READY;
        }
        logger.With("ROB_ID", buffer[i].id)
            .With("Taken", buffer[i].is_taken)
            .With("TargetPC", buffer[i].target_pc)
            .Info("ROB branch entry updated, ready to commit.");
      }
    }
  }
};