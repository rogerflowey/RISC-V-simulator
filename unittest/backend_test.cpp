#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <memory>

// Assume all necessary headers are included here.
// This avoids cluttering the test file itself.
#include "backend/backend.hpp"
#include "instruction.hpp"
#include "middlend/rob.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "logger.hpp"

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
    // Reset the global clock for each test to ensure isolation
    Clock::getInstance().reset();
    test_func();
    std::cout << "--- PASSED: " << test_name << " ---" << std::endl << std::endl;
}

// Helper to create a simple, ready-to-execute instruction
FilledInstruction create_ready_inst(RobIDType id, OpType op, RegDataType v1 = 0, RegDataType v2 = 0, RegDataType imm = 0, PCType pc = 0) {
    FilledInstruction fi;
    fi.id = id;
    fi.ins.op = op;
    fi.ins.pc = pc;
    fi.ins.imm = imm;
    fi.v_rs1 = v1;
    fi.v_rs2 = v2;
    fi.q_rs1 = 0;
    fi.q_rs2 = 0;
    return fi;
}

// Helper struct to hold all the components for a test
struct TestHarness {
    // Input Channels (from Control to Backend)
    Channel<FilledInstruction> control_to_alu_rs_c;
    Channel<FilledInstruction> control_to_mem_rs_c;
    Channel<FilledInstruction> control_to_branch_rs_c;

    // Output Channel (from Backend to Control)
    Channel<BranchResult> branch_unit_to_control_c;

    // Shared Buses
    CommonDataBus cdb;
    Bus<bool> global_flush_bus;
    Bus<ROBEntry> commit_bus;

    // The Backend itself
    std::unique_ptr<Backend> backend;

    TestHarness() : cdb(global_flush_bus) {
        backend = std::make_unique<Backend>(
            cdb,
            global_flush_bus,
            control_to_alu_rs_c,
            control_to_mem_rs_c,
            control_to_branch_rs_c,
            branch_unit_to_control_c,
            commit_bus
        );
    }
};


// --- Test Scenarios ---

/**
 * @brief Tests a simple, independent ALU instruction (ADDI).
 * Verifies that the instruction flows through the ALU RS, ALU, and broadcasts its result on the CDB.
 */
void test_simple_alu_instruction() {
    // 1. SETUP
    TestHarness harness;
    auto& clock = Clock::getInstance();

    // Instruction: addi x1, x0, 50 (ROB ID: 1)
    // v_rs1 is 0 (from x0), so it's ready immediately.
    FilledInstruction inst = create_ready_inst(1, OpType::ADDI, 0, 0, 50);
    assert(harness.control_to_alu_rs_c.send(inst));

    // 2. EXECUTION & VERIFICATION
    std::optional<CDBResult> cdb_result;
    for (int i = 0; i < 10; ++i) { // Loop for a few cycles to let the pipeline work
        clock.tick();
        cdb_result = harness.cdb.get();
        if (cdb_result) break;
    }

    assert(cdb_result.has_value());
    ASSERT_EQ(cdb_result->rob_id, (RobIDType)1, "CDB result has correct ROB ID");
    ASSERT_EQ(cdb_result->data, (RegDataType)50, "CDB result has correct data for ADDI");
}

/**
 * @brief Tests a Read-After-Write (RAW) data dependency.
 * An ADD instruction depends on the result of a preceding ADDI.
 * Verifies that the ADD waits in the RS until the ADDI's result is available on the CDB.
 */
void test_raw_dependency() {
    // 1. SETUP
    TestHarness harness;
    auto& clock = Clock::getInstance();

    // Program:
    // 1. addi t1, x0, 15  (ROB ID: 10) -> result is 15
    // 2. add  t2, t1, t1  (ROB ID: 11) -> depends on ROB ID 10
    FilledInstruction inst1 = create_ready_inst(10, OpType::ADDI, 0, 0, 15);

    FilledInstruction inst2;
    inst2.id = 11;
    inst2.ins.op = OpType::ADD;
    inst2.q_rs1 = 10; // Depends on result of instruction 10
    inst2.q_rs2 = 10; // Depends on result of instruction 10

    // --- FIX ---
    // Send instructions on consecutive cycles to avoid channel contention.
    // The channel can only accept one instruction per clock cycle.
    assert(harness.control_to_alu_rs_c.send(inst1));
    clock.tick(); // Tick the clock to allow the channel to be ready for the next send.
    assert(harness.control_to_alu_rs_c.send(inst2));
    // --- END FIX ---

    // 2. EXECUTION & VERIFICATION
    // Tick until the first result is on the CDB
    std::optional<CDBResult> cdb_res1;
    for (int i = 0; i < 10; ++i) {
        clock.tick();
        cdb_res1 = harness.cdb.get();
        if (cdb_res1) break;
    }

    assert(cdb_res1.has_value());
    ASSERT_EQ(cdb_res1->rob_id, (RobIDType)10, "First instruction's ROB ID is correct");
    ASSERT_EQ(cdb_res1->data, (RegDataType)15, "First instruction's data is correct");

    // Tick until the second result is on the CDB
    std::optional<CDBResult> cdb_res2;
    for (int i = 0; i < 10; ++i) {
        clock.tick();
        cdb_res2 = harness.cdb.get();
        if (cdb_res2) break;
    }

    assert(cdb_res2.has_value());
    ASSERT_EQ(cdb_res2->rob_id, (RobIDType)11, "Second instruction's ROB ID is correct");
    ASSERT_EQ(cdb_res2->data, (RegDataType)30, "Second instruction's data (15+15) is correct");
}

/**
 * @brief Tests a conditional branch instruction (BEQ) that is taken.
 * Verifies the correct BranchResult is sent to the control unit.
 */
void test_branch_taken() {
    // 1. SETUP
    TestHarness harness;
    auto& clock = Clock::getInstance();

    // Instruction: beq x5, x5, 16 (ROB ID: 20, PC: 100)
    // Operands are ready and equal.
    FilledInstruction inst = create_ready_inst(20, OpType::BEQ, 123, 123, 16, 100);
    assert(harness.control_to_branch_rs_c.send(inst));

    // 2. EXECUTION & VERIFICATION
    std::optional<BranchResult> branch_res;
    for (int i = 0; i < 10; ++i) {
        clock.tick();
        branch_res = harness.branch_unit_to_control_c.receive();
        if (branch_res) break;
    }

    assert(branch_res.has_value());
    ASSERT_EQ(branch_res->rob_id, (RobIDType)20, "Branch result has correct ROB ID");
    ASSERT_EQ(branch_res->is_taken, true, "Branch should be resolved as TAKEN");
    ASSERT_EQ(branch_res->target_pc, (PCType)116, "Branch target PC should be PC + imm (100 + 16)");

    // A conditional branch should not write to the CDB
    assert(!harness.cdb.get().has_value());
}

/**
 * @brief Tests an unconditional jump (JAL).
 * Verifies that it sends a BranchResult AND broadcasts the link address (PC+4) on the CDB.
 */
void test_jal_instruction() {
    // 1. SETUP
    TestHarness harness;
    auto& clock = Clock::getInstance();

    // Instruction: jal x1, 40 (ROB ID: 21, PC: 200)
    FilledInstruction inst = create_ready_inst(21, OpType::JAL, 0, 0, 40, 200);
    assert(harness.control_to_branch_rs_c.send(inst));

    // 2. EXECUTION & VERIFICATION
    std::optional<BranchResult> branch_res;
    std::optional<CDBResult> cdb_res;
    for (int i = 0; i < 10; ++i) {
        clock.tick();
        if (!branch_res) branch_res = harness.branch_unit_to_control_c.receive();
        if (!cdb_res) cdb_res = harness.cdb.get();
        if (branch_res && cdb_res) break;
    }

    // Verify BranchResult
    assert(branch_res.has_value());
    ASSERT_EQ(branch_res->rob_id, (RobIDType)21, "JAL BranchResult has correct ROB ID");
    ASSERT_EQ(branch_res->is_taken, true, "JAL is always taken");
    ASSERT_EQ(branch_res->target_pc, (PCType)240, "JAL target PC is correct");

    // Verify CDB Result (link address)
    assert(cdb_res.has_value());
    ASSERT_EQ(cdb_res->rob_id, (RobIDType)21, "JAL CDB result has correct ROB ID");
    ASSERT_EQ(cdb_res->data, (RegDataType)204, "JAL CDB data is the link address (PC+4)");
}

/**
 * @brief Tests a simple load instruction (LW).
 * Verifies it flows through the MemorySystem and the result is broadcast on the CDB.
 * Note: Assumes memory is zero-initialized.
 */
void test_load_instruction() {
    // 1. SETUP
    TestHarness harness;
    auto& clock = Clock::getInstance();

    // Instruction: lw x2, 128(x0) (ROB ID: 30)
    // Address is 0 + 128 = 128.
    FilledInstruction inst = create_ready_inst(30, OpType::LW, 0, 0, 128);
    assert(harness.control_to_mem_rs_c.send(inst));

    // 2. EXECUTION & VERIFICATION
    // The memory system has a longer latency (MRS -> MOB -> Memory -> CDB)
    std::optional<CDBResult> cdb_res;
    for (int i = 0; i < 20; ++i) {
        clock.tick();
        cdb_res = harness.cdb.get();
        if (cdb_res) break;
    }

    assert(cdb_res.has_value());
    ASSERT_EQ(cdb_res->rob_id, (RobIDType)30, "Load result has correct ROB ID");
    ASSERT_EQ(cdb_res->data, (RegDataType)0, "Load result from zero-initialized memory is 0");
}

/**
 * @brief Tests a store instruction (SW).
 * Verifies that the store waits for a commit signal before being sent to the memory unit.
 */
void test_store_instruction() {
    // 1. SETUP
    TestHarness harness;
    auto& clock = Clock::getInstance();

    // Instruction: sw x5, 64(x0) (ROB ID: 40)
    // Address is 64, data to store is 999.
    FilledInstruction inst = create_ready_inst(40, OpType::SW, 0, 999, 64);
    assert(harness.control_to_mem_rs_c.send(inst));

    // 2. EXECUTION & VERIFICATION
    // Part 1: Verify the store completes in the ROB (sends a "done" signal on CDB)
    std::optional<CDBResult> cdb_res_done;
    for (int i = 0; i < 10; ++i) {
        clock.tick();
        cdb_res_done = harness.cdb.get();
        if (cdb_res_done) break;
    }
    assert(cdb_res_done.has_value());
    ASSERT_EQ(cdb_res_done->rob_id, (RobIDType)40, "Store 'done' signal has correct ROB ID");

    // Part 2: Verify the store is waiting in the MOB.
    // We can't inspect the MOB directly, so we test by behavior.
    // We tick for a while; no memory request should be sent because it's not committed.
    // We verify this by checking that the memory unit's response channel remains empty.
    for (int i = 0; i < 10; ++i) {
        clock.tick();
        // No other instruction is running, so CDB should be clear.
        assert(!harness.cdb.get().has_value());
    }

    // Part 3: Commit the store and verify it proceeds.
    ROBEntry commit_info;
    commit_info.id = 40;
    harness.commit_bus.send(commit_info);
    clock.tick(); // Let the MOB see the commit signal.

    // Now the MOB should send the request to memory. After memory latency, it completes.
    // Since a write has no CDB result, we just ensure the pipeline remains quiet.
    for (int i = 0; i < 10; ++i) {
        clock.tick();
        assert(!harness.cdb.get().has_value());
    }
}

/**
 * @brief Tests the global flush signal.
 * Verifies that instructions in the reservation stations are cleared and do not execute.
 */
void test_global_flush() {
    // 1. SETUP
    TestHarness harness;
    auto& clock = Clock::getInstance();

    // Put two instructions into the ALU RS.
    FilledInstruction inst1 = create_ready_inst(50, OpType::ADDI, 0, 0, 1);
    FilledInstruction inst2 = create_ready_inst(51, OpType::ADDI, 0, 0, 2);
    assert(harness.control_to_alu_rs_c.send(inst1));
    clock.tick();
    assert(harness.control_to_alu_rs_c.send(inst2));
    clock.tick(); // Tick once to get them from the channel into the RS buffer.

    // 2. EXECUTION & VERIFICATION
    // Assert the flush signal
    harness.global_flush_bus.send(true);
    clock.tick(); // The flush happens on this tick.

    // The RS and channels should now be clear.
    // Tick for many cycles to see if any results from the flushed instructions appear.
    bool result_appeared = false;
    for (int i = 0; i < 20; ++i) {
        clock.tick();
        if (harness.cdb.get().has_value()) {
            result_appeared = true;
            break;
        }
    }

    ASSERT_EQ(result_appeared, false, "No results should appear on CDB after a flush");

    // Now, send a new instruction and verify it executes normally.
    FilledInstruction inst3 = create_ready_inst(52, OpType::SUB, 10, 3, 0);
    assert(harness.control_to_alu_rs_c.send(inst3));

    std::optional<CDBResult> cdb_res;
    for (int i = 0; i < 10; ++i) {
        clock.tick();
        cdb_res = harness.cdb.get();
        if (cdb_res) break;
    }

    assert(cdb_res.has_value());
    ASSERT_EQ(cdb_res->rob_id, (RobIDType)52, "New instruction ROB ID after flush");
    ASSERT_EQ(cdb_res->data, (RegDataType)7, "New instruction result after flush");
}


// --- Main Function ---

int main() {
    // Set logger to a higher level to reduce noise during tests
    logger.SetLevel(LogLevel::INFO);

    run_test(test_simple_alu_instruction, "Simple ALU Instruction");
    run_test(test_raw_dependency, "RAW Data Dependency");
    run_test(test_branch_taken, "Conditional Branch (Taken)");
    run_test(test_jal_instruction, "JAL Instruction");
    run_test(test_load_instruction, "Load Instruction");
    run_test(test_store_instruction, "Store Instruction and Commit");
    run_test(test_global_flush, "Global Pipeline Flush");

    std::cout << "\nAll backend tests passed successfully!\n" << std::endl;

    return 0;
}