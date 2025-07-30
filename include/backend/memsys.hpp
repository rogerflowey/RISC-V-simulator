#pragma once

#include "backend/cdb.hpp"
#include "backend/memsys/memory.hpp"
#include "backend/memsys/mob.hpp"
#include "backend/memsys/mrs.hpp" 
#include "middlend/rob.hpp"
#include "constants.hpp"
#include "instruction.hpp"
#include "utils/bus.hpp"

using MemoryRS = MemoryReservationStation<RS_MEM_SIZE>;

class MemorySystem {
private:
    Memory memory;
    MemoryOrderBuffer mob;
    MemoryRS memory_rs;

    Channel<std::pair<RobIDType, MemoryRequestType>> rs_to_mob_mark_c;

    Channel<FilledInstruction> mrs_to_mob_fill_c;
    HandshakeChannel<MemoryRequest> mob_to_mem_req_c;
    Channel<CDBResult> mem_read_response_c;
    Channel<CDBResult> mob_write_commit_c;

public:
    MemorySystem(
        CommonDataBus& cdb,
        Channel<FilledInstruction>& mem_instr_in_c,
        Bus<ROBEntry>& commit_bus,
        Bus<bool>& global_flush_bus
    ) : memory(mob_to_mem_req_c, mem_read_response_c, global_flush_bus),
        mob(rs_to_mob_mark_c, mrs_to_mob_fill_c, mob_to_mem_req_c, mob_write_commit_c, commit_bus, global_flush_bus),
        memory_rs(cdb, mem_instr_in_c, mrs_to_mob_fill_c, rs_to_mob_mark_c, global_flush_bus), mob_to_mem_req_c() {
        cdb.connect(mem_read_response_c);
        cdb.connect(mob_write_commit_c);
    }
};