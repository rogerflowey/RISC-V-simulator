#pragma once

#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"
#include "constants.hpp"
#include "instruction.hpp"
#include "logger.hpp"
#include "utils/bus.hpp"

#include "middlend/reg.hpp" 
#include "middlend/rob.hpp" 
#include "middlend/dispatch.hpp"
#include "middlend/commit.hpp"         

#include <memory> 


class Controller {
private:
    ReorderBuffer rob_;
    RegisterFile reg_;

    std::unique_ptr<Dispatcher> renamer_;
    std::unique_ptr<Committer> committer_;

    Bus<bool>& flush_bus_;

public:
    Controller(

        Channel<Instruction>& ins_channel,
        Channel<BranchResult>& branch_result_channel,
        CommonDataBus& cdb,

        Channel<FilledInstruction>& alu_channel,
        Channel<FilledInstruction>& mem_channel,
        Channel<FilledInstruction>& branch_channel,
        Bus<ROBEntry>& commit_bus,
        Bus<bool>& flush_bus,
        Channel<PCType>& flush_pc_channel
    ) :
        flush_bus_(flush_bus)
    {
       
        committer_ = std::make_unique<Committer>(
            rob_,
            reg_,
            cdb,
            branch_result_channel,
            commit_bus,
            flush_bus_,
            flush_pc_channel
        );

        
        renamer_ = std::make_unique<Dispatcher>(
            ins_channel,
            cdb,
            rob_,
            reg_,
            alu_channel,
            mem_channel,
            branch_channel,
            flush_bus_
        );

        Clock::getInstance().subscribe([this] {
            if (this->flush_bus_.get()) {
                this->flush();
            }
        });

        logger.Info("Control subsystem initialized and wired.");
    }

    void flush() {
        logger.Warn("Control unit flush initiated.");
        reg_.flush();
        rob_.flush();
        
    }

    /**
     * @brief Provides a read-only snapshot of the architectural registers for testing/debugging.
     * @return A const reference to the register array.
     */
    const std::array<RegDataType, REG_SIZE>& get_reg_snapshot() const {
        return reg_.get_snapshot();
    }
};