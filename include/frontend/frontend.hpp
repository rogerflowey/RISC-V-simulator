#pragma once

#include "decoder.hpp"
#include "fetcher.hpp"
#include "middlend/rob.hpp"
#include "pc.hpp"
#include "instruction.hpp"
#include "utils/bus.hpp"
#include "utils/bus.hpp"

class Frontend {
private:
    Channel<PCType> pc_to_fetch_c;
    Channel<FetchResult> fetch_to_decode_c;
    Channel<PCType> decode_to_pc_pred_c;
    Bus<bool> frontend_flush_bus;

    PCLogic pc_logic;
    Fetcher fetcher;
    Decoder decoder;

public:
    Frontend(
        std::array<std::byte, MEMORY_SIZE>& unified_memory,
        Channel<Instruction>& decoded_instruction_c,
        Channel<PCType>& mispredict_flush_pc_c,
        Bus<bool>& global_flush_bus,
        Bus<ROBEntry>& commit_bus
    ) : pc_logic(decode_to_pc_pred_c, mispredict_flush_pc_c, pc_to_fetch_c),
        fetcher(unified_memory, pc_to_fetch_c, global_flush_bus, frontend_flush_bus, fetch_to_decode_c), // Pass it to Fetcher
        decoder(decoded_instruction_c, fetch_to_decode_c, decode_to_pc_pred_c, global_flush_bus, frontend_flush_bus, commit_bus)
    {}
};