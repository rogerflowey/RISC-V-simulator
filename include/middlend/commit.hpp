#pragma once
#include "utils/bus.hpp"
#include "middlend/rob.hpp"
#include "middlend/reg.hpp"
#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"
#include <iostream> // For std::cout
#include <string>   // For std::string
#include "utils/reg_dump.hpp" // For RegisterDumper

class Committer {
private:
    CommonDataBus& cdb_;
    Channel<BranchResult>& branch_result_channel_;

    WritePort<CDBResult>& rob_cdb_port_;
    WritePort<BranchResult>& rob_branch_port_;
    ReadPort<bool, std::optional<ROBEntry>> rob_head_port_;
    WritePort<bool>& rob_pop_port_;

    WritePort<FillRequest>& reg_fill_port_;
    ReadPort<RegIDType, std::pair<RegDataType, RobIDType>> reg_get_port_; // For reading a0 on HALT

    Bus<ROBEntry>& commit_bus_;
    Bus<bool>& flush_bus_;
    Channel<PCType>& flush_pc_channel_;


    //only for dump
    RegisterFile& reg_;
    norb::RegisterDumper<32, RegDataType> dumper_;

public:
    Committer(
        ReorderBuffer& rob,
        RegisterFile& reg,
        CommonDataBus& cdb,
        Channel<BranchResult>& branch_result_channel,
        Bus<ROBEntry>& commit_bus,
        Bus<bool>& flush_bus,
        Channel<PCType>& flush_pc_channel
    ) :
        cdb_(cdb),
        reg_(reg),
        branch_result_channel_(branch_result_channel),
        // Wire up the ports
        rob_cdb_port_(rob.create_cdb_port()),
        rob_branch_port_(rob.create_branch_port()),
        rob_head_port_(rob.create_front_port()),
        rob_pop_port_(rob.create_pop_port()),
        reg_fill_port_(reg.create_fill_port()),
        reg_get_port_(reg.create_get_port()), // Create port to read register file
        commit_bus_(commit_bus),
        flush_bus_(flush_bus),
        flush_pc_channel_(flush_pc_channel),
        dumper_("../dump/my.dump") // Initialize the dumper
    {
        Clock::getInstance().subscribe([this] { this->work(); });
    }

    void work() {
        if(flush_bus_.get()) {
            logger.Warn("Commit unit flush initiated.");
            branch_result_channel_.clear();
            return;
        }
        if (auto cdb_result = cdb_.get()) {
            if (rob_cdb_port_.can_push()) {
                rob_cdb_port_.push(*cdb_result);
            }
        }
        if (auto branch_result = branch_result_channel_.receive()) {
            if (rob_branch_port_.can_push()) {
                rob_branch_port_.push(*branch_result);
            }
        }

        auto head_entry_opt = rob_head_port_.read(true);
        if (!head_entry_opt) {
            return;
        }
        const auto& head_entry = *head_entry_opt;

        if (head_entry.state == ISHALT) {
            auto a0_state = reg_get_port_.read(10);
            RegDataType a0_value = a0_state.first;
            std::cout << (a0_value & 0xff) << std::endl;
            exit(0);
        }

        if (head_entry.state == COMMIT_READY) {
            if (!reg_fill_port_.can_push() || !rob_pop_port_.can_push()) {
                return;
            }

            ROBEntry commit_result = head_entry;

            // ----DUMP Logic----
            auto reg_snapshot = reg_.get_snapshot();
            if (commit_result.reg_id != 0) {
                reg_snapshot[commit_result.reg_id] = commit_result.value;
            }
            dumper_.dump(commit_result.pc, reg_snapshot);
            // --- END DUMP LOGIC ---

            if (commit_result.reg_id != 0) {
                reg_fill_port_.push({commit_result.reg_id, commit_result.id, commit_result.value});
            }

            commit_bus_.send(commit_result);

            if (is_branch(commit_result.type) && commit_result.predicted_taken != commit_result.is_taken) {
                PCType correct_pc = commit_result.is_taken ? commit_result.target_pc : (commit_result.pc + 4);
                flush_pc_channel_.send(correct_pc);
                flush_bus_.send(true);
            }
            rob_pop_port_.push(true);
        }
    }
};