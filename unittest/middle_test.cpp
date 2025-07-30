#include <iostream>
#include <cassert>
#include <string>

// Assume all these headers are available and correct, including the modified control.hpp
#include "middlend/control.hpp"
#include "constants.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "backend/cdb.hpp"
#include "backend/units/branch.hpp"
#include "instruction.hpp"
#include "middlend/rob.hpp"
#include "utils/logger/logger.hpp"

// --- Test Helper Functions ---

Instruction create_add_inst(RegIDType rd, RegIDType rs1, RegIDType rs2, PCType pc) {
    return {OpType::ADD, pc, rd, rs1, rs2, 0, false, false};
}

Instruction create_beq_inst(RegIDType rs1, RegIDType rs2, int32_t imm, PCType pc, bool predicted_taken) {
    return {OpType::BEQ, pc, 0, rs1, rs2, std::bit_cast<RegDataType>(imm), true, predicted_taken};
}

// --- SIMPLIFIED HELPER FUNCTION ---
// As requested, this function now only advances the clock. All component work
// functions are triggered automatically via their clock subscriptions.
void tick(const std::string& message = "") {
    Clock::getInstance().tick();
    std::cout << "--- Cycle " << Clock::getInstance().getTime() << ": " << message << " ---" << std::endl;
}

void run_test_suite() {
    // --- Test 1: Simple ALU Instruction and Commit Flow ---
    std::cout << "\n===== Running Test 1: Simple ALU and Commit =====\n";
    {
        // 1. Setup
        Clock::getInstance().reset();
        Channel<Instruction> ins_c;
        Channel<BranchResult> branch_res_c;
        Bus<bool> flush_b;
        CommonDataBus cdb(flush_b);
        Channel<CDBResult> alu_to_cdb_c;
        cdb.connect(alu_to_cdb_c);
        Channel<FilledInstruction> alu_c, mem_c, branch_c;
        Bus<ROBEntry> commit_b;
        Channel<PCType> flush_pc_c;
        Control control(ins_c, branch_res_c, alu_c, mem_c, branch_c, commit_b, flush_b, flush_pc_c, cdb);

        // 2. Issue an instruction: add x1, x0, x0 (pc=0)
        ins_c.send(create_add_inst(1, 0, 0, 0));
        tick("Send ADD to Control unit"); // Cycle 1: ins_c writer_slot is filled.

        tick("Control issues ADD to ALU"); // Cycle 2: Control reads ins_c, sends to alu_c.
        auto fetched = alu_c.receive();
        assert(fetched.has_value() && "Instruction should be dispatched to ALU channel");
        assert(fetched->id == 1 && "ROB ID should be 1");

        // 4. Simulate ALU execution and send result to its output channel
        alu_to_cdb_c.send({fetched->id, 42});
        tick("ALU sends result to CDB channel"); // Cycle 3: alu_to_cdb_c writer_slot filled.

        tick("Control processes CDB, ROB entry ready"); // Cycle 5: Control reads CDB, updates ROB state to COMMIT.

        tick("Control commits instruction"); // Cycle 6: Control sees ROB entry is ready and at head, commits it.
        auto committed = commit_b.get();
        assert(committed.has_value() && "Instruction should be on commit bus");
        assert(committed->id == 1 && "Committed ROB ID should be 1");
        assert(committed->value == 42 && "Committed value should be 42");

        // 7. Verify Register File update
        ins_c.send(create_add_inst(2, 1, 0, 4)); // add x2, x1, x0
        tick("Send dependent ADD"); // Cycle 7

        tick("Issue dependent ADD"); // Cycle 8
        auto fetched2 = alu_c.receive();
        assert(fetched2.has_value() && "Second instruction should be dispatched");
        assert(fetched2->q_rs1 == 0 && "Dependency q_rs1 should be resolved from RegFile");
        assert(fetched2->v_rs1 == 42 && "Value for rs1 (x1) should be read correctly");

        std::cout << "===== Test 1 Passed =====\n";
    }

    // --- Test 2: RAW Dependency and ROB Forwarding ---
    std::cout << "\n===== Running Test 2: RAW Dependency & Forwarding =====\n";
    {
        // 1. Setup
        Clock::getInstance().reset();
        Channel<Instruction> ins_c;
        Channel<BranchResult> branch_res_c;
        Bus<bool> flush_b;
        CommonDataBus cdb(flush_b);
        Channel<CDBResult> alu_to_cdb_c;
        cdb.connect(alu_to_cdb_c);
        Channel<FilledInstruction> alu_c, mem_c, branch_c;
        Bus<ROBEntry> commit_b;
        Channel<PCType> flush_pc_c;
        Control control(ins_c, branch_res_c, alu_c, mem_c, branch_c, commit_b, flush_b, flush_pc_c, cdb);

        // 2. Issue a pair of dependent instructions
        ins_c.send(create_add_inst(1, 0, 0, 0)); // add x1, x0, x0
        tick("Send ADD x1"); // Cycle 1
        tick("Issue ADD x1"); // Cycle 2
        auto fetched1 = alu_c.receive();
        assert(fetched1.has_value() && fetched1->id == 1);

        ins_c.send(create_add_inst(2, 1, 0, 4)); // add x2, x1, x0
        tick("Send dependent ADD x2"); // Cycle 3
        tick("Issue dependent ADD x2"); // Cycle 4
        auto fetched2 = alu_c.receive();
        assert(fetched2.has_value() && "Dependent instruction should be dispatched");
        assert(fetched2->q_rs1 == 1 && "q_rs1 should hold ROB ID of producer instruction (1)");

        // 3. Simulate first instruction finishing
        alu_to_cdb_c.send({1, 100}); // Result for x1 is 100
        tick("ADD x1 result (100) sent to CDB channel"); // Cycle 5
        tick("CDB broadcasts result"); // Cycle 6
        tick("Control processes CDB, ROB entry 1 ready"); // Cycle 7
        // At the start of cycle 7, ROB entry 1 is marked COMMIT with value 100.

        // 4. Issue a third instruction to test ROB forwarding
        ins_c.send(create_add_inst(3, 1, 0, 8)); // add x3, x1, x0
        tick("Send ADD x3"); // Cycle 8
        tick("Issue ADD x3, testing forwarding"); // Cycle 9
        auto fetched3 = alu_c.receive();
        assert(fetched3.has_value() && "Third instruction should be dispatched");
        assert(fetched3->q_rs1 == 0 && "Dependency should be resolved via forwarding");
        assert(fetched3->v_rs1 == 100 && "Value for rs1 (x1) should be forwarded from ROB");

        std::cout << "===== Test 2 Passed =====\n";
    }

    // --- Test 3: Branch Misprediction and Flush ---
    std::cout << "\n===== Running Test 3: Branch Misprediction & Flush =====\n";
    // Final, Corrected Test Code for Test 3

// ... setup ...

// --- Test 3: Branch Misprediction and Flush ---
std::cout << "\n===== Running Test 3: Branch Misprediction & Flush =====\n";
{
    // 1. Setup
    Clock::getInstance().reset();
    Channel<Instruction> ins_c;
    Channel<BranchResult> branch_res_c;
    Channel<CDBResult> alu_to_cdb_c;
    Channel<FilledInstruction> alu_c, mem_c, branch_c;
    Bus<ROBEntry> commit_b;
    Bus<bool> flush_b;
    CommonDataBus cdb(flush_b);
    cdb.connect(alu_to_cdb_c);
    Channel<PCType> flush_pc_c;
    Control control(ins_c, branch_res_c, alu_c, mem_c, branch_c, commit_b, flush_b, flush_pc_c, cdb);

    // 2. Issue a branch (predicted not taken)
    ins_c.send(create_beq_inst(0, 0, 20, 0, false));
    tick("Send BEQ"); // Cycle 1
    tick("Issue BEQ"); // Cycle 2
    assert(branch_c.receive()->id == 1);

    // 3. The Frontend, operating speculatively, fetches and sends the next instruction
    //    This instruction is on the wrong path.
    ins_c.send(create_add_inst(5, 0, 0, 4));
    tick("Frontend speculatively sends wrong-path ADD"); // Cycle 3
    // At the end of cycle 3, the ADD is in the ins_c writer slot.

    // 4. Control issues the wrong-path ADD. The branch is still executing.
    tick("Control issues wrong-path ADD"); // Cycle 4
    assert(alu_c.receive()->id == 2);
    // At this point, the pipeline has two speculative instructions.

    // 5. The Frontend sends ANOTHER speculative instruction. At the same time,
    //    the branch unit finishes and reports the misprediction.
    ins_c.send(create_add_inst(9,9,9,8)); // This is the instruction that will be caught in the channel.
    branch_res_c.send({1, true, 20});
    tick("Branch unit reports misprediction; Frontend sends another wrong-path instruction"); // Cycle 5
    // At the end of cycle 5:
    // - The branch result is in its reader slot.
    // - The new ADD (9,9,9) is in the ins_c writer slot.

    // 6. Control processes the branch result, commits the branch, and sends the flush signal.
    //    It does NOT process the new ADD instruction because it's busy with the branch result and commit.
    tick("Control processes branch result, commits, and triggers flush"); // Cycle 6
    // In this cycle: Control commits the BEQ and sends `true` to `flush_bus`.
    // At the falling edge of this cycle: The ADD (9,9,9) moves to the ins_c reader slot.
    assert(commit_b.get().has_value());
    assert(flush_b.get().has_value());

    // 7. The FLUSH cycle. No new instructions are sent by the (now squashed) Frontend.
    tick("Flush cycle"); // Cycle 7
    // At the start of cycle 7:
    // - `Control::work()` sees `flush_bus.get()` is true.
    // - It calls `control.flush()`.
    // - `control.flush()` calls `ins_channel.receive()`.
    // - This `receive()` call successfully finds and consumes the ADD (9,9,9) that was waiting in the reader slot.

    // 8. Verify the flush was successful.
    assert(!ins_c.peek().has_value() && "Instruction channel should be flushed");
    assert(!flush_b.get().has_value() && "Flush signal should be consumed and gone");

    // 9. Verify recovery
    // The Frontend would now be fetching from the correct PC (20).
    ins_c.send(create_add_inst(10, 0, 0, 20));
    tick("Frontend sends correct-path instruction"); // Cycle 8
    tick("Control issues correct-path instruction"); // Cycle 9
    auto fetched_correct = alu_c.receive();
    assert(fetched_correct.has_value() && "Should be able to issue after flush");
    assert(fetched_correct->ins.pc == 20 && "PC should be the corrected one");
    assert(fetched_correct->id == 1 && "ROB ID should reset to 1 after flush");

    std::cout << "===== Test 3 Passed =====\n";
}
}

int main() {
    logger.SetLevel(LogLevel::INFO);
    run_test_suite();
    std::cout << "\n[SUCCESS] All middle-end tests passed!\n";
    return 0;
}