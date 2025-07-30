#pragma once

// Include all component headers
#include "backend/cdb.hpp"
#include "backend/units/alu.hpp"
#include "backend/units/branch.hpp"
#include "backend/rs.hpp"
#include "backend/memsys.hpp"

// Include utility and data structure headers
#include "utils/bus.hpp"
#include "instruction.hpp"
#include "constants.hpp"

/**
 * @class Backend
 * @brief Encapsulates the execution stage of the pipeline.
 *
 * The Backend class contains the reservation stations, functional units (ALU, Branch),
 * and the memory system. It receives dispatched instructions from an external Control unit,
 * executes them, and broadcasts results on the Common Data Bus (CDB). It is decoupled
 * from the Control unit, communicating solely through channels and buses.
 */
class Backend {
private:
    // Internal Channels: From Reservation Stations to Functional Units
    Channel<FilledInstruction> alu_rs_to_alu_c;
    Channel<FilledInstruction> branch_rs_to_branch_unit_c;

    // Internal Channels: From Functional Units to CDB
    Channel<CDBResult> alu_to_cdb_c;
    Channel<CDBResult> branch_unit_to_cdb_c;

    // Core Backend Components
    ReservationStation<RS_ALU_SIZE> alu_rs;
    ReservationStation<RS_BRANCH_SIZE> branch_rs;
    ALU alu;
    BranchUnit branch_unit;
    MemorySystem memory_system;

public:
    /**
     * @brief Constructs the Backend.
     * @param cdb The shared Common Data Bus for result broadcasting.
     * @param global_flush_bus The shared bus for pipeline flush signals.
     * @param control_to_alu_rs_c Channel for receiving dispatched ALU instructions.
     * @param control_to_mem_rs_c Channel for receiving dispatched Memory instructions.
     * @param control_to_branch_rs_c Channel for receiving dispatched Branch instructions.
     * @param branch_unit_to_control_c Channel for sending branch resolution results back to Control.
     * @param commit_bus Bus for receiving committed instruction info, used by the MemorySystem.
     */
    Backend(
        // Shared communication infrastructure
        CommonDataBus& cdb,
        Bus<bool>& global_flush_bus,

        // Input channels from Control/Dispatcher
        Channel<FilledInstruction>& control_to_alu_rs_c,
        Channel<FilledInstruction>& control_to_mem_rs_c,
        Channel<FilledInstruction>& control_to_branch_rs_c,

        // Output channel to Control for branch results
        Channel<BranchResult>& branch_unit_to_control_c,

        // Input bus for commit information (for MemorySystem)
        Bus<ROBEntry>& commit_bus
    ) :
        // Initialize internal communication channels
        alu_rs_to_alu_c(),
        branch_rs_to_branch_unit_c(),
        alu_to_cdb_c(),
        branch_unit_to_cdb_c(),

        // Initialize main components, wiring them with external and internal channels
        alu_rs(
            cdb,
            control_to_alu_rs_c,      // Input from Control
            alu_rs_to_alu_c,          // Output to ALU
            global_flush_bus
        ),
        branch_rs(
            cdb,
            control_to_branch_rs_c,       // Input from Control
            branch_rs_to_branch_unit_c, // Output to BranchUnit
            global_flush_bus
        ),
        alu(
            alu_rs_to_alu_c,          // Input from ALU RS
            alu_to_cdb_c,             // Output to CDB
            global_flush_bus
        ),
        branch_unit(
            branch_rs_to_branch_unit_c, // Input from Branch RS
            branch_unit_to_control_c,   // Output to Control
            branch_unit_to_cdb_c,       // Output to CDB
            global_flush_bus
        ),
        memory_system(
            cdb,
            control_to_mem_rs_c,      // Input from Control
            commit_bus,               // Input from Control (ROB)
            global_flush_bus
        )
    {
        // Connect functional unit outputs to the CDB.
        // The MemorySystem connects itself to the CDB internally.
        cdb.connect(alu_to_cdb_c);
        cdb.connect(branch_unit_to_cdb_c);
    }

    // The get_control() method is removed as Control is now an external component.
};