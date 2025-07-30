#pragma once

#include "frontend/frontend.hpp"
#include "middlend/control.hpp"
#include "backend/backend.hpp"

#include "utils/bus.hpp"
#include "instruction.hpp"
#include "constants.hpp"

#include <vector>
#include <cstdint>
#include <array>
#include <cstddef>
#include <algorithm>

class CPU {
private:
    std::array<std::byte, MEMORY_SIZE> unified_memory{};

    Channel<Instruction> decoded_instruction_c;
    Channel<FilledInstruction> control_to_alu_rs_c;
    Channel<FilledInstruction> control_to_mem_rs_c;
    Channel<FilledInstruction> control_to_branch_rs_c;
    Channel<BranchResult> branch_unit_to_control_c;
    Channel<PCType> mispredict_flush_pc_c;
    CommonDataBus cdb;
    Bus<bool> global_flush_bus;
    Bus<ROBEntry> commit_bus;

    // Core Pipeline Stages
    Frontend frontend;
    Control control;
    Backend backend;

public:
    CPU(const std::vector<std::byte>& initial_memory_image) :
        decoded_instruction_c(),
        control_to_alu_rs_c(),
        control_to_mem_rs_c(),
        control_to_branch_rs_c(),
        branch_unit_to_control_c(),
        mispredict_flush_pc_c(),
        cdb(global_flush_bus),
        global_flush_bus(),
        commit_bus(),

        frontend(
            unified_memory,             
            decoded_instruction_c,
            mispredict_flush_pc_c,
            global_flush_bus,
            commit_bus
        ),
        control(
            decoded_instruction_c,
            branch_unit_to_control_c,
            control_to_alu_rs_c,
            control_to_mem_rs_c,
            control_to_branch_rs_c,
            commit_bus,
            global_flush_bus,
            mispredict_flush_pc_c,
            cdb
        ),
        backend(
            unified_memory,
            cdb,
            global_flush_bus,
            control_to_alu_rs_c,
            control_to_mem_rs_c,
            control_to_branch_rs_c,
            branch_unit_to_control_c,
            commit_bus
        )
    {
        std::copy_n(initial_memory_image.begin(),
                    std::min(initial_memory_image.size(), unified_memory.size()),
                    unified_memory.begin());
                    
    }
};