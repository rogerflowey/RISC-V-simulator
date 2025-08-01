#pragma once
#include "utils/bus.hpp"
#include "middlend/rob.hpp"
#include "middlend/reg.hpp"
#include "instruction.hpp"
#include "backend/cdb.hpp" // <-- Include CDB header

class RenameDispatch {
private:
    // --- Input Ports/Channels/Buses ---
    Channel<Instruction>& ins_channel_;
    CommonDataBus& cdb_; // <-- Add reference to the CDB
    ReadPort<bool, bool> rob_stall_port_;
    ReadPort<bool, RobIDType> rob_next_id_port_;
    ReadPort<RegIDType, std::pair<RegDataType, RobIDType>> reg_get_port_;
    ReadPort<RobIDType, std::optional<RegDataType>> rob_bypass_port_;

    // --- Output Ports/Channels ---
    WritePort<ROBEntry> rob_allocate_port_;
    WritePort<PresetRequest> reg_preset_port_;
    Channel<FilledInstruction>& alu_channel_;
    Channel<FilledInstruction>& mem_channel_;
    Channel<FilledInstruction>& branch_channel_;

public:
    RenameDispatch(
        Channel<Instruction>& ins_channel,
        CommonDataBus& cdb, // <-- Add CDB to constructor
        ReorderBuffer& rob,
        RegisterFile& reg,
        Channel<FilledInstruction>& alu_channel,
        Channel<FilledInstruction>& mem_channel,
        Channel<FilledInstruction>& branch_channel
    ) :
        ins_channel_(ins_channel),
        cdb_(cdb), // <-- Initialize CDB reference
        rob_stall_port_(rob.create_stall_port()),
        rob_next_id_port_(rob.create_next_id_port()),
        reg_get_port_(reg.create_get_port()),
        rob_bypass_port_(rob.create_get_port()),
        rob_allocate_port_(rob.create_allocate_port()),
        reg_preset_port_(reg.create_preset_port()),
        alu_channel_(alu_channel),
        mem_channel_(mem_channel),
        branch_channel_(branch_channel)
    {
        Clock::getInstance().subscribe([this] { this->work(); });
    }

    void work() {
        if (rob_stall_port_.read(true) || !ins_channel_.peek()) {
            return;
        }
        Instruction ins = *ins_channel_.peek();
        if (!can_dispatch(ins.op) || !rob_allocate_port_.can_push() || !reg_preset_port_.can_push()) {
            return;
        }
        
        bool is_halt_instruction = (ins.op == OpType::ADDI && ins.rd == 10 && ins.rs1 == 0 && ins.imm == 255);
        if (is_halt_instruction) {
            ins_channel_.receive();
            RobIDType new_rob_id = rob_next_id_port_.read(true);
            ROBEntry halt_entry = {new_rob_id, ins.op, ins.pc, ins.rd, 0, ISHALT};
            rob_allocate_port_.push(halt_entry);
            return;
        }

        RobIDType new_rob_id = rob_next_id_port_.read(true);
        FilledInstruction fetched = {ins, new_rob_id};
        auto cdb_broadcast = cdb_.get();

        if (ins.rs1 != 0) {
            auto [val, tag] = reg_get_port_.read(ins.rs1);
            if (tag != 0) { 
                if (cdb_broadcast && cdb_broadcast->rob_id == tag) {
                    fetched.v_rs1 = cdb_broadcast->data;
                    fetched.q_rs1 = 0;
                } 
                else if (auto bypass_val = rob_bypass_port_.read(tag)) {
                    fetched.v_rs1 = *bypass_val;
                    fetched.q_rs1 = 0;
                } 
                else {
                    fetched.q_rs1 = tag;
                }
            } else {
                fetched.v_rs1 = val;
                fetched.q_rs1 = 0;
            }
        }

        if (ins.rs2 != 0) {
            auto [val, tag] = reg_get_port_.read(ins.rs2);
            if (tag != 0) {
                if (cdb_broadcast && cdb_broadcast->rob_id == tag) {
                    fetched.v_rs2 = cdb_broadcast->data;
                    fetched.q_rs2 = 0;
                } else if (auto bypass_val = rob_bypass_port_.read(tag)) {
                    fetched.v_rs2 = *bypass_val;
                    fetched.q_rs2 = 0;
                } else {
                    fetched.q_rs2 = tag;
                }
            } else {
                fetched.v_rs2 = val;
                fetched.q_rs2 = 0;
            }
        }

        ins_channel_.receive();
        ROBEntry new_entry = {0, ins.op, ins.pc, ins.rd, 0, ISSUED, ins.is_branch, ins.predicted_taken};
        rob_allocate_port_.push(new_entry);
        if (ins.rd != 0) {
            reg_preset_port_.push({ins.rd, new_rob_id});
        }
        if (is_alu(ins.op)) alu_channel_.send(fetched);
        else if (is_mem(ins.op)) mem_channel_.send(fetched);
        else if (is_branch(ins.op)) branch_channel_.send(fetched);
    }

private:
    bool can_dispatch(OpType op) {
        if (is_alu(op)) return alu_channel_.can_send();
        if (is_mem(op)) return mem_channel_.can_send();
        if (is_branch(op)) return branch_channel_.can_send();
        return true;
    }
};