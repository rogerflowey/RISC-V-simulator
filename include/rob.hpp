#pragma once

#include "cdb.hpp"
#include "constants.hpp"
#include "logger.hpp"
#include "utils/queue.hpp"
#include <sys/types.h>

enum ROBState{
    ISSUE,COMMIT,WRITE_RESULT,EXECUTE
};

enum class InstType {
    ALU,
    LOAD,
    STORE,
    BRANCH
};

struct ROBEntry {
    RobIDType id;
    InstType type;
    ROBState state;
    PCType instruction_pc;
    RegIDType dest_reg;

    bool is_ready;
    RegDataType value;

    bool predicted_taken;
    PCType actual_target_pc;
    bool is_mispredicted;

    ROBEntry() : is_ready(false), is_mispredicted(false) {}
};

class ReorderBuffer{
    queue<ROBEntry, ROB_SIZE> buffer;
    RobIDType next_rob_id;
    CommonDataBus& cdb;
public:
    ReorderBuffer(CommonDataBus& cdb):cdb(cdb){}

    void Finish(ROBEntry entry){
        if(buffer.empty()){
            throw logger.With("ROB_ID",entry.id).Error("Buffer is empty");
        }
        auto offset = entry.id - buffer.front().id;
        if(!(buffer[offset].id==entry.id)){
            throw logger.With("ROB_ID",entry.id).Error("Buffer ID mismatch");
        }
        buffer[offset] = entry;
        logger.With("ROB_ID",entry.id).With("Value",entry.value).Info("Buffer entry updated");
    }

    void work(){
    }
};