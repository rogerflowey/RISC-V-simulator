#pragma once

#include "cdb.hpp"
#include "constants.hpp"
#include "reg.hpp"
#include "utils/logger/logger.hpp"
#include "utils/queue.hpp"
#include <sys/types.h>

enum ROBState{
    COMMIT,WRITE_RESULT,EXECUTE
};

struct ROBEntry{
    ROBState state;
    RobIDType id;
    u_int32_t value;
};

class ReorderBuffer{
    queue<ROBEntry, ROB_SIZE> buffer;
    RobIDType next_rob_id;
    CommonDataBus& cdb;
public:
    ReorderBuffer(CommonDataBus& cdb):cdb(cdb){}

    void Finish(ROBEntry entry){
        if(buffer.empty()){
            throw logger().With("ROB_ID",entry.id).Error("Buffer is empty");
        }
        auto offset = entry.id - buffer.front().id;
        if(!(buffer[offset].id==entry.id)){
            throw logger().With("ROB_ID",entry.id).Error("Buffer ID mismatch");
        }
        buffer[offset] = entry;
    }

    void work(){
        if(buffer.front().state==WRITE_RESULT){
            auto entry = buffer.front();
            buffer.pop_front();
            cdb.set(entry.id,entry.value);
        }
    }
};