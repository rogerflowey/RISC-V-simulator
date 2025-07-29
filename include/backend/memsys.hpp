// file: backend/memory_system.hpp

#pragma once
#include "cdb.hpp"
#include "backend/memsys/memory.hpp"
#include "backend/memsys/mob.hpp"
#include "backend/memsys/mrs.hpp" // Assuming this is the header for MemoryRS
#include "middlend/rob.hpp"

class MemorySystem {
private:
    // --- Internal Modules (Owned by MemorySystem) ---
    Memory memory;
    MemoryOrderBuffer mob;
    MemoryRS memory_rs;

    // --- Internal Channels (The "internal wires") ---
    Channel<std::pair<RobIDType, MemoryRequestType>> mob_mark_c;
    Channel<MemoryRequest> mrs_to_mob_fill_c;
    Channel<MemoryRequest> mob_to_mem_req_c;
    Channel<CDBResult> response_to_cdb;

public:
    // The constructor is the internal wiring diagram for this subsystem.
    // It takes references to the EXTERNAL things it needs to connect to.
    MemorySystem(CommonDataBus& cdb,
                 Channel<MemoryIns>& mem_instr, Bus<ROBEntry>& commit_channel)
        // Initialize internal modules, passing them the internal channels
        : memory(mob_to_mem_req_c, response_to_cdb),
          mob(mob_mark_c, mrs_to_mob_fill_c, mob_to_mem_req_c, commit_channel),
          memory_rs(cdb, mem_instr, mrs_to_mob_fill_c)
    {
        // This subsystem is responsible for connecting its output to the CDB
        cdb.connect(response_to_cdb);
    }
};