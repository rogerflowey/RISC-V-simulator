#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <optional>

// --- Core Component Headers ---
#include "frontend/frontend.hpp"
#include "middlend/control.hpp"
#include "instruction.hpp"
#include "middlend/rob.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "logger.hpp"
#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"

// --- Test Utilities ---

// A simple assertion helper to provide more context on failure.
template<typename T, typename U>
void ASSERT_EQ(T a, U b, const std::string& message) {
    if (a != b) {
        std::cerr << "\n--- ASSERTION FAILED ---\n"
                  << "MESSAGE: " << message << "\n"
                  << "EXPECTED: " << b << "\n"
                  << "GOT: " << a << "\n"
                  << "------------------------" << std::endl;
        exit(1);
    }
}

// Test runner function
void run_test(void (*test_func)(), const std::string& test_name) {
    std::cout << "--- Running test: " << test_name << " ---" << std::endl;
    Clock::getInstance().reset();
    test_func();
    std::cout << "--- PASSED: " << test_name << " ---" << std::endl << std::endl;
}

// Helper to advance the clock and print a message for clarity
void tick(const std::string& message = "") {
    Clock::getInstance().tick();
    if (!message.empty()) {
        std::cout << "Cycle " << Clock::getInstance().getTime() + 1 << ": " << message << std::endl;
    }
}

/**
 * @brief Tests the full pipeline from Frontend to Control, including a branch
 * misprediction, global flush, and recovery.
 */
/**
 * @brief Tests the full pipeline from Frontend to Control, including a branch
 * misprediction, global flush, and recovery.
 */
void test_full_pipeline_misprediction_and_recovery() {
    // 1. SETUP (Same as before)
    std::vector<uint32_t> instructions;
    instructions.resize(12);
    instructions[0] = 0x00000463; // beq x0, x0, 8
    instructions[1] = 0x06300293; // addi x5, x0, 99
    instructions[2] = 0x06400313; // addi x6, x0, 100
    Channel<Instruction> decoded_instruction_c;
    Channel<PCType> mispredict_flush_pc_c;
    Bus<bool> global_flush_bus;
    Bus<ROBEntry> commit_bus;
    CommonDataBus cdb;
    Channel<BranchResult> branch_result_c;
    Channel<FilledInstruction> alu_c, mem_c, branch_c;
    Channel<CDBResult> alu_to_cdb_c, mem_to_cdb_c;
    cdb.connect(alu_to_cdb_c);
    cdb.connect(mem_to_cdb_c);
    Frontend frontend(instructions, decoded_instruction_c, mispredict_flush_pc_c, global_flush_bus, commit_bus);
    Control control(decoded_instruction_c, branch_result_c, alu_c, mem_c, branch_c, commit_bus, global_flush_bus, mispredict_flush_pc_c, cdb);
    Clock::getInstance().subscribe([&]{
        auto fetched = branch_c.receive();
        if (fetched) {
            branch_result_c.send({fetched->id, true, fetched->ins.pc + fetched->ins.imm});
        }
    });
    std::optional<FilledInstruction> alu_inflight;
    int alu_timer = 0;
    Clock::getInstance().subscribe([&]{
        if (alu_inflight) {
            alu_timer--;
            if (alu_timer <= 0) {
                alu_to_cdb_c.send({alu_inflight->id, 42});
                alu_inflight.reset();
            }
        }
        if (!alu_inflight) {
            auto fetched = alu_c.receive();
            if (fetched) {
                alu_inflight = fetched;
                alu_timer = 2;
            }
        }
    });

    // 2. EXECUTION & VERIFICATION

    // --- Phases 1 & 2: Speculative Execution (Cycles 1-5) ---
    tick("C1: PCLogic -> 0");
    tick("C2: Fetcher -> 0");
    tick("C3: Decoder -> 0 (BEQ, predicted NOT-TAKEN)");
    tick("C4: Control issues BEQ@0; Decoder -> 4 (wrong ADDI)");
    tick("C5: Control issues ADDI@4; Branch Unit finishes");

    // --- Phase 3: Misprediction Detection (Cycle 6) ---
    tick("C6: Control processes branch result, commits BEQ, and triggers flush.");
    auto commit_info = commit_bus.get();
    assert(commit_info.has_value());
    ASSERT_EQ(commit_info->id, (RobIDType)1, "BEQ should be committed in Cycle 6");
    assert(global_flush_bus.get().has_value());
    ASSERT_EQ(global_flush_bus.get().value(), true, "Global flush signal should be asserted in Cycle 6");
    assert(mispredict_flush_pc_c.peek().has_value());
    ASSERT_EQ(mispredict_flush_pc_c.peek().value(), (PCType)8, "Flush PC should be the correct target (8)");

    // --- Phase 4: Pipeline Flush and Recovery ---
    tick("C7: FLUSH CYCLE. All components see flush signal and clear their state.");
    // PCLogic gets the new PC=8 and sends it to the Fetcher.
    // Fetcher and Decoder see the flush signal, clear their inputs, and return early.
    // The pipeline is now empty. The Control unit has consumed the stale instruction.

    tick("C8: RECOVERY BUBBLE. Fetcher processes PC=8 and sends to Decoder.");
    // The correct instruction is now between the Fetcher and Decoder.
    // The output to Control is still empty.
    assert(!decoded_instruction_c.peek().has_value());

    // --- Phase 5: Correct-Path Execution ---
    tick("C9: RECOVERY COMPLETE. Decoder processes instruction and sends to Control.");
    // The correct instruction has now traversed the entire Frontend and is waiting for Control.
    auto inst_addi_correct = decoded_instruction_c.peek();
    assert(inst_addi_correct.has_value());
    ASSERT_EQ(inst_addi_correct->pc, (PCType)8, "Correct-path ADDI@8 should be decoded by C9");
    ASSERT_EQ(inst_addi_correct->op, OpType::ADDI, "Opcode should be ADDI");

    tick("C10: Control issues correct-path ADDI@8 to ALU.");
    // Control's work() function now receives the instruction that was sent in C9.
    assert(alu_c.peek().has_value());
    ASSERT_EQ(alu_c.peek()->id, (RobIDType)1, "Correct-path ADDI should get new ROB ID 1 after flush");
    ASSERT_EQ(alu_c.peek()->ins.pc, (PCType)8, "Correct-path ADDI PC should be 8");
}

// --- Main Function ---

int main() {
    // Set logger to a higher level to reduce noise during tests
    logger.SetLevel(LogLevel::INFO);

    run_test(test_full_pipeline_misprediction_and_recovery, "Full Pipeline Misprediction and Recovery Test");

    std::cout << "\nAll integrated pipeline tests passed successfully!\n" << std::endl;

    return 0;
}