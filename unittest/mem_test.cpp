#include <iostream>
#include <cassert>
#include <string>
#include <functional>

#include "constants.hpp"
#include "utils/clock.hpp"
#include "utils/bus.hpp"
#include "instruction.hpp"
#include "backend/cdb.hpp"
#include "backend/memsys.hpp"
#include "backend/units/branch.hpp"
#include "middlend/control.hpp"
#include "middlend/rob.hpp"
#include "logger.hpp"

// --- Test Helper Functions ---

// Creates an ADDI instruction (rd = rs1 + imm)
Instruction create_addi_inst(RegIDType rd, RegIDType rs1, int32_t imm, PCType pc) {
    return {OpType::ADDI, pc, rd, rs1, 0, std::bit_cast<RegDataType>(imm), false, false};
}

// Creates a Load Word instruction (rd = mem[rs1 + imm])
Instruction create_lw_inst(RegIDType rd, RegIDType rs1, int32_t imm, PCType pc) {
    return {OpType::LW, pc, rd, rs1, 0, std::bit_cast<RegDataType>(imm), false, false};
}

// Creates a Store Word instruction (mem[rs1 + imm] = rs2)
Instruction create_sw_inst(RegIDType rs2, RegIDType rs1, int32_t imm, PCType pc) {
    // For stores, rd is not used; rs2 is the data source.
    return {OpType::SW, pc, 0, rs1, rs2, std::bit_cast<RegDataType>(imm), false, false};
}

// Advances the clock and prints a message for clarity.
void tick(const std::string& message = "") {
    if (!message.empty()) {
        std::cout << "--- Cycle " << Clock::getInstance().getTime() + 1 << ": " << message << " ---" << std::endl;
    }
    Clock::getInstance().tick();
}

// --- Main Test Suite ---
// In mem_test.cpp

void run_mem_test_suite() {
    std::cout << "\n\n" << std::string(50, '=') << std::endl;
    std::cout << "      RUNNING MEMORY SYSTEM TEST SUITE" << std::endl;
    std::cout << std::string(50, '=') << "\n\n";

    // --- Test 1: Simple Store followed by Load (This test was already correct) ---
    std::cout << "===== Test 1: Simple Store then Load =====\n";
    {
        // 1. Setup
        Clock::getInstance().reset();
        Channel<Instruction> ins_c;
        Channel<BranchResult> branch_res_c;
        Channel<FilledInstruction> alu_c, mem_c, branch_c;
        Bus<ROBEntry> commit_b;
        Bus<bool> flush_b;
        Channel<PCType> flush_pc_c;
        CommonDataBus cdb(flush_b);
        MemorySystem mem_sys(cdb, mem_c, commit_b, flush_b);
        Control control(ins_c, branch_res_c, alu_c, mem_c, branch_c, commit_b, flush_b, flush_pc_c, cdb);
        Channel<CDBResult> alu_to_cdb_c;
        cdb.connect(alu_to_cdb_c);
        auto alu_simulator = [&] {
            auto ins = alu_c.receive();
            if (ins && ins->ins.op == OpType::ADDI) {
                RegDataType result = ins->v_rs1 + ins->ins.imm;
                alu_to_cdb_c.send({ins->id, result});
            }
        };
        Clock::getInstance().subscribe(alu_simulator);

        const RegDataType test_addr = 128;
        const RegDataType test_val = 98765;
        RegIDType data_reg = 5;
        RegIDType dest_reg = 6;

        // 2. Execution
        ins_c.send(create_addi_inst(data_reg, 0, test_val, 0));

        int cycles = 0;
        bool addi_committed = false;
        while(cycles++ < 20 && !addi_committed) {
            tick();
            auto committed = commit_b.get();
            if (committed && committed->type == OpType::ADDI) {
                assert(committed->value == test_val);
                addi_committed = true;
                logger.Info("ADDI committed successfully.");
            }
        }
        assert(addi_committed && "SETUP FAILED: ADDI instruction did not commit in time");

        ins_c.send(create_sw_inst(data_reg, 0, test_addr, 4));

        cycles = 0;
        bool sw_committed = false;
        while(cycles++ < 20 && !sw_committed) {
            tick();
            auto committed = commit_b.get();
            if (committed && committed->type == OpType::SW) {
                sw_committed = true;
                logger.Info("SW committed successfully.");
            }
        }
        assert(sw_committed && "TEST FAILED: SW instruction did not commit in time");

        ins_c.send(create_lw_inst(dest_reg, 0, test_addr, 8));

        cycles = 0;
        bool lw_committed = false;
        while(cycles++ < 30 && !lw_committed) {
            tick();
            auto committed = commit_b.get();
            if (committed && committed->type == OpType::LW) {
                logger.With("Value", committed->value).Info("LW committed.");
                assert(committed->value == test_val && "TEST FAILED: Loaded value does not match stored value");
                lw_committed = true;
            }
        }
        assert(lw_committed && "TEST FAILED: LW instruction did not commit in time");

        std::cout << "===== Test 1 Passed =====\n";
    }

    // --- Test 2: RAW and WAW Hazards ---
    std::cout << "\n===== Test 2: RAW and WAW Hazards =====\n";
    {
        // 1. Setup (same as Test 1)
        Clock::getInstance().reset();
        Channel<Instruction> ins_c;
        Channel<BranchResult> branch_res_c;
        Channel<FilledInstruction> alu_c, mem_c, branch_c;
        Bus<ROBEntry> commit_b;
        Bus<bool> flush_b;
        Channel<PCType> flush_pc_c;
        CommonDataBus cdb(flush_b);
        MemorySystem mem_sys(cdb, mem_c, commit_b, flush_b);
        Control control(ins_c, branch_res_c, alu_c, mem_c, branch_c, commit_b, flush_b, flush_pc_c, cdb);
        Channel<CDBResult> alu_to_cdb_c;
        cdb.connect(alu_to_cdb_c);
        auto alu_simulator = [&] {
            auto ins = alu_c.receive();
            if (ins && ins->ins.op == OpType::ADDI) {
                alu_to_cdb_c.send({ins->id, ins->v_rs1 + ins->ins.imm});
            }
        };
        Clock::getInstance().subscribe(alu_simulator);

        const RegDataType addr = 64;
        const RegDataType val1 = 5555;
        const RegDataType val2 = 7777;
        RegIDType reg_val1 = 5, reg_val2 = 7;
        RegIDType reg_load1 = 6, reg_load2 = 8;

        // 2. Execution: CORRECTED - Send instructions one per cycle
        ins_c.send(create_addi_inst(reg_val1, 0, val1, 0));      // rob_id=1
        tick("Send ADDI 1");
        ins_c.send(create_addi_inst(reg_val2, 0, val2, 4));      // rob_id=2
        tick("Send ADDI 2");
        ins_c.send(create_sw_inst(reg_val1, 0, addr, 8));        // rob_id=3
        tick("Send SW 1");
        ins_c.send(create_lw_inst(reg_load1, 0, addr, 12));      // rob_id=4, should get val1
        tick("Send LW 1");
        ins_c.send(create_sw_inst(reg_val2, 0, addr, 16));       // rob_id=5
        tick("Send SW 2");
        ins_c.send(create_lw_inst(reg_load2, 0, addr, 20));      // rob_id=6, should get val2
        tick("Send LW 2");

        int committed_loads = 0;
        int cycles = 0;
        while(cycles++ < 100 && committed_loads < 2) {
            tick();
            auto committed = commit_b.get();
            if (committed && committed->type == OpType::LW) {
                if (committed->reg_id == reg_load1) { // First LW
                    logger.With("Value", committed->value).Info("First LW (to x6) committed.");
                    assert(committed->value == val1 && "RAW Hazard FAILED: First LW got wrong value.");
                    committed_loads++;
                } else if (committed->reg_id == reg_load2) { // Second LW
                    logger.With("Value", committed->value).Info("Second LW (to x8) committed.");
                    assert(committed->value == val2 && "WAW Hazard FAILED: Second LW got wrong value.");
                    committed_loads++;
                }
            }
        }
        assert(committed_loads == 2 && "TEST FAILED: Not all load instructions committed in time");

        std::cout << "===== Test 2 Passed =====\n";
    }

    // --- Test 3: Pipeline Flush (This test was already correct) ---
    std::cout << "\n===== Test 3: Pipeline Flush =====\n";
    {
        // 1. Setup
        Clock::getInstance().reset();
        Channel<Instruction> ins_c;
        Channel<BranchResult> branch_res_c;
        Channel<FilledInstruction> alu_c, mem_c, branch_c;
        Bus<ROBEntry> commit_b;
        Bus<bool> flush_b;
        Channel<PCType> flush_pc_c;
        CommonDataBus cdb(flush_b);
        MemorySystem mem_sys(cdb, mem_c, commit_b, flush_b);
        Control control(ins_c, branch_res_c, alu_c, mem_c, branch_c, commit_b, flush_b, flush_pc_c, cdb);
        Channel<CDBResult> alu_to_cdb_c;
        cdb.connect(alu_to_cdb_c);
        auto alu_simulator = [&] {
            auto ins = alu_c.receive();
            if (ins && ins->ins.op == OpType::ADDI) {
                alu_to_cdb_c.send({ins->id, ins->v_rs1 + ins->ins.imm});
            }
        };
        Clock::getInstance().subscribe(alu_simulator);

        // 2. Execution
        ins_c.send(create_sw_inst(1, 0, 100, 0));
        tick("Send SW");
        ins_c.send(create_lw_inst(2, 0, 104, 4));
        tick("Send LW");
        ins_c.send(create_sw_inst(3, 0, 108, 8));
        tick("Send another SW");

        tick("Propagate instructions");

        flush_b.send(true);
        tick("FLUSH SIGNAL SENT");

        tick("Post-flush cycle");
        assert(!flush_b.get().has_value() && "Flush signal should be consumed");

        ins_c.send(create_addi_inst(10, 0, 99, 1000));
        tick("Send post-flush ADDI");

        bool new_ins_issued = false;
        for (int i = 0; i < 5; ++i) {
            tick();
            auto alu_ins = alu_c.peek();
            if (alu_ins) {
                logger.With("ROB_ID", alu_ins->id).Info("Post-flush instruction issued.");
                assert(alu_ins->id == 1 && "TEST FAILED: ROB did not reset; new instruction got wrong ID.");
                new_ins_issued = true;
                break;
            }
        }
        assert(new_ins_issued && "TEST FAILED: New instruction was not issued after flush.");

        std::cout << "===== Test 3 Passed =====\n";
    }
}

int main() {
    // Set a higher log level to reduce noise, or INFO to see everything.
    logger.SetLevel(LogLevel::INFO);
    run_mem_test_suite();
    std::cout << "\n[SUCCESS] All memory system tests passed!\n";
    return 0;
}