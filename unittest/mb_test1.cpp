#include <iostream>
#include <cassert>
#include <string>
#include <memory>

// Include all necessary headers for the full pipeline
#include "middlend/control.hpp"
#include "backend/backend.hpp"
#include "constants.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "instruction.hpp"
#include "utils/logger/logger.hpp"

// --- Test Helper Functions ---

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

Instruction create_inst(OpType op, RegIDType rd, RegIDType rs1, RegIDType rs2, int32_t imm, PCType pc, bool is_branch = false, bool pred_taken = false) {
    return {op, pc, rd, rs1, rs2, std::bit_cast<RegDataType>(imm), is_branch, pred_taken};
}

// Helper struct to hold all the components for a test
struct TestHarness {
    // --- Communication Infrastructure ---
    // Frontend -> Control
    Channel<Instruction> ins_channel;
    // Control <-> Backend
    Channel<FilledInstruction> alu_channel;
    Channel<FilledInstruction> mem_channel;
    Channel<FilledInstruction> branch_channel;
    Channel<BranchResult> branch_result_channel;
    // Global Buses
    Bus<bool> global_flush_bus;
    Bus<ROBEntry> commit_bus;
    CommonDataBus cdb;
    // Flush PC (Control -> Frontend)
    Channel<PCType> flush_pc_channel;

    // --- Core Components ---
    std::unique_ptr<Control> control;
    std::unique_ptr<Backend> backend;

    TestHarness() : cdb(global_flush_bus) {
        // Instantiate the two main components
        control = std::make_unique<Control>(
            ins_channel, branch_result_channel,
            alu_channel, mem_channel, branch_channel,
            commit_bus, global_flush_bus, flush_pc_channel, cdb
        );

        backend = std::make_unique<Backend>(
            cdb, global_flush_bus,
            alu_channel, mem_channel, branch_channel,
            branch_result_channel, commit_bus
        );
    }
};

// Helper to run the simulation for a number of cycles, stopping if a condition is met.
void run_sim(int max_cycles, std::function<bool()> stop_condition = []{ return false; }) {
    for (int i = 0; i < max_cycles; ++i) {
        Clock::getInstance().tick();
        if (stop_condition()) {
            std::cout << "--- Stop condition met after " << i + 1 << " cycles. ---\n";
            return;
        }
    }
    std::cout << "--- Max cycles (" << max_cycles << ") reached. ---\n";
}

// --- Test Scenarios ---

/**
 * @brief Tests a single ADDI instruction from issue to commit.
 */
void test_simple_end_to_end_flow() {
    std::cout << "\n===== Running Test 1: Simple End-to-End Flow =====\n";
    Clock::getInstance().reset();
    TestHarness harness;

    // 1. Issue instruction: addi x1, x0, 123
    harness.ins_channel.send(create_inst(OpType::ADDI, 1, 0, 0, 123, 0));

    // 2. Run simulation until the instruction is committed.
    run_sim(20, [&]() { return harness.commit_bus.get().has_value(); });

    // 3. Verification
    auto committed = harness.commit_bus.get();
    assert(committed.has_value() && "Instruction should have been committed");
    ASSERT_EQ(committed->id, (RobIDType)1, "Committed ROB ID should be 1");
    ASSERT_EQ(committed->value, (RegDataType)123, "Committed value should be 123");

    // Check register file state
    auto reg_val = harness.control->get_reg().get(1).first;
    ASSERT_EQ(reg_val, (RegDataType)123, "Register x1 should be updated to 123");
    std::cout << "===== Test 1 Passed =====\n";
}

/**
 * @brief Tests a RAW dependency between two ADD instructions.
 */
void test_raw_dependency_flow() {
    std::cout << "\n===== Running Test 2: RAW Dependency Flow =====\n";
    Clock::getInstance().reset();
    TestHarness harness;

    // 1. Issue two dependent instructions
    // addi x1, x0, 10
    // add x2, x1, x1
    harness.ins_channel.send(create_inst(OpType::ADDI, 1, 0, 0, 10, 0));
    run_sim(1); // Tick to let the first instruction be sent
    harness.ins_channel.send(create_inst(OpType::ADD, 2, 1, 1, 0, 4));

    // 2. Run simulation until the second instruction is committed.
    run_sim(30, [&]() {
        auto committed = harness.commit_bus.get();
        return committed.has_value() && committed->id == 2;
    });

    // 3. Verification
    auto committed = harness.commit_bus.get();
    assert(committed.has_value() && "Second instruction should have been committed");
    ASSERT_EQ(committed->id, (RobIDType)2, "Committed ROB ID should be 2");
    ASSERT_EQ(committed->value, (RegDataType)20, "Committed value for ADD should be 20 (10+10)");

    auto reg_val = harness.control->get_reg().get(2).first;
    ASSERT_EQ(reg_val, (RegDataType)20, "Register x2 should be updated to 20");
    std::cout << "===== Test 2 Passed =====\n";
}

/**
 * @brief Tests a branch misprediction and the full pipeline flush/recovery.
 */
void test_branch_misprediction_and_flush() {
    std::cout << "\n===== Running Test 3: Branch Misprediction & Flush =====\n";
    Clock::getInstance().reset();
    TestHarness harness;

    // 1. Issue a branch (predicted NOT taken) and a wrong-path instruction
    // beq x0, x0, 8 (pc=0, predicted false, but will be taken)
    // addi x5, x0, 99 (pc=4, wrong path)
    harness.ins_channel.send(create_inst(OpType::BEQ, 0, 0, 0, 8, 0, true, false));
    run_sim(1);
    harness.ins_channel.send(create_inst(OpType::ADDI, 5, 0, 0, 99, 4));

    // 2. Run until the flush signal is asserted
    run_sim(20, [&]() { return harness.global_flush_bus.get().has_value(); });

    // 3. Verification of flush
    assert(harness.global_flush_bus.get().has_value() && "Flush signal should be asserted");
    auto flush_pc = harness.flush_pc_channel.peek();
    assert(flush_pc.has_value() && "Flush PC should be sent");
    ASSERT_EQ(*flush_pc, (PCType)8, "Flush PC should be the correct target (8)");

    // The wrong-path instruction (ROB ID 2) should be in the ROB but will be flushed before commit.
    // Let's verify its value is NOT in the register file.
    auto reg_val_wrong = harness.control->get_reg().get(5).first;
    ASSERT_EQ(reg_val_wrong, (RegDataType)0, "Wrong-path instruction should not have updated register x5");

    // 4. Issue the correct-path instruction and verify recovery
    run_sim(1); // Let the flush signal be consumed
    harness.ins_channel.send(create_inst(OpType::ADDI, 6, 0, 0, 55, 8));

    // 5. Run until the correct-path instruction commits
    run_sim(20, [&]() {
        auto c = harness.commit_bus.get();
        // After a flush, ROB IDs restart. The first committed instruction will be the branch (ID 1).
        // The next one will be the correct-path instruction (ID 2, but since ROB is flushed, it becomes ID 1 again).
        return c.has_value() && c->pc == 8;
    });

    auto committed_correct = harness.commit_bus.get();
    assert(committed_correct.has_value() && "Correct-path instruction should have committed");
    ASSERT_EQ(committed_correct->value, (RegDataType)55, "Correct-path instruction value should be 55");
    auto reg_val_correct = harness.control->get_reg().get(6).first;
    ASSERT_EQ(reg_val_correct, (RegDataType)55, "Register x6 should be updated by correct-path instruction");

    std::cout << "===== Test 3 Passed =====\n";
}

/**
 * @brief Tests that a load correctly reads a value written by a preceding, committed store.
 */
void test_store_load_dependency() {
    std::cout << "\n===== Running Test 4: Store-Load Dependency =====\n";
    Clock::getInstance().reset();
    TestHarness harness;

    // 1. Issue a sequence: ADDI, SW, LW
    // addi x1, x0, 777
    // sw x1, 64(x0)
    // lw x2, 64(x0)
    harness.ins_channel.send(create_inst(OpType::ADDI, 1, 0, 0, 777, 0));
    run_sim(1);
    // --- FIX ---
    // Corrected arguments for SW: sw rs2, imm(rs1) -> sw x1, 64(x0)
    // op, rd, rs1 (base), rs2 (data), imm, pc
    harness.ins_channel.send(create_inst(OpType::SW, 0, 0, 1, 64, 4));
    // --- END FIX ---
    run_sim(1);
    harness.ins_channel.send(create_inst(OpType::LW, 2, 0, 0, 64, 8));

    // 2. Run simulation until the final LW instruction is committed.
    run_sim(50, [&]() {
        auto c = harness.commit_bus.get();
        return c.has_value() && c->type == OpType::LW;
    });

    // 3. Verification
    auto committed_lw = harness.commit_bus.get();
    assert(committed_lw.has_value() && "LW should have committed");
    ASSERT_EQ(committed_lw->id, (RobIDType)3, "LW should be the 3rd instruction");
    ASSERT_EQ(committed_lw->value, (RegDataType)777, "LW should read the value written by SW");

    auto reg_val = harness.control->get_reg().get(2).first;
    ASSERT_EQ(reg_val, (RegDataType)777, "Register x2 should be updated with the loaded value");
    std::cout << "===== Test 4 Passed =====\n";
}


int main() {
    logger.SetLevel(LogLevel::INFO); // Set to WARN or ERROR to reduce log spam

    test_simple_end_to_end_flow();
    test_raw_dependency_flow();
    test_branch_misprediction_and_flush();
    test_store_load_dependency();

    std::cout << "\n[SUCCESS] All combined middle-end/back-end tests passed!\n";
    return 0;
}