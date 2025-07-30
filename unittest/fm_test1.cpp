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
    if (!message.empty()) {
        std::cout << "Cycle " << Clock::getInstance().getTime() + 1 << ": " << message << std::endl;
    }
    Clock::getInstance().tick();
}

/**
 * @brief Tests the full pipeline from Frontend to Control, including a branch
 * misprediction, global flush, and recovery.
 */
void test_full_pipeline_misprediction_and_recovery() {
    // 1. SETUP
    // --- Program Memory ---
    // 0x0: beq x0, x0, 8  (branch to PC=8, always taken)
    // 0x4: addi x5, x0, 99 (wrong path)
    // 0x8: addi x6, x0, 100 (correct path)
    std::vector<uint32_t> instructions;
    instructions.resize(12); // Pad for PC indexing
    instructions[0] = 0x00000463; // beq x0, x0, 8
    instructions[1] = 0x06300293; // addi x5, x0, 99
    instructions[2] = 0x06400313; // addi x6, x0, 100

    // --- Communication Channels & Buses ---
    Channel<Instruction> decoded_instruction_c;
    Channel<PCType> mispredict_flush_pc_c;
    Bus<bool> global_flush_bus;
    Bus<ROBEntry> commit_bus;
    CommonDataBus cdb;
    Channel<BranchResult> branch_result_c;
    Channel<FetchedInstruction> alu_c, mem_c, branch_c;
    Channel<CDBResult> alu_to_cdb_c, mem_to_cdb_c;
    cdb.connect(alu_to_cdb_c);
    cdb.connect(mem_to_cdb_c);

    // --- Core Components ---
    Frontend frontend(instructions, decoded_instruction_c, mispredict_flush_pc_c, global_flush_bus, commit_bus);
    Control control(decoded_instruction_c, branch_result_c, alu_c, mem_c, branch_c, commit_bus, global_flush_bus, mispredict_flush_pc_c, cdb);

    // --- Simulated Backend Units (as simple lambdas) ---
    // Branch unit: takes 1 cycle to execute
    Clock::getInstance().subscribe([&]{
        auto fetched = branch_c.receive();
        if (fetched) {
            // BEQ x0, x0 is always taken.
            branch_result_c.send({fetched->id, true, fetched->ins.pc + fetched->ins.imm});
        }
    });
    // ALU: takes 2 cycles to execute
    std::optional<FetchedInstruction> alu_inflight;
    int alu_timer = 0;
    Clock::getInstance().subscribe([&]{
        if (alu_inflight) {
            alu_timer--;
            if (alu_timer <= 0) {
                alu_to_cdb_c.send({alu_inflight->id, 42}); // Send dummy result
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

    // --- Phase 1: Fetch and Predict-Not-Taken ---
    tick("PCLogic sends PC=0 to Fetcher.");
    tick("Fetcher gets PC=0; PCLogic sends PC=4.");
    tick("Decoder gets BEQ@0, predicts NOT-TAKEN, sends to Control.");
    auto inst_beq = decoded_instruction_c.peek();
    assert(inst_beq.has_value());
    ASSERT_EQ(inst_beq->pc, (PCType)0, "BEQ instruction should be decoded");
    ASSERT_EQ(inst_beq->predicted_taken, false, "BEQ should be predicted NOT-TAKEN initially");

    // --- Phase 2: Speculative (Wrong-Path) Execution ---
    tick("Control issues BEQ@0 to Branch Unit. Frontend decodes wrong-path ADDI@4.");
    assert(branch_c.peek().has_value());
    ASSERT_EQ(branch_c.peek()->id, (RobIDType)1, "BEQ should be issued with ROB ID 1");
    auto inst_addi_wrong = decoded_instruction_c.peek();
    assert(inst_addi_wrong.has_value());
    ASSERT_EQ(inst_addi_wrong->pc, (PCType)4, "Wrong-path ADDI should be decoded");

    tick("Control issues wrong-path ADDI@4 to ALU. Branch Unit finishes.");
    // Control consumes the ADDI from the channel and sends it to the ALU channel
    assert(alu_c.peek().has_value());
    ASSERT_EQ(alu_c.peek()->id, (RobIDType)2, "Wrong-path ADDI should be issued with ROB ID 2");
    // Branch unit sends its result back
    assert(branch_result_c.peek().has_value());
    ASSERT_EQ(branch_result_c.peek()->rob_id, (RobIDType)1, "Branch result for ROB ID 1 should be ready");
    ASSERT_EQ(branch_result_c.peek()->is_taken, true, "Branch result should be TAKEN");

    // --- Phase 3: Misprediction Detection and Flush Trigger ---
    tick("Control processes branch result, ROB entry for BEQ is updated.");
    // Control's work() will consume the branch result and update the ROB.
    // The BEQ is not yet at the head of the ROB, so it cannot commit.

    tick("Control commits BEQ@0, detects misprediction, and triggers flush.");
    // Now the BEQ is at the head of the ROB and can be committed.
    // During commit, Control compares predicted (false) vs actual (true) and finds the misprediction.
    auto commit_info = commit_bus.get();
    assert(commit_info.has_value());
    ASSERT_EQ(commit_info->id, (RobIDType)1, "BEQ should be committed");

    // The flush signals should be sent in the same cycle as the commit.
    assert(global_flush_bus.get().has_value());
    ASSERT_EQ(global_flush_bus.get().value(), true, "Global flush signal should be asserted");
    assert(mispredict_flush_pc_c.peek().has_value());
    ASSERT_EQ(mispredict_flush_pc_c.peek().value(), (PCType)8, "Flush PC should be the correct target (8)");

    // --- Phase 4: Pipeline Flush and Recovery ---
    tick("FLUSH CYCLE. Frontend and Control see flush signal and clear their state.");
    // Frontend's PCLogic will see the flush PC and use it next cycle.
    // Frontend's Fetcher/Decoder will see the flush bus and clear their inputs.
    // Control will see the flush bus and call its flush() method.
    // No new instruction should be decoded this cycle.
    assert(!decoded_instruction_c.peek().has_value());

    tick("Frontend restarts: PCLogic sends correct PC=8 to Fetcher.");
    tick("Fetcher gets PC=8.");
    tick("Decoder gets correct-path ADDI@8.");
    assert(!decoded_instruction_c.peek().has_value()); // Still in frontend pipeline

    // --- Phase 5: Correct-Path Execution ---
    tick("Correct-path ADDI@8 is sent to Control.");
    auto inst_addi_correct = decoded_instruction_c.peek();
    assert(inst_addi_correct.has_value());
    ASSERT_EQ(inst_addi_correct->pc, (PCType)8, "Correct-path ADDI@8 should be decoded");
    ASSERT_EQ(inst_addi_correct->op, OpType::ADDI, "Opcode should be ADDI");

    tick("Control issues correct-path ADDI@8 to ALU.");
    // The ROB was flushed, so the new instruction should get ROB ID 1.
    assert(alu_c.peek().has_value());
    ASSERT_EQ(alu_c.peek()->id, (RobIDType)1, "Correct-path ADDI should get new ROB ID 1 after flush");
    ASSERT_EQ(alu_c.peek()->ins.pc, (PCType)8, "Correct-path ADDI PC should be 8");
}

// --- Main Function ---

int main() {
    // Set logger to a higher level to reduce noise during tests
    logger.SetLevel(LogLevel::WARN);

    run_test(test_full_pipeline_misprediction_and_recovery, "Full Pipeline Misprediction and Recovery Test");

    std::cout << "\nAll integrated pipeline tests passed successfully!\n" << std::endl;

    return 0;
}