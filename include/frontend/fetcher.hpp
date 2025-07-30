#pragma once

#include "constants.hpp"
#include "logger.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include <cstdint>
#include <vector>



//this is the Entrance of instruction, it is not pure Hardware design.
struct FetchResult{
    PCType pc;
    uint32_t instruction;
};

class Fetcher {
    //input
    Channel<PCType>& pc_chan;
    Bus<bool> &flush_bus; 
    Bus<bool>& frontend_flush_bus;
    //output
    Channel<FetchResult>& instruction_chan;
    //internal
    std::vector<uint32_t>& instruction_buffer;

public:
    Fetcher(Channel<PCType>& pc_channel, Bus<bool>& flush_bus, Bus<bool>& frontend_flush_bus, Channel<FetchResult>& instruction_channel, std::vector<uint32_t>& instruction_buffer)
        : pc_chan(pc_channel), flush_bus(flush_bus), frontend_flush_bus(frontend_flush_bus), instruction_chan(instruction_channel), instruction_buffer(instruction_buffer) {
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
            auto inst = instruction_buffer[(*pc)/4];

            logger.With("pc",*pc).With("Inst",inst).Info("Fetched Instruction");
            instruction_chan.send({*pc, inst});
        }
    }
};