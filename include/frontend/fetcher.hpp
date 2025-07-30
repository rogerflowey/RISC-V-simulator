#pragma once

#include "constants.hpp"
#include "logger.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include <cstdint>
#include <array>
#include <cstddef>

struct FetchResult{
    PCType pc;
    uint32_t instruction;
};

class Fetcher {
    Channel<PCType>& pc_chan;
    Bus<bool> &flush_bus;
    Bus<bool>& frontend_flush_bus;
    Channel<FetchResult>& instruction_chan;
    std::array<std::byte, MEMORY_SIZE>& unified_memory;

public:
    Fetcher(std::array<std::byte, MEMORY_SIZE>& memory,
            Channel<PCType>& pc_channel,
            Bus<bool>& flush_bus,
            Bus<bool>& frontend_flush_bus,
            Channel<FetchResult>& instruction_channel)
        : unified_memory(memory),
          pc_chan(pc_channel),
          flush_bus(flush_bus),
          frontend_flush_bus(frontend_flush_bus),
          instruction_chan(instruction_channel) {
            Clock::getInstance().subscribe([this]{this->work();});
    }

    void work(){
        if (frontend_flush_bus.get() || flush_bus.get()) {
            pc_chan.receive(); // Consume and discard the wrong-path PC.
            return;
        }
        if(!instruction_chan.can_send()){
            return;
        }
        if(auto pc = pc_chan.receive()){
            PCType addr = *pc;

            // Safety check for out-of-bounds access
            if (addr + 3 >= MEMORY_SIZE) {
                logger.Warn("Instruction fetch out of bounds at PC: " + std::to_string(addr));
                instruction_chan.send({addr, 0x00000000});
                return;
            }
            uint32_t inst =
                (static_cast<uint32_t>(std::to_integer<uint8_t>(unified_memory[addr + 3])) << 24) |
                (static_cast<uint32_t>(std::to_integer<uint8_t>(unified_memory[addr + 2])) << 16) |
                (static_cast<uint32_t>(std::to_integer<uint8_t>(unified_memory[addr + 1])) << 8)  |
                (static_cast<uint32_t>(std::to_integer<uint8_t>(unified_memory[addr])));

            logger.With("pc",*pc).With("Inst",inst).Info("Fetched Instruction");
            instruction_chan.send({*pc, inst});
        }
    }
};