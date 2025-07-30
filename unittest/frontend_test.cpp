#include <iostream>
#include <vector>
#include <cassert>
#include <string>

// Assume all necessary headers are included here.
// This avoids cluttering the test file itself.
#include "frontend/frontend.hpp"
#include "instruction.hpp"
#include "middlend/rob.hpp"
#include "utils/bus.hpp"
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

// --- Test Scenarios ---

/**
 * @brief Tests basic sequential instruction fetching and decoding.
 * The pipeline should process a simple stream of instructions without any control flow changes.
 */
void test_sequential_execution() {
    // 1. SETUP
    // Program:
    // 0x0: addi x1, x0, 5
    // 0x4: addi x2, x0, 1
    // 0x8: addi x3, x0, 2
    std::vector<uint32_t> instructions = {
        0x00500093, // PC=0
        0x00100113, // PC=4
        0x00200193, // PC=8
        0x00000000  // NOP
    };
    // Note: The instruction buffer is indexed by PC, so we need to pad it.
    instructions.resize(12);

    Channel<Instruction> decoded_instruction_c;
    Channel<PCType> mispredict_flush_pc_c;
    Bus<bool> mispredict_flush_signal_bus;
    Bus<ROBEntry> commit_bus;

    Frontend frontend(instructions, decoded_instruction_c, mispredict_flush_pc_c, mispredict_flush_signal_bus, commit_bus);
    auto& clock = Clock::getInstance();

    // 2. EXECUTION & VERIFICATION
    // The frontend has a 3-stage pipeline: PC -> Fetch -> Decode
    // So the first instruction will appear on the output channel after 3 clock ticks.

    // Cycle 1: PCLogic sends PC=0 to Fetcher
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 2: Fetcher gets PC=0, fetches instruction. PCLogic sends PC=4.
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 3: Decoder gets instruction from PC=0. Fetcher gets PC=4. PCLogic sends PC=8.
    clock.tick();
    auto inst1 = decoded_instruction_c.receive();
    assert(inst1.has_value());
    ASSERT_EQ(inst1->pc, (PCType)0, "Inst 1 PC");
    ASSERT_EQ(inst1->op, OpType::ADDI, "Inst 1 Opcode");

    // Cycle 4: Decoder gets instruction from PC=4.
    clock.tick();
    auto inst2 = decoded_instruction_c.receive();
    assert(inst2.has_value());
    ASSERT_EQ(inst2->pc, (PCType)4, "Inst 2 PC");
    ASSERT_EQ(inst2->op, OpType::ADDI, "Inst 2 Opcode");

    // Cycle 5: Decoder gets instruction from PC=8.
    clock.tick();
    auto inst3 = decoded_instruction_c.receive();
    assert(inst3.has_value());
    ASSERT_EQ(inst3->pc, (PCType)8, "Inst 3 PC");
    ASSERT_EQ(inst3->op, OpType::ADDI, "Inst 3 Opcode");
}

/**
 * @brief Tests that the entire frontend pipeline stalls if the output channel is blocked.
 */
void test_stall_condition() {
    // 1. SETUP
    std::vector<uint32_t> instructions = { 0x00500093, 0x00100113 };
    instructions.resize(8);

    Channel<Instruction> decoded_instruction_c;
    Channel<PCType> mispredict_flush_pc_c;
    Bus<bool> mispredict_flush_signal_bus;
    Bus<ROBEntry> commit_bus;

    Frontend frontend(instructions, decoded_instruction_c, mispredict_flush_pc_c, mispredict_flush_signal_bus, commit_bus);
    auto& clock = Clock::getInstance();

    // 2. EXECUTION & VERIFICATION
    // Fill the pipeline
    clock.tick(); // PCLogic sends PC=0
    clock.tick(); // Fetcher gets PC=0
    clock.tick(); // Decoder gets inst from PC=0. Output channel is now ready to be read.

    // At the end of cycle 3, the first instruction is in the output channel.
    // We will NOT read it, simulating a stall from the next pipeline stage.

    // Cycle 4: Stall
    // Decoder.work() should see `output_c.can_send()` is false and return.
    // Fetcher.work() should see its output channel is full and return.
    // PCLogic.work() should see its output channel is full and return.
    clock.tick();
    // The instruction from PC=0 should still be in the output channel, unread.
    auto peek_inst = decoded_instruction_c.peek();
    assert(peek_inst.has_value());
    ASSERT_EQ(peek_inst->pc, (PCType)0, "Stalled instruction PC should be 0");

    // Cycle 5: Still stalled
    clock.tick();
    peek_inst = decoded_instruction_c.peek();
    assert(peek_inst.has_value());
    ASSERT_EQ(peek_inst->pc, (PCType)0, "Stalled instruction PC should still be 0");

    // Now, "unstall" the pipeline by reading the instruction.
    auto inst1 = decoded_instruction_c.receive();
    assert(inst1.has_value());
    ASSERT_EQ(inst1->pc, (PCType)0, "Received stalled instruction PC");

    // Cycle 6: Pipeline resumes. The next instruction (from PC=4) should now be in the output.
    clock.tick();
    auto inst2 = decoded_instruction_c.receive();
    assert(inst2.has_value());
    ASSERT_EQ(inst2->pc, (PCType)4, "Next instruction PC after stall");
    ASSERT_EQ(inst2->op, OpType::ADDI, "Next instruction Opcode after stall");
}

/**
 * @brief Tests the handling of a JAL instruction.
 * The Decoder should predict the jump, flush the frontend, and redirect the PC.
 */
void test_jal_prediction() {
    // 1. SETUP
    // Program:
    // 0x0: jal x1, 8  (jumps to PC=8)
    // 0x4: addi x2, x0, 99 (wrong path, should be flushed)
    // 0x8: addi x3, x0, 1
    std::vector<uint32_t> instructions;
    instructions.resize(12);
    instructions[0] = 0x008000EF; // jal x1, 8
    instructions[1] = 0x06300113; // addi x2, x0, 99
    instructions[2] = 0x00100193; // addi x3, x0, 1

    Channel<Instruction> decoded_instruction_c;
    Channel<PCType> mispredict_flush_pc_c;
    Bus<bool> mispredict_flush_signal_bus;
    Bus<ROBEntry> commit_bus;

    Frontend frontend(instructions, decoded_instruction_c, mispredict_flush_pc_c, mispredict_flush_signal_bus, commit_bus);
    auto& clock = Clock::getInstance();

    // 2. EXECUTION & VERIFICATION
    // Cycle 1: PCLogic sends PC=0
    clock.tick();
    // Cycle 2: Fetcher gets JAL (PC=0). PCLogic sends PC=4 (speculative).
    clock.tick();
    // Cycle 3: Decoder gets JAL. Fetcher gets wrong-path inst (PC=4). PCLogic sends PC=8.
    // At end of cycle 3, Decoder processes JAL:
    // - Sends decoded JAL to output.
    // - Sends flush signal on `frontend_flush_bus`.
    // - Sends predicted target PC=8 to `decode_to_pc_pred_c`.
    clock.tick();
    auto inst_jal = decoded_instruction_c.receive();
    assert(inst_jal.has_value());
    ASSERT_EQ(inst_jal->pc, (PCType)0, "JAL PC");
    ASSERT_EQ(inst_jal->op, OpType::JAL, "JAL Opcode");

    // Cycle 4:
    // - PCLogic sees predicted PC=8 and uses it. Sends PC=8 to Fetcher.
    // - Fetcher sees flush signal, discards its input (the wrong-path PC=4).
    // - Decoder sees flush signal, discards its input (the wrong-path instruction from PC=4).
    clock.tick();
    // No instruction should be output this cycle because of the flush.
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 5: Fetcher gets new instruction from correct PC=8. PCLogic sends PC=12.
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 6: Decoder gets instruction from PC=8.
    clock.tick();
    auto inst_target = decoded_instruction_c.receive();
    assert(inst_target.has_value());
    ASSERT_EQ(inst_target->pc, (PCType)8, "Target instruction PC");
    ASSERT_EQ(inst_target->op, OpType::ADDI, "Target instruction Opcode");
}

/**
 * @brief Tests a conditional branch that is initially predicted as "not taken".
 * Also tests the predictor update mechanism.
 *//**
 * @brief Tests a conditional branch that is initially predicted as "not taken",
 * followed by a misprediction recovery and a correct "taken" prediction.
 */
/**
 * @brief Tests a conditional branch that is initially predicted as "not taken",
 * followed by a misprediction recovery and a correct "taken" prediction.
 */
void test_conditional_branch_and_update() {
    // 1. SETUP
    std::vector<uint32_t> instructions;
    instructions.resize(16);
    instructions[0] = 0x00000463; // beq x0, x0, 8
    instructions[1] = 0x00100113; // addi x2, x0, 1
    instructions[2] = 0x00200193; // addi x3, x0, 2

    Channel<Instruction> decoded_instruction_c;
    Channel<PCType> mispredict_flush_pc_c;
    Bus<bool> mispredict_flush_signal_bus;
    Bus<ROBEntry> commit_bus;

    Frontend frontend(instructions, decoded_instruction_c, mispredict_flush_pc_c, mispredict_flush_signal_bus, commit_bus);
    auto& clock = Clock::getInstance();

    // --- PART 1: Predicted Not Taken ---
    std::cout << "  Part 1: Predicted Not Taken\n";
    clock.tick(); // Cycle 1
    clock.tick(); // Cycle 2
    clock.tick(); // Cycle 3: BEQ decoded, predicted not taken
    auto inst_beq = decoded_instruction_c.receive();
    assert(inst_beq.has_value() && inst_beq->pc == 0);
    ASSERT_EQ(inst_beq->predicted_taken, false, "BEQ should be predicted not taken initially");

    clock.tick(); // Cycle 4: Wrong-path ADDI decoded
    auto inst_seq = decoded_instruction_c.receive();
    assert(inst_seq.has_value() && inst_seq->pc == 4);

    // --- PART 2: Update Predictor & Backend Flush ---
    std::cout << "  Part 2: Update Predictor & Backend Flush\n";
    // Send signals before cycle 5. They will be visible during cycle 6.
    ROBEntry commit_info;
    commit_info.pc = 0;
    commit_info.is_branch = true;
    commit_info.is_taken = true;
    commit_bus.send(commit_info);

    mispredict_flush_signal_bus.send(true);
    mispredict_flush_pc_c.send(0);

    clock.tick(); // Cycle 5: Pipeline continues on wrong path while signals propagate.
    decoded_instruction_c.receive(); // Consume wrong-path inst from PC=8.

    // --- PART 3: Re-fetch and Predict Taken ---
    std::cout << "  Part 3: Re-fetch and Predict Taken\n";

    // Cycle 6: Backend flush signal is visible. PCLogic gets PC=0. Pipeline is cleared.
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 7: Fetcher gets PC=0 and fetches BEQ.
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 8: Decoder gets BEQ, predicts TAKEN, and sends it to output.
    clock.tick();
    auto inst_beq_retaken = decoded_instruction_c.receive();
    assert(inst_beq_retaken.has_value());
    ASSERT_EQ(inst_beq_retaken->pc, (PCType)0, "BEQ PC on second fetch");
    ASSERT_EQ(inst_beq_retaken->predicted_taken, true, "BEQ should now be predicted taken");

    // Cycle 9: BUBBLE 1. The Decoder's internal flush (from cycle 8) propagates.
    // PCLogic gets the new target PC=8.
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 10: BUBBLE 2. Fetcher gets PC=8 and fetches the target instruction.
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 11: The correct target instruction (from PC=8), decoded in cycle 10, is now available.
    clock.tick();
    auto inst_target = decoded_instruction_c.receive();
    assert(inst_target.has_value());
    ASSERT_EQ(inst_target->pc, (PCType)8, "Correct branch target PC");
    ASSERT_EQ(inst_target->op, OpType::ADDI, "Correct branch target Opcode");
}
/**
 * @brief Tests a full pipeline flush triggered by a backend misprediction signal.
 *//**
 * @brief Tests a full pipeline flush triggered by a backend misprediction signal.
 */
/**
 * @brief Tests a full pipeline flush triggered by a backend misprediction signal.
 */
void test_backend_mispredict_flush() {
    // 1. SETUP
    std::vector<uint32_t> instructions;
    instructions.resize(20);
    instructions[0] = 0x00100093; // addi x1, x0, 1
    instructions[1] = 0x00200113; // addi x2, x0, 2
    instructions[2] = 0x00300193; // addi x3, x0, 3
    // PC=12 is a NOP
    instructions[4] = 0x00400213; // PC=16: addi x4, x0, 4 (Correct path)

    Channel<Instruction> decoded_instruction_c;
    Channel<PCType> mispredict_flush_pc_c;
    Bus<bool> mispredict_flush_signal_bus;
    Bus<ROBEntry> commit_bus;

    Frontend frontend(instructions, decoded_instruction_c, mispredict_flush_pc_c, mispredict_flush_signal_bus, commit_bus);
    auto& clock = Clock::getInstance();

    // 2. EXECUTION & VERIFICATION
    // Fill the pipeline with wrong-path instructions
    clock.tick(); // Cycle 1: PCLogic -> 0
    clock.tick(); // Cycle 2: Fetcher -> 0, PCLogic -> 4
    clock.tick(); // Cycle 3: Decoder -> 0, Fetcher -> 4, PCLogic -> 8
    auto inst1 = decoded_instruction_c.receive();
    assert(inst1.has_value() && inst1->pc == 0);

    // At the start of Cycle 4, the instruction from PC=4 is in the Decoder's input.
    // Send flush signal before cycle 4. It will be visible during Cycle 5.
    mispredict_flush_signal_bus.send(true);
    mispredict_flush_pc_c.send(16);

    // Cycle 4: The flush signal is NOT yet visible. The Decoder processes the
    // instruction from PC=4 that was already in its input channel.
    clock.tick();
    auto inst2_wrong = decoded_instruction_c.receive();
    assert(inst2_wrong.has_value());
    ASSERT_EQ(inst2_wrong->pc, (PCType)4, "Wrong-path instruction from PC=4 should be decoded");

    // Cycle 5: The flush signal is now visible.
    // - PCLogic gets the flush PC=16 and sends it to the Fetcher.
    // - Decoder sees the flush signal and discards its input (inst from PC=8).
    // - No instruction is output.
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 6: Fetcher gets PC=16 and fetches the correct instruction.
    clock.tick();
    assert(!decoded_instruction_c.receive().has_value());

    // Cycle 7: Decoder gets the instruction from PC=16 and sends it to the output.
    clock.tick();
    auto inst_correct = decoded_instruction_c.receive();
    assert(inst_correct.has_value());
    ASSERT_EQ(inst_correct->pc, (PCType)16, "PC after backend flush");
    ASSERT_EQ(inst_correct->op, OpType::ADDI, "Opcode after backend flush");
    ASSERT_EQ(inst_correct->rd, 4, "Destination register of correct instruction");
}

// --- Main Function ---

int main() {
    // Set logger to a higher level to reduce noise during tests
    logger.SetLevel(LogLevel::INFO);

    run_test(test_sequential_execution, "Sequential Execution");
    run_test(test_stall_condition, "Pipeline Stall Condition");
    run_test(test_jal_prediction, "JAL Prediction and Frontend Flush");
    run_test(test_conditional_branch_and_update, "Conditional Branch and Predictor Update");
    run_test(test_backend_mispredict_flush, "Backend Misprediction Flush");

    std::cout << "\nAll frontend tests passed successfully!\n" << std::endl;

    return 0;
}