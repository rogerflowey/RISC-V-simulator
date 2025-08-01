# RISC-V Simulator

A C++ simulator for a 32-bit RISC-V processor, featuring a speculative out-of-order superscalar core based on the Tomasulo algorithm. This simulator is built with a strong emphasis on microarchitectural realism, modeling the flow of data and control through distinct, physically-motivated hardware modules.

## Architectural Philosophy

The simulator's design is guided by a strict **Worker/Holder** pattern to mirror the structure of a physical chip:

*   **Workers:** These are stateless, transformational pipeline stages (e.g., Fetch, Decode, Execute) that perform a specific task. They communicate with each other exclusively through **Channels**. In hardware, a Channel corresponds to a **pipeline register** (a set of flip-flops) that decouples stages.
*   **Holders:** These are stateful storage units (e.g., Re-Order Buffer, Register Files) that represent the machine's state. They are accessed by Workers via dedicated, limited **Ports**. A Port corresponds to a **specialized read/write interface** on a physical memory block (like an SRAM), and their limited number makes structural hazards an explicit part of the design.

This distinction is a guiding principle. However, when a stateful component is small and exclusively used by a single Worker (e.g., a branch predictor's history table), it is treated as an internal implementation detail of that Worker, rather than being exposed as a full Holder module with explicit ports.

## Features

*   **Out-of-Order Core:** Implements the Tomasulo algorithm for dynamic instruction scheduling.
*   **Speculative Execution:** Utilizes a Re-Order Buffer (ROB) and branch prediction to execute instructions speculatively.
*   **Detailed Pipeline Model:** The architecture is modularized into three main sections:
    *   **Front-End:** Fetches, decodes, and predicts branches.
    *   **Middle-End:** Handles instruction dispatch, register renaming, and in-order retirement.
    *   **Back-End:** Executes instructions out-of-order using Reservation Stations (RS) and a Common Data Bus (CDB).
*   **Memory Subsystem:** Includes a Memory Order Buffer (MOB) to manage memory operations and ensure correct ordering.

## Architecture Deep Dive

The simulator models a modern pipelined CPU divided into three main conceptual sections.

### 1. The Front-End (In-Order)

The Front-End's mission is to fetch instructions, predict the direction of control flow, and pass them down the pipeline.

*   **PC Generation Logic:** A dedicated logic block responsible for selecting the next Program Counter (PC). It arbitrates between three sources with the following priority:
    1.  **`FLUSH` (Highest Priority):** An override signal from the Middle-End due to a misprediction.
    2.  **`PREDICT`:** A predicted target address from the Decode stage.
    3.  **`INCREMENT` (Lowest Priority):** The default sequential address (`PC + 4`).
*   **Fetch:** Reads raw instruction bits from memory based on the PC provided by the PC Generation Logic.
*   **Decode & Predict Stage:** A combined logical stage that parses the instruction and, if it's a branch, consults a Branch Predictor.

### 2. The Middle-End (Allocation & Commit Core)

This section is the heart of the out-of-order machine, breaking the dependency chain. It is composed of two primary **Worker** stages (`Rename/Dispatch` and `Commit`) that orchestrate the core **Holder** modules.

*   **`Rename/Dispatch` Unit:**
    *   Receives decoded instructions from the Front-End.
    *   Allocates entries in the `ReorderBuffer` and `Load-Store Queue`.
    *   Performs register renaming by reading the `Register Alias Table` and querying the `ReorderBuffer`'s bypass port for ready operands.
    *   Dispatches the fully renamed instruction to the appropriate Reservation Station.
*   **`Commit` Unit:**
    *   Responsible for in-order retirement. It inspects the head of the `ReorderBuffer` each cycle.
    *   If an instruction is ready, it writes the result to the `Architectural Register File`.
    *   It detects branch mispredictions by comparing predicted and actual outcomes, triggering a pipeline flush when necessary.
*   **`ReorderBuffer (ROB)` & `RegisterFile (RAT/ARF)`:** These are the central **Holder** modules of the processor. They manage the speculative and architectural state of the machine, providing specialized read/write ports for renaming, bypassing, and committing.

### 3. The Back-End (Out-of-Order)

The Back-End's mission is to execute micro-ops as soon as their operands are ready.

*   **Reservation Stations (RS):** Distributed buffers that hold instructions. They "snoop" the CDB broadcast bus, waiting for their source operands to become available before dispatching to an Execution Unit.
*   **Execution Units (EU):** The functional units that perform calculations (ALU operations, etc.).
*   **Memory Order Buffer (MOB):** A queue that manages all memory operations. It accepts notification from Memory's RS to record the order, and ensures that loads and stores are issued in the correct order. Stores wait at the head until they are committed by the `Commit` stage to prevent speculative memory writes.
*   **Common Data Bus (CDB):** A broadcast bus that distributes results from the Execution Units. A central arbiter manages contention for the bus.

## Future Work

*   **Enhanced Branch Prediction:** Implement a more advanced Branch Target Buffer (BTB) in the Fetch stage for earlier predictions.
*   **Store-to-Load Forwarding:** Add logic to the MOB to allow loads to receive data directly from pending stores, improving performance.
*   **Handling Memory Ambiguities:** Implement mechanisms to resolve potential memory ordering conflicts between loads and stores.