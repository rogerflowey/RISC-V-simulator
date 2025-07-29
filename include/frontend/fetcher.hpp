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
    Channel<PCType>& pc_chan;
    std::vector<uint32_t> instruction_buffer;
    Channel<FetchResult>& instruction_chan;


public:
    Fetcher(Channel<PCType>& pc_channel, std::vector<uint32_t>& instruction_buffer, Channel<FetchResult>& instruction_channel)
        : pc_chan(pc_channel), instruction_buffer(instruction_buffer), instruction_chan(instruction_channel) {
            Clock::getInstance().subscribe([this]{this->work();});
        }

    void work(){
        if(!instruction_chan.can_send()){
            return;
        }
        if(auto pc = pc_chan.receive()){
            logger.With("pc",*pc).Info("Fetched PC");
            auto inst = instruction_buffer[*pc];
            instruction_chan.send({*pc, inst});
        }
    }
};