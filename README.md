# RISC-V Simulator

A C++ simulator for a 32-bit RISC-V processor, featuring a speculative out-of-order superscalar core based on the Tomasulo algorithm.

## Features

*   **Out-of-Order Core:** Implements the Tomasulo algorithm for dynamic instruction scheduling.
*   **Speculative Execution:** Utilizes a Re-Order Buffer (ROB) and branch prediction to execute instructions speculatively.
*   **Detailed Pipeline Model:** The architecture is modularized into three main stages:
    *   **Front-End:** Fetches, decodes, and predicts branches.
    *   **Middle-End:** Handles instruction dispatch, register renaming, and in-order retirement.
    *   **Back-End:** Executes instructions out-of-order using Reservation Stations (RS) and a Common Data Bus (CDB).
*   **Memory Subsystem:** Includes a Load-Store Buffer (LSB) to manage memory operations and ensure correct ordering.


## Architecture Deep Dive

The simulator models a modern pipelined CPU divided into three main conceptual sections: the **Front-End**, the **Middle-End (Allocation & Commit Core)**, and the **Back-End**.

### 1. The Front-End (In-Order)

The Front-End's mission is to fetch instructions, predict the direction of control flow, and decode them into micro-operations (uOps) for the Back-End.

*   **PC Generation Logic:** A dedicated logic block responsible for selecting the next Program Counter (PC). It arbitrates between three sources with the following priority:
    1.  **`FLUSH` (Highest Priority):** An override signal from the ROB (in the Middle-End) due to a branch misprediction or exception. This resets the pipeline to a known-correct PC.
    2.  **`PREDICT`:** A predicted target address for a branch, provided by the Decode stage.
    3.  **`INCREMENT` (Lowest Priority):** The default sequential address (`PC + 4`).
*   **Fetch:** Reads raw instruction bits from memory based on the PC provided by the PC Generation Logic. It "stamps" the instruction with its PC.
*   **Decode & Predict Stage:** This is a combined logical stage that performs two tasks in parallel on a fetched instruction:
    *   **Decode:** Parses the raw instruction bits into a structured format (opcode, registers, immediate).
    *   **Predict:** If the instruction is a branch, the Branch Predictor is consulted. The prediction outcome and target are stamped onto the instruction packet, and a `PREDICT` request is sent to the PC Generation Logic.
    *   **Note on Realism:** *This combined stage is a functional simplification. A higher-performance CPU would use a Branch Target Buffer (BTB) in the Fetch stage to make predictions earlier. This can be implemented as a future optimization.*

### 2. The Middle-End (Allocation & Commit Core)

This section acts as the bridge between the in-order Front-End and the out-of-order Back-End. It manages the instruction lifecycle, register renaming, and ensures in-order retirement. Due to practical constraints, this part uses **Combinational Logic**, meaning modules interact via method calls rather than channels.

*   **Rename/Dispatch Unit (Control):** This is the final stage of the Front-End and the entry point to the Middle-End. For each decoded instruction, it:
    1.  Allocates a new entry in the Re-Order Buffer (ROB).
    2.  Allocates an entry in a corresponding Reservation Station (RS) or the Load-Store Buffer (LSB).
    3.  Performs register renaming using the Register Alias Table (RAT) to resolve data dependencies, finding either a ready value or a ROB tag for each source operand.
    4.  Updates the RAT to map the instruction's destination register to its new ROB tag.
*   **Re-Order Buffer (ROB):** The central data structure for managing all in-flight instructions.
    *   It stores the complete state of each instruction, including its original PC, destination register, prediction data, and eventual result.
    *   It commits instructions from its head in strict program order, writing results to the Architectural Register File.
    *   It detects branch mispredictions by comparing the stored `predicted_outcome` with the `actual_outcome` received from the Back-End, triggering a `FLUSH` if they mismatch.
*   **Register Alias Table (RAT) & Architectural Register File (ARF):** The core components for managing register state. The RAT holds the speculative mappings, while the ARF holds the final, committed state.

### 3. The Back-End (Out-of-Order)

The Back-End's mission is to execute micro-ops as soon as their operands are ready, regardless of their original program order.

*   **Reservation Stations (RS):** A distributed set of buffers, one for each class of functional unit (e.g., ALU, FPU). They hold micro-ops from the Rename stage and "watch" the CDB for their source operands to become available.
*   **Execution Units (EU):** The functional units that perform the actual calculations (ALU operations, etc.).
*   **Load-Store Buffer (LSB) & Memory Order Buffer (MOB):**
    *   The **LSB** acts as a specialized Reservation Station for all memory operations, calculating effective addresses.
    -   The **MOB** receives fully-formed memory requests from the LSB. It is a FIFO queue responsible for ensuring correct memory ordering. Loads are executed when they reach the head. Stores wait at the head until they are committed by the ROB to prevent speculative memory writes.
*   **Common Data Bus (CDB):** A broadcast bus that distributes results from the Execution Units. Each result consists of a `value` and the `ROB tag` of the instruction that produced it. The RS, ROB, and RAT listen to the CDB.

## Implementation Notes & Design Decisions

*   **CDB Arbitration:** To solve CDB contention, the simulator uses a two-stage channel system. Execution units each write to their own dedicated output channel. A central CDB arbiter then selects one result per cycle to broadcast on the main CDB.
*   **Memory Ordering:** The LSB/MOB division separates address calculation from memory ordering. The MOB enforces a simple in-order execution for memory operations for initial correctness.
*   **Rollback Handling:** The Rename/Dispatch unit is responsible for fetching initial register values/tags. This centralizes the logic and simplifies state restoration on a rollback, as the RAT and ROB are the primary structures that need to be managed.
*   **Instruction "Wrapping":** The simulator models the pipeline by "wrapping" an instruction with more metadata as it flows through the stages. Distinct C++ `structs` are used for the data packet at each major pipeline interface (e.g., `FetchedInstr`, `DecodedInstr`, `MicroOp`).

## Future Work

*   **Enhanced Branch Prediction:** Implement a more advanced Branch Target Buffer (BTB) in the Fetch stage for earlier predictions.
*   **Store-to-Load Forwarding:** Add logic to the MOB to allow loads to receive data directly from pending stores, improving performance.
*   **Handling Memory Ambiguities:** Implement mechanisms to resolve potential memory ordering conflicts between loads and stores.