#pragma once

// This header file defines the top-level CPU class, which integrates the
// Frontend, Control, and Backend components of the processor.

// Include component headers
#include "frontend/frontend.hpp"
#include "middlend/control.hpp"
#include "backend/backend.hpp"

#include "utils/bus.hpp"
#include "instruction.hpp"
#include "constants.hpp"

#include <vector>
#include <cstdint>

class CPU {
private:
    // Communication Infrastructure: Channels for point-to-point communication
    Channel<Instruction> decoded_instruction_c;
    Channel<FilledInstruction> control_to_alu_rs_c;
    Channel<FilledInstruction> control_to_mem_rs_c;
    Channel<FilledInstruction> control_to_branch_rs_c;
    Channel<BranchResult> branch_unit_to_control_c;
    Channel<PCType> mispredict_flush_pc_c;

    // Communication Infrastructure: Buses for broadcast communication
    CommonDataBus cdb;
    Bus<bool> global_flush_bus;
    Bus<ROBEntry> commit_bus;

    // Core Pipeline Stages
    Frontend frontend;
    Control control;
    Backend backend;

public:
    /**
     * @brief Constructs the entire CPU and wires up its components.
     * @param instruction_buffer A vector of 32-bit words representing the
     *        program memory. This is passed to the Frontend for instruction fetching.
     */
    CPU(std::vector<uint32_t>& instruction_buffer) :
        // 1. Initialize all communication channels and buses.
        // Their constructors register them with the global clock.
        decoded_instruction_c(),
        control_to_alu_rs_c(),
        control_to_mem_rs_c(),
        control_to_branch_rs_c(),
        branch_unit_to_control_c(),
        mispredict_flush_pc_c(),
        cdb(global_flush_bus),
        global_flush_bus(),
        commit_bus(),

        // 2. Initialize the main pipeline stages, passing the appropriate
        //    channels and buses to connect them.
        frontend(
            instruction_buffer,
            decoded_instruction_c,      // Output: Decoded instructions
            mispredict_flush_pc_c,      // Input:  PC for flush due to misprediction
            global_flush_bus,           // Input:  Global flush signal
            commit_bus                  // Input:  Commit info (for branch predictor update)
        ),
        control(
            decoded_instruction_c,      // Input:  Decoded instructions from Frontend
            branch_unit_to_control_c,   // Input:  Branch execution results from Backend
            control_to_alu_rs_c,        // Output: Dispatched instructions to ALU RS
            control_to_mem_rs_c,        // Output: Dispatched instructions to Mem RS
            control_to_branch_rs_c,     // Output: Dispatched instructions to Branch RS
            commit_bus,                 // Output: Committed instruction info
            global_flush_bus,           // Output: Global flush signal
            mispredict_flush_pc_c,      // Output: PC for flush due to misprediction
            cdb                         // Bidi:   Receives results for ROB/RegFile updates
        ),
        backend(
            cdb,                        // Bidi:   Connects FUs to CDB, RSs listen to CDB
            global_flush_bus,           // Input:  Global flush signal
            control_to_alu_rs_c,        // Input:  Dispatched ALU instructions
            control_to_mem_rs_c,        // Input:  Dispatched Memory instructions
            control_to_branch_rs_c,     // Input:  Dispatched Branch instructions
            branch_unit_to_control_c,   // Output: Branch execution results to Control
            commit_bus                  // Input:  Commit info (for MemorySystem stores)
        )
    {
    }
};