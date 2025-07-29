#pragma once

#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"
#include "constants.hpp"
#include "instruction.hpp"
#include "logger.hpp"
#include "utils/queue.hpp"
#include <sys/types.h>

enum ROBState { ISSUE, COMMIT, WRITE_RESULT, EXECUTE };

struct ROBEntry {
  RobIDType id;
  OpType type;
  PCType pc;
  RegDataType value;
  ROBState state = ISSUE;
  bool is_branch = false;
  bool predicted_taken = false;
  bool is_taken;
};

class ReorderBuffer {
  queue<ROBEntry, ROB_SIZE> buffer;
  RobIDType next_id = 1;

public:
  RobIDType allocate(ROBEntry e) {
    e.id = next_id++;
    buffer.push_back(e);
    return e.id;
  }

  std::optional<RegDataType> get(RobIDType id) {
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == id) {
        return buffer[i].value;
      }
    }
    return std::nullopt;
  }

  void flush(){
    buffer.clear();
  }

  void process_cdb(CDBResult result) {
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == result.rob_id) {
        buffer[i].value = result.data;
        buffer[i].state = COMMIT;
        logger.With("ROB_ID", buffer[i].id)
            .With("Value", buffer[i].value)
            .Info("Buffer entry committed");
      }
    }
  }

  void process_branch(BranchResult result){
    for (int i = 0; i < buffer.size(); i++) {
      if (buffer[i].id == result.rob_id) {
        buffer[i].is_taken = result.is_taken;
        buffer[i].pc = result.target_pc;
      }
    }
  }

  std::optional<ROBEntry> process_commit() {
    if (!buffer.empty()) {
      // Commit the first instruction in the buffer
      auto entry = buffer.front();
      if (entry.state == COMMIT) {
        buffer.pop_front();
        return entry;
      }
    }
    return std::nullopt;
  }
};