#pragma once

#include "backend/cdb.hpp"
#include "backend/units/alu.hpp"
#include "backend/units/branch.hpp"
#include "backend/rs.hpp"
#include "backend/memsys.hpp"

#include "utils/bus.hpp"
#include "instruction.hpp"
#include "constants.hpp"

class Backend {
private:
    Channel<FilledInstruction> alu_rs_to_alu_c;
    Channel<FilledInstruction> branch_rs_to_branch_unit_c;

    Channel<CDBResult> alu_to_cdb_c;
    Channel<CDBResult> branch_unit_to_cdb_c;

    ReservationStation<RS_ALU_SIZE> alu_rs;
    ReservationStation<RS_BRANCH_SIZE> branch_rs;
    ALU alu;
    BranchUnit branch_unit;
    MemorySystem memory_system;

public:
    Backend(
        std::array<std::byte, MEMORY_SIZE>& unified_memory,

        CommonDataBus& cdb,
        Bus<bool>& global_flush_bus,

        Channel<FilledInstruction>& control_to_alu_rs_c,
        Channel<FilledInstruction>& control_to_mem_rs_c,
        Channel<FilledInstruction>& control_to_branch_rs_c,

        Channel<BranchResult>& branch_unit_to_control_c,

        Bus<ROBEntry>& commit_bus
    ) :
        alu_rs_to_alu_c(),
        branch_rs_to_branch_unit_c(),
        alu_to_cdb_c(),
        branch_unit_to_cdb_c(),

        alu_rs(
            cdb,
            control_to_alu_rs_c,     
            alu_rs_to_alu_c,        
            global_flush_bus
        ),
        branch_rs(
            cdb,
            control_to_branch_rs_c,     
            branch_rs_to_branch_unit_c,
            global_flush_bus
        ),
        alu(
            alu_rs_to_alu_c,        
            alu_to_cdb_c,           
            global_flush_bus
        ),
        branch_unit(
            branch_rs_to_branch_unit_c, 
            branch_unit_to_control_c,   
            branch_unit_to_cdb_c,      
            global_flush_bus
        ),
        memory_system(
            unified_memory,
            cdb,
            control_to_mem_rs_c,     
            commit_bus,               
            global_flush_bus
        )
    {
        cdb.connect(alu_to_cdb_c);
        cdb.connect(branch_unit_to_cdb_c);
    }

};